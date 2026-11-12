#include "app_screens.h"

#include <stdbool.h>
#include <stddef.h>

#include "board_layout.h"
#include "diagnostics_values.h"
#include "hal/power.h"
#include "hal/system.h"
#include "playback/spoken.h"

typedef enum MusicSource {
    MUSIC_LIBRARY,
    MUSIC_FAVORITES
} MusicSource;

typedef struct MusicEntry {
    const char *label;
    MusicSource source;
    BrowseKind kind;
} MusicEntry;

enum {
    MUSIC_FAVORITES_ENTRY = 0,
    MUSIC_PLAYLISTS_ENTRY = 1,
    MUSIC_ALBUMS_ENTRY = 2,
    MUSIC_ARTISTS_ENTRY = 3,
    MUSIC_SONGS_ENTRY = 5
};

static const MusicEntry MUSIC_ENTRIES[MUSIC_ITEM_COUNT] = {
    {"Favorites", MUSIC_FAVORITES, BROWSE_ALBUMS}, {"Playlists", MUSIC_LIBRARY, BROWSE_PLAYLISTS},
    {"Albums", MUSIC_LIBRARY, BROWSE_ALBUMS},      {"Artists", MUSIC_LIBRARY, BROWSE_ARTISTS},
    {"Genres", MUSIC_LIBRARY, BROWSE_GENRES},      {"Songs", MUSIC_LIBRARY, BROWSE_ALL_SONGS},
};

MenuItem app_screens_open_item(const char *label, Screen *target)
{
    return (MenuItem){.label = label,
                      .select = menu_open_screen,
                      .context = target,
                      .value = NULL,
                      .opens = true};
}

Screen *app_screens_build_menu(MenuScreen *menu, const char *title, const MenuItem *items,
                               int count, const Theme *theme)
{
    return menu_screen_init(
        menu, (MenuConfig){.title = title, .items = items, .item_count = count, .theme = theme});
}

static Screen *browse_screen_for(BrowseEntry *entry)
{
    if (!entry->store->available) {
        return entry->unavailable;
    }
    if (entry->favorites) {
        return browse_pool_open_favorites(entry->pool, 0);
    }
    if (entry->shelf != SPOKEN_NONE) {
        return browse_pool_open_shelf(entry->pool, 0, entry->shelf);
    }
    return browse_pool_open(entry->pool, 0, entry->query);
}

static ScreenAction open_browse(void *context)
{
    return screen_action_push(browse_screen_for(context));
}

static MenuItem music_item(AppScreens *screens, const LibraryStore *store, int index)
{
    const MusicEntry *entry = &MUSIC_ENTRIES[index];
    screens->browse_entries[index] = (BrowseEntry){
        .pool = &screens->browse,
        .store = store,
        .unavailable = &screens->no_library.screen,
        .query = browse_root_query(entry->kind),
        .favorites = entry->source == MUSIC_FAVORITES,
        .shelf = SPOKEN_NONE,
    };
    return (MenuItem){.label = entry->label,
                      .select = open_browse,
                      .context = &screens->browse_entries[index],
                      .value = NULL,
                      .opens = true};
}

static void build_music(AppScreens *screens, AppScreensContext context)
{
    BrowsePoolConfig browse = {
        .library = &context.store->library,
        .theme = context.theme,
        .player = context.player,
        .now_playing = &screens->now_playing.screen,
        .favorites = context.favorites,
        .toast = context.toast,
        .spoken = context.spoken,
        .covers = context.covers,
        .settings = context.settings != NULL ? &context.settings->settings : NULL,
    };
    browse_pool_init(&screens->browse, browse);
    for (int index = 0; index < MUSIC_ITEM_COUNT; index++) {
        screens->music_items[index] = music_item(screens, context.store, index);
    }
    app_screens_build_menu(&screens->music, "Music", screens->music_items, MUSIC_ITEM_COUNT,
                           context.theme);
}

static void build_settings(AppScreens *screens, AppScreensContext context)
{
    SettingsContext settings = {
        .store = context.settings,
        .player = context.player,
        .power = context.power,
        .toast = context.toast,
        .rescan_requested = context.rescan_requested,
        .bluetooth = &screens->bluetooth,
        .diagnostics = &screens->diagnostics.screen,
        .files = files_screen_init(&screens->files, context.theme),
        .sleep_option = 0,
        .library = context.store,
    };
    settings_screens_init(&screens->settings, settings, context.theme);
}

static const char *const SHELF_LABELS[SHELF_COUNT] = {"Audiobooks", "Podcasts", "Quran"};

static MenuItem shelf_item(AppScreens *screens, AppScreensContext context, SpokenKind shelf)
{
    BrowseEntry *entry = &screens->shelf_entries[shelf - SPOKEN_AUDIOBOOK];
    *entry = (BrowseEntry){
        .pool = &screens->browse,
        .store = context.store,
        .unavailable = &screens->no_library.screen,
        .shelf = shelf,
    };
    return (MenuItem){.label = SHELF_LABELS[shelf - SPOKEN_AUDIOBOOK],
                      .select = open_browse,
                      .context = entry,
                      .opens = true};
}

