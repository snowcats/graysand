#pragma once

#include <stdbool.h>

#include "gfx/canvas.h"
#include "ui/theme.h"

void header_set_parent(const char *title);
bool header_shows_clock(void);
bool header_is_bare(void);
void header_draw_bare(void);
void header_draw(Canvas *canvas, const Theme *theme, const char *title);
void header_draw_centered(Canvas *canvas, const Theme *theme, const char *title);
void header_mark(Canvas *canvas, Rect area, ThemeStyle style, Color color);
const Font *header_font(const Theme *theme, int code);
void header_fill_disc(Canvas *canvas, Point center, int radius, Color color);
