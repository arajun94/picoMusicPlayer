#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"

#include "i2s.h"

#include "hw_config.h"
#include "f_util.h"
#include "file_stream.h"
#include "ff.h"

#define CPU_FREQ 250000000

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

int main()
{
	stdio_init_all();
	set_sys_clock_khz(CPU_FREQ / 1000, true);

	i2s_init();

	//スタートボタンが押されるまで待機
	gpio_init(15);
    gpio_set_dir(15, GPIO_IN);
    while (!gpio_get(15));

	printf("start\n");

	//マウント
    FATFS fs;
    FRESULT fr = f_mount(&fs, "", 1);
    if (FR_OK != fr) {
        panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    }

	FILE *wav = open_file_stream("sound.wav", "r");
	if (!wav) {
		panic("Failed to open file: %s\n", "sound.wav");
	}
	static char vbuf[2048] __attribute__((aligned));
    int err = setvbuf(wav, vbuf, _IOFBF, sizeof vbuf);


	//読み出し
	uint32_t buffer[256];
	int16_t* play_buffer;
	play_buffer = (int16_t*)calloc(PLAY_BUF_SIZE, sizeof(int16_t));
	uint32_t i,j;

	Riff* riff;
	riff = (Riff*)malloc(sizeof(Riff));
	fread(riff, 12, 1, wav);
	printf("size:%d\n", riff->size);
	printf("\n");

	for(;;){
		fread(buffer, 4, 2, wav);
		if(FR_OK != fr) {
			panic("f_read error: %s (%d)\n", FRESULT_str(fr), fr);
		}
		if(memcmp(buffer, "fmt ", 4)!=0){
			fread(buffer, buffer[1], 1, wav);
			if(FR_OK != fr) {
				panic("f_read error: %s (%d)\n", FRESULT_str(fr), fr);
			}
		}else{
			if(buffer[1] >= 16){
				WaveFormat* waveformat;
				waveformat = (WaveFormat*)malloc(sizeof(WaveFormat));
				fread(waveformat, 16, 1, wav);
				if(FR_OK != fr) {
					panic("f_read error: %s (%d)\n", FRESULT_str(fr), fr);
				}
				/*if((*(uint32_t*)buffer+4)>16){
					f_read(&wav, buffer, (*(uint32_t*)buffer+4)-16, &br);
					if(FR_OK != fr) {
						panic("f_read error: %s (%d)\n", FRESULT_str(fr), fr);
					}
				}*/
				printf("wFormatTag: %d\n", waveformat->wFormatTag);
				printf("nChannels: %d\n", waveformat->nChannels);
				printf("nSamplesPerSec: %d\n", waveformat->nSamplesPerSec);
				printf("nAvgBytesPerSec: %d\n", waveformat->nAvgBytesPerSec);
				printf("nBlockAlign: %d\n", waveformat->nBlockAlign);
				printf("wBitsPerSample: %d\n", waveformat->wBitsPerSample);
			}else{
				panic("unsupported wave format size: %d\n", *(uint32_t*)(buffer+4));
			}
			break;
		}
	}
    // DMA設定
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config dcfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dcfg, DMA_SIZE_32);
    channel_config_set_dreq(&dcfg, DREQ_PIO0_TX0);

	for(;;){
		fread(buffer, 4, 2, wav);
		if(memcmp(buffer, "data", 4)!=0){
			fread(buffer, buffer[1], 1, wav);
		}else{
			while(1){
				fread(play_buffer, 2, PLAY_BUF_SIZE, wav);
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
			break;
		}
	}

	free(play_buffer);

    // Close the file
	fclose(wav);

    // Unmount the SD card
    f_unmount("");

    puts("Goodbye, world!");

}