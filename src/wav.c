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

#include "play.h"

static uint8_t* playBuffer;
static int32_t** playBuffer32;
static WaveFormat* waveformat;
static uint8_t playBuffer32_index = 0;

Metadata wav_init(FIL* play_file) {
    uint32_t buffer[256];
    uint32_t i;
    UINT br;
    
    //メモリ確保
    playBuffer = (uint8_t*)malloc(PLAY_BUF_SIZE*sizeof(uint8_t)*4);
    playBuffer32 = (int32_t**)malloc(2*sizeof(int32_t*));
    playBuffer32[0] = (int32_t*)malloc(PLAY_BUF_SIZE*sizeof(int32_t));
    playBuffer32[1] = (int32_t*)malloc(PLAY_BUF_SIZE*sizeof(int32_t));
	Riff* riff = (Riff*)malloc(sizeof(Riff));

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
    if(waveformat->wBitsPerSample%8!=0 || waveformat->wBitsPerSample>I2S_BITS){//I2S_BITSより大きい、バイト単位でない
        panic("unsupported wave format.\n");
    }
    if(waveformat->wFormatTag!=1){//PCMでない
        panic("unsupported wave format.\n");
    }

    //dataチャンクまで飛ばす
    while(f_read(play_file, buffer, 8, &br)==FR_OK && br == 8 && memcmp(buffer, "data", 4)!=0){
        f_lseek(play_file, f_tell(play_file)+buffer[1]);
    }
    return (Metadata){
        .bitDeps = waveformat->wBitsPerSample,
        .channels = waveformat->nChannels,
        .samplingRrate = waveformat->nSamplesPerSec
    };
}

int32_t* wav_read(FIL* play_file, Metadata metadata){
    UINT br;
    uint16_t i;
    uint8_t sampleBytes = metadata.bitDeps/8;
    playBuffer32_index^=1;

    f_read(play_file, playBuffer, PLAY_BUF_SIZE * sampleBytes, &br);
    for(i=0; i<PLAY_BUF_SIZE; i++){
        memcpy(&playBuffer32[playBuffer32_index][i], playBuffer + i * sampleBytes, sampleBytes);
        playBuffer32[playBuffer32_index][i] = playBuffer32[playBuffer32_index][i] << (I2S_BITS - metadata.bitDeps) >> (I2S_BITS - metadata.bitDeps);
    }
    return playBuffer32[playBuffer32_index];
}