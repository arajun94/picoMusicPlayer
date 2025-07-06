#ifndef PLAY_H
#define PLAY_H

#include <stdint.h>

void play (char* path);

#define PLAY_BUF_SIZE 512

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

#endif // PLAY_H