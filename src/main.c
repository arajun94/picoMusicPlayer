#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"

#include "i2s.h"

#include "hw_config.h"
#include "f_util.h"
#include "ff.h"

#define CPU_FREQ 200000000

typedef struct {
	char riff[4];        // "RIFF"
	uint32_t size;      // ファイルサイズ - 8
	char data[4];       // "WAVE"
} Riff;
Riff* riff;

typedef struct {
	uint16_t wFormatTag;          // フォーマットタグ (1: PCM)
	uint16_t nChannels;           // チャンネル数 (1: モノラル, 2: ステレオ)
	uint32_t nSamplesPerSec;      // サンプリングレート (例: 44100)
	uint32_t nAvgBytesPerSec;     // 平均バイト/秒 (サンプリングレート * チャンネル数 * ビット深度 / 8)
	uint16_t nBlockAlign;         // ブロックアライメント (チャンネル数 * ビット深度 / 8)
	uint16_t wBitsPerSample;      // ビット深度 (例: 16)
} WaveFormat;


UINT out_stream (   /* 戻り値: 転送されたバイト数またはストリームの状態 */
    const BYTE *p,  /* 転送するデータを指すポインタ */
    UINT btf        /* >0: 転送を行う(バイト数). 0: ストリームの状態を調べる */
)
{
	uint32_t cnt = 0;
	if(btf == 0) {
		return i2s_ready();
	}
	do {
		i2s_write(*(int16_t*)(p+cnt) << 16);
		cnt+=2;
	}while(i2s_ready() && cnt < btf);
    return cnt;
}



int main()
{
	stdio_init_all();
	set_sys_clock_khz(CPU_FREQ / 1000, true);

	i2s_init();

	//スタートボタンが押されるまで待機
	gpio_init(15);
    gpio_set_dir(15, GPIO_IN);
    while (!gpio_get(15));

	printf("start\n");

	//マウント
    FATFS fs;
    FRESULT fr = f_mount(&fs, "", 1);
    if (FR_OK != fr) {
        panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    }

    // ファイルオープン
    FIL wav;
	fr = f_open(&wav, "sound.wav", FA_READ);
    if (FR_OK != fr && FR_EXIST != fr) {
        panic("f_open(sound.wav) error: %s (%d)\n", FRESULT_str(fr), fr);
	}
	FIL log;
	fr = f_open(&log, "log.txt", FA_OPEN_APPEND | FA_WRITE);
    if (FR_OK != fr && FR_EXIST != fr) {
        panic("f_open(log.txt) error: %s (%d)\n", FRESULT_str(fr), fr);
	}

	//読み出し
	uint8_t buffer[256];
	int16_t* play_buffer;
	play_buffer = (int16_t*)calloc(1<<16, sizeof(int16_t));
	uint br;
	uint32_t i,j;
	fr = f_read(&wav, buffer, 12, &br);
	printf("f_read: %s\n", FRESULT_str(fr));
	printf("%d bytes are readed.\n", br);
	
	riff = (Riff*)malloc(sizeof(Riff));
	riff = (Riff*)buffer;
	printf("size:%d\n", riff->size);
	printf("\n");

	for(;;){
		fr = f_read(&wav, buffer, 8, &br);
		if(FR_OK != fr) {
			panic("f_read error: %s (%d)\n", FRESULT_str(fr), fr);
		}
		if(memcmp(buffer, "fmt ", 4)!=0){
			f_read(&wav, buffer, *(uint32_t*)(buffer+4) , &br);
			if(FR_OK != fr) {
				panic("f_read error: %s (%d)\n", FRESULT_str(fr), fr);
			}
		}else{
			WaveFormat* waveformat;
			waveformat = (WaveFormat*)malloc(sizeof(WaveFormat));
			if(*(uint32_t*)(buffer+4) >= 16){
				f_read(&wav, waveformat, 16, &br);
				if(FR_OK != fr) {
					panic("f_read error: %s (%d)\n", FRESULT_str(fr), fr);
				}
				/*if((*(uint32_t*)buffer+4)>16){
					f_read(&wav, buffer, (*(uint32_t*)buffer+4)-16, &br);
					if(FR_OK != fr) {
						panic("f_read error: %s (%d)\n", FRESULT_str(fr), fr);
					}
				}*/
				printf("wFormatTag: %d\n", waveformat->wFormatTag);
				printf("nChannels: %d\n", waveformat->nChannels);
				printf("nSamplesPerSec: %d\n", waveformat->nSamplesPerSec);
				printf("nAvgBytesPerSec: %d\n", waveformat->nAvgBytesPerSec);
				printf("nBlockAlign: %d\n", waveformat->nBlockAlign);
				printf("wBitsPerSample: %d\n", waveformat->wBitsPerSample);
			}else{
				panic("unsupported wave format size: %d\n", *(uint32_t*)(buffer+4));
			}
			break;
		}
	}


	
	for(;;){
		fr = f_read(&wav, buffer, 8, &br);
		if(FR_OK != fr) {
			panic("f_read error: %s (%d)\n", FRESULT_str(fr), fr);
		}
		if(memcmp(buffer, "data", 4)!=0){
			f_read(&wav, buffer, *(uint32_t*)(buffer+4) , &br);
			if(FR_OK != fr) {
				panic("f_read error: %s (%d)\n", FRESULT_str(fr), fr);
			}
		}else{
			while(1){
				fr = f_forward(&wav, out_stream, 1<<8, &br);
				if(FR_OK != fr) {
					panic("f_read error: %s (%d)\n", FRESULT_str(fr), fr);
				}
			}
			break;
		}
	}

	free(play_buffer);

    // Close the file
	fr = f_close(&wav);
    if (FR_OK != fr) {
        printf("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
    }
	fr = f_close(&log);
    if (FR_OK != fr) {
        printf("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
    }

    // Unmount the SD card
    f_unmount("");

    puts("Goodbye, world!");

}