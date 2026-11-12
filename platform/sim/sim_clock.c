#include "sim_clock.h"

#include <SDL.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hal/clock.h"

enum {
    MICROSECONDS_PER_SECOND = 1000000
};

typedef struct SimClock {
    bool virtual_time;
    uint32_t virtual_ms;
} SimClock;

static SimClock clock_state;

void sim_clock_use_virtual_time(uint32_t start_ms)
{
    clock_state = (SimClock){.virtual_time = true, .virtual_ms = start_ms};
}

void sim_clock_advance(uint32_t milliseconds)
{
    clock_state.virtual_ms += milliseconds;
}

uint32_t hal_clock_milliseconds(void)
{
    return clock_state.virtual_time ? clock_state.virtual_ms : (uint32_t)SDL_GetTicks64();
}

uint32_t hal_clock_microseconds(void)
{
    uint64_t counter = SDL_GetPerformanceCounter();
    uint64_t frequency = SDL_GetPerformanceFrequency();
    uint64_t whole_seconds = counter / frequency;
    uint64_t remainder = counter % frequency;
    return (uint32_t)(whole_seconds * MICROSECONDS_PER_SECOND +
                      remainder * MICROSECONDS_PER_SECOND / frequency);
}

static int two_digits(const char *text)
{
    return (text[0] - '0') * 10 + (text[1] - '0');
}

bool hal_clock_wall_time(WallTime *wall)
{
    const char *fixed = getenv("ECHO_SIM_CLOCK");
    if (clock_state.virtual_time && fixed != NULL && strlen(fixed) == 5) {
        *wall = (WallTime){.year = 2026,
                           .month = 11,
                           .day = 8,
                           .hour = (uint8_t)two_digits(fixed),
                           .minute = (uint8_t)two_digits(fixed + 3)};
        return true;
    }
    time_t now = time(NULL);
    const struct tm *local = clock_state.virtual_time || now == (time_t)-1 ? NULL : localtime(&now);
    if (local == NULL) {
        return false;
    }
    *wall = (WallTime){.year = (uint16_t)(local->tm_year + 1900),
                       .month = (uint8_t)(local->tm_mon + 1),
                       .day = (uint8_t)local->tm_mday,
                       .hour = (uint8_t)local->tm_hour,
                       .minute = (uint8_t)local->tm_min,
                       .second = (uint8_t)local->tm_sec};
    return true;
}
