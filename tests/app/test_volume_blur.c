#include <string.h>

#include "check.h"
#include "hal/display.h"
#include "volume_blur.h"

enum {
    WIDTH = HAL_DISPLAY_WIDTH,
    HEIGHT = HAL_DISPLAY_HEIGHT,
    STRIP = 23
};

static Color page[HEIGHT][WIDTH];
static Color shown[HEIGHT][WIDTH];
static Color strip[STRIP][WIDTH];

static void whole_frame(bool apply)
{
    memcpy(shown, page, sizeof page);
    Canvas canvas = canvas_make(&shown[0][0], (Rect){0, 0, WIDTH, HEIGHT});
    volume_blur_strip(&canvas, apply);
}

static void strip_frame(bool apply)
{
    for (int top = 0; top < HEIGHT; top += STRIP) {
        int height = HEIGHT - top < STRIP ? HEIGHT - top : STRIP;
        memcpy(strip, page[top], sizeof(Color) * WIDTH * (size_t)height);
        Canvas canvas = canvas_make(&strip[0][0], (Rect){0, top, WIDTH, height});
        volume_blur_strip(&canvas, apply);
        memcpy(shown[top], strip, sizeof(Color) * WIDTH * (size_t)height);
    }
}

static void fill(Color color)
{
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            page[y][x] = color;
        }
    }
}

static int red(Color color)
{
    return color >> 11;
}

static void a_flat_color_stays_flat_and_dimmed(void)
{
    Color color = COLOR_RGB(200, 120, 64);
    Color dimmed = COLOR_RGB(104, 60, 32);
    fill(color);
    whole_frame(false);
    whole_frame(true);
    int wrong = 0;
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            wrong += shown[y][x] != dimmed;
        }
    }
    CHECK_EQUAL(0, wrong);
}

static void a_sharp_edge_becomes_a_smooth_ramp(void)
{
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            page[y][x] = x < WIDTH / 2 ? COLOR_RGB(255, 255, 255) : 0;
        }
    }
    whole_frame(false);
    whole_frame(true);
    for (int y = 0; y < HEIGHT; y += 37) {
        CHECK_EQUAL(16, red(shown[y][WIDTH / 2 - 12]));
        CHECK_EQUAL(0, red(shown[y][WIDTH / 2 + 12]));
        int levels = 0;
        for (int x = WIDTH / 2 - 12; x < WIDTH / 2 + 12; x++) {
            int step = red(shown[y][x]) - red(shown[y][x + 1]);
            CHECK(step >= 0 && step <= 2);
            levels += step != 0;
        }
        CHECK(levels >= 8);
    }
}

static void checkers_with_noise(void)
{
    uint32_t seed = 12345;
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            seed = seed * 1103515245u + 12345u;
            page[y][x] = (Color)(((x / 16 + y / 16) & 1 ? 0xFFFF : 0) ^ (seed >> 16 & 0x18E3));
        }
    }
}

static void strips_match_one_big_canvas(void)
{
    static Color whole[HEIGHT][WIDTH];
    checkers_with_noise();
    whole_frame(false);
    whole_frame(true);
    memcpy(whole, shown, sizeof shown);
    fill(0);
    strip_frame(false);
    checkers_with_noise();
    strip_frame(false);
    strip_frame(true);
    CHECK_EQUAL(0, memcmp(whole, shown, sizeof shown));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_flat_color_stays_flat_and_dimmed),
        TEST_CASE(a_sharp_edge_becomes_a_smooth_ramp),
        TEST_CASE(strips_match_one_big_canvas),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
