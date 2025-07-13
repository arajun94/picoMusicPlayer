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


static int dma_chan;
static dma_channel_config dcfg;

static int32_t* play_buffer;
static FIL play_file;
static int32_t* null_buffer;

static I2S i2s;

static struct {
    uint32_t t;
    uint8_t playing;
    uint8_t ended;
    Metadata metadata;
} player;

void dma_handler() {
    static int32_t* dma_buf;
    dma_hw->ints0 = 1u << dma_chan;

    dma_buf = play_buffer;
    dma_channel_configure(
        dma_chan,
        &dcfg,
        &(i2s.pio)->txf[i2s.sm],
        dma_buf,
        PLAY_BUF_SIZE,
        true
    );


    if(player.t>=player.metadata.samples)player.ended = 1;

    if(player.playing && !player.ended){
        play_buffer = wav_read(&play_file, &player.metadata, player.t);
        player.t += PLAY_BUF_SIZE;
    }else{
        play_buffer = null_buffer;
    }
}

void play_abort (){
    i2s_close(&i2s);
    dma_channel_set_irq0_enabled(dma_chan, false);
    irq_set_enabled(DMA_IRQ_0, false);
    player.ended = 0;
    player.t =0;
    free(null_buffer);
}

void stop (){
    player.playing = 0;
}

void start (){
    player.playing = 1;
}

void restart(){
    player.ended = 0;
    player.t = 0;
}

uint8_t isPlaying(){
    return player.playing;
}

uint8_t isEnded(){
    return player.ended;
}

void play (char* path){
	uint32_t i,j;
    FRESULT fr;

    null_buffer = (int32_t*)calloc(PLAY_BUF_SIZE, sizeof(int32_t));

    // ファイルを開く
    fr = f_open(&play_file, path, FA_READ);
	if (fr!=FR_OK) {
        printf("%s\n", FRESULT_str(fr));
		panic("Failed to open &play_file: %s\n", path);
	}

    player.metadata = wav_init(&play_file);

    i2s = i2s_init(player.metadata.bitDeps, player.metadata.samplingRrate, player.metadata.channels);
    
    // DMA設定
    dma_chan = dma_claim_unused_channel(true);
    dcfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dcfg, DMA_SIZE_32);
    channel_config_set_dreq(&dcfg, DREQ_PIO0_TX0);

    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    dma_channel_set_irq0_enabled(dma_chan, true);

    player.playing = 1;
    
    dma_handler();
}