#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

#include "hw_config.h"
#include "f_util.h"
#include "file_stream.h"
#include "ff.h"

#include "play.h"
#include "config.h"

int main()
{
	stdio_init_all();
	set_sys_clock_khz(CPU_FREQ / 1000, true);

    gpio_init(2);
    gpio_set_dir(2, GPIO_OUT);
    gpio_put(2, 1);

    
    gpio_init(16);
    gpio_init(17);
    gpio_init(18);

    //スタートボタンが押されるまで待機
    gpio_set_dir(16, GPIO_IN);
    gpio_set_dir(17, GPIO_IN);
    gpio_set_dir(18, GPIO_IN);
    while (!gpio_get(17));
    while(gpio_get(17));

    FATFS fs;
    FRESULT fr = f_mount(&fs, "", 1);
    if (FR_OK != fr) {
        panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    }

	play("sound.wav");

    while(1){
        if(gpio_get(17)){
            if(isPlaying())stop();
            else start();
            printf("isPlaying:%d\n",isPlaying());
            while(gpio_get(17));
        }
        if(isEnded()){
            restart();
        }
    }

    // Unmount the SD card
    f_unmount("");

    puts("Goodbye, world!");

}