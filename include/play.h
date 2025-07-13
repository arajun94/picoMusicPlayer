#ifndef PLAY_H
#define PLAY_H

#include <stdint.h>

void play (char*);

void stop ();

void start();

void restart();

uint8_t isPlaying();

uint8_t isEnded();

typedef struct {
    uint8_t channels;
    uint8_t bitDeps;
    uint32_t samplingRrate;
    uint32_t samples;
} Metadata;


#endif // PLAY_H