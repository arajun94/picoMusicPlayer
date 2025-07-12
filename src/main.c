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

    
	//スタートボタンが押されるまで待機
	gpio_init(29);
    gpio_set_dir(29, GPIO_IN);
    while (!gpio_get(29));

	printf("start\n");

    FATFS fs;
    FRESULT fr = f_mount(&fs, "", 1);
    if (FR_OK != fr) {
        panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    }

	play("sound.wav");

    while(1){
        tight_loop_contents();
    }

    // Unmount the SD card
    f_unmount("");

    puts("Goodbye, world!");

}