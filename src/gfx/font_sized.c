#include "gfx/font_sized.h"

#include <stdint.h>

enum {
    INTER_UNITS = 2048,
    INTER_ASCENDER = 1984,
    INTER_DESCENDER = 494
};

static Font sized[FONT_SIZED_COUNT];

static uint8_t scaled(int pixels, int units)
{
    return (uint8_t)((pixels * units + INTER_UNITS - 1) / INTER_UNITS);
}

const Font *font_sized(int pixels, bool bold)
{
    pixels = pixels < FONT_SIZED_SMALLEST  ? FONT_SIZED_SMALLEST
             : pixels > FONT_SIZED_LARGEST ? FONT_SIZED_LARGEST
                                           : pixels;
    int index = 2 * (pixels - FONT_SIZED_SMALLEST) + (bold ? 1 : 0);
    Font *font = &sized[index];
    font->ascent = scaled(pixels, INTER_ASCENDER);
    font->descent = scaled(pixels, INTER_DESCENDER);
    font->pack_face = (uint8_t)(FONT_SIZED_FIRST_FACE + index + 1);
    return font;
}

const Font *font_sized_code(int code)
{
    return font_sized(code & FONT_SIZED_PIXELS, (code & FONT_SIZED_BOLD) != 0);
}
