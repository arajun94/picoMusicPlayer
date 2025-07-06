#include "play.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "pico/stdlib.h"

#include "i2s.h"
#include "i2s_config.h"

#include "wav.h"

#include "config.h"

#include "hw_config.h"
#include "f_util.h"
#include "file_stream.h"
#include "ff.h"


static uint32_t* dma_buf;
static int dma_chan;
static dma_channel_config dcfg;

static int32_t* play_buffer;
static FILE *play_file;


void dma_handler() {
    dma_hw->ints0 = 1u << dma_chan;
    uint16_t i;
    dma_buf = play_buffer;
    dma_channel_configure(
        dma_chan, &dcfg,
        &I2S_PIO->txf[I2S_SM],
        dma_buf,
        PLAY_BUF_SIZE,
        true
    );
    play_buffer = wav_read(play_file);
    /*if(fread(play_buffer, 2, PLAY_BUF_SIZE, play_file) != PLAY_BUF_SIZE){
        // EOFに達した場合、DMAを停止
        dma_channel_abort(dma_chan);
        dma_channel_set_irq0_enabled(dma_chan, false);
        irq_set_enabled(DMA_IRQ_0, false);
        fclose(play_file);
        free(play_buffer);
        free(riff);
        free(waveformat);
        puts("Playback finished.");
    }*/
}


void play (char* path){
	uint32_t i,j;

    //メモリ確保
    play_buffer = (int32_t*)malloc(PLAY_BUF_SIZE*sizeof(int32_t));
    //dma_buf = (uint32_t*)malloc(PLAY_BUF_SIZE*sizeof(uint32_t));


    // ファイルを開く
    play_file = open_file_stream(path, "r");
	if (!play_file) {
		panic("Failed to open play_file: %s\n", path);
	}
    
    //stdバッファリング
    static char vbuf[2048] __attribute__((aligned));
    int err = setvbuf(play_file, vbuf, _IOFBF, sizeof vbuf);

    wav_init(play_file);
    
    // DMA設定
    dma_chan = dma_claim_unused_channel(true);
    dcfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dcfg, DMA_SIZE_32);
    channel_config_set_dreq(&dcfg, DREQ_PIO0_TX0);

    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    dma_channel_set_irq0_enabled(dma_chan, true);

    dma_handler();
}