#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gfx/canvas.h"
#include "settings/settings.h"
#include "ui/theme.h"

typedef struct CassetteColors {
    Color shell;
    Color edge;
    Color label;
    Color ink;
    Color ink_soft;
    Color stripe;
    Color window;
    Color reel;
    Color tape;
} CassetteColors;

typedef struct CassetteParts {
    Rect body;
    Rect label;
    Rect window;
    Rect base;
    Point left_reel;
    Point right_reel;
} CassetteParts;

enum {
    CASSETTE_PERMILLE = 1000,
    CASSETTE_TURN_STEPS = 12,
    CASSETTE_LABEL_TOP = 10,
    CASSETTE_LABEL_HEIGHT = 126,
    CASSETTE_STRIPE_TOP = 56,
    CASSETTE_WINDOW_TOP = 62,
    CASSETTE_WINDOW_HEIGHT = 42,
    CASSETTE_BASE_GAP = 6
};

CassetteColors cassette_colors(ThemeStyle style);
CassetteParts cassette_parts(Rect area);
typedef struct CassetteBackdrop {
    Rect area;
    Color color;
} CassetteBackdrop;

void cassette_draw_body(Canvas *canvas, const CassetteParts *parts, const CassetteColors *colors,
                        CassetteBackdrop backdrop);
void cassette_draw_reels(Canvas *canvas, const CassetteParts *parts, const CassetteColors *colors,
                         uint32_t progress_permille, int turn);
Rect cassette_window_middle(const CassetteParts *parts);
void cassette_draw_notice(Canvas *canvas, const Theme *theme, const char *heading,
                          const char *message, bool dac);
