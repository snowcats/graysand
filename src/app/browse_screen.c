#include "browse_screen.h"

#include <stdbool.h>
#include <stddef.h>

#include "core/string_builder.h"
#include "hal/audio.h"
#include "hal/clock.h"
#include "library/sort_key.h"
#include "ui/header.h"
#include "ui/label.h"
#include "ui/list_navigation.h"

enum {
    NO_LEVEL = -1
};

static const char EMPTY_MESSAGE[] = "Nothing here";
static const char NO_FAVORITES_MESSAGE[] = "Hold Play on an album to add it";
static const char NO_BOOKS_MESSAGE[] = "Put books in an Audiobooks folder";
static const char NO_SHOWS_MESSAGE[] = "Put shows in a Podcasts folder";
static const char NO_RECITATIONS_MESSAGE[] = "Put recitations in a Quran folder";
static const char FAVORITES_TITLE[] = "Favorites";
static const char FAVORITE_SONGS_TITLE[] = "Favorite Songs";
static const char SONGS_TITLE[] = "Songs";
static const char ADD_FAVORITE_LABEL[] = "Add to Favorites";
static const char REMOVE_FAVORITE_LABEL[] = "Remove from Favorites";
static const char AUDIOBOOKS_TITLE[] = "Audiobooks";
static const char PODCASTS_TITLE[] = "Podcasts";
static const char QURAN_TITLE[] = "Quran";
static const char STAR[] = "\xE2\x98\x85";

static int min_of(int first, int second)
{
    return first < second ? first : second;
}

static BrowseRows *rows_of_level(BrowsePool *pool, int level)
{
    for (int slot = 0; slot < BROWSE_ROW_CACHES; slot++) {
        if (pool->rows[slot].level == level) {
            return &pool->rows[slot];
        }
    }
    return NULL;
}

static const BrowseRows *cached_rows(const BrowseScreen *browse)
{
    const BrowsePool *pool = browse->pool;
    for (int slot = 0; slot < BROWSE_ROW_CACHES; slot++) {
        if (pool->rows[slot].level == browse->level) {
            return &pool->rows[slot];
        }
    }
    return NULL;
}

static BrowseRows *rows_to_fill(BrowseScreen *browse)
{
    BrowsePool *pool = browse->pool;
    BrowseRows *rows = rows_of_level(pool, browse->level);
    if (rows == NULL) {
        rows = &pool->rows[0];
        for (int slot = 1; slot < BROWSE_ROW_CACHES; slot++) {
            if (pool->rows[slot].used < rows->used) {
                rows = &pool->rows[slot];
            }
        }
    }
    rows->used = ++pool->row_uses;
    return rows;
}

static void forget_rows(BrowseScreen *browse)
{
    BrowseRows *rows = rows_of_level(browse->pool, browse->level);
    if (rows != NULL) {
        rows->level = NO_LEVEL;
    }
}

static bool rows_current(const BrowseScreen *browse)
{
    const BrowseRows *rows = cached_rows(browse);
    return rows != NULL && rows->first == browse->list.top;
}

static bool is_pinned_favorites(const BrowseScreen *browse, int index)
{
    return (browse->favorites_pinned || browse->songs_pinned) && index == 0;
}

static uint32_t source_position(const BrowseScreen *browse, int index)
{
    int shift = browse->favorites_pinned || browse->songs_pinned ? 1 : 0;
    if (browse->favorites_only) {
        return browse->pool->favorite_albums[index - shift];
    }
    return (uint32_t)(index - shift);
}

static bool item_at(const BrowseScreen *browse, int index, BrowseItem *item)
{
    if (browse->favorite_songs) {
        bool inside = index >= 0 && index < (int)browse->pool->favorite_track_count;
        *item = (BrowseItem){.kind = BROWSE_ITEM_TRACK,
                             .index = inside ? browse->pool->favorite_tracks[index] : 0};
        return inside;
    }
    return !is_pinned_favorites(browse, index) &&
           browse_item(browse->pool->config.library, browse->query, source_position(browse, index),
                       item);
}

