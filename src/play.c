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


static int32_t* dma_buf;
static int dma_chan;
static dma_channel_config dcfg;

static int32_t* play_buffer;
static FIL play_file;

static I2S i2s;

static Metadata metadata;

void dma_handler() {
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
    play_buffer = wav_read(&play_file, &metadata);
}


void play (char* path){
	uint32_t i,j;
    FRESULT fr;

    // ファイルを開く
    fr = f_open(&play_file, path, FA_READ);
	if (fr!=FR_OK) {
        printf("%s\n", FRESULT_str(fr));
		panic("Failed to open &play_file: %s\n", path);
	}

    metadata = wav_init(&play_file);

    i2s = i2s_init(metadata.bitDeps, metadata.samplingRrate, metadata.channels);
    
    // DMA設定
    dma_chan = dma_claim_unused_channel(true);
    dcfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dcfg, DMA_SIZE_32);
    channel_config_set_dreq(&dcfg, DREQ_PIO0_TX0);

    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    dma_channel_set_irq0_enabled(dma_chan, true);

    play_buffer = wav_read(&play_file, &metadata);

    dma_handler();
}