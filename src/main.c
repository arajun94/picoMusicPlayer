#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

#include "hw_config.h"
#include "f_util.h"
#include "file_stream.h"
#include "ff.h"

#include "play.h"
#include "config.h"

#define MAX_FILES 16

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

    char** files;
    uint16_t i;
    files = (char**)malloc(MAX_FILES*sizeof(char*));
    for(i=0;i<MAX_FILES;i++){
        files[i] = (char*)malloc((FF_MAX_LFN+1)*sizeof(char));
    }

    DIR dir;
    FILINFO fno;
    f_opendir(&dir, "");
    for (i=0;;i++) {
        f_readdir(&dir, &fno);           /* Read a directory item */
        if (fno.fname[0] == 0) break;          /* Error or end of dir */
        if (!(fno.fattrib & AM_DIR)) {            /* It is a directory */
            sprintf(files[i],"%s", fno.fname);
        }
    }
    f_closedir(&dir);

    uint16_t filesIndex = 1;
    uint16_t filesNum = i;

    printf("filesNum:%d\n",filesNum);

	play(files[filesIndex]);

    while(1){
        if(gpio_get(17)){
            if(isPlaying())stop();
            else start();
            while(gpio_get(17));
        }
        if(gpio_get(18)){
            play_abort();
            filesIndex--;
            if(filesIndex<1)filesIndex=filesNum-1;
            play(files[filesIndex]);
            while(gpio_get(18));
        }
        if(gpio_get(16)){
            play_abort();
            filesIndex++;
            if(filesIndex>=filesNum)filesIndex=1;
            play(files[filesIndex]);
            while(gpio_get(16));
        }
        if(isEnded()){
            play_abort();
            filesIndex++;
            if(filesIndex>=filesNum)filesIndex=1;
            play(files[filesIndex]);
        }
        sleep_ms(100);
    }

    // Unmount the SD card
    f_unmount("");

    puts("Goodbye, world!");

}