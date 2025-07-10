#ifndef PLAY_H
#define PLAY_H

#include <stdint.h>

void play (char*);

typedef struct {
    uint8_t channels;
    uint8_t bitDeps;
    uint32_t samplingRrate;
} Metadata;


#endif // PLAY_H