#pragma once

#include <stdbool.h>

#include "gfx/font.h"

enum {
    FONT_SIZED_FIRST_FACE = 4,
    FONT_SIZED_SMALLEST = 7,
    FONT_SIZED_LARGEST = 18,
    FONT_SIZED_COUNT = 2 * (FONT_SIZED_LARGEST - FONT_SIZED_SMALLEST + 1),
    FONT_SIZED_BOLD = 0x80,
    FONT_SIZED_PIXELS = 0x7F
};

const Font *font_sized(int pixels, bool bold);
const Font *font_sized_code(int code);
