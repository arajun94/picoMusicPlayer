#include "play.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

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

static FIL playFile;
static Metadata metadata;


#define CLAMP(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
#define SINC_RADIUS 8
//#define M_PI = 3.141

#define SINC_RESOLUTION 1024
#define SINC_RADIUS     8
#define SINC_TAPS       (2 * SINC_RADIUS + 1)

float sinc_table[SINC_RESOLUTION][SINC_TAPS];

void init_sinc_table() {
    for (int i = 0; i < SINC_RESOLUTION; i++) {
        float frac = (float)i / SINC_RESOLUTION;

        for (int k = -SINC_RADIUS; k <= SINC_RADIUS; k++) {
            float x = (float)k - frac;
            float pi_x = M_PI * x;
            float sinc = (x == 0.0f) ? 1.0f : sinf(pi_x) / pi_x;

            // Blackman窓
            float w = 0.42f - 0.5f * cosf(2.0f * M_PI * (x + SINC_RADIUS) / (2.0f * SINC_RADIUS))
                            + 0.08f * cosf(4.0f * M_PI * (x + SINC_RADIUS) / (2.0f * SINC_RADIUS));

            sinc_table[i][k + SINC_RADIUS] = sinc * w;
        }
    }
}

int resample_lut(
    const int32_t *in, int in_len,
    int32_t *out
) {
    float ratio = (float)metadata.samplingRrate / I2S_SAMPLING_RATE;
    int out_len = (int)(in_len / ratio);

    for (int i = 0; i < out_len; i++) {
        float t = i * ratio;
        int t_int = (int)t;
        float frac = t - t_int;
        int frac_idx = (int)(frac * SINC_RESOLUTION);
        if (frac_idx >= SINC_RESOLUTION) frac_idx = SINC_RESOLUTION - 1;

        float sample_f = 0.0f;

        for (int k = -SINC_RADIUS; k <= SINC_RADIUS; k++) {
            int idx = t_int + k;
            if (idx < 0 || idx >= in_len) continue;

            float weight = sinc_table[frac_idx][k + SINC_RADIUS];
            sample_f += (float)in[idx] * weight;
        }

        int64_t s = ((int64_t)(sample_f + 0.5f))<<16;
        s = CLAMP(s, -2147483648LL, 2147483647LL);
        out[i] = (int32_t)s;
    }

    return out_len;
}


static int32_t* playBuffer;

uint32_t getPlayBuffer(){
    return resample_lut(wav_read(&playFile, metadata), PLAY_BUF_SIZE , playBuffer);
}

/*
int32_t* getPlayBuffer(){
    static int32_t* fileBuffer;
    static uint64_t readedFileBuffer = 0;
    static int32_t playBuffer[PLAY_BUF_SIZE];
    static uint64_t playBufferIndex = 0;
    uint64_t endPlayBufferIndex = playBufferIndex + PLAY_BUF_SIZE;
    uint64_t fileBufferIndex;
    uint32_t fileBufferIndexOffset;

    while(playBufferIndex < endPlayBufferIndex){
        fileBufferIndex = playBufferIndex*metadata.samplingRrate/I2S_SAMPLING_RATE;
        fileBufferIndexOffset = 32768*playBufferIndex*metadata.samplingRrate/I2S_SAMPLING_RATE - fileBufferIndex*32768;
        if(readedFileBuffer <= fileBufferIndex+1){
            fileBuffer = wav_read(&playFile, metadata);
            readedFileBuffer+=PLAY_BUF_SIZE;
        }
        //playBuffer[playBufferIndex%PLAY_BUF_SIZE] = (fileBuffer[(fileBufferIndex)%PLAY_BUF_SIZE] + fileBufferIndexOffset*(fileBuffer[(fileBufferIndex+1)%PLAY_BUF_SIZE]-fileBuffer[fileBufferIndex%PLAY_BUF_SIZE])/32768)<<(I2S_BITS-metadata.bitDeps);
        playBuffer[playBufferIndex%PLAY_BUF_SIZE] =
        playBufferIndex++;
    }
    return playBuffer;
}
*/

void dma_handler() {
    static uint32_t playBufferLength;
    dma_hw->ints0 = 1u << dma_chan;

    memcpy(dma_buf, playBuffer, playBufferLength*sizeof(int32_t));//最初は空
    
    dma_channel_configure(
        dma_chan,
        &dcfg,
        &I2S_PIO->txf[I2S_SM],
        dma_buf,
        playBufferLength,
        true
    );
    
    playBufferLength = getPlayBuffer();
}


void play (char* path){
	uint32_t i,j;
    FRESULT fr;

    dma_buf = (int32_t*)malloc(PLAY_BUF_SIZE*sizeof(int32_t));
    playBuffer = (int32_t*)malloc(PLAY_BUF_SIZE*64*sizeof(int32_t));

    init_sinc_table();

    // ファイルを開く
    fr = f_open(&playFile, path, FA_READ);
	if (fr!=FR_OK) {
        printf("%s\n", FRESULT_str(fr));
		panic("Failed to open : %s\n", path);
	}

    metadata = wav_init(&playFile);
    
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