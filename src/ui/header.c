#include "ui/header.h"

#include "gfx/font_sized.h"
#include "gfx/text.h"
#include "ui/label.h"
#include "ui/theme_style.h"

enum {
    ROOT_X = 12,
    CRUMB_X = 22,
    CHEVRON = 5,
    PART_GAP = 8,
    CRUMB_FONT = 13,
    TITLE_FONT = 15 | FONT_SIZED_BOLD,
    PLAYER_FONT = 14 | FONT_SIZED_BOLD,
    PLAYER_SHIFT = 8,
    PLAYER_MIDDLE = 12,
    MIDDLE_HALF = 70
};

static const char *parent_title;
static bool player_shown;
static bool bare_shown;

void header_set_parent(const char *title)
{
    parent_title = title;
}

bool header_shows_clock(void)
{
    return player_shown;
}

bool header_is_bare(void)
{
    return bare_shown;
}

void header_draw_bare(void)
{
    player_shown = false;
    bare_shown = true;
}

void header_fill_disc(Canvas *canvas, Point center, int radius, Color color)
{
    int first = canvas->clip.y - center.y;
    int last = canvas->clip.y + canvas->clip.height - 1 - center.y;
    for (int dy = first > -radius ? first : -radius; dy <= radius && dy <= last; dy++) {
        int half = radius;
        while (half > 0 && half * half + dy * dy > radius * radius + radius / 2) {
            half--;
        }
        canvas_fill_rect(canvas, (Rect){center.x - half, center.y + dy, 2 * half + 1, 1}, color);
    }
}

static Rect draw_part(Canvas *canvas, TextStyle style, Rect line, const char *text)
{
    int width = text_width(style.font, text);
    Rect area = {line.x, line.y, width < line.width ? width : line.width, line.height};
    label_draw(canvas, style, (Label){.area = area, .text = text});
    int used = area.width + PART_GAP;
    return (Rect){line.x + used, line.y, line.width > used ? line.width - used : 0, line.height};
}

static void draw_bars(Canvas *canvas, Rect area, Color color)
{
    int count = 4;
    int bar = (area.width - (count - 1)) / count;
    if (bar < 1) {
        bar = 1;
    }
    for (int index = 0; index < count; index++) {
        int height = area.height * (index + 1) / count;
        if (height < 1) {
            height = 1;
        }
        canvas_fill_rect(
            canvas, (Rect){area.x + index * (bar + 1), area.y + area.height - height, bar, height},
            color);
    }
}

static void draw_ball(Canvas *canvas, Rect area)
{
    int size = area.width < area.height ? area.width : area.height;
    int radius = (size - 1) / 2;
    Point center = {area.x + area.width / 2, area.y + area.height / 2};
    Color shell = COLOR_RGB(24, 24, 30);
    Color cream = COLOR_RGB(250, 246, 232);
    header_fill_disc(canvas, center, radius, cream);
    Canvas top = canvas_clipped(canvas, (Rect){area.x, area.y, area.width, center.y - area.y});
    header_fill_disc(&top, center, radius - 1, COLOR_RGB(222, 30, 16));
    int band = radius / 7 > 2 ? radius / 7 : 2;
    canvas_fill_rect(canvas, (Rect){center.x - radius, center.y - band / 2, 2 * radius + 1, band},
                     shell);
    int hub = radius * 2 / 5 + 1;
    header_fill_disc(canvas, center, hub, shell);
    header_fill_disc(canvas, center, hub - band / 2, cream);
}

static void fill_triangle(Canvas *canvas, int apex_x, int apex_y, int height, Color color)
{
    for (int row = 0; row < height; row++) {
        int half = row * 3 / 5;
        canvas_fill_rect(canvas, (Rect){apex_x - half, apex_y + row, half * 2 + 1, 1}, color);
    }
}

static void draw_triangles(Canvas *canvas, Rect area, Color color)
{
    int height = area.height / 2 > 0 ? area.height / 2 : 1;
    int apex_x = area.x + area.width / 2;
    int shift = height * 3 / 5;
    fill_triangle(canvas, apex_x, area.y, height, color);
    fill_triangle(canvas, apex_x - shift, area.y + height, height, color);
    fill_triangle(canvas, apex_x + shift, area.y + height, height, color);
}

