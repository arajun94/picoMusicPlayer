#ifndef CONFIG_H
#define CONFIG_H

#include "hardware/pio.h"

#define CPU_FREQ 200000000

#define DATA_PIN 20              //DIN
#define CLOCK_PIN_BASE 18        //LRCLK、BCLK
#define DAC_SAMPLING_RATE 48000
#define DAC_BITS 32              //16 or 32
#define STEREO 1                 //0 or 1

#endif