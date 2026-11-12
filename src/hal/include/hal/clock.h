#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core/wall_time.h"

uint32_t hal_clock_milliseconds(void);
uint32_t hal_clock_microseconds(void);
bool hal_clock_wall_time(WallTime *time);
