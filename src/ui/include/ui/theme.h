#pragma once

#include "gfx/color.h"
#include "gfx/font.h"
#include "gfx/rect.h"
#include "ui/marquee.h"

typedef struct ThemeColors {
    Color background;
    Color text;
    Color text_secondary;
    Color text_tertiary;
    Color selection_background;
    Color selection_top;
    Color selection_shine;
    Color selection_text;
    Color header_background;
    Color header_top;
    Color header_text;
    Color header_muted;
    Color divider;
    Color accent;
    Color scrollbar_thumb;
    Color danger;
    Color success;
    Color chevron;
} ThemeColors;

typedef struct ThemeFonts {
    const Font *title;
    const Font *body;
    const Font *small;
    const Font *heading;
    const Font *row;
} ThemeFonts;

typedef struct ThemeMetrics {
    int screen_width;
    int screen_height;
    int header_height;
    int header_inset;
    int row_height;
    int padding;
    int scrollbar_width;
    int scrollbar_inset;
    int detail_gap;
    int status_icon_size;
    int selection_inset;
    int selection_radius;
} ThemeMetrics;

typedef enum ThemeStyle {
    THEME_STYLE_PLAIN,
    THEME_STYLE_POKEDEX,
    THEME_STYLE_WIRED,
    THEME_STYLE_HYRULE,
    THEME_STYLE_ICE
} ThemeStyle;

typedef struct Theme {
    ThemeColors colors;
    ThemeFonts fonts;
    ThemeMetrics metrics;
    MarqueeTiming marquee;
    ThemeStyle style;
} Theme;

Rect theme_header_area(const Theme *theme);
Rect theme_content_area(const Theme *theme);
int theme_visible_rows(const Theme *theme);
