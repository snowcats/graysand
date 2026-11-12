#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core/wall_time.h"
#include "gfx/canvas.h"
#include "library/library.h"
#include "player.h"
#include "ui/theme.h"

enum {
    IDLE_TRACK_TEXT_CAPACITY = 64
};

typedef struct IdleTrack {
    bool has_track;
    uint16_t track;
    char title[IDLE_TRACK_TEXT_CAPACITY];
    char artist[IDLE_TRACK_TEXT_CAPACITY];
    bool clock_known;
    WallTime clock;
    uint32_t clock_ms;
} IdleTrack;

void idle_track_update(IdleTrack *idle, const Library *library, PlayerStatus status);
void idle_track_draw(const IdleTrack *idle, Canvas *canvas, const Theme *theme,
                     PlayerStatus status);
uint32_t idle_track_picture(const IdleTrack *idle, PlayerStatus status);
