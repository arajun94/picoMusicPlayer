#include "i2s.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "config.h"

#if (I2S_BITS == 16) && !I2S_STEREO
#include "i2s16m.pio.h"
#elif (I2S_BITS == 32) && I2S_STEREO
#include "i2s32s.pio.h"
#else 
#error "Unsupported DAC configuration"
#endif

void i2s_init() {
    //gpio initialize
	gpio_set_function(I2S_DATA_PIN, GPIO_FUNC_PIO0);
	gpio_set_function(I2S_CLOCK_PIN_BASE, GPIO_FUNC_PIO0);
	gpio_set_function(I2S_CLOCK_PIN_BASE + 1, GPIO_FUNC_PIO0);

    uint offset = pio_add_program(I2S_PIO, &i2s_program);
    double div = CPU_FREQ / (I2S_SAMPLING_RATE * I2S_BITS * 2 * 2);
    i2s_program_init(I2S_PIO, I2S_SM, offset, div, I2S_DATA_PIN, I2S_CLOCK_PIN_BASE);
}

void i2s_write(int32_t value) {
    pio_sm_put_blocking(I2S_PIO, I2S_SM, value);
}

uint8_t i2s_ready() {
    return !pio_sm_is_tx_fifo_full(I2S_PIO, I2S_SM);
}