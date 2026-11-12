#include "ui/theme_style.h"

#include <stdbool.h>

#include "ui/header.h"

enum {
    ROW_PILL_TALL = 28,
    ROW_PILL_RADIUS = 9,
    SHORT_PILL_RADIUS = 7,
    PLAYER_HEAD = 26,
    PLAYER_MIDDLE = 12,
    PLAYER_DIM = 3,
    PLAIN_SEGMENT = 96,
    PLAIN_SEGMENT_ALPHA = 10,
    DIAMOND_FIRST = 30,
    DIAMOND_PITCH = 60,
    HATCH_PITCH = 5,
    EMBLEM_GAP = 12,
    PILL_TINT = 3
};

static int half_chord(int radius, int offset)
{
    int half = radius;
    while (half > 0 && half * half > radius * radius - offset * offset) {
        half--;
    }
    return half;
}

static void round_corners(Canvas *canvas, Rect area, int radius, Color outside)
{
    for (int row = 0; row < radius; row++) {
        int cut = radius - half_chord(radius, radius - 1 - row);
        int right = area.x + area.width - cut;
        int top = area.y + row;
        int bottom = area.y + area.height - 1 - row;
        canvas_fill_rect(canvas, (Rect){area.x, top, cut, 1}, outside);
        canvas_fill_rect(canvas, (Rect){right, top, cut, 1}, outside);
        canvas_fill_rect(canvas, (Rect){area.x, bottom, cut, 1}, outside);
        canvas_fill_rect(canvas, (Rect){right, bottom, cut, 1}, outside);
    }
}

void theme_style_corners(Canvas *canvas, const Theme *theme, Rect area, int radius, Color outside)
{
    if (theme->style != THEME_STYLE_WIRED) {
        round_corners(canvas, area, radius, outside);
    }
}

void theme_style_selection(Canvas *canvas, const Theme *theme, Rect pill)
{
    const ThemeColors *colors = &theme->colors;
    canvas_fill_gradient(canvas, pill, colors->selection_top, colors->selection_background);
    Color shine = colors->selection_shine;
    if (theme->style == THEME_STYLE_WIRED) {
        canvas_fill_rect(canvas, (Rect){pill.x, pill.y, pill.width, 1}, shine);
        canvas_fill_rect(canvas, (Rect){pill.x, pill.y + pill.height - 1, pill.width, 1}, shine);
        canvas_fill_rect(canvas, (Rect){pill.x, pill.y, 1, pill.height}, shine);
        canvas_fill_rect(canvas, (Rect){pill.x + pill.width - 1, pill.y, 1, pill.height}, shine);
        return;
    }
    if (theme->metrics.selection_radius == 0) {
        canvas_fill_rect(canvas, (Rect){pill.x, pill.y, pill.width, 1}, shine);
        return;
    }
    int radius = pill.height < ROW_PILL_TALL ? SHORT_PILL_RADIUS : ROW_PILL_RADIUS;
    round_corners(canvas, pill, radius < pill.height / 2 ? radius : pill.height / 2,
                  colors->background);
}

Color theme_style_mark_color(const Theme *theme)
{
    switch (theme->style) {
    case THEME_STYLE_WIRED:
        return theme->colors.selection_shine;
    case THEME_STYLE_POKEDEX:
        return COLOR_RGB(255, 206, 18);
    case THEME_STYLE_ICE:
        return theme->colors.accent;
    default:
        return theme->colors.selection_background;
    }
}

static void player_lines(Canvas *canvas, const Theme *theme, int width)
{
    const ThemeColors *colors = &theme->colors;
    Color dim = color_blend(colors->header_background, colors->accent, PLAYER_DIM);
    switch (theme->style) {
    case THEME_STYLE_POKEDEX:
        canvas_fill_rect(canvas, (Rect){0, 0, width, 1}, COLOR_RGB(255, 120, 100));
        canvas_fill_rect(canvas, (Rect){0, PLAYER_HEAD - 3, width, 3}, COLOR_RGB(24, 20, 24));
        return;
    case THEME_STYLE_HYRULE:
        canvas_fill_rect(canvas, (Rect){0, PLAYER_HEAD - 3, width, 3}, dim);
        canvas_fill_rect(canvas, (Rect){0, PLAYER_HEAD - 4, width, 1}, colors->accent);
        for (int x = DIAMOND_FIRST; x < width; x += DIAMOND_PITCH) {
            canvas_fill_rect(canvas, (Rect){x, PLAYER_HEAD - 6, 1, 5}, colors->accent);
            canvas_fill_rect(canvas, (Rect){x - 1, PLAYER_HEAD - 5, 3, 3}, colors->accent);
            canvas_fill_rect(canvas, (Rect){x, PLAYER_HEAD - 4, 1, 1}, colors->header_background);
        }
        return;
    case THEME_STYLE_WIRED:
        for (int y = 1; y < PLAYER_HEAD; y += 2) {
            canvas_fill_rect(canvas, (Rect){0, y, width, 1}, colors->header_background);
        }
        canvas_fill_rect(canvas, (Rect){0, PLAYER_HEAD - 1, width, 1}, colors->selection_shine);
        return;
    case THEME_STYLE_ICE:
        for (int y = 0; y < PLAYER_HEAD - 1; y++) {
            for (int x = (HATCH_PITCH - y % HATCH_PITCH) % HATCH_PITCH; x < width;
                 x += HATCH_PITCH) {
                canvas_blend_pixel(canvas, (Point){x, y}, COLOR_RGB(255, 255, 255), 1);
            }
        }
        break;
    default:
        canvas_fill_rect(canvas, (Rect){0, PLAYER_HEAD - 2, width, 2}, colors->accent);
        canvas_fill_rect(canvas, (Rect){0, PLAYER_HEAD - 2, PLAIN_SEGMENT, 2},
                         color_blend(colors->background, colors->accent, PLAIN_SEGMENT_ALPHA));
        return;
    }
    canvas_fill_rect(canvas, (Rect){0, PLAYER_HEAD - 1, width, 1}, colors->accent);
}

