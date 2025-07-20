#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"

#include "hw_config.h"
#include "f_util.h"
#include "file_stream.h"
#include "ff.h"

#include "play.h"
#include "config.h"

#define MAX_FILES 128


void quickSort(char **files, uint32_t length){
    if(length <= 1)return;
    uint32_t left,right;
    char* pivot;
    char* tmp;
    
    left = 0;
    right = length-1;
    pivot = files[0];
    
    while(1){
        while(left<length && strcmp(files[left],pivot)<=0)left++;
        while(left<right && strcmp(files[right],pivot)>=0)right--;
        
        if(left==right || left==length)break;
        tmp = files[left];
        files[left] = files[right];
        files[right] = tmp;
    }
    files[0] = files[left-1];
    files[left-1] = pivot;
    
    quickSort(files, left-1);
    quickSort(files+left, length-left);
}




int main()
{
	stdio_init_all();
	set_sys_clock_khz(CPU_FREQ / 1000, true);

    //電源ランプ
    gpio_init(2);
    gpio_set_dir(2, GPIO_OUT);
    gpio_put(2, 1);

    //操作ボタン初期化
    gpio_init(16);
    gpio_init(17);
    gpio_init(18);
    gpio_init(19);
    gpio_set_dir(16, GPIO_IN);
    gpio_set_dir(17, GPIO_IN);
    gpio_set_dir(18, GPIO_IN);
    gpio_set_dir(19, GPIO_IN);

    adc_init();
    adc_gpio_init(28);                //ADC0(GPIO26)の端子設定
    adc_select_input(2);

    //中央ボタンが押されるまで待機
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

    uint16_t filesIndex = 0;
    uint16_t filesNum = i-1;
    uint8_t tBlink = 0;
    double vsysVoltage;

    files = &files[1];

    quickSort(files, filesNum);

    printf("play_start\n");
	play(files[filesIndex]);

    while(1){
        vsysVoltage = (double)adc_read()*3.3*2/4096;
        if(gpio_get(17)){
            if(isStopped()){
                start();
            }else{
                stop();
            }
            while(gpio_get(17));
        }
        if(gpio_get(18)){
            if(filesIndex==0){
                filesIndex=filesNum-1;
            }else{
                filesIndex--;
            }
            play_abort();
            play(files[filesIndex]);
            while(gpio_get(18));
        }
        if(gpio_get(16)){
            if(filesIndex>=filesNum-1){
                filesIndex=0;
            }else{
                filesIndex++;
            }
            play_abort();
            play(files[filesIndex]);
            while(gpio_get(16));
        }
        if(gpio_get(19)){
            skip(10);
        }
        if(isEnded()){
            if(filesIndex==0){
                filesIndex=filesNum;
            }else{
                filesIndex--;
            }
            play_abort();
            play(files[filesIndex]);
        }
        if(vsysVoltage<3.0){
            gpio_put(2, (tBlink++)%8);
        }
        sleep_ms(100);
    }

    // Unmount the SD card
    f_unmount("");
}