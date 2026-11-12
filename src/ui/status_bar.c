#include "ui/status_bar.h"

#include "core/string_builder.h"
#include "gfx/font_sized.h"
#include "gfx/text.h"
#include "ui/header.h"
#include "ui/icons.h"
#include "ui/label.h"
#include "ui/theme_style.h"

enum {
    BATTERY_WIDTH = 21,
    BATTERY_HEIGHT = 11,
    NUB_WIDTH = 2,
    NUB_HEIGHT = 3,
    FILL_INSET = 2,
    BOLT_WIDTH = 6,
    ICON_GAP = 6,
    BLUETOOTH_WIDTH_TENTHS = 7,
    TENTHS = 10,
    FULL_PERCENT = 100,
    PILL_X = 7,
    PILL_Y = 4,
    PILL_WIDTH = 46,
    PILL_HEIGHT = 15,
    CLOCK_FONT = 11 | FONT_SIZED_BOLD,
    MIDDLE_NAME_AT = 8
};

typedef struct Layout {
    uint8_t battery_right;
    uint8_t battery_top;
    uint8_t gap;
    uint8_t band;
    uint8_t font;
} Layout;

static const Layout LAYOUTS[2] = {
    {.battery_right = 38, .battery_top = 9, .gap = 8, .band = 30, .font = 13},
    {.battery_right = 30, .battery_top = 6, .gap = 4, .band = 24, .font = CLOCK_FONT},
};

static const char WIDEST_PERCENT[] = "100%";

static const Layout *layout(void)
{
    return &LAYOUTS[header_shows_clock() ? 1 : 0];
}

Rect status_bar_battery_area(const Theme *theme)
{
    const Layout *at = layout();
    return (Rect){theme->metrics.screen_width - at->battery_right, at->battery_top,
                  BATTERY_WIDTH + NUB_WIDTH, BATTERY_HEIGHT};
}

Rect status_bar_bolt_area(const Theme *theme)
{
    Rect battery = status_bar_battery_area(theme);
    return (Rect){battery.x - layout()->gap - BOLT_WIDTH, battery.y, BOLT_WIDTH, battery.height};
}

Rect status_bar_percent_area(const Theme *theme, const char *text, bool charging)
{
    const Layout *at = layout();
    Rect beside = charging ? status_bar_bolt_area(theme) : status_bar_battery_area(theme);
    int width = text_width(header_font(theme, at->font), text);
    return (Rect){beside.x - at->gap - width, 0, width, at->band};
}

Rect status_bar_bluetooth_area(const Theme *theme)
{
    int size = theme->metrics.status_icon_size;
    int width = size * BLUETOOTH_WIDTH_TENTHS / TENTHS;
    Rect percent = status_bar_percent_area(theme, WIDEST_PERCENT, true);
    return (Rect){percent.x - ICON_GAP - width, (percent.height - size) / 2, width, size};
}

Rect status_bar_playback_area(const Theme *theme)
{
    int size = theme->metrics.status_icon_size;
    Rect bluetooth = status_bar_bluetooth_area(theme);
    return (Rect){bluetooth.x - ICON_GAP - size, bluetooth.y, size, size};
}

int status_bar_battery_fill_width(uint8_t percent, int inner_width)
{
    int clamped = percent < FULL_PERCENT ? percent : FULL_PERCENT;
    int width = clamped * inner_width / FULL_PERCENT;
    return clamped > 0 && width == 0 ? 1 : width;
}

void status_bar_percent_text(uint8_t percent, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    string_builder_append_unsigned(&builder, percent < FULL_PERCENT ? percent : FULL_PERCENT);
    string_builder_append_char(&builder, '%');
}

static void put_two(char *text, int value)
{
    text[0] = (char)('0' + value / 10);
    text[1] = (char)('0' + value % 10);
}

static void draw_text(Canvas *canvas, const Theme *theme, int code, Rect area, const char *text)
{
    TextStyle style = {.font = header_font(theme, code), .color = theme->colors.header_text};
    label_draw(canvas, style,
               (Label){.area = label_centered_area(style.font, area, text), .text = text});
}

