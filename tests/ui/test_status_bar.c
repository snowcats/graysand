#include <string.h>

#include "check.h"
#include "ui/header.h"
#include "ui/status_bar.h"
#include "ui_test_canvas.h"
#include "ui_test_theme.h"

enum {
    STATUS_WIDTH = 480,
    STATUS_HEADER_HEIGHT = 29,
    STATUS_ICON_SIZE = 10,
    BATTERY_GAP = 8,
    LOW_BATTERY = 10,
    HALF_BATTERY = 50
};

static Color pixels[STATUS_WIDTH * STATUS_HEADER_HEIGHT];

static Theme status_theme(void)
{
    Theme theme = TEST_THEME;
    theme.metrics.screen_width = STATUS_WIDTH;
    theme.metrics.header_height = STATUS_HEADER_HEIGHT;
    theme.metrics.status_icon_size = STATUS_ICON_SIZE;
    return theme;
}

static Canvas draw_status(const Theme *theme, StatusBarState state)
{
    Canvas canvas = canvas_make(pixels, (Rect){0, 0, STATUS_WIDTH, STATUS_HEADER_HEIGHT});
    header_draw(&canvas, theme, "");
    status_bar_draw(&canvas, theme, state);
    return canvas;
}

static StatusBarState battery_at(uint8_t percent, bool charging)
{
    return (StatusBarState){.playback = STATUS_PLAYBACK_NONE,
                            .battery_known = true,
                            .battery_percent = percent,
                            .charging = charging};
}

static void an_idle_player_without_a_battery_reading_draws_nothing(void)
{
    Theme theme = status_theme();
    Canvas canvas = draw_status(&theme, (StatusBarState){.playback = STATUS_PLAYBACK_NONE});
    Rect header = {0, 0, STATUS_WIDTH, STATUS_HEADER_HEIGHT - 2};
    CHECK(ui_test_area_has_only(&canvas, header, theme.colors.header_background));
}

static void playing_and_paused_draw_no_icon(void)
{
    Theme theme = status_theme();
    Rect header = {0, 0, STATUS_WIDTH, STATUS_HEADER_HEIGHT - 2};
    Canvas playing = draw_status(&theme, (StatusBarState){.playback = STATUS_PLAYBACK_PLAYING});
    CHECK(ui_test_area_has_only(&playing, header, theme.colors.header_background));
    Canvas paused = draw_status(&theme, (StatusBarState){.playback = STATUS_PLAYBACK_PAUSED});
    CHECK(ui_test_area_has_only(&paused, header, theme.colors.header_background));
}

static void the_battery_sits_inside_the_right_inset(void)
{
    Theme theme = status_theme();
    Canvas canvas = draw_status(&theme, battery_at(HALF_BATTERY, false));
    Rect battery = status_bar_battery_area(&theme);
    CHECK(battery.x > STATUS_WIDTH - 2 * battery.width);
    CHECK(battery.x + battery.width < STATUS_WIDTH);
    CHECK(battery.y + battery.height < STATUS_HEADER_HEIGHT);
    CHECK(ui_test_area_has_color(&canvas, battery, theme.colors.header_text));
}

static void the_fill_follows_the_charge(void)
{
    CHECK_EQUAL(0, status_bar_battery_fill_width(0, 16));
    CHECK_EQUAL(1, status_bar_battery_fill_width(1, 16));
    CHECK_EQUAL(8, status_bar_battery_fill_width(HALF_BATTERY, 16));
    CHECK_EQUAL(16, status_bar_battery_fill_width(100, 16));
    CHECK_EQUAL(16, status_bar_battery_fill_width(250, 16));
}

static void a_low_battery_is_drawn_in_the_danger_color(void)
{
    Theme theme = status_theme();
    Canvas canvas = draw_status(&theme, battery_at(LOW_BATTERY, false));
    CHECK(ui_test_area_has_color(&canvas, status_bar_battery_area(&theme), theme.colors.danger));
}

static void a_charging_battery_is_drawn_in_the_success_color(void)
{
    Theme theme = status_theme();
    Canvas canvas = draw_status(&theme, battery_at(LOW_BATTERY, true));
    Rect battery = status_bar_battery_area(&theme);
    CHECK(ui_test_area_has_color(&canvas, battery, theme.colors.success));
    CHECK(!ui_test_area_has_color(&canvas, battery, theme.colors.danger));
}

