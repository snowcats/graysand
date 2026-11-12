#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    EQUALIZER_BANDS = 3,
    EQUALIZER_CHANNELS = 2,
    EQUALIZER_TERMS = 5,
    EQUALIZER_PRESET_COUNT = 7
};

typedef struct Equalizer {
    uint8_t preset;
    uint32_t rate;
    bool active;
    int32_t coefficients[EQUALIZER_BANDS][EQUALIZER_TERMS];
    int32_t history[EQUALIZER_CHANNELS][EQUALIZER_BANDS][4];
} Equalizer;

void equalizer_set(Equalizer *equalizer, uint8_t preset, uint32_t rate);
int32_t equalizer_run(Equalizer *equalizer, int channel, int32_t sample);
