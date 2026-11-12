#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gfx/image.h"
#include "hal/storage.h"
#include "library/covers.h"
#include "library/library.h"

typedef enum CoverState {
    COVER_EMPTY,
    COVER_LOADED,
    COVER_MISSING
} CoverState;

typedef struct CoverCache {
    const Library *library;
    HalFile file;
    Covers covers;
    bool available;
    uint16_t full_album;
    CoverState full_state;
    Color full[COVER_FULL_PIXELS];
} CoverCache;

extern const char COVER_CACHE_PATH[];

void cover_cache_open(CoverCache *cache, const Library *library);
void cover_cache_close(CoverCache *cache);
CoverState cover_cache_full(const CoverCache *cache, uint16_t album, Image *image);
bool cover_cache_load_full(CoverCache *cache, uint16_t album);
bool cover_cache_find(const CoverCache *cache, uint16_t album, uint32_t *record);
void cover_cache_draw(const CoverCache *cache, uint16_t album, Canvas *canvas, Rect cell,
                      Color empty);
bool cover_cache_read_half_rows(const CoverCache *cache, uint32_t record, int first_row, int rows,
                                Color *pixels);
