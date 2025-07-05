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
#include "ff.h"

#define CPU_FREQ 200000000

typedef struct {
	char riff[4];        // "RIFF"
	uint32_t size;      // ファイルサイズ - 8
	char data[4];       // "WAVE"
} Riff;
Riff* riff;

typedef struct {
	uint16_t wFormatTag;          // フォーマットタグ (1: PCM)
	uint16_t nChannels;           // チャンネル数 (1: モノラル, 2: ステレオ)
	uint32_t nSamplesPerSec;      // サンプリングレート (例: 44100)
	uint32_t nAvgBytesPerSec;     // 平均バイト/秒 (サンプリングレート * チャンネル数 * ビット深度 / 8)
	uint16_t nBlockAlign;         // ブロックアライメント (チャンネル数 * ビット深度 / 8)
	uint16_t wBitsPerSample;      // ビット深度 (例: 16)
} WaveFormat;

#define DMA_BUF_SIZE 2048
static uint32_t dma_buf[DMA_BUF_SIZE];

static int dma_chan;
static PIO pio = pio0;
static uint sm = 0;


UINT out_stream (   /* 戻り値: 転送されたバイト数またはストリームの状態 */
    const BYTE *p,  /* 転送するデータを指すポインタ */
    UINT btf        /* >0: 転送を行う(バイト数). 0: ストリームの状態を調べる */
)
{
    if (btf == 0) {
        return i2s_ready();
    }

    UINT i;
    for (i=0; i+1 < btf && i/2 < DMA_BUF_SIZE;i+=2) {
		int16_t sample = *(int16_t*)(p+i);
        dma_buf[i] = (int32_t)sample << 16;
    }
    return i;
}



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

    // ファイルオープン
    FIL wav;
	fr = f_open(&wav, "sound.wav", FA_READ);
    if (FR_OK != fr && FR_EXIST != fr) {
        panic("f_open(sound.wav) error: %s (%d)\n", FRESULT_str(fr), fr);
	}
	FIL log;
	fr = f_open(&log, "log.txt", FA_OPEN_APPEND | FA_WRITE);
    if (FR_OK != fr && FR_EXIST != fr) {
        panic("f_open(log.txt) error: %s (%d)\n", FRESULT_str(fr), fr);
	}

	//読み出し
	uint8_t buffer[256];
	int16_t* play_buffer;
	play_buffer = (int16_t*)calloc(1<<16, sizeof(int16_t));
	uint br;
	uint32_t i,j;
	fr = f_read(&wav, buffer, 12, &br);
	printf("f_read: %s\n", FRESULT_str(fr));
	printf("%d bytes are readed.\n", br);
	
	riff = (Riff*)malloc(sizeof(Riff));
	riff = (Riff*)buffer;
	printf("size:%d\n", riff->size);
	printf("\n");

	for(;;){
		fr = f_read(&wav, buffer, 8, &br);
		if(FR_OK != fr) {
			panic("f_read error: %s (%d)\n", FRESULT_str(fr), fr);
		}
		if(memcmp(buffer, "fmt ", 4)!=0){
			f_read(&wav, buffer, *(uint32_t*)(buffer+4) , &br);
			if(FR_OK != fr) {
				panic("f_read error: %s (%d)\n", FRESULT_str(fr), fr);
			}
		}else{
			WaveFormat* waveformat;
			waveformat = (WaveFormat*)malloc(sizeof(WaveFormat));
			if(*(uint32_t*)(buffer+4) >= 16){
				f_read(&wav, waveformat, 16, &br);
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
		fr = f_read(&wav, buffer, 8, &br);
		if(FR_OK != fr) {
			panic("f_read error: %s (%d)\n", FRESULT_str(fr), fr);
		}
		if(memcmp(buffer, "data", 4)!=0){
			f_read(&wav, buffer, *(uint32_t*)(buffer+4) , &br);
			if(FR_OK != fr) {
				panic("f_read error: %s (%d)\n", FRESULT_str(fr), fr);
			}
		}else{
			while(1){
				fr = f_forward(&wav, out_stream, 1<<8, &br);
				if(FR_OK != fr) {
					panic("f_read error: %s (%d)\n", FRESULT_str(fr), fr);
				}

				// DMA転送開始（WAV -> PIO）
				dma_channel_configure(
					dma_chan, 
					&dcfg,
					&pio->txf[sm],
					dma_buf,
					br/2,
					true
				);
				dma_channel_wait_for_finish_blocking(dma_chan);
			}
			break;
		}
	}

	free(play_buffer);

    // Close the file
	fr = f_close(&wav);
    if (FR_OK != fr) {
        printf("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
    }
	fr = f_close(&log);
    if (FR_OK != fr) {
        printf("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
    }

    // Unmount the SD card
    f_unmount("");

    puts("Goodbye, world!");
}