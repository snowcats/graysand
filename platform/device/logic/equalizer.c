#include "device/equalizer.h"

enum {
    COEFFICIENT_SHIFT = 28,
    ONE = 1 << COEFFICIENT_SHIFT,
    RATE_COUNT = 4,
    GAIN_OFFSET = 6,
    GAIN_STEPS = 2 * GAIN_OFFSET + 1
};

static const uint32_t RATES[RATE_COUNT] = {44100, 48000, 88200, 96000};
static const int32_t COSINES[RATE_COUNT][EQUALIZER_BANDS] = {
    {268408211, 265715526, 112144278},
    {268412458, 266138953, 134217728},
    {268428645, 267754610, 226010060},
    {268429707, 267860715, 232471924},
};
static const int32_t ALPHAS[RATE_COUNT][EQUALIZER_BANDS] = {
    {2731734, 27225935, 174205565},
    {2509794, 25027041, 166051374},
    {1365902, 13647583, 103453250},
    {1254924, 12540371, 95869806},
};
static const int32_t AMPLITUDES[GAIN_STEPS] = {
    190037749, 201298194, 213225862, 225860289, 239243352, 253419412, 268435456,
    284341257, 301189535, 319036137, 337940217, 357964434, 379175160,
};
static const int8_t PRESET_GAINS[EQUALIZER_PRESET_COUNT][EQUALIZER_BANDS] = {
    {0, 0, 0}, {4, 2, -4}, {6, 0, 0}, {5, -2, 4}, {-1, 3, 2}, {3, -1, 2}, {2, -3, 5},
};

static int32_t product(int64_t left, int64_t right)
{
    return (int32_t)(left * right / ONE);
}

static int32_t ratio(int64_t numerator, int64_t denominator)
{
    return (int32_t)(numerator * ONE / denominator);
}

static int rate_row(uint32_t rate)
{
    for (int row = 0; row < RATE_COUNT; row++) {
        if (RATES[row] == rate) {
            return row;
        }
    }
    return -1;
}

void equalizer_set(Equalizer *equalizer, uint8_t preset, uint32_t rate)
{
    *equalizer = (Equalizer){.preset = preset, .rate = rate};
    int row = rate_row(rate);
    if (preset == 0 || preset >= EQUALIZER_PRESET_COUNT || row < 0) {
        return;
    }
    const int8_t *gains = PRESET_GAINS[preset];
    int boost = 0;
    for (int band = 0; band < EQUALIZER_BANDS; band++) {
        boost = gains[band] > boost ? gains[band] : boost;
    }
    int32_t loudest = AMPLITUDES[boost + GAIN_OFFSET];
    int32_t preamp = ratio(ONE, product(loudest, loudest));
    for (int band = 0; band < EQUALIZER_BANDS; band++) {
        int32_t amplitude = AMPLITUDES[gains[band] + GAIN_OFFSET];
        int32_t alpha = ALPHAS[row][band];
        int32_t lift = product(alpha, amplitude);
        int32_t damp = ratio(alpha, amplitude);
        int64_t first = (int64_t)ONE + damp;
        int64_t middle = -2 * (int64_t)COSINES[row][band];
        int32_t scale = band == 0 ? preamp : ONE;
        int32_t *terms = equalizer->coefficients[band];
        terms[0] = product(ratio(ONE + lift, first), scale);
        terms[1] = product(ratio(middle, first), scale);
        terms[2] = product(ratio(ONE - lift, first), scale);
        terms[3] = ratio(middle, first);
        terms[4] = ratio(ONE - damp, first);
    }
    equalizer->active = true;
}

int32_t equalizer_run(Equalizer *equalizer, int channel, int32_t sample)
{
    int32_t value = sample;
    for (int band = 0; band < EQUALIZER_BANDS; band++) {
        const int32_t *terms = equalizer->coefficients[band];
        int32_t *past = equalizer->history[channel][band];
        int64_t sum = (int64_t)terms[0] * value + (int64_t)terms[1] * past[0] +
                      (int64_t)terms[2] * past[1] - (int64_t)terms[3] * past[2] -
                      (int64_t)terms[4] * past[3];
        past[1] = past[0];
        past[0] = value;
        past[3] = past[2];
        value =
            (int32_t)(sum >= 0 ? sum >> COEFFICIENT_SHIFT : -((-sum - 1) >> COEFFICIENT_SHIFT) - 1);
        past[2] = value;
    }
    return value;
}
