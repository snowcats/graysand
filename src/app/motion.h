#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gfx/canvas.h"
#include "settings/settings.h"

enum {
    MOTION_PATH_CAPACITY = 48,
    MOTION_MAX_FRAMES = 80
};

typedef struct Motion {
    char folder[MOTION_PATH_CAPACITY];
    int frames;
    uint32_t frame_ms;
    uint32_t started_ms;
    uint32_t frame_bytes;
    bool loops;
    bool active;
} Motion;

bool motion_start(Motion *motion, const char *kind, ThemeChoice theme, uint32_t frame_ms,
                  bool loops, uint32_t frame_bytes, uint32_t now_ms);
void motion_stop(Motion *motion);
bool motion_running(const Motion *motion, uint32_t now_ms);
int motion_frame(const Motion *motion, uint32_t now_ms);
void motion_frame_path(const Motion *motion, int frame, char *path, int capacity);
void motion_draw(const Motion *motion, Canvas *canvas, Rect area, uint32_t now_ms);
