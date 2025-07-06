#ifndef I2S_CONFIG_H
#define I2S_CONFIG_H

#include "hardware/pio.h"

//I2S
#define I2S_DATA_PIN 0               //DIN
#define I2S_CLOCK_PIN_BASE 6         //LRCLK、BCLK
#define I2S_SAMPLING_RATE 48000
#define I2S_BITS 32              //16 or 32
#define I2S_STEREO 1                 //0 or 1
#define I2S_PIO pio0
#define I2S_SM 0

#endif