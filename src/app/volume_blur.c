#include "volume_blur.h"

#include "hal/display.h"

enum {
    BLOCK = 8,
    COLUMNS = HAL_DISPLAY_WIDTH / BLOCK,
    ROWS = (HAL_DISPLAY_HEIGHT + BLOCK - 1) / BLOCK
};

#define SPREAD 0x07E0F81Fu
#define FIELD_ONE 0x00200801u

static uint32_t sums[COLUMNS];
static Color low[2][ROWS][COLUMNS];
static unsigned back;

static uint32_t spread(Color color)
{
    return (color | (uint32_t)color << 16) & SPREAD;
}

static Color pack(uint32_t value)
{
    return (Color)(value | value >> 16);
}

static void take_row(const Color *row, int y)
{
    unsigned part = (unsigned)y % BLOCK;
    unsigned repeat = y == HAL_DISPLAY_HEIGHT - 1 ? BLOCK - part : 1;
    for (int column = 0; column < COLUMNS; column++) {
        uint32_t sum = 4 * FIELD_ONE;
        for (int x = 0; x < BLOCK; x += 2) {
            uint32_t pair;
            __builtin_memcpy(&pair, &row[column * BLOCK + x], sizeof pair);
            sum += (pair & SPREAD) + ((pair >> 16 | pair << 16) & SPREAD);
        }
        sum = ((sum >> 3) & SPREAD) * repeat;
        sums[column] = part == 0 ? sum : sums[column] + sum;
        if (part + repeat == BLOCK) {
            low[back][y / BLOCK][column] = pack(((sums[column] + 4 * FIELD_ONE) >> 3) & SPREAD);
        }
    }
}

static void put_row(Color *row, int y)
{
    int at = 2 * y + 9;
    int lower = at >> 4;
    int upper = lower > 0 ? lower - 1 : 0;
    lower = lower < ROWS ? lower : ROWS - 1;
    uint32_t weight = (uint32_t)at & 15u;
    const Color *top = low[back ^ 1u][upper];
    const Color *bottom = low[back ^ 1u][lower];
    uint32_t line[COLUMNS + 2];
    for (int column = 0; column < COLUMNS; column++) {
        line[column + 1] = ((spread(top[column]) * (16 - weight) + spread(bottom[column]) * weight +
                             8 * FIELD_ONE) >>
                            4) &
                           SPREAD;
    }
    line[0] = line[1];
    line[COLUMNS + 1] = line[COLUMNS];
    for (int column = 0; column <= COLUMNS; column++) {
        uint32_t left = line[column] * 15 + 16 * FIELD_ONE;
        uint32_t right = line[column + 1];
        for (int x = column * BLOCK - BLOCK / 2; x < column * BLOCK + BLOCK / 2; x++) {
            if ((unsigned)x < HAL_DISPLAY_WIDTH) {
                row[x] = pack(((left + right) >> 5) & SPREAD);
            }
            left -= 2 * line[column];
            right += 2 * line[column + 1];
        }
    }
}

void volume_blur_strip(Canvas *canvas, bool apply)
{
    Rect strip = canvas->bounds;
    for (int y = strip.y; y < strip.y + strip.height; y++) {
        Color *row = canvas->pixels + (y - strip.y) * strip.width;
        take_row(row, y);
        if (apply) {
            put_row(row, y);
        }
    }
    if (strip.y + strip.height == HAL_DISPLAY_HEIGHT) {
        back ^= 1u;
    }
}
