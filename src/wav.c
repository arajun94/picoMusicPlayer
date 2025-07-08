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
static int32_t** play_buffer32;
static Riff* riff;
static WaveFormat* waveformat;
static uint8_t play_buffer32_index = 0;

void wav_init(FIL* play_file) {
    uint32_t buffer[256];
    uint32_t i;
    UINT br;
        //メモリ確保
    play_buffer = (uint8_t*)malloc(PLAY_BUF_SIZE*sizeof(uint8_t)*4);
    play_buffer32 = (int32_t**)malloc(2*sizeof(int32_t*));
    play_buffer32[0] = (int32_t*)malloc(PLAY_BUF_SIZE*sizeof(int32_t));
    play_buffer32[1] = (int32_t*)malloc(PLAY_BUF_SIZE*sizeof(int32_t));
	riff = (Riff*)malloc(sizeof(Riff));

    // RIFFチャンクの読み込み
	f_read(play_file, riff, 12, &br);


    //fmtチャンクまで飛ばす
    while(f_read(play_file, buffer, 8, &br)==FR_OK && memcmp(buffer, "fmt ", 4)!=0){
        f_lseek(play_file, f_tell(play_file)+buffer[1]);
    }

    //fmtチャンクの読み込み
    if(buffer[1] >= 16){
        waveformat = (WaveFormat*)malloc(sizeof(WaveFormat));
        f_read(play_file, waveformat, 16, &br);
        if(buffer[1]>16){//fmtチャンクのサイズが16より大きい場合飛ばす
            f_lseek(play_file, f_tell(play_file)-16);
        }
    }else{
        panic("unsupported wave format.\n");
    }
    if(waveformat->nChannels>2){//2chより多い
        panic("unsupported wave format.\n");
    }
    if(waveformat->wBitsPerSample%8!=0 || waveformat->wBitsPerSample>32){//32bitより大きい、バイト単位でない
        panic("unsupported wave format.\n");
    }
    if(waveformat->wFormatTag!=1){//PCMでない
        panic("unsupported wave format.\n");
    }

    //dataチャンクまで飛ばす
    while(f_read(play_file, buffer, 8, &br)==FR_OK && br == 8 && memcmp(buffer, "data", 4)!=0){
        f_lseek(play_file, f_tell(play_file)+buffer[1]);
    }
}

int32_t* wav_read(FIL* play_file){
    UINT br;
    uint16_t i, j;
    play_buffer32_index^=1;
    uint8_t sampleBytes = waveformat->wBitsPerSample/8;

    if(waveformat->nChannels==2){
        f_read(play_file, play_buffer, PLAY_BUF_SIZE*sampleBytes, &br);
        for(i=0; i<PLAY_BUF_SIZE; i++){
            memcpy(&play_buffer32[play_buffer32_index][i], play_buffer+i*sampleBytes, sampleBytes);
            play_buffer32[play_buffer32_index][i] = play_buffer32[play_buffer32_index][i] << 16;
        }
    }else{
        f_read(play_file, play_buffer, PLAY_BUF_SIZE*sampleBytes/2, &br);
        for(i=0; i<PLAY_BUF_SIZE; i++){
            memcpy(&play_buffer32[play_buffer32_index][i], play_buffer+i*sampleBytes/2, sampleBytes);
            play_buffer32[play_buffer32_index][i] = play_buffer32[play_buffer32_index][i] << 16;
        }
    }
    return play_buffer32[play_buffer32_index];
}