static bool is_favorite(const BrowseScreen *browse, BrowseItem item)
{
    const FavoritesStore *favorites = browse->pool->config.favorites;
    if (favorites == NULL) {
        return false;
    }
    if (item.kind == BROWSE_ITEM_TRACK) {
        return favorites_store_contains_track(favorites, (uint16_t)item.index);
    }
    return item.kind == BROWSE_ITEM_ALBUM && favorites_store_contains_album(favorites, item.index);
}

static void mark_favorite(char *detail)
{
    char year[BROWSE_DETAIL_CAPACITY];
    StringBuilder copy = string_builder_make(year, sizeof year);
    string_builder_append(&copy, detail);
    StringBuilder builder = string_builder_make(detail, BROWSE_DETAIL_CAPACITY);
    string_builder_append(&builder, STAR);
    if (year[0] != '\0') {
        string_builder_append_char(&builder, ' ');
        string_builder_append(&builder, year);
    }
}

static void describe_spoken(const BrowseScreen *browse, BrowseItem item, char *detail)
{
    const SpokenStore *spoken = browse->pool->config.spoken;
    if (browse->shelf == SPOKEN_NONE || spoken == NULL) {
        return;
    }
    SpokenRow row = {
        .shelf = browse->shelf, .item = item, .detail = detail, .capacity = BROWSE_DETAIL_CAPACITY};
    spoken_store_describe(spoken, row);
}

static void load_row(BrowseScreen *browse, BrowseRows *rows, int row, int index)
{
    BrowseText text = {rows->labels[row], BROWSE_LABEL_CAPACITY, rows->details[row],
                       BROWSE_DETAIL_CAPACITY};
    BrowseItem item;
    rows->details[row][0] = '\0';
    rows->opens[row] = false;
    if (is_pinned_favorites(browse, index)) {
        StringBuilder label = string_builder_make(rows->labels[row], BROWSE_LABEL_CAPACITY);
        string_builder_append(&label, browse->songs_pinned ? SONGS_TITLE : FAVORITES_TITLE);
        rows->opens[row] = true;
        return;
    }
    if (!item_at(browse, index, &item) ||
        !browse_item_text(browse->pool->config.library, browse->query, item, text)) {
        rows->labels[row][0] = '\0';
        rows->details[row][0] = '\0';
        return;
    }
    rows->opens[row] = item.kind != BROWSE_ITEM_TRACK;
    if (is_favorite(browse, item)) {
        mark_favorite(rows->details[row]);
    }
    describe_spoken(browse, item, rows->details[row]);
}

static void refresh_rows(BrowseScreen *browse)
{
    if (rows_current(browse)) {
        return;
    }
    BrowseRows *rows = rows_to_fill(browse);
    rows->level = NO_LEVEL;
    int top = browse->list.top;
    int count = min_of(min_of(browse->list.visible_rows, BROWSE_ROWS), (int)browse->count - top);
    for (int row = 0; row < count; row++) {
        load_row(browse, rows, row, top + row);
    }
    rows->level = browse->level;
    rows->first = top;
    rows->count = count;
}

static const char *cached_text(const BrowseScreen *browse, int index, bool detail)
{
    const BrowseRows *rows = cached_rows(browse);
    int row = rows != NULL ? index - rows->first : -1;
    if (rows == NULL || row < 0 || row >= rows->count) {
        return "";
    }
    return detail ? rows->details[row] : rows->labels[row];
}

const char *browse_row_label(const BrowseScreen *browse, int index)
{
    return cached_text(browse, index, false);
}

bool browse_album_at(const BrowseScreen *browse, int index, uint16_t *album)
{
    BrowseItem item;
    if (!item_at(browse, index, &item) || item.kind != BROWSE_ITEM_ALBUM) {
        return false;
    }
    *album = item.index;
    return true;
}

