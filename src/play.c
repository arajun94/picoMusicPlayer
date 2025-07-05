#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"

#include "i2s.h"

#include "hw_config.h"
#include "f_util.h"
#include "file_stream.h"
#include "ff.h"

#define CPU_FREQ 200000000

#define PLAY_BUF_SIZE 512
uint32_t dma_buf[PLAY_BUF_SIZE];

static PIO pio = pio0;
static uint sm = 0;
static int dma_chan;

typedef struct {
	char riff[4];        // "RIFF"
	uint32_t size;      // ファイルサイズ - 8
	char data[4];       // "WAVE"
} Riff;

typedef struct {
	uint16_t wFormatTag;          // フォーマットタグ (1: PCM)
	uint16_t nChannels;           // チャンネル数 (1: モノラル, 2: ステレオ)
	uint32_t nSamplesPerSec;      // サンプリングレート (例: 44100)
	uint32_t nAvgBytesPerSec;     // 平均バイト/秒 (サンプリングレート * チャンネル数 * ビット深度 / 8)
	uint16_t nBlockAlign;         // ブロックアライメント (チャンネル数 * ビット深度 / 8)
	uint16_t wBitsPerSample;      // ビット深度 (例: 16)
} WaveFormat;

void play (char* path){
	uint32_t buffer[256];
	int16_t* play_buffer;
	uint32_t i,j;
	Riff* riff;
    WaveFormat* waveformat;


    // ファイルを開く
    FILE *file = open_file_stream(path, "r");
	if (!file) {
		panic("Failed to open file: %s\n", path);
	}
    
    //stdバッファリング	
    static char vbuf[2048] __attribute__((aligned));
    int err = setvbuf(file, vbuf, _IOFBF, sizeof vbuf);


    //メモリ確保
    play_buffer = (int16_t*)calloc(PLAY_BUF_SIZE, sizeof(int16_t));
	riff = (Riff*)malloc(sizeof(Riff));


    // RIFFチャンクの読み込み
	fread(riff, 12, 1, file);


    //fmtチャンクまで飛ばす
    while(fread(buffer, 4, 2, file)==2 && memcmp(buffer, "fmt ", 4)!=0){
        fseek(file, buffer[1], SEEK_CUR);
    }

    //fmtチャンクの読み込み
    if(buffer[1] >= 16){
        waveformat = (WaveFormat*)malloc(sizeof(WaveFormat));
        fread(waveformat, 16, 1, file);
        if(buffer[1]>16){//fmtチャンクのサイズが16より大きい場合飛ばす
            fseek(file, buffer[1]-16, SEEK_CUR);
        }
    }else{
        panic("unsupported wave format size: %d\n", buffer[1]);
    }


    // DMA設定
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config dcfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dcfg, DMA_SIZE_32);
    channel_config_set_dreq(&dcfg, DREQ_PIO0_TX0);


    //dataチャンクまで飛ばす
    while(fread(buffer, 4, 2, file)==2 && memcmp(buffer, "data", 4)!=0){
        fseek(file, buffer[1], SEEK_CUR);
    }

    //data逐次処理    
    while(fread(play_buffer, 2, PLAY_BUF_SIZE, file)== PLAY_BUF_SIZE){
        dma_channel_wait_for_finish_blocking(dma_chan);
        for(i=0; i<PLAY_BUF_SIZE; i++){
            dma_buf[i] = (int32_t)play_buffer[i]<<16;
        }
        dma_channel_configure(
            dma_chan,
            &dcfg,
            &pio->txf[sm],
            dma_buf,
            PLAY_BUF_SIZE,
            true
        );
    }

	free(play_buffer);
    free(riff);
	fclose(file);
}