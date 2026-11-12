#include "check.h"
#include <stddef.h>

#include "ui/header.h"
#include "ui_test_canvas.h"
#include "ui_test_theme.h"

enum {
    WIDE = 480,
    TALL = 40,
    HEAD = 29,
    INSET = 132,
    CLEAR = 0x0821
};

static Color pixels[WIDE * TALL];

static Theme wide_theme(void)
{
    Theme theme = TEST_THEME;
    theme.metrics.screen_width = WIDE;
    theme.metrics.screen_height = TALL;
    theme.metrics.header_height = HEAD;
    theme.metrics.header_inset = INSET;
    return theme;
}

static Canvas wide_canvas(void)
{
    Canvas canvas = canvas_make(pixels, (Rect){0, 0, WIDE, TALL});
    canvas_clear(&canvas, CLEAR);
    return canvas;
}

static void the_band_shades_from_its_top_color_down(void)
{
    Theme theme = wide_theme();
    theme.colors.header_top = 0xFFFF;
    Canvas canvas = wide_canvas();
    header_draw(&canvas, &theme, "");
    CHECK_EQUAL(0xFFFF, canvas_pixel_at(&canvas, 0, 0));
    CHECK_EQUAL(theme.colors.header_background, canvas_pixel_at(&canvas, 0, HEAD - 2));
}

static void a_divider_line_separates_the_header_from_the_content(void)
{
    Theme theme = wide_theme();
    Canvas canvas = wide_canvas();
    header_draw(&canvas, &theme, "");
    CHECK(ui_test_area_has_only(&canvas, (Rect){0, HEAD - 1, WIDE, 1}, theme.colors.divider));
    CHECK(ui_test_area_has_only(&canvas, (Rect){0, 0, WIDE, HEAD - 1},
                                theme.colors.header_background));
}

static void the_header_stays_inside_its_area(void)
{
    Theme theme = TEST_THEME;
    Canvas canvas = ui_test_canvas();
    header_set_parent("B");
    header_draw(&canvas, &theme, "A");
    header_set_parent(NULL);
    Rect below = {0, TEST_HEADER_HEIGHT, TEST_SCREEN_WIDTH,
                  TEST_SCREEN_HEIGHT - TEST_HEADER_HEIGHT};
    CHECK(ui_test_area_has_only(&canvas, below, CLEAR));
}

static void only_the_player_header_shows_the_clock(void)
{
    Theme theme = wide_theme();
    Canvas canvas = wide_canvas();
    header_draw(&canvas, &theme, "A");
    CHECK(!header_shows_clock());
    header_draw_centered(&canvas, &theme, "A");
    CHECK(header_shows_clock());
    header_set_parent("B");
    header_draw(&canvas, &theme, "A");
    header_set_parent(NULL);
    CHECK(!header_shows_clock());
}

static void the_title_starts_at_the_left_padding(void)
{
    Theme theme = wide_theme();
    Canvas canvas = wide_canvas();
    header_draw(&canvas, &theme, "AB");
    CHECK(ui_test_area_has_color(&canvas, (Rect){12, 0, 8, HEAD}, theme.colors.header_text));
    CHECK(!ui_test_area_has_color(&canvas, (Rect){0, 0, 12, HEAD}, theme.colors.header_text));
}

static void the_parent_title_comes_first_in_a_muted_color(void)
{
    Theme theme = wide_theme();
    Canvas canvas = wide_canvas();
    header_set_parent("B");
    header_draw(&canvas, &theme, "A");
    header_set_parent(NULL);
    Rect chevron = {12, 0, 8, HEAD};
    Rect crumb = {22, 0, 19, HEAD};
    CHECK(ui_test_area_has_color(&canvas, chevron, theme.colors.header_muted));
    CHECK(ui_test_area_has_color(&canvas, crumb, theme.colors.header_muted));
    CHECK(!ui_test_area_has_color(&canvas, (Rect){0, 0, 41, HEAD}, theme.colors.header_text));
    CHECK(ui_test_area_has_color(&canvas, (Rect){41, 0, 8, HEAD}, theme.colors.header_text));
}

static void a_long_title_stays_clear_of_the_status_icons(void)
{
    Theme theme = wide_theme();
    Canvas canvas = wide_canvas();
    header_draw(&canvas, &theme,
                "ABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABAB"
                "ABABABABABABABABABABABABABABABABABAB");
    Rect right = {WIDE - INSET, 0, INSET, HEAD};
    CHECK(!ui_test_area_has_color(&canvas, right, theme.colors.header_text));
}

static void the_player_header_centers_its_title_over_an_accent_line(void)
{
    Theme theme = wide_theme();
    Canvas canvas = wide_canvas();
    header_draw_centered(&canvas, &theme, "A");
    CHECK(
        ui_test_area_has_color(&canvas, (Rect){WIDE / 2 + 6, 0, 4, 24}, theme.colors.header_text));
    CHECK(ui_test_area_has_only(&canvas, (Rect){96, 24, WIDE - 96, 2}, theme.colors.accent));
    CHECK(!ui_test_area_has_color(&canvas, (Rect){0, 24, 96, 2}, theme.colors.accent));
}

static void every_style_paints_the_whole_player_band(void)
{
    static const ThemeStyle STYLES[] = {THEME_STYLE_PLAIN, THEME_STYLE_POKEDEX, THEME_STYLE_WIRED,
                                        THEME_STYLE_HYRULE, THEME_STYLE_ICE};
    for (size_t index = 0; index < sizeof STYLES / sizeof STYLES[0]; index++) {
        Theme theme = wide_theme();
        theme.style = STYLES[index];
        Canvas canvas = wide_canvas();
        header_draw_centered(&canvas, &theme, "A");
        CHECK(!ui_test_area_has_color(&canvas, (Rect){0, 0, WIDE, 26}, CLEAR));
        CHECK(ui_test_area_has_only(&canvas, (Rect){0, 26, WIDE, TALL - 26}, CLEAR));
    }
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_band_shades_from_its_top_color_down),
        TEST_CASE(a_divider_line_separates_the_header_from_the_content),
        TEST_CASE(the_header_stays_inside_its_area),
        TEST_CASE(only_the_player_header_shows_the_clock),
        TEST_CASE(the_title_starts_at_the_left_padding),
        TEST_CASE(the_parent_title_comes_first_in_a_muted_color),
        TEST_CASE(a_long_title_stays_clear_of_the_status_icons),
        TEST_CASE(the_player_header_centers_its_title_over_an_accent_line),
        TEST_CASE(every_style_paints_the_whole_player_band),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