static void album_thumb(const void *context, int index, Canvas *canvas, Rect cell)
{
    const BrowseScreen *browse = context;
    uint16_t album = 0;
    Color empty = browse->pool->config.theme->colors.divider;
    if (browse->pool->config.covers == NULL || !browse_album_at(browse, index, &album)) {
        canvas_fill_rect(canvas, cell, empty);
        return;
    }
    cover_cache_draw(browse->pool->config.covers, album, canvas, cell, empty);
}

static int track_lead(const void *context, int index)
{
    (void)context;
    return index + 1;
}

static int row_count(const void *context)
{
    const BrowseScreen *browse = context;
    return (int)browse->count;
}

static const char *row_label(const void *context, int index)
{
    return cached_text(context, index, false);
}

static const char *row_detail(const void *context, int index)
{
    const char *detail = cached_text(context, index, true);
    return detail[0] != '\0' ? detail : NULL;
}

static bool row_opens(const void *context, int index)
{
    const BrowseRows *rows = cached_rows(context);
    int row = rows != NULL ? index - rows->first : -1;
    return rows != NULL && row >= 0 && row < rows->count && rows->opens[row];
}

static bool is_lettered(BrowseKind kind)
{
    switch (kind) {
    case BROWSE_ARTISTS:
    case BROWSE_ALBUM_ARTISTS:
    case BROWSE_GENRES:
    case BROWSE_ALBUMS:
    case BROWSE_ALL_SONGS:
    case BROWSE_PLAYLISTS:
        return true;
    case BROWSE_GROUP_ALBUMS:
    case BROWSE_ALBUM_TRACKS:
    case BROWSE_FOLDER:
    case BROWSE_PLAYLIST_TRACKS:
        break;
    }
    return false;
}

static int row_section(const void *context, int index)
{
    const BrowseScreen *browse = context;
    char label[BROWSE_LABEL_CAPACITY];
    char detail[BROWSE_DETAIL_CAPACITY];
    BrowseText text = {label, sizeof label, detail, sizeof detail};
    BrowseItem item;
    label[0] = '\0';
    if (item_at(browse, index, &item)) {
        browse_item_text(browse->pool->config.library, browse->query, item, text);
    }
    char initial[2] = {sort_key_initial(label), '\0'};
    return list_section_of_key(initial);
}

static bool is_loaded_track(const BrowseScreen *browse, int index)
{
    BrowseItem item;
    PlayerStatus status = player_status(browse->pool->config.player);
    return status.has_track && status.state != PLAYER_IDLE && item_at(browse, index, &item) &&
           item.kind == BROWSE_ITEM_TRACK && item.index == status.track;
}

ScreenAction browse_start_list(BrowseScreen *browse, BrowseQuery source, uint32_t position)
{
    BrowsePoolConfig *config = &browse->pool->config;
    PlayerListStart start = {.source = source,
                             .position = position,
                             .seed = hal_clock_microseconds(),
                             .in_order = browse->shelf != SPOKEN_NONE};
    if (!player_play_from(config->player, start)) {
        return screen_action_none();
    }
    return screen_action_push(config->now_playing);
}

static ScreenAction play_from(BrowseScreen *browse, int index)
{
    if (is_loaded_track(browse, index)) {
        Player *player = browse->pool->config.player;
        if (player_status(player).state == PLAYER_PAUSED) {
            player_toggle_pause(player);
        }
        return screen_action_push(browse->pool->config.now_playing);
    }
    return browse_start_list(browse, browse->query, (uint32_t)index);
}

static ScreenAction open_child(BrowseScreen *browse, BrowseItem item)
{
    BrowseQuery child;
    if (!browse_child(browse->query, item, &child)) {
        return screen_action_none();
    }
    Screen *next = browse_pool_open(browse->pool, browse->level + 1, child);
    if (next == NULL) {
        return screen_action_none();
    }
    browse->pool->screens[browse->level + 1].shelf = browse->shelf;
    return screen_action_push(next);
}

