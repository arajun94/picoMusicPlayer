#include "wav.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "pico/stdlib.h"

#include "i2s.h"
#include "i2s_config.h"

#include "config.h"

#include "hw_config.h"
#include "f_util.h"
#include "file_stream.h"
#include "ff.h"

static uint8_t* play_buffer;
static int32_t* play_buffer32;
static Riff* riff;
static WaveFormat* waveformat;

void wav_init(FILE* play_file) {
    uint32_t buffer[256];
    uint32_t i;
        //メモリ確保
    play_buffer = (uint8_t*)malloc(PLAY_BUF_SIZE*sizeof(uint8_t));
    play_buffer32 = (int32_t*)malloc(PLAY_BUF_SIZE*sizeof(int32_t));
	riff = (Riff*)malloc(sizeof(Riff));

    // RIFFチャンクの読み込み
	fread(riff, 12, 1, play_file);


    //fmtチャンクまで飛ばす
    while(fread(buffer, 4, 2, play_file)==2 && memcmp(buffer, "fmt ", 4)!=0){
        fseek(play_file, buffer[1], SEEK_CUR);
    }

    //fmtチャンクの読み込み
    if(buffer[1] >= 16){
        waveformat = (WaveFormat*)malloc(sizeof(WaveFormat));
        fread(waveformat, 16, 1, play_file);
        if(buffer[1]>16){//fmtチャンクのサイズが16より大きい場合飛ばす
            fseek(play_file, buffer[1]-16, SEEK_CUR);
        }
    }else{
        panic("unsupported wave format size: %d\n", buffer[1]);
    }


    //dataチャンクまで飛ばす
    while(fread(buffer, 4, 2, play_file)==2 && memcmp(buffer, "data", 4)!=0){
        fseek(play_file, buffer[1], SEEK_CUR);
    }
}

int32_t* wav_read(FILE* play_file){
    uint32_t i;
    if(waveformat->wBitsPerSample==16){
        fread(play_buffer, 1, PLAY_BUF_SIZE*2, play_file);
        for(i=0; i<PLAY_BUF_SIZE; i++){
            play_buffer32[i] = *(int16_t*)(play_buffer+i*2) << 16;
        }
    }else if(waveformat->wBitsPerSample==24){
        fread(play_buffer, 3, PLAY_BUF_SIZE, play_file);
        for(i=0; i<PLAY_BUF_SIZE; i++){
            play_buffer32[i] = play_buffer[i*3+2]<<8 | play_buffer[i*3+1]<<16 | play_buffer[i*3]<<24;
        }
    }else if(waveformat->wBitsPerSample==32){
        fread(play_buffer, 4, PLAY_BUF_SIZE, play_file);
        for(i=0; i<PLAY_BUF_SIZE; i++){
            play_buffer32[i] = play_buffer[i*4+3] | play_buffer[i*4+2]<<8 | play_buffer[i*4+1]<<16 | play_buffer[i*4]<<24;
        }
    }
    return play_buffer32;
}