void theme_style_player_band(Canvas *canvas, const Theme *theme)
{
    int width = theme->metrics.screen_width;
    bool plain = theme->style == THEME_STYLE_PLAIN;
    canvas_fill_gradient(canvas, (Rect){0, 0, width, PLAYER_HEAD},
                         plain ? COLOR_RGB(29, 29, 29) : theme->colors.header_top,
                         plain ? COLOR_RGB(19, 19, 19) : theme->colors.header_background);
    player_lines(canvas, theme, width);
}

static void butterfly(Canvas *canvas, Point center, Color wing)
{
    Color edge = COLOR_RGB(6, 16, 48);
    for (int pass = 0; pass < 2; pass++) {
        int size = pass == 0 ? 5 : 3;
        for (int step = 1; step <= 5; step++) {
            for (int side = -1; side <= 1; side += 2) {
                int x = center.x + side * step - size / 2;
                Color color = pass == 0 ? edge : wing;
                canvas_fill_rect(canvas, (Rect){x, center.y - step - size / 2, size, size}, color);
                if (step < 5) {
                    canvas_fill_rect(canvas, (Rect){x, center.y + step - size / 2, size, size},
                                     color);
                }
            }
        }
    }
    canvas_fill_rect(canvas, (Rect){center.x - 1, center.y - 1, 3, 3}, COLOR_RGB(126, 210, 255));
}

void theme_style_player_emblem(Canvas *canvas, const Theme *theme, int left, int right)
{
    static const uint8_t BARS[] = {5, 9, 7, 11};
    const ThemeColors *colors = &theme->colors;
    int x = left - EMBLEM_GAP;
    int y = PLAYER_MIDDLE;
    switch (theme->style) {
    case THEME_STYLE_POKEDEX:
        header_fill_disc(canvas, (Point){x, y}, 7, COLOR_RGB(24, 20, 24));
        header_mark(canvas, (Rect){x - 6, y - 6, 13, 13}, theme->style, colors->accent);
        return;
    case THEME_STYLE_HYRULE:
        header_mark(canvas, (Rect){x - 8, y - 8, 17, 14}, theme->style, colors->accent);
        return;
    case THEME_STYLE_WIRED:
        canvas_fill_rect(canvas, (Rect){x - 2, y - 6, 5, 12}, colors->header_text);
        canvas_fill_rect(canvas, (Rect){right + 3, y - 6, 6, 13}, colors->header_text);
        return;
    case THEME_STYLE_ICE:
        butterfly(canvas, (Point){x, y}, colors->accent);
        return;
    default:
        for (int index = 0; index < 4; index++) {
            Color color = index == 3 ? color_blend(colors->header_background, colors->header_text,
                                                   PLAIN_SEGMENT_ALPHA)
                                     : colors->header_text;
            canvas_fill_rect(
                canvas, (Rect){x - 5 + index * 3 + index / 3, y + 5 - BARS[index], 2, BARS[index]},
                color);
        }
        return;
    }
}

void theme_style_clock_pill(Canvas *canvas, const Theme *theme, Rect pill)
{
    const ThemeColors *colors = &theme->colors;
    if (theme->style == THEME_STYLE_HYRULE || theme->style == THEME_STYLE_WIRED) {
        Color fill = color_blend(colors->header_top, colors->header_text, PILL_TINT);
        int corner = theme->style == THEME_STYLE_HYRULE ? 1 : 0;
        canvas_fill_rect(
            canvas, (Rect){pill.x + corner, pill.y, pill.width - 2 * corner, pill.height}, fill);
        canvas_fill_rect(
            canvas, (Rect){pill.x, pill.y + corner, pill.width, pill.height - 2 * corner}, fill);
        return;
    }
    Color fill = color_blend(colors->header_background, COLOR_RGB(0, 0, 0),
                             theme->style == THEME_STYLE_POKEDEX ? 4 : 7);
    int radius = (pill.height - 1) / 2;
    int middle = pill.y + radius;
    header_fill_disc(canvas, (Point){pill.x + radius, middle}, radius, fill);
    header_fill_disc(canvas, (Point){pill.x + pill.width - 1 - radius, middle}, radius, fill);
    canvas_fill_rect(canvas, (Rect){pill.x + radius, pill.y, pill.width - 2 * radius, pill.height},
                     fill);
}
