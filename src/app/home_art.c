#include "home_art.h"

#include "core/string_builder.h"
#include "gfx/image.h"
#include "hal/storage.h"
#include "home_emblem.h"

enum {
    COVER_MARGIN_TOP = 8,
    REFLECTION_LIGHT = 80
};

void home_art_init(HomeArt *art, CoverCache *covers, const LibraryStore *store, Player *player,
                   const Theme *theme)
{
    *art = (HomeArt){.covers = covers, .store = store, .player = player, .theme = theme};
}

static uint32_t album_count(const HomeArt *art)
{
    return art->store->available ? library_music_count(&art->store->library, LIBRARY_ALBUMS) : 0;
}

static bool playing_album(const HomeArt *art, uint16_t *album)
{
    PlayerStatus status = player_status(art->player);
    LibraryTrack track;
    if (!status.has_track || !library_read_track(&art->store->library, status.track, &track)) {
        return false;
    }
    *album = track.album;
    return true;
}

static bool cover_missing(const HomeArt *art, uint16_t album)
{
    Image ignored;
    cover_cache_load_full(art->covers, album);
    return cover_cache_full(art->covers, album, &ignored) == COVER_MISSING;
}

static void show_next_with_a_cover(HomeArt *art, uint32_t count)
{
    for (uint32_t tried = 0; tried < count; tried++) {
        art->album = (uint16_t)((art->album + 1) % count);
        if (!cover_missing(art, art->album)) {
            return;
        }
    }
}

static void cycle(HomeArt *art, uint32_t count, uint32_t now_ms)
{
    if (!art->cycling) {
        art->cycling = true;
        art->album = (uint16_t)(count - 1);
        show_next_with_a_cover(art, count);
        art->next_change_ms = now_ms + HOME_ART_CYCLE_MS;
        return;
    }
    if ((int32_t)(now_ms - art->next_change_ms) >= 0) {
        show_next_with_a_cover(art, count);
        art->next_change_ms = now_ms + HOME_ART_CYCLE_MS;
    }
}

static bool showing_emblem(const HomeArt *art)
{
    return art->settings != NULL && art->settings->home_art == HOME_ART_EMBLEM;
}

static ThemeChoice style(const HomeArt *art)
{
    return art->settings != NULL ? art->settings->theme : THEME_PODIUM;
}

static const char *const PAGE_THEMES[THEME_CHOICE_COUNT] = {"graysand", "pokemon", "zelda", "lain",
                                                            "noah"};

void home_art_page_path(ThemeChoice theme, int count, bool quran, const char *label, char *path,
                        int capacity)
{
    StringBuilder builder = string_builder_make(path, (size_t)capacity);
    string_builder_append(&builder, "/.echo/home/");
    string_builder_append(&builder, PAGE_THEMES[theme]);
    string_builder_append(&builder, "/");
    string_builder_append_unsigned(&builder, (uint32_t)count);
    string_builder_append(&builder, quran ? "/" : "-noquran/");
    for (const char *letter = label; *letter != '\0'; letter++) {
        char lower[2] = {*letter >= 'A' && *letter <= 'Z' ? (char)(*letter - 'A' + 'a') : *letter,
                         '\0'};
        string_builder_append(&builder, lower);
    }
    string_builder_append(&builder, ".bin");
}

static uint32_t page_bytes(const HomeArt *art)
{
    const ThemeMetrics *metrics = &art->theme->metrics;
    return (uint32_t)metrics->screen_width * (uint32_t)metrics->screen_height * sizeof(Color);
}

static void choose_page(HomeArt *art)
{
    char wanted[HOME_PAGE_PATH_CAPACITY];
    const MenuScreen *menu = art->menu;
    int count = menu != NULL ? menu->model.count(menu) : 0;
    if (count == 0) {
        art->has_page = false;
        return;
    }
    const char *label = menu->model.label(menu, menu->list.selected);
    bool quran = art->quran_shown == NULL || *art->quran_shown;
    home_art_page_path(style(art), count, quran, label, wanted, sizeof wanted);
    if (string_same(wanted, art->page_path)) {
        return;
    }
    StringBuilder copy = string_builder_make(art->page_path, sizeof art->page_path);
    string_builder_append(&copy, wanted);
    art->page_version++;
    HalFile file;
    art->has_page = hal_file_open(&file, wanted);
    if (art->has_page) {
        art->has_page = file.size == page_bytes(art);
        hal_file_close(&file);
    }
}

uint32_t home_art_picture(const void *context)
{
    const HomeArt *art = context;
    return art->has_page ? art->page_version : 0;
}

static bool read_rows(HalFile file, Canvas *canvas, Rect area, Rect rows, int count)
{
    Color *first = &canvas->pixels[(rows.y - canvas->bounds.y) * canvas->bounds.width +
                                   (rows.x - canvas->bounds.x)];
    uint32_t offset =
        ((uint32_t)(rows.y - area.y) * (uint32_t)area.width + (uint32_t)(rows.x - area.x)) *
        sizeof(Color);
    return hal_file_read(file, offset, first, (uint32_t)(rows.width * count) * sizeof(Color));
}

const char *home_art_theme_folder(ThemeChoice theme)
{
    return PAGE_THEMES[theme];
}

void home_art_draw_file(const char *path, Canvas *canvas, Rect area)
{
    Rect rows = rect_intersection(canvas->clip, area);
    HalFile file;
    if (rect_is_empty(rows) || !hal_file_open(&file, path)) {
        return;
    }
    if (rows.width == area.width && rows.width == canvas->bounds.width) {
        read_rows(file, canvas, area, rows, rows.height);
    } else {
        for (Rect row = {rows.x, rows.y, rows.width, 1}; row.y < rows.y + rows.height; row.y++) {
            if (!read_rows(file, canvas, area, row, 1)) {
                break;
            }
        }
    }
    hal_file_close(&file);
}

void home_art_update(void *context, uint32_t now_ms)
{
    HomeArt *art = context;
    choose_page(art);
    uint32_t count = showing_emblem(art) || art->has_page ? 0 : album_count(art);
    art->has_album = count > 0;
    if (!art->has_album) {
        return;
    }
    uint16_t album;
    if (playing_album(art, &album)) {
        art->cycling = false;
        art->album = album;
    } else {
        cycle(art, count, now_ms);
    }
    cover_cache_load_full(art->covers, art->album);
}

void home_art_draw(void *context, Canvas *canvas, Rect area)
{
    const HomeArt *art = context;
    if (art->has_page) {
        home_art_draw_file(art->page_path, canvas, area);
        return;
    }
    canvas_fill_rect(canvas, area, home_emblem_panel_color(style(art)));
    canvas_fill_rect(canvas, (Rect){area.x, area.y, 1, area.height}, art->theme->colors.divider);
    if (!art->has_album) {
        home_emblem_draw(canvas, area, style(art));
        return;
    }
    Image cover;
    if (cover_cache_full(art->covers, art->album, &cover) != COVER_LOADED) {
        return;
    }
    int left = area.x + (area.width - COVER_FULL_SIZE) / 2;
    int top = area.y + COVER_MARGIN_TOP;
    Quad quad = {left, COVER_FULL_SIZE, top, COVER_FULL_SIZE, top, COVER_FULL_SIZE};
    image_draw_quad(canvas, cover, quad);
    int rows = area.y + area.height - (top + COVER_FULL_SIZE);
    image_draw_reflection(canvas, cover, quad, (QuadLight){REFLECTION_LIGHT, REFLECTION_LIGHT},
                          rows);
}