static void draw_middle(Canvas *canvas, const Theme *theme, StatusBarState state)
{
    char text[32] = "00:00   ";
    char *name = text + MIDDLE_NAME_AT;
    if (state.clock_known && !header_is_bare()) {
        put_two(text, state.hour);
        put_two(text + 3, state.minute);
    } else {
        name = text;
    }
    StringBuilder builder = string_builder_make(name, sizeof text - (size_t)(name - text));
    string_builder_append(&builder, state.theme_name != NULL ? state.theme_name : "");
    if (text[0] != '\0') {
        draw_text(canvas, theme, layout()->font,
                  (Rect){0, 0, theme->metrics.screen_width, layout()->band}, text);
    }
}

static void draw_pill(Canvas *canvas, const Theme *theme, StatusBarState state, int top)
{
    if (!state.clock_known) {
        return;
    }
    char text[6] = "00:00";
    put_two(text, state.hour);
    put_two(text + 3, state.minute);
    Rect pill = {PILL_X, top, PILL_WIDTH, PILL_HEIGHT};
    theme_style_clock_pill(canvas, theme, pill);
    draw_text(canvas, theme, CLOCK_FONT, pill, text);
}

static void draw_clock(Canvas *canvas, const Theme *theme, StatusBarState state)
{
    if (header_shows_clock()) {
        draw_pill(canvas, theme, state, PILL_Y);
        return;
    }
    if (header_is_bare()) {
        draw_pill(canvas, theme, state, (layout()->band - PILL_HEIGHT) / 2);
    }
    draw_middle(canvas, theme, state);
}

static Color fill_color(const Theme *theme, StatusBarState state)
{
    if (state.charging) {
        return theme->colors.success;
    }
    if (state.battery_percent <= STATUS_BAR_LOW_BATTERY_PERCENT) {
        return theme->colors.danger;
    }
    return header_shows_clock() ? theme->colors.accent : theme->colors.header_text;
}

static void draw_battery(Canvas *canvas, const Theme *theme, StatusBarState state)
{
    Rect area = status_bar_battery_area(theme);
    Color color = theme->colors.header_text;
    int right = area.x + BATTERY_WIDTH - 1;
    canvas_fill_rect(canvas, (Rect){area.x + 1, area.y, BATTERY_WIDTH - 2, 1}, color);
    canvas_fill_rect(canvas, (Rect){area.x + 1, area.y + BATTERY_HEIGHT - 1, BATTERY_WIDTH - 2, 1},
                     color);
    canvas_fill_rect(canvas, (Rect){area.x, area.y + 1, 1, BATTERY_HEIGHT - 2}, color);
    canvas_fill_rect(canvas, (Rect){right, area.y + 1, 1, BATTERY_HEIGHT - 2}, color);
    canvas_fill_rect(
        canvas,
        (Rect){right + 1, area.y + (BATTERY_HEIGHT - NUB_HEIGHT) / 2, NUB_WIDTH, NUB_HEIGHT},
        color);
    int inner = BATTERY_WIDTH - 2 * FILL_INSET;
    canvas_fill_rect(canvas,
                     (Rect){area.x + FILL_INSET, area.y + FILL_INSET,
                            status_bar_battery_fill_width(state.battery_percent, inner),
                            BATTERY_HEIGHT - 2 * FILL_INSET},
                     fill_color(theme, state));
}

void status_bar_draw(Canvas *canvas, const Theme *theme, StatusBarState state)
{
    draw_clock(canvas, theme, state);
    Color color = theme->colors.header_text;
    if (state.bluetooth != STATUS_BLUETOOTH_OFF) {
        Color tint =
            state.bluetooth == STATUS_BLUETOOTH_CONNECTED ? color : theme->colors.text_tertiary;
        icon_draw_bluetooth(canvas, status_bar_bluetooth_area(theme), tint);
    }
    if (state.battery_known) {
        draw_battery(canvas, theme, state);
        if (state.charging) {
            icon_draw_bolt(canvas, status_bar_bolt_area(theme), theme->colors.success);
        }
        char text[STATUS_BAR_PERCENT_CAPACITY];
        status_bar_percent_text(state.battery_percent, text, sizeof text);
        draw_text(canvas, theme, layout()->font,
                  status_bar_percent_area(theme, text, state.charging), text);
    }
}
