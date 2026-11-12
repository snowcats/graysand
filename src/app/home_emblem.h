#pragma once

#include "gfx/canvas.h"
#include "settings/settings.h"

Color home_emblem_panel_color(ThemeChoice choice);
void home_emblem_draw(Canvas *canvas, Rect area, ThemeChoice choice);