static void draw_wired(Canvas *canvas, Rect area, Color color)
{
    int bar = area.width / 5 > 0 ? area.width / 5 : 1;
    int dot = area.height / 3;
    canvas_fill_rect(canvas, (Rect){area.x, area.y, bar, area.height}, color);
    canvas_fill_rect(canvas, (Rect){area.x + area.width - bar, area.y, bar, area.height}, color);
    canvas_fill_rect(
        canvas, (Rect){area.x + (area.width - dot) / 2, area.y + (area.height - dot) / 2, dot, dot},
        color);
}

static void draw_asterisk(Canvas *canvas, Rect area, Color color)
{
    int size = area.width < area.height ? area.width : area.height;
    int radius = size / 2;
    int thick = size / 7 > 1 ? size / 7 : 1;
    int mid_x = area.x + area.width / 2;
    int mid_y = area.y + area.height / 2;
    canvas_fill_rect(canvas, (Rect){mid_x - thick / 2, mid_y - radius, thick, 2 * radius}, color);
    for (int dx = 0; dx <= radius * 7 / 8; dx++) {
        int dy = dx * 4 / 7;
        int left = mid_x - dx - thick / 2;
        int right = mid_x + dx - thick / 2;
        canvas_fill_rect(canvas, (Rect){left, mid_y - dy - thick / 2, thick, thick}, color);
        canvas_fill_rect(canvas, (Rect){left, mid_y + dy - thick / 2, thick, thick}, color);
        canvas_fill_rect(canvas, (Rect){right, mid_y - dy - thick / 2, thick, thick}, color);
        canvas_fill_rect(canvas, (Rect){right, mid_y + dy - thick / 2, thick, thick}, color);
    }
}

void header_mark(Canvas *canvas, Rect area, ThemeStyle style, Color color)
{
    switch (style) {
    case THEME_STYLE_POKEDEX:
        draw_ball(canvas, area);
        return;
    case THEME_STYLE_HYRULE:
        draw_triangles(canvas, area, color);
        return;
    case THEME_STYLE_WIRED:
        draw_wired(canvas, area, color);
        return;
    case THEME_STYLE_ICE:
        draw_asterisk(canvas, area, color);
        return;
    default:
        draw_bars(canvas, area, color);
        return;
    }
}

const Font *header_font(const Theme *theme, int code)
{
    return theme->fonts.heading == theme->fonts.body ? theme->fonts.body : font_sized_code(code);
}

void header_draw_centered(Canvas *canvas, const Theme *theme, const char *title)
{
    player_shown = true;
    bare_shown = false;
    theme_style_player_band(canvas, theme);
    TextStyle style = {.font = header_font(theme, PLAYER_FONT), .color = theme->colors.header_text};
    int width = text_width(style.font, title);
    int x = theme->metrics.screen_width / 2 + PLAYER_SHIFT - width / 2;
    label_draw(canvas, style, (Label){.area = {x, 0, width, PLAYER_MIDDLE * 2}, .text = title});
    theme_style_player_emblem(canvas, theme, x, x + width);
}

void header_draw(Canvas *screen, const Theme *theme, const char *title)
{
    Rect area = theme_header_area(theme);
    Canvas clipped = canvas_clipped(screen, area);
    Canvas *canvas = &clipped;
    int band = area.height - 1;
    canvas_fill_gradient(canvas, (Rect){area.x, area.y, area.width, band}, theme->colors.header_top,
                         theme->colors.header_background);
    canvas_fill_rect(canvas, (Rect){area.x, area.y + band, area.width, 1}, theme->colors.divider);
    player_shown = false;
    bare_shown = false;
    int right = area.width / 2 - MIDDLE_HALF;
    Rect line = {ROOT_X, area.y, right - ROOT_X, band};
    if (parent_title != NULL) {
        TextStyle muted = {.font = header_font(theme, CRUMB_FONT),
                           .color = theme->colors.header_muted};
        int middle = area.y + band / 2;
        for (int step = 0; step <= CHEVRON; step++) {
            canvas_fill_rect(canvas, (Rect){ROOT_X + step, middle - step, 2, 1}, muted.color);
            canvas_fill_rect(canvas, (Rect){ROOT_X + step, middle + step, 2, 1}, muted.color);
        }
        line = draw_part(canvas, muted, (Rect){CRUMB_X, area.y, right - CRUMB_X, band + 2},
                         parent_title);
        line.height = band;
        canvas_fill_rect(canvas, (Rect){line.x - 1, middle - 1, 2, 2}, muted.color);
        line.x += PART_GAP;
        line.width = line.width > PART_GAP ? line.width - PART_GAP : 0;
    }
    TextStyle style = {.font = header_font(theme, TITLE_FONT), .color = theme->colors.header_text};
    draw_part(canvas, style, line, title);
}
