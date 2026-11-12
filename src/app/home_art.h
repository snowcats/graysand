#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "cover_cache.h"
#include "gfx/canvas.h"
#include "library_store.h"
#include "player.h"
#include "settings/settings.h"
#include "ui/menu_screen.h"
#include "ui/theme.h"

enum {
    HOME_ART_CYCLE_MS = 5000,
    HOME_PAGE_PATH_CAPACITY = 48
};

typedef struct HomeArt {
    CoverCache *covers;
    const LibraryStore *store;
    Player *player;
    const Theme *theme;
    const Settings *settings;
    bool cycling;
    bool has_album;
    uint16_t album;
    uint32_t next_change_ms;
    const MenuScreen *menu;
    const bool *quran_shown;
    bool has_page;
    uint32_t page_version;
    char page_path[HOME_PAGE_PATH_CAPACITY];
} HomeArt;

void home_art_init(HomeArt *art, CoverCache *covers, const LibraryStore *store, Player *player,
                   const Theme *theme);
void home_art_update(void *context, uint32_t now_ms);
void home_art_draw(void *context, Canvas *canvas, Rect area);
uint32_t home_art_picture(const void *context);
void home_art_page_path(ThemeChoice theme, int count, bool quran, const char *label, char *path,
                        int capacity);
const char *home_art_theme_folder(ThemeChoice theme);
void home_art_draw_file(const char *path, Canvas *canvas, Rect area);
