#include <math.h>
#include <stdint.h>

#include "check.h"
#include "device/equalizer.h"

static const double TAU = 6.283185307179586;
static const double AMPLITUDE = 1000000;

enum {
    RATE = 44100,
    SETTLE = RATE / 2,
    MEASURE = RATE / 2,
    OFF = 0,
    BASS = 2,
    UNIQUE = 6
};

static double peak_after(uint8_t preset, uint32_t rate, double hertz)
{
    static Equalizer equalizer;
    equalizer_set(&equalizer, preset, rate);
    double peak = 0;
    for (int index = 0; index < SETTLE + MEASURE; index++) {
        int32_t in = (int32_t)lround(AMPLITUDE * sin(TAU * hertz * index / rate));
        int32_t out = equalizer.active ? equalizer_run(&equalizer, 0, in) : in;
        if (index >= SETTLE && fabs((double)out) > peak) {
            peak = fabs((double)out);
        }
    }
    return 20 * log10(peak / AMPLITUDE);
}

static void off_is_not_active_and_leaves_the_sound_alone(void)
{
    Equalizer equalizer;
    equalizer_set(&equalizer, OFF, RATE);
    CHECK(!equalizer.active);
}

static void rates_above_ninety_six_kilohertz_play_without_it(void)
{
    Equalizer equalizer;
    equalizer_set(&equalizer, BASS, 192000);
    CHECK(!equalizer.active);
    equalizer_set(&equalizer, BASS, 96000);
    CHECK(equalizer.active);
    equalizer_set(&equalizer, EQUALIZER_PRESET_COUNT, RATE);
    CHECK(!equalizer.active);
}

static void bass_lifts_the_lows_over_the_mids_without_getting_louder_overall(void)
{
    double low = peak_after(BASS, RATE, 100);
    double mid = peak_after(BASS, RATE, 1000);
    CHECK(low - mid > 4.5);
    CHECK(low < 0.5);
    CHECK(peak_after(BASS, 48000, 100) - peak_after(BASS, 48000, 1000) > 4.5);
}

static void unique_lifts_the_highs_and_cuts_the_mids(void)
{
    double mid = peak_after(UNIQUE, RATE, 1000);
    double high = peak_after(UNIQUE, RATE, 8000);
    CHECK(high - mid > 6);
    CHECK(high < 0.5);
}

static void silence_stays_silent(void)
{
    Equalizer equalizer;
    equalizer_set(&equalizer, UNIQUE, RATE);
    for (int index = 0; index < 1000; index++) {
        CHECK_EQUAL(0, equalizer_run(&equalizer, 1, 0));
    }
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(off_is_not_active_and_leaves_the_sound_alone),
        TEST_CASE(rates_above_ninety_six_kilohertz_play_without_it),
        TEST_CASE(bass_lifts_the_lows_over_the_mids_without_getting_louder_overall),
        TEST_CASE(unique_lifts_the_highs_and_cuts_the_mids),
        TEST_CASE(silence_stays_silent),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