static void the_percentage_reads_as_a_number_and_a_percent_sign(void)
{
    char text[STATUS_BAR_PERCENT_CAPACITY];
    status_bar_percent_text(73, text, sizeof text);
    CHECK(strcmp("73%", text) == 0);
    status_bar_percent_text(100, text, sizeof text);
    CHECK(strcmp("100%", text) == 0);
    status_bar_percent_text(0, text, sizeof text);
    CHECK(strcmp("0%", text) == 0);
}

static void the_percentage_sits_just_left_of_the_battery(void)
{
    Theme theme = status_theme();
    Rect battery = status_bar_battery_area(&theme);
    Rect percent = status_bar_percent_area(&theme, "73%", false);
    CHECK(percent.width > 0);
    CHECK(percent.x + percent.width < battery.x);
    CHECK(percent.x + percent.width >= battery.x - BATTERY_GAP);
}

static void a_known_battery_shows_its_percentage(void)
{
    Theme theme = status_theme();
    Canvas canvas = draw_status(&theme, battery_at(HALF_BATTERY, false));
    Rect percent = status_bar_percent_area(&theme, "50%", false);
    CHECK(!ui_test_area_has_only(&canvas, percent, theme.colors.header_background));
}

static void an_unknown_battery_shows_no_percentage(void)
{
    Theme theme = status_theme();
    StatusBarState state = battery_at(HALF_BATTERY, false);
    state.battery_known = false;
    Canvas canvas = draw_status(&theme, state);
    Rect percent = status_bar_percent_area(&theme, "50%", false);
    percent.height = STATUS_HEADER_HEIGHT - 2;
    CHECK(ui_test_area_has_only(&canvas, percent, theme.colors.header_background));
}

static void a_charging_battery_has_a_bolt_beside_it(void)
{
    Theme theme = status_theme();
    Rect bolt = status_bar_bolt_area(&theme);
    Rect battery = status_bar_battery_area(&theme);
    CHECK(bolt.x + bolt.width < battery.x);
    Canvas charging = draw_status(&theme, battery_at(50, true));
    CHECK(ui_test_area_has_color(&charging, bolt, theme.colors.success));
    Canvas resting = draw_status(&theme, battery_at(50, false));
    CHECK(!ui_test_area_has_color(&resting, bolt, theme.colors.success));
}

static void the_percentage_moves_left_of_the_bolt_while_charging(void)
{
    Theme theme = status_theme();
    Rect bolt = status_bar_bolt_area(&theme);
    Rect percent = status_bar_percent_area(&theme, "50%", true);
    CHECK(percent.x + percent.width < bolt.x);
    Canvas canvas = draw_status(&theme, battery_at(50, true));
    CHECK(!ui_test_area_has_only(&canvas, percent, theme.colors.header_background));
}

static void a_bare_header_puts_the_time_in_a_pill_on_the_left(void)
{
    Theme theme = status_theme();
    Canvas canvas = canvas_make(pixels, (Rect){0, 0, STATUS_WIDTH, STATUS_HEADER_HEIGHT});
    canvas_fill_rect(&canvas, canvas.bounds, theme.colors.header_background);
    header_draw_bare();
    status_bar_draw(
        &canvas, &theme,
        (StatusBarState){.clock_known = true, .hour = 12, .minute = 34, .theme_name = "Noah"});
    CHECK(!ui_test_area_has_only(&canvas, (Rect){0, 0, 60, STATUS_HEADER_HEIGHT},
                                 theme.colors.header_background));
    CHECK(!ui_test_area_has_only(&canvas, (Rect){200, 0, 80, STATUS_HEADER_HEIGHT},
                                 theme.colors.header_background));
    header_draw(&canvas, &theme, "");
    CHECK(!header_is_bare());
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_percentage_reads_as_a_number_and_a_percent_sign),
        TEST_CASE(the_percentage_sits_just_left_of_the_battery),
        TEST_CASE(a_known_battery_shows_its_percentage),
        TEST_CASE(an_unknown_battery_shows_no_percentage),
        TEST_CASE(an_idle_player_without_a_battery_reading_draws_nothing),
        TEST_CASE(playing_and_paused_draw_no_icon),
        TEST_CASE(a_bare_header_puts_the_time_in_a_pill_on_the_left),
        TEST_CASE(the_battery_sits_inside_the_right_inset),
        TEST_CASE(the_fill_follows_the_charge),
        TEST_CASE(a_low_battery_is_drawn_in_the_danger_color),
        TEST_CASE(a_charging_battery_is_drawn_in_the_success_color),
        TEST_CASE(a_charging_battery_has_a_bolt_beside_it),
        TEST_CASE(the_percentage_moves_left_of_the_bolt_while_charging),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
