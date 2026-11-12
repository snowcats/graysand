#pragma once

#include <stdint.h>

#include "cover_cache.h"
#include "favorites_store.h"
#include "library/favorites.h"
#include "library/library_browse.h"
#include "player.h"
#include "settings/settings.h"
#include "spoken_store.h"
#include "ui/list_view.h"
#include "ui/menu_screen.h"
#include "ui/screen.h"
#include "ui/theme.h"
#include "ui/toast.h"

enum {
    BROWSE_DEPTH = 6,
    BROWSE_ROWS = 10,
    BROWSE_ROW_CACHES = 2,
    BROWSE_LABEL_CAPACITY = 128,
    BROWSE_DETAIL_CAPACITY = 64,
    BROWSE_TITLE_CAPACITY = 96,
    SONG_ACTION_COUNT = 3,
    ALBUM_GRID_COLUMNS = 5,
    ALBUM_GRID_ROWS = 2,
    ALBUM_PAGE_ROWS = 6
};

typedef struct BrowseRows {
    int level;
    uint32_t used;
    int first;
    int count;
    char labels[BROWSE_ROWS][BROWSE_LABEL_CAPACITY];
    char details[BROWSE_ROWS][BROWSE_DETAIL_CAPACITY];
    bool opens[BROWSE_ROWS];
} BrowseRows;

typedef struct BrowsePool BrowsePool;

typedef struct BrowseScreen {
    Screen screen;
    BrowsePool *pool;
    int level;
    BrowseQuery query;
    uint32_t count;
    bool favorites_only;
    bool favorites_pinned;
    bool favorite_songs;
    bool songs_pinned;
    SpokenKind shelf;
    bool shelf_root;
    bool grid;
    bool page_open;
    char title[BROWSE_TITLE_CAPACITY];
    ListModel model;
    ListView list;
    uint32_t now_ms;
} BrowseScreen;

typedef struct BrowsePoolConfig {
    const Library *library;
    const Theme *theme;
    Player *player;
    Screen *now_playing;
    FavoritesStore *favorites;
    Toast *toast;
    SpokenStore *spoken;
    CoverCache *covers;
    const Settings *settings;
} BrowsePoolConfig;

typedef struct AlbumPage {
    uint16_t album;
    BrowseQuery tracks;
    uint32_t track_count;
    char title[BROWSE_TITLE_CAPACITY];
    char line[BROWSE_LABEL_CAPACITY];
    uint8_t artist_length;
    char labels[ALBUM_PAGE_ROWS][BROWSE_LABEL_CAPACITY];
    int labels_top;
    ListModel model;
    ListView list;
} AlbumPage;

typedef struct SongActions {
    MenuScreen menu;
    MenuItem items[SONG_ACTION_COUNT];
    uint16_t track;
    uint32_t now_ms;
    char title[BROWSE_LABEL_CAPACITY];
} SongActions;

struct BrowsePool {
    BrowsePoolConfig config;
    BrowseRows rows[BROWSE_ROW_CACHES];
    uint32_t row_uses;
    BrowseScreen screens[BROWSE_DEPTH];
    uint16_t favorite_albums[FAVORITES_CAPACITY];
    uint16_t favorite_count;
    uint16_t favorite_tracks[FAVORITES_CAPACITY];
    uint16_t favorite_track_count;
    bool favorites_collected;
    uint32_t favorites_revision;
    SongActions song_actions;
    AlbumPage album_page;
};

void browse_pool_init(BrowsePool *pool, BrowsePoolConfig config);
Screen *browse_pool_open(BrowsePool *pool, int level, BrowseQuery query);
Screen *browse_pool_open_favorites(BrowsePool *pool, int level);
Screen *browse_pool_open_favorite_songs(BrowsePool *pool, int level);
Screen *browse_pool_open_shelf(BrowsePool *pool, int level, SpokenKind shelf);
const char *browse_row_label(const BrowseScreen *browse, int index);
bool browse_album_at(const BrowseScreen *browse, int index, uint16_t *album);
ScreenAction browse_start_list(BrowseScreen *browse, BrowseQuery source, uint32_t position);
void album_grid_keep_rows(BrowseScreen *browse);
void album_grid_draw(const BrowseScreen *browse, Canvas *canvas, Rect content);
void album_counter_draw(const BrowseScreen *browse, Canvas *canvas);
void album_page_open(BrowseScreen *browse, uint32_t now_ms);
ScreenAction album_page_handle_key(BrowseScreen *browse, KeyEvent event, uint32_t now_ms);
void album_page_draw(const BrowseScreen *browse, Canvas *canvas, Rect content);
uint32_t album_page_picture(const BrowseScreen *browse);
