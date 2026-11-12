#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    WALL_TIME_REGISTER_COUNT = 7
};

typedef struct WallTime {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} WallTime;

bool wall_time_from_registers(const uint8_t *registers, WallTime *time);