static bool playing_folder(const BrowseScreen *browse, uint16_t folder)
{
    LibraryTrack record;
    PlayerStatus status = player_status(browse->pool->config.player);
    return status.has_track && status.state != PLAYER_IDLE &&
           library_read_track(browse->pool->config.library, status.track, &record) &&
           record.folder == folder;
}

static ScreenAction resume_book(BrowseScreen *browse, BrowseItem item)
{
    const BrowsePoolConfig *config = &browse->pool->config;
    LibraryFolder folder;
    BrowseQuery child;
    if (config->spoken == NULL || !library_read_folder(config->library, item.index, &folder) ||
        folder.track_count == 0 || !browse_child(browse->query, item, &child)) {
        return open_child(browse, item);
    }
    if (playing_folder(browse, item.index)) {
        if (player_status(config->player).state == PLAYER_PAUSED) {
            player_toggle_pause(config->player);
        }
        return screen_action_push(config->now_playing);
    }
    uint16_t chapter = spoken_store_resume_chapter(config->spoken, item.index);
    return browse_start_list(browse, child, (uint32_t)folder.subfolder_count + chapter);
}

static bool resumes_books(const BrowseScreen *browse, BrowseItem item)
{
    return browse->shelf == SPOKEN_AUDIOBOOK && browse->shelf_root &&
           item.kind == BROWSE_ITEM_FOLDER;
}

static ScreenAction select_row(void *context, int index)
{
    BrowseScreen *browse = context;
    BrowseItem item;
    if (is_pinned_favorites(browse, index)) {
        Screen *favorites = browse->songs_pinned
                                ? browse_pool_open_favorite_songs(browse->pool, browse->level + 1)
                                : browse_pool_open_favorites(browse->pool, browse->level + 1);
        return favorites != NULL ? screen_action_push(favorites) : screen_action_none();
    }
    if (browse->favorite_songs && item_at(browse, index, &item)) {
        BrowsePool *pool = browse->pool;
        uint16_t left = (uint16_t)(pool->favorite_track_count - index);
        if (!player_play_tracks(pool->config.player, &pool->favorite_tracks[index], left)) {
            return screen_action_none();
        }
        return screen_action_push(pool->config.now_playing);
    }
    if (!item_at(browse, index, &item)) {
        return screen_action_none();
    }
    if (item.kind == BROWSE_ITEM_TRACK) {
        return play_from(browse, index);
    }
    if (resumes_books(browse, item)) {
        return resume_book(browse, item);
    }
    if (browse->grid && item.kind == BROWSE_ITEM_ALBUM) {
        album_page_open(browse, browse->now_ms);
        return screen_action_none();
    }
    return open_child(browse, item);
}

static void keep_audio_fed(void *context)
{
    (void)context;
    hal_audio_service();
}

static void refresh_favorites(BrowsePool *pool)
{
    const FavoritesStore *store = pool->config.favorites;
    if (pool->favorites_collected && pool->favorites_revision == store->revision) {
        return;
    }
    FavoritesCollector collector = {.found = pool->favorite_albums,
                                    .capacity = FAVORITES_CAPACITY,
                                    .between_albums = keep_audio_fed};
    pool->favorite_count = favorites_collect(&store->favorites, pool->config.library, collector);
    collector.found = pool->favorite_tracks;
    pool->favorite_track_count =
        favorites_collect_tracks(&store->favorites, pool->config.library, collector);
    pool->favorites_revision = store->revision;
    pool->favorites_collected = true;
}

static void collect_favorites(BrowseScreen *browse)
{
    refresh_favorites(browse->pool);
    if (browse->favorite_songs) {
        browse->count = browse->pool->favorite_track_count;
    } else {
        browse->songs_pinned = browse->pool->favorite_track_count > 0;
        browse->count = browse->pool->favorite_count + (browse->songs_pinned ? 1u : 0u);
    }
    list_view_select(&browse->list, browse->list.selected, browse->now_ms);
}