static bool podcasts_together(const void *context)
{
    const Settings *settings = context;
    return !settings->podcasts_separate;
}

static bool podcasts_apart(const void *context)
{
    const Settings *settings = context;
    return settings->podcasts_separate;
}

static bool quran_found(const void *context)
{
    return *(const bool *)context;
}

static MenuItem shown_when(MenuItem item, bool (*shown)(const void *), const Settings *settings)
{
    item.shown = shown;
    item.shown_context = settings;
    return item;
}

static void build_home(AppScreens *screens, AppScreensContext context)
{
    const Theme *theme = context.theme;
    const Settings *settings = &context.settings->settings;
    MenuItem audiobooks = shelf_item(screens, context, SPOKEN_AUDIOBOOK);
    MenuItem podcasts = shelf_item(screens, context, SPOKEN_PODCAST);
    screens->books_items[0] = audiobooks;
    screens->books_items[1] = podcasts;
    app_screens_build_menu(&screens->books, "Books", screens->books_items, BOOKS_ITEM_COUNT, theme);
    screens->home_items[0] = app_screens_open_item("Music", &screens->music.screen);
    uint16_t quran_folder = 0;
    screens->has_quran = context.store->available &&
                         spoken_shelf_folder(&context.store->library, SPOKEN_QURAN, &quran_folder);
    screens->home_items[1] = shelf_item(screens, context, SPOKEN_QURAN);
    screens->home_items[1].shown = quran_found;
    screens->home_items[1].shown_context = &screens->has_quran;
    screens->home_items[2] = shown_when(app_screens_open_item("Books", &screens->books.screen),
                                        podcasts_together, settings);
    screens->home_items[3] = shown_when(audiobooks, podcasts_apart, settings);
    screens->home_items[4] = shown_when(podcasts, podcasts_apart, settings);
    screens->home_items[5] = (MenuItem){.label = "Bluetooth",
                                        .select = bluetooth_screen_open,
                                        .context = &screens->bluetooth,
                                        .value = bluetooth_screen_summary,
                                        .opens = true};
    screens->home_items[6] = app_screens_open_item("Settings", &screens->settings.menu.screen);
    home_art_init(&screens->home_art, context.covers, context.store, context.player, theme);
    screens->home_art.settings = settings;
    screens->home_art.menu = &screens->home;
    screens->home_art.quran_shown = &screens->has_quran;
    MenuConfig home = {.title = "Home",
                       .items = screens->home_items,
                       .item_count = HOME_ITEM_COUNT,
                       .theme = theme,
                       .panel = {.width = BOARD_LAYOUT.home_art_width,
                                 .update = home_art_update,
                                 .draw = home_art_draw,
                                 .picture = home_art_picture,
                                 .context = &screens->home_art}};
    menu_screen_init(&screens->home, home);
}

Screen *app_screens_init(AppScreens *screens, AppScreensContext context)
{
    const Theme *theme = context.theme;
    MessageConfig no_library = {
        .title = "No library", .message = "Copy music to the player over USB", .theme = theme};
    UpNextConfig up_next = {
        .library = &context.store->library, .theme = theme, .player = context.player};
    NowPlayingConfig now_playing = {.player = context.player,
                                    .library = &context.store->library,
                                    .theme = theme,
                                    .covers = context.covers,
                                    .up_next = up_next_screen_init(&screens->up_next, up_next),
                                    .settings = &context.settings->settings};
    message_screen_init(&screens->no_library, no_library);
    bluetooth_screen_init(&screens->bluetooth, context.bluetooth, theme);
    now_playing_screen_init(&screens->now_playing, now_playing);
    build_music(screens, context);
    build_settings(screens, context);
    diagnostics_menu_build(screens, context.options, theme);
    build_home(screens, context);
    return &screens->home.screen;
}

static int music_path(AppScreens *screens, int entry, Screen **path)
{
    BrowseEntry *browse = &screens->browse_entries[entry];
    if (!browse->store->available) {
        return 0;
    }
    list_view_select(&screens->music.list, entry, 0);
    path[0] = &screens->music.screen;
    path[1] = browse_screen_for(browse);
    return path[1] != NULL ? START_PATH_CAPACITY : 1;
}

int app_screens_start_path(AppScreens *screens, StartScreen start, Screen **path)
{
    switch (start) {
    case START_MAIN_MENU:
        return 0;
    case START_NOW_PLAYING:
        path[0] = &screens->now_playing.screen;
        return 1;
    case START_ALBUMS:
        return music_path(screens, MUSIC_ALBUMS_ENTRY, path);
    case START_ARTISTS:
        return music_path(screens, MUSIC_ARTISTS_ENTRY, path);
    case START_FAVORITES:
        return music_path(screens, MUSIC_FAVORITES_ENTRY, path);
    case START_PLAYLISTS:
        return music_path(screens, MUSIC_PLAYLISTS_ENTRY, path);
    case START_COVER_FLOW:
        return music_path(screens, MUSIC_SONGS_ENTRY, path);
    }
    return 0;
}
