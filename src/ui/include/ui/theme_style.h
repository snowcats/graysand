#pragma once

#include "gfx/canvas.h"
#include "ui/theme.h"

void theme_style_selection(Canvas *canvas, const Theme *theme, Rect pill);
void theme_style_corners(Canvas *canvas, const Theme *theme, Rect area, int radius, Color outside);
Color theme_style_mark_color(const Theme *theme);
void theme_style_player_band(Canvas *canvas, const Theme *theme);
void theme_style_player_emblem(Canvas *canvas, const Theme *theme, int left, int right);
void theme_style_clock_pill(Canvas *canvas, const Theme *theme, Rect pill);
