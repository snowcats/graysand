#include "home_emblem.h"

#include "default_theme.h"
#include "ui/header.h"

enum {
    EMBLEM_SIZE = 104
};

static const Color PANEL_COLORS[THEME_CHOICE_COUNT] = {
    [THEME_PODIUM] = COLOR_RGB(8, 8, 10),  [THEME_POKEMON] = COLOR_RGB(24, 26, 36),
    [THEME_ZELDA] = COLOR_RGB(12, 22, 18), [THEME_LAIN] = COLOR_RGB(10, 7, 14),
    [THEME_NOAH] = COLOR_RGB(12, 16, 34),
};

Color home_emblem_panel_color(ThemeChoice choice)
{
    return PANEL_COLORS[choice];
}

void home_emblem_draw(Canvas *canvas, Rect area, ThemeChoice choice)
{
    const Theme *theme = default_theme_for(choice);
    Rect mark = {area.x + (area.width - EMBLEM_SIZE) / 2, area.y + (area.height - EMBLEM_SIZE) / 2,
                 EMBLEM_SIZE, EMBLEM_SIZE};
    header_mark(canvas, mark, theme->style, theme->colors.header_text);
}
