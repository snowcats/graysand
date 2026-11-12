#pragma once

#include <stdint.h>

#include "gfx/canvas.h"
#include "ui/theme.h"

typedef enum ArtSlotKind {
    ART_FILE,
    ART_CLOCK,
    ART_BATTERY,
    ART_ELAPSED,
    ART_REMAINING,
    ART_FORMAT,
    ART_CODEC,
    ART_RATE,
    ART_ARTIST,
    ART_ALBUM,
    ART_TEXT_COUNT,
    ART_PROGRESS = ART_TEXT_COUNT,
    ART_BATTERY_LEVEL,
    ART_KIND_COUNT
} ArtSlotKind;

enum {
    ART_INFO_VERSION = ART_FILE,
    ART_INFO_MEMORY = ART_ARTIST,
    ART_INFO_CARD = ART_ALBUM,
    ART_INFO_LIBRARY = ART_FORMAT,
    ART_INFO_BUILD = ART_CLOCK,
    ART_INFO_MEMORY_TOTAL = ART_BATTERY,
    ART_INFO_CARD_TOTAL = ART_REMAINING,
    ART_INFO_MEMORY_LEVEL = ART_PROGRESS,
    ART_INFO_CARD_LEVEL = ART_BATTERY_LEVEL
};

typedef enum ArtSlotStyle {
    ART_LEFT,
    ART_CENTER,
    ART_RIGHT,
    ART_BAR,
    ART_DISC,
    ART_MARK
} ArtSlotStyle;

typedef struct ArtSlot {
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
    uint8_t kind;
    uint8_t style;
    uint8_t font;
    uint8_t after;
    Color color;
    uint16_t unused;
} ArtSlot;

enum {
    ART_SLOT_CAPACITY = 16,
    ART_LEVEL_COUNT = ART_KIND_COUNT - ART_TEXT_COUNT,
    ART_PERMILLE = 1000
};

typedef struct ArtSlotValues {
    const char *texts[ART_TEXT_COUNT];
    uint16_t permille[ART_LEVEL_COUNT];
} ArtSlotValues;

int art_slots_load(const char *path, uint32_t pixel_bytes, ArtSlot *slots, int capacity);
void art_slots_draw(Canvas *canvas, const Theme *theme, const ArtSlot *slots, int count,
                    const ArtSlotValues *values);
