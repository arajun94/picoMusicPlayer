#include "play.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "pico/stdlib.h"

#include "i2s.h"
#include "i2s_config.h"

#include "hw_config.h"
#include "f_util.h"
#include "file_stream.h"
#include "ff.h"


static uint32_t dma_buf[PLAY_BUF_SIZE];
static int dma_chan;
static dma_channel_config dcfg;

static int16_t* play_buffer;
static Riff* riff;
static WaveFormat* waveformat;
static FILE *play_file;


void dma_handler() {
    dma_hw->ints0 = 1u << dma_chan;
    uint16_t i;
    UINT done;
    for(i=0; i<PLAY_BUF_SIZE; i++){
        dma_buf[i] = (int32_t)play_buffer[i]<<16;
        //printf("%d\n", play_buffer[i]);
    }
    dma_channel_configure(
        dma_chan, &dcfg,
        &I2S_PIO->txf[I2S_SM],
        dma_buf,
        PLAY_BUF_SIZE,
        true
    );
    if(fread(play_buffer, 2, PLAY_BUF_SIZE, play_file) != PLAY_BUF_SIZE){
        // EOFに達した場合、DMAを停止
        dma_channel_abort(dma_chan);
        dma_channel_set_irq0_enabled(dma_chan, false);
        irq_set_enabled(DMA_IRQ_0, false);
        fclose(play_file);
        free(play_buffer);
        free(riff);
        free(waveformat);
        puts("Playback finished.");
    }
}


void play (char* path){
	uint32_t buffer[256];
	uint32_t i,j;


    //メモリ確保
    play_buffer = (int16_t*)calloc(PLAY_BUF_SIZE, sizeof(int16_t));
	riff = (Riff*)malloc(sizeof(Riff));


    // ファイルを開く
    play_file = open_file_stream(path, "r");
	if (!play_file) {
		panic("Failed to open play_file: %s\n", path);
	}
    
    //stdバッファリング	
    static char vbuf[2048] __attribute__((aligned));
    int err = setvbuf(play_file, vbuf, _IOFBF, sizeof vbuf);


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

        // DMA設定
    dma_chan = dma_claim_unused_channel(true);
    dcfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dcfg, DMA_SIZE_32);
    channel_config_set_dreq(&dcfg, DREQ_PIO0_TX0);

    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    dma_channel_set_irq0_enabled(dma_chan, true);

    if(fread(play_buffer, 2, PLAY_BUF_SIZE, play_file) != PLAY_BUF_SIZE){
        panic("Failed to read initial data from play_file: %s\n", path);
    }
    printf("Playing file: %s\n", path);

    dma_handler(); 

    while(1){
        tight_loop_contents();
    }
}