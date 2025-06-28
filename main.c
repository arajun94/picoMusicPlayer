/* main.c
Copyright 2021 Carl John Kugler III

Licensed under the Apache License, Version 2.0 (the License); you may not use
this file except in compliance with the License. You may obtain a copy of the
License at

   http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an AS IS BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied. See the License for the
specific language governing permissions and limitations under the License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "pico/float.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "i2s_audio.pio.h"

#include "hw_config.h"
#include "f_util.h"
#include "ff.h"


#define CPU_FREQ 200000000

#define DATA_PIN 20
#define CLOCK_PIN_BASE 18
#define BUTTON_PIN 13

#define SAMPLING_RATE 44100
#define DAC_BIT_DEPTH 16

#define TABLE_BIT_DEPTH 10

#define KEY_POLY 12
#define OP 2

#define UART_ID uart0
#define BAUD_RATE 115200
#define UART_TX_PIN 0
#define UART_RX_PIN 1

int main() {
    // Initialize stdio
    stdio_init_all();

    //スイッチ入力があるまで待機(usbシリアルコンソールの監視の開始まで)
    gpio_init(15);
    gpio_set_dir(15, GPIO_IN);
    while (!gpio_get(15));

    puts("Hello, world!");

    // See FatFs - Generic FAT Filesystem Module, "Application Interface",
    // http://elm-chan.org/fsw/ff/00index_e.html
    FATFS fs;
    FRESULT fr = f_mount(&fs, "", 1);
    if (FR_OK != fr) {
        panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    }
    printf("fs_type:%d\n", fs.fs_type);

    // Open a file and write to it
    FIL fil;
    const char* const filename = "test.txt";
    fr = f_open(&fil, filename, FA_OPEN_APPEND | FA_WRITE);
    if (FR_OK != fr && FR_EXIST != fr) {
        panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
    }
    if (f_printf(&fil, "Hello, world!\n") < 0) {
        printf("f_printf failed\n");
    }

    // Close the file
    fr = f_close(&fil);
    if (FR_OK != fr) {
        printf("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
    }

    // Unmount the SD card
    f_unmount("");

    puts("Goodbye, world!");

}





typedef struct {
	uint8_t l_curve;  // AB A:curve form B:direction
	uint8_t r_curve;  // AB A:curve form B:direction
	uint8_t depth;
	uint8_t breakpoint;
} Level_Scaling;

typedef struct {
	int16_t level[4];
	int32_t rate[4];
	int32_t slope[4];
	uint8_t rate_scaling;
	Level_Scaling level_scaling;
} EG;

typedef struct {
	int16_t volume;
	uint8_t wave_form;	// 0:sine,1:rectangle,2:saw,3:triangule,4:noiz
	EG eg;
	uint16_t freq_scale;// /4096
} OP_Param;

typedef struct {
	uint8_t LPF;
	uint8_t HPF;
    uint8_t algorithm;
	uint8_t fb;
    EG pitch_eg;
    OP_Param op_params[OP+1];
} Tone_Param;

typedef struct {
	uint8_t note;
	uint16_t freq;
	uint16_t table_t[OP+1];  // 2*pi*t/65536
	uint32_t eg_t[OP+1];  // 0:pitch, 1~6:op
	uint8_t fb_count;
	bool release;
} Key_State;

typedef struct 
{
	uint8_t pitch;
	uint32_t start;
	uint32_t end;
} Note;


void note_start(Key_State *key_states, uint8_t note)
{
	uint8_t i;
	for(i=KEY_POLY-1;i>0;i--){
		key_states[i] = key_states[i-1];
	}
	key_states[0] = (Key_State){0};
	key_states[0].note = note;
}

void note_release(Key_State *key_states, uint8_t note)
{
	for (int i = 0; i < KEY_POLY; i++) {
		if (key_states[i].note == note && !key_states[i].release) {
			key_states[i].release = 1;
			return;
		}
	}
}

int16_t readout_table(int16_t *table, uint16_t t)  // t=0~65535
{
    static uint8_t cache = 16 - TABLE_BIT_DEPTH;
	return table[t >> cache];
}

uint16_t eg_calculate(EG eg, Key_State *key_state, uint8_t op_num)
{
    int32_t t = (*key_state).eg_t[op_num];
	uint32_t value;

    if (t <= eg.rate[0]) value = eg.slope[0]*t/32768+eg.level[3];
    else if (t <= eg.rate[1]) value = eg.slope[1]*(t - eg.rate[0])/32768+eg.level[0];
	else if (t < eg.rate[2]) value = eg.slope[2]*(t - eg.rate[1])/32768+eg.level[1];
	else if (t == eg.rate[2]) value = eg.level[2];
	else if (t <= eg.rate[3]) value = eg.slope[3]*(t - eg.rate[2])/32768+eg.level[2];
	else value = eg.level[3];

	if(((*key_state).release || t != eg.rate[2]) && t!=eg.rate[3]) (*key_state).eg_t[op_num]++;
	return value;
}

void eg_slope_calculate(EG *eg)
{
	(*eg).slope[0] = 32768*((*eg).level[0] - (*eg).level[3]) / (*eg).rate[0];
	(*eg).slope[1] = 32768*((*eg).level[1] - (*eg).level[0]) / ((*eg).rate[1] - (*eg).rate[0]);
	(*eg).slope[2] = 32768*((*eg).level[2] - (*eg).level[1]) / ((*eg).rate[2] - (*eg).rate[1]);
	(*eg).slope[3] = 32768*((*eg).level[3] - (*eg).level[2]) / ((*eg).rate[3] - (*eg).rate[2]);
}

int16_t operator(uint8_t algorithm[7][6], uint8_t op_num, int16_t *table,
				 Key_State *key_state, OP_Param *op_params)
{
	uint16_t t2 = (*key_state).table_t[op_num];
	if((*key_state).fb_count==0)(*key_state).table_t[op_num] +=op_params[op_num].freq_scale*(*key_state).freq/4096;

	if (algorithm[op_num][0] != 0) {
        uint8_t i = 0;
        while (algorithm[op_num][i] != 0) {
            if (op_num >= algorithm[op_num][i]) {
                if ((*key_state).fb_count >= 4){
					(*key_state).fb_count=0;
					break;
				}
                (*key_state).fb_count++;
            }
            t2 += operator(algorithm, algorithm[op_num][i], table, key_state, op_params);
            i++;
        }
    }
	return (op_params[op_num].volume*eg_calculate((*op_params).eg, key_state, op_num)/128)* readout_table(table, t2) / 32768;
}

void core1_entry() {

	uart_init(UART_ID, 115200);
    gpio_set_function(UART_TX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_TX_PIN));
    gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_RX_PIN));
	uart_set_hw_flow(UART_ID, false, false);
	int16_t data[3];
	uint8_t uart_order = 0;
	uint8_t ch,num;

	while(1){
		if(uart_is_readable(UART_ID)){
			ch = uart_getc(UART_ID);
			num = ch&((1<<6)-1);
			switch(uart_order%3){
				case 0:
					data[uart_order/3]=0;
					data[uart_order/3] |= num;
					break;
				case 1:
					data[uart_order/3] |= num<<6;
					break;
				case 2:
					data[uart_order/3] |= num<<12;
					break;
			}
			uart_order++;
			if(ch>>7){
				uart_order=0;
				//printf("%d,%d,%d\n",data[0],data[1],data[2]);
			}
		}
	}
}

int main()
{
	stdio_init_all();
	set_sys_clock_khz(CPU_FREQ / 1000, true);
	//multicore_launch_core1(core1_entry);

	//gpio initialize
	uint8_t func = GPIO_FUNC_PIO0;
	gpio_set_function(DATA_PIN, func);
	gpio_set_function(CLOCK_PIN_BASE, func);
	gpio_set_function(CLOCK_PIN_BASE + 1, func);

	adc_init();
	adc_gpio_init(26);
	adc_select_input(0);

	gpio_init(2);
	gpio_set_dir(2, GPIO_OUT);
	gpio_put(2, 1);

	for (uint8_t i = 0; i < 3; i++) {
		gpio_init(BUTTON_PIN + i);
		gpio_set_dir(BUTTON_PIN + i, GPIO_IN);
	}

	uart_init(UART_ID, 115200);
    gpio_set_function(UART_TX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_TX_PIN));
    gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_RX_PIN));
	uart_set_hw_flow(UART_ID, false, false);


	//i2s initialize
	const PIO pio = pio0;
	const uint sm = 0;
	uint offset = pio_add_program(pio, &i2s_audio_program);
	double div = CPU_FREQ / (SAMPLING_RATE * DAC_BIT_DEPTH * 2 * 2);
	i2s_audio_program_init(pio, sm, offset, div, DATA_PIN, CLOCK_PIN_BASE);

	/*********************変数の初期化*******************/
	Note musicSheet[192] = {
		{48,0,88200},
		{52,11025,88200},
		{55,22050,33075},
		{60,33075,44100},
		{64,44100,55125},
		{55,55125,66150},
		{60,66150,77175},
		{64,77175,88200},
		{48,88200,176400},
		{52,99225,176400},
		{55,110250,121275},
		{60,121275,132300},
		{64,132300,143325},
		{55,143325,154350},
		{60,154350,165375},
		{64,165375,176400},
		{48,176400,264600},
		{50,187425,264600},
		{57,198450,209475},
		{62,209475,220500},
		{65,220500,231525},
		{57,231525,242550},
		{62,242550,253575},
		{65,253575,264600},
		{48,264600,352800},
		{50,275625,352800},
		{57,286650,297675},
		{62,297675,308700},
		{65,308700,319725},
		{57,319725,330750},
		{62,330750,341775},
		{65,341775,352800},
		{47,352800,441000},
		{50,363825,441000},
		{55,374850,385875},
		{62,385875,396900},
		{65,396900,407925},
		{55,407925,418950},
		{62,418950,429975},
		{65,429975,441000},
		{47,441000,529200},
		{50,452025,529200},
		{55,463050,474075},
		{62,474075,485100},
		{65,485100,496125},
		{55,496125,507150},
		{62,507150,518175},
		{65,518175,529200},
		{48,529200,617400},
		{52,540225,617400},
		{55,551250,562275},
		{60,562275,573300},
		{64,573300,584325},
		{55,584325,595350},
		{60,595350,606375},
		{64,606375,617400},
		{48,617400,705600},
		{52,628425,705600},
		{55,639450,650475},
		{60,650475,661500},
		{64,661500,672525},
		{55,672525,683550},
		{60,683550,694575},
		{64,694575,705600},
		{48,705600,793800},
		{52,716625,793800},
		{57,727650,738675},
		{64,738675,749700},
		{69,749700,760725},
		{57,760725,771750},
		{64,771750,782775},
		{69,782775,793800},
		{48,793800,882000},
		{52,804825,882000},
		{57,815850,826875},
		{64,826875,837900},
		{69,837900,848925},
		{57,848925,859950},
		{64,859950,870975},
		{69,870975,882000},
		{48,882000,970200},
		{50,893025,970200},
		{54,904050,915075},
		{57,915075,926100},
		{62,926100,937125},
		{54,937125,948150},
		{57,948150,959175},
		{62,959175,970200},
		{48,970200,1058400},
		{50,981225,1058400},
		{54,992250,1003275},
		{57,1003275,1014300},
		{62,1014300,1025325},
		{54,1025325,1036350},
		{57,1036350,1047375},
		{62,1047375,1058400},
		{47,1058400,1146600},
		{50,1069425,1146600},
		{55,1080450,1091475},
		{62,1091475,1102500},
		{67,1102500,1113525},
		{55,1113525,1124550},
		{62,1124550,1135575},
		{67,1135575,1146600},
		{47,1146600,1234800},
		{50,1157625,1234800},
		{55,1168650,1179675},
		{62,1179675,1190700},
		{67,1190700,1201725},
		{55,1201725,1212750},
		{62,1212750,1223775},
		{67,1223775,1234800},
		{47,1234800,1323000},
		{48,1245825,1323000},
		{52,1256850,1267875},
		{55,1267875,1278900},
		{60,1278900,1289925},
		{52,1289925,1300950},
		{55,1300950,1311975},
		{60,1311975,1323000},
		{47,1323000,1411200},
		{48,1334025,1411200},
		{52,1345050,1356075},
		{55,1356075,1367100},
		{60,1367100,1378125},
		{52,1378125,1389150},
		{55,1389150,1400175},
		{60,1400175,1411200},
		{45,1411200,1499400},
		{48,1422225,1499400},
		{52,1433250,1444275},
		{55,1444275,1455300},
		{60,1455300,1466325},
		{52,1466325,1477350},
		{55,1477350,1488375},
		{60,1488375,1499400},
		{45,1499400,1587600},
		{48,1510425,1587600},
		{52,1521450,1532475},
		{55,1532475,1543500},
		{60,1543500,1554525},
		{52,1554525,1565550},
		{55,1565550,1576575},
		{60,1576575,1587600},
		{38,1587600,1675800},
		{45,1598625,1675800},
		{50,1609650,1620675},
		{54,1620675,1631700},
		{60,1631700,1642725},
		{50,1642725,1653750},
		{54,1653750,1664775},
		{60,1664775,1675800},
		{38,1675800,1764000},
		{45,1686825,1764000},
		{50,1697850,1708875},
		{54,1708875,1719900},
		{60,1719900,1730925},
		{50,1730925,1741950},
		{54,1741950,1752975},
		{60,1752975,1764000},
		{43,1764000,1852200},
		{47,1775025,1852200},
		{50,1786050,1797075},
		{55,1797075,1808100},
		{59,1808100,1819125},
		{50,1819125,1830150},
		{55,1830150,1841175},
		{59,1841175,1852200},
		{43,1852200,1940400},
		{47,1863225,1940400},
		{50,1874250,1885275},
		{55,1885275,1896300},
		{59,1896300,1907325},
		{50,1907325,1918350},
		{55,1918350,1929375},
		{59,1929375,1940400},
		{43,1940400,2028600},
		{47,1951425,2028600},
		{50,1962450,1973475},
		{55,1973475,1984500},
		{59,1984500,1995525},
		{50,1995525,2006550},
		{55,2006550,2017575},
		{59,2017575,2028600},
		{43,2028600,2116800},
		{47,2039625,2116800},
		{50,2050650,2061675},
		{55,2061675,2072700},
		{59,2072700,2083725},
		{50,2083725,2094750},
		{55,2094750,2105775},
		{59,2105775,2116800}
	};
	uint32_t sheet_t=0;
	uint16_t sheet_order=0;

	uint32_t freq_table[88];
	int16_t sine_wave_table[1 << TABLE_BIT_DEPTH];
	uint8_t algorithms[5][7][6] = {{{1, 3}, {2}, {0}, {4}, {5}, {6}, {6}},
									{{1, 3}, {2}, {2}, {4}, {5}, {6}, {0}},
									{{1, 4}, {2}, {3}, {0}, {5}, {6}, {6}},
									{{1, 4}, {2}, {3}, {0}, {5}, {6}, {4}},
									{{1}, {2}, {0}, {0}, {0}, {0}, {0}}};
									//{{carrier},{OP1},{OP2},{OP3},{OP4},{OP5},{OP6}}
	EG eg = {.level = {32767, 32768/2, 32768/4, 0},
			 .rate = {SAMPLING_RATE * 0.01, SAMPLING_RATE * 0.12,
					  SAMPLING_RATE * 0.2, SAMPLING_RATE*0.4},
			 .rate_scaling = 0};
	OP_Param op_params[OP+1];
	Tone_Param tone = {};

	bool button_old_state[3] = {0};
	bool button_state_temp;
	int16_t value = 0;
	Key_State key_states[KEY_POLY];
	int16_t i,j;

	int16_t pitch = 40;
	int16_t pitch_old = 40;
	uint16_t gain_t = 0;

	int16_t data[3];
	uint8_t uart_order = 0;
	uint8_t ch,num;


	for (i = 0; i < 88; i++) 
		freq_table[i] = 27.5 * pow(2, (double)(i + 1) / 12)*65536/SAMPLING_RATE;

	for (i = 0; i < 1 << TABLE_BIT_DEPTH; i++)
		sine_wave_table[i] = round(sin(2 * i * M_PI / (1 << TABLE_BIT_DEPTH)) * 32767/ (float)M_PI);
	
	eg_slope_calculate(&eg);

	for (i = 0; i < OP; i++) {
		op_params[i].eg = eg;
		op_params[i].volume=127;
		op_params[i].freq_scale=4096;
	}
	op_params[2].eg = (EG){.level = {32767, 32767, 32767, 0},
			 .rate = {SAMPLING_RATE * 0.1, SAMPLING_RATE * 0.13,
					  SAMPLING_RATE * 0.2, SAMPLING_RATE*0.4},
			 .rate_scaling = 0};
	eg_slope_calculate(&op_params[2].eg);

	for (i = 0; i < KEY_POLY; i++) {
		key_states[i]= (Key_State){0};
	}
	/*****************************************************************************/
	sleep_ms(2400);
	note_start(key_states,28);
	note_start(key_states,28+3);
	note_start(key_states,28+7);
	while (1) {
		if(uart_is_readable(UART_ID)){
			ch = uart_getc(UART_ID);
			num = ch&((1<<6)-1);
			if(uart_order%3==0)data[uart_order/3]=0;
			data[uart_order/3] |= num<<(uart_order%3)*6;

			uart_order++;
			if(ch>>7){
				uart_order=0;
				op_params[2].volume = data[0]/128;
				op_params[1].freq_scale = 4096;
				op_params[2].freq_scale = (data[1]+32768)/2;
				//pitch = ((uint)(data[2]+32768)>>10)+1;
				//eg_slope_calculate(&op_params[1].eg);
			}
		}
		/*if(pitch_old!=pitch){
			note_release(key_states,pitch_old);
			note_start(key_states,pitch);
		}*/
		/*
		if(sheet_t==0)sheet_order=0;
		for(i=sheet_order-KEY_POLY+1;i<=sheet_order;i++){
			j=i;
			if(i<0)j+=192;
			if(musicSheet[j].end==sheet_t){
				note_release(key_states,musicSheet[j].pitch);
			}
		}
		while(musicSheet[sheet_order].start<=sheet_t){
			note_start(key_states,musicSheet[sheet_order].pitch);
			sheet_order++;
		}*/
/*
		for (i = 0; i < 3; i++) {
			button_state_temp = gpio_get(BUTTON_PIN + i);
			if (button_state_temp ^ button_old_state[i]) {
				if (button_state_temp)note_start(key_states, i + 26);
				else note_release(key_states, i + 26);
			}
			button_old_state[i] = button_state_temp;
		}
*/
		value = 0;
		for (i = 0; i < KEY_POLY; i++) {
			if (key_states[i].note) {
				//key_states[i].freq = eg_calculate(eg, &key_states[i], 0)*freq_table[key_states[i].note];
				key_states[i].freq = freq_table[key_states[i].note]+32*readout_table(sine_wave_table, gain_t)/32768/256;
				value += operator(algorithms[4], algorithms[4][0][0], sine_wave_table, &key_states[i], op_params)/4;
			}
		}
		//pitch_old = pitch;
		gain_t+=8;
		sheet_t++;

		if(sheet_t ==2116801)sheet_t=0;
		pio_sm_put_blocking(pio, sm, value);
	}
}