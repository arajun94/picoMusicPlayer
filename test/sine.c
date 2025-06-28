#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/uart.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "pico/float.h"
#include "pico/stdlib.h"

#include "i2s.pio.h"

#define CPU_FREQ 200000000

#define DATA_PIN 20
#define CLOCK_PIN_BASE 18

#define DAC_SAMPLING_RATE 44100
#define DAC_BIT_DEPTH 16

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

	//gpio initialize
	uint8_t func = GPIO_FUNC_PIO0;
	gpio_set_function(DATA_PIN, func);
	gpio_set_function(CLOCK_PIN_BASE, func);
	gpio_set_function(CLOCK_PIN_BASE + 1, func);

	uart_init(UART_ID, 115200);
    gpio_set_function(UART_TX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_TX_PIN));
    gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_RX_PIN));
	uart_set_hw_flow(UART_ID, false, false);

	//i2s initialize
	const PIO pio = pio0;
	const uint sm = 0;
	uint offset = pio_add_program(pio, &i2s_program);
	double div = CPU_FREQ / (DAC_SAMPLING_RATE * DAC_BIT_DEPTH * 2 * 2);
	i2s_program_init(pio, sm, offset, div, DATA_PIN, CLOCK_PIN_BASE);

	int16_t *sine_wave_table = malloc((1 << TABLE_BIT_DEPTH) * sizeof(int16_t));
	uint32_t i;
	for (i = 0; i < 1 << TABLE_BIT_DEPTH; i++)
		sine_wave_table[i] = sin(2 * i * M_PI / (1 << TABLE_BIT_DEPTH)) * 32767;

    int16_t value;
	uint16_t t=0;
	while (1) {
		value = sine_wave_table[t]/4;
		t+=440*(1 << TABLE_BIT_DEPTH)/DAC_SAMPLING_RATE;
		pio_sm_put_blocking(pio, sm, value);
	}
}