static ScreenAction toggle_favorite(void *context, int index)
{
    BrowseScreen *browse = context;
    BrowsePoolConfig *config = &browse->pool->config;
    BrowseItem item;
    if (config->favorites == NULL || !item_at(browse, index, &item) ||
        item.kind != BROWSE_ITEM_ALBUM) {
        return screen_action_none();
    }
    FavoriteResult result = favorites_store_toggle_album(config->favorites, item.index);
    if (config->toast != NULL) {
        toast_show(config->toast, favorites_store_message(result), browse->now_ms);
    }
    forget_rows(browse);
    if (browse->favorites_only) {
        collect_favorites(browse);
    }
    return screen_action_none();
}

static const char *up_next_message(PlayerUpNextResult result, bool next)
{
    switch (result) {
    case PLAYER_UP_NEXT_ADDED:
        return next ? "Playing next" : "Added to Up Next";
    case PLAYER_UP_NEXT_STARTED:
        return "Playing";
    case PLAYER_UP_NEXT_FULL:
        return "Up Next is full";
    case PLAYER_UP_NEXT_FAILED:
        break;
    }
    return "Couldn't play the song";
}

static ScreenAction queue_song(BrowsePool *pool, bool next)
{
    SongActions *actions = &pool->song_actions;
    Player *player = pool->config.player;
    PlayerUpNextResult result = next ? player_play_next(player, actions->track)
                                     : player_add_to_up_next(player, actions->track);
    if (pool->config.toast != NULL) {
        toast_show(pool->config.toast, up_next_message(result, next), actions->now_ms);
    }
    return screen_action_pop();
}

static ScreenAction play_song_next(void *context)
{
    return queue_song(context, true);
}

static ScreenAction add_song_to_up_next(void *context)
{
    return queue_song(context, false);
}

static ScreenAction toggle_song_favorite(void *context)
{
    BrowsePool *pool = context;
    SongActions *actions = &pool->song_actions;
    FavoriteResult result = favorites_store_toggle_track(pool->config.favorites, actions->track);
    if (pool->config.toast != NULL) {
        toast_show(pool->config.toast, favorites_store_message(result), actions->now_ms);
    }
    return screen_action_pop();
}

static ScreenAction open_song_actions(BrowseScreen *browse, int index, uint16_t track)
{
    BrowsePool *pool = browse->pool;
    SongActions *actions = &pool->song_actions;
    actions->track = track;
    actions->now_ms = browse->now_ms;
    StringBuilder title = string_builder_make(actions->title, sizeof actions->title);
    string_builder_append(&title, row_label(browse, index));
    actions->items[0] = (MenuItem){.label = "Play Next", .select = play_song_next, .context = pool};
    actions->items[1] =
        (MenuItem){.label = "Add to Up Next", .select = add_song_to_up_next, .context = pool};
    int count = SONG_ACTION_COUNT - 1;
    if (pool->config.favorites != NULL) {
        bool favorite = favorites_store_contains_track(pool->config.favorites, track);
        actions->items[count++] =
            (MenuItem){.label = favorite ? REMOVE_FAVORITE_LABEL : ADD_FAVORITE_LABEL,
                       .select = toggle_song_favorite,
                       .context = pool};
    }
    MenuConfig config = {.title = actions->title,
                         .items = actions->items,
                         .item_count = count,
                         .theme = pool->config.theme};
    return screen_action_push(menu_screen_init(&actions->menu, config));
}

static ScreenAction hold_row(void *context, int index)
{
    BrowseScreen *browse = context;
    BrowseItem item;
    if (!item_at(browse, index, &item)) {
        return screen_action_none();
    }
    if (browse->pool->config.player != NULL && item.kind == BROWSE_ITEM_TRACK) {
        return open_song_actions(browse, index, item.index);
    }
    if (browse->shelf != SPOKEN_NONE && item.kind == BROWSE_ITEM_FOLDER) {
        return open_child(browse, item);
    }
    return toggle_favorite(context, index);
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    BrowseScreen *browse = context;
    browse->now_ms = now_ms;
    if (browse->page_open) {
        return album_page_handle_key(browse, event, now_ms);
    }
    ListSelection selection = {.select = select_row, .hold = hold_row, .context = browse};
    ScreenAction action = list_navigation_handle(&browse->list, event, now_ms, selection);
    if (browse->grid) {
        album_grid_keep_rows(browse);
    }
    return action;
}

