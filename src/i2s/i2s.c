#include "i2s.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "i2s_config.h"

#if (DAC_BITS == 16) && !STEREO
#include "i2s16m.pio.h"
#elif (DAC_BITS == 32) && STEREO
#include "i2s32s.pio.h"
#else 
#error "Unsupported DAC configuration"
#endif

void i2s_init() {
    //gpio initialize
	gpio_set_function(DATA_PIN, GPIO_FUNC_PIO0);
	gpio_set_function(CLOCK_PIN_BASE, GPIO_FUNC_PIO0);
	gpio_set_function(CLOCK_PIN_BASE + 1, GPIO_FUNC_PIO0);

    //i2s initialize
    const PIO pio = pio0;
    const uint sm = 0;
    uint offset = pio_add_program(pio, &i2s_program);
    double div = CPU_FREQ / (DAC_SAMPLING_RATE * DAC_BITS * 2 * 2);
    i2s_program_init(pio, sm, offset, div, DATA_PIN, CLOCK_PIN_BASE);
}

void i2s_write(int32_t value) {
    const PIO pio = pio0;
    const uint sm = 0;
    pio_sm_put_blocking(pio, sm, value);
}