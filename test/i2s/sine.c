#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/uart.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "pico/float.h"
#include "pico/stdlib.h"

#include "i2s.h"
#include "i2s_config.h"

#define CPU_FREQ 200000000

#define TABLE_BIT_DEPTH 16

#define UART_ID uart0
#define BAUD_RATE 115200
#define UART_TX_PIN 0
#define UART_RX_PIN 1

int main()
{
	stdio_init_all();
	set_sys_clock_khz(CPU_FREQ / 1000, true);
	//multicore_launch_core1(core1_entry);

	i2s_init();

	uart_init(UART_ID, 115200);
    gpio_set_function(UART_TX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_TX_PIN));
    gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_RX_PIN));
	uart_set_hw_flow(UART_ID, false, false);

	int16_t *sine_wave_table = malloc((1 << TABLE_BIT_DEPTH) * sizeof(int16_t));
	uint32_t i;
	for (i = 0; i < 1 << TABLE_BIT_DEPTH; i++)
		sine_wave_table[i] = sin(2 * i * M_PI / (1 << TABLE_BIT_DEPTH)) * 32767;

    int32_t value;
	uint16_t t=0;
	while (1) {
		value = sine_wave_table[t]/2;
		t+=440*(1 << TABLE_BIT_DEPTH)/DAC_SAMPLING_RATE;
		i2s_write(value<<16);
	}
}