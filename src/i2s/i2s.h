#ifndef I2S_H
#define I2S_H

#include <stdint.h>
#include "hardware/pio.h"

void i2s_init();

void i2s_write(int32_t value);

void i2s_write_blocking(int32_t value);

uint8_t i2s_ready();

#endif