static void enter(void *context, uint32_t now_ms)
{
    BrowseScreen *browse = context;
    forget_rows(browse);
    browse->now_ms = now_ms;
    if (browse->favorites_only || browse->favorite_songs) {
        collect_favorites(browse);
    }
    list_view_restart_marquee(&browse->list, now_ms);
    refresh_rows(browse);
}

static void update(void *context, uint32_t now_ms)
{
    BrowseScreen *browse = context;
    browse->now_ms = now_ms;
    refresh_rows(browse);
}

static void draw_empty(Canvas *canvas, const Theme *theme, Rect content, const char *message)
{
    TextStyle style = {.font = theme->fonts.body, .color = theme->colors.text_secondary};
    canvas_fill_rect(canvas, content, theme->colors.background);
    Rect area = label_centered_area(style.font, content, message);
    label_draw(canvas, style, (Label){.area = area, .text = message});
}

static const char *empty_message(const BrowseScreen *browse)
{
    if (browse->favorites_only) {
        return NO_FAVORITES_MESSAGE;
    }
    if (browse->shelf_root) {
        return browse->shelf == SPOKEN_PODCAST ? NO_SHOWS_MESSAGE
               : browse->shelf == SPOKEN_QURAN ? NO_RECITATIONS_MESSAGE
                                               : NO_BOOKS_MESSAGE;
    }
    return EMPTY_MESSAGE;
}

static void draw(void *context, Canvas *canvas)
{
    const BrowseScreen *browse = context;
    const Theme *theme = browse->pool->config.theme;
    Rect content = theme_content_area(theme);
    header_draw(canvas, theme, browse->title);
    if (browse->count == 0) {
        draw_empty(canvas, theme, content, empty_message(browse));
        return;
    }
    if (browse->grid) {
        if (browse->page_open) {
            album_page_draw(browse, canvas, content);
        } else {
            album_grid_draw(browse, canvas, content);
        }
        album_counter_draw(browse, canvas);
        return;
    }
    list_view_draw(&browse->list, canvas, content, browse->now_ms);
}

static uint32_t picture(const void *context)
{
    const BrowseScreen *browse = context;
    if (browse->count == 0) {
        return 0;
    }
    uint32_t list = list_view_picture(&browse->list, theme_content_area(browse->pool->config.theme),
                                      browse->now_ms, browse->title);
    return browse->page_open ? screen_picture_mix(list, album_page_picture(browse)) : list;
}

void browse_pool_init(BrowsePool *pool, BrowsePoolConfig config)
{
    pool->config = config;
    pool->row_uses = 0;
    pool->favorites_collected = false;
    for (int slot = 0; slot < BROWSE_ROW_CACHES; slot++) {
        pool->rows[slot] = (BrowseRows){.level = NO_LEVEL};
    }
}

Screen *browse_pool_open(BrowsePool *pool, int level, BrowseQuery query)
{
    if (level < 0 || level >= BROWSE_DEPTH) {
        return NULL;
    }
    BrowseScreen *browse = &pool->screens[level];
    browse->pool = pool;
    browse->level = level;
    browse->query = query;
    browse->favorites_only = false;
    browse->favorites_pinned = query.kind == BROWSE_PLAYLISTS && pool->config.favorites != NULL;
    browse->favorite_songs = false;
    browse->songs_pinned = false;
    browse->shelf = SPOKEN_NONE;
    browse->shelf_root = false;
    browse->count = browse_count(pool->config.library, query) + (browse->favorites_pinned ? 1 : 0);
    browse_title(pool->config.library, query, browse->title, sizeof browse->title);
    browse->model = (ListModel){.context = browse,
                                .count = row_count,
                                .label = row_label,
                                .detail = row_detail,
                                .opens = row_opens,
                                .section = is_lettered(query.kind) ? row_section : NULL};
    bool albums = query.kind == BROWSE_ALBUMS || query.kind == BROWSE_GROUP_ALBUMS;
    browse->model.two_line = albums || query.kind == BROWSE_ALBUM_TRACKS;
    browse->model.thumb = albums ? album_thumb : NULL;
    browse->model.lead = query.kind == BROWSE_ALBUM_TRACKS ? track_lead : NULL;
    browse->grid = query.kind == BROWSE_ALBUMS && pool->config.settings != NULL &&
                   pool->config.settings->album_view == ALBUM_VIEW_GRID;
    browse->page_open = false;
    list_view_init(&browse->list, &browse->model, pool->config.theme, 0);
    if (browse->grid) {
        browse->list.visible_rows = ALBUM_GRID_COLUMNS * ALBUM_GRID_ROWS;
    }
    browse->screen = (Screen){
        .title = browse->title,
        .context = browse,
        .enter = enter,
        .handle_key = handle_key,
        .update = update,
        .draw = draw,
        .picture = picture,
    };
    return &browse->screen;
}

Screen *browse_pool_open_favorites(BrowsePool *pool, int level)
{
    if (pool->config.favorites == NULL) {
        return NULL;
    }
    Screen *screen = browse_pool_open(pool, level, browse_root_query(BROWSE_ALBUMS));
    if (screen == NULL) {
        return NULL;
    }
    BrowseScreen *browse = &pool->screens[level];
    StringBuilder title = string_builder_make(browse->title, sizeof browse->title);
    string_builder_append(&title, FAVORITES_TITLE);
    browse->favorites_only = true;
    browse->favorites_pinned = false;
    browse->model.section = NULL;
    browse->grid = false;
    list_view_init(&browse->list, &browse->model, pool->config.theme, 0);
    collect_favorites(browse);
    return screen;
}

Screen *browse_pool_open_favorite_songs(BrowsePool *pool, int level)
{
    if (pool->config.favorites == NULL) {
        return NULL;
    }
    Screen *screen = browse_pool_open(pool, level, browse_root_query(BROWSE_ALL_SONGS));
    if (screen == NULL) {
        return NULL;
    }
    BrowseScreen *browse = &pool->screens[level];
    StringBuilder title = string_builder_make(browse->title, sizeof browse->title);
    string_builder_append(&title, FAVORITE_SONGS_TITLE);
    browse->favorite_songs = true;
    browse->model.section = NULL;
    collect_favorites(browse);
    return screen;
}

static BrowseQuery shelf_query(const BrowsePool *pool, SpokenKind shelf, bool *found)
{
    uint16_t folder = 0;
    BrowseQuery query = browse_root_query(BROWSE_FOLDER);
    *found = spoken_shelf_folder(pool->config.library, shelf, &folder);
    if (*found) {
        BrowseItem item = {.kind = BROWSE_ITEM_FOLDER, .index = folder};
        browse_child(query, item, &query);
    }
    return query;
}

Screen *browse_pool_open_shelf(BrowsePool *pool, int level, SpokenKind shelf)
{
    bool found = false;
    Screen *screen = browse_pool_open(pool, level, shelf_query(pool, shelf, &found));
    if (screen == NULL) {
        return NULL;
    }
    BrowseScreen *browse = &pool->screens[level];
    StringBuilder title = string_builder_make(browse->title, sizeof browse->title);
    string_builder_append(&title, shelf == SPOKEN_PODCAST ? PODCASTS_TITLE
                                  : shelf == SPOKEN_QURAN ? QURAN_TITLE
                                                          : AUDIOBOOKS_TITLE);
    browse->shelf = shelf;
    browse->shelf_root = true;
    browse->count = found ? browse->count : 0;
    return screen;
}
