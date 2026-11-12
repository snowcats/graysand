#include <stddef.h>

#include "browse_screen.h"
#include "core/string_builder.h"
#include "gfx/font_sized.h"
#include "ui/header.h"
#include "ui/label.h"
#include "ui/list_navigation.h"
#include "ui/navigation.h"
#include "ui/screen.h"
#include "ui/theme_style.h"

enum {
    GRID_COVER = 72,
    GRID_GAP = 16,
    GRID_TOP = 8,
    GRID_PITCH = 94,
    GRID_NAME_MIDDLE = 10,
    GRID_NAME_SIZE = 11,
    GRID_SCROLL_ROOM = 8,
    GRID_RING = 3,
    LINE_BOX = 20,
    PAGE_COVER = 146,
    PAGE_LEFT = 14,
    PAGE_TOP = 11,
    PAGE_GAP = 16,
    PAGE_RIGHT = 14,
    PAGE_LIST_LEFT = 14,
    PAGE_TITLE_MIDDLE = 19,
    PAGE_TITLE_SIZE = 18,
    PAGE_LINE_MIDDLE = 40,
    PAGE_ARTIST_SIZE = 13,
    PAGE_LINE_SIZE = 12,
    PAGE_DIVIDER = 53,
    PAGE_LIST_TOP = 56,
    PAGE_ROW = 26,
    DOT_GAP = 13,
    DOT_PITCH = 12,
    DOT_COUNT = 5,
    COUNTER_RIGHT = 88,
    COUNTER_SIZE = 12
};

static const char SEPARATOR[] = "  \xC2\xB7  ";

static int grid_left(const Theme *theme)
{
    int width = ALBUM_GRID_COLUMNS * GRID_COVER + (ALBUM_GRID_COLUMNS - 1) * GRID_GAP;
    return (theme->metrics.screen_width - GRID_SCROLL_ROOM - width) / 2;
}

static Rect line_box(int left, int middle, int width)
{
    return (Rect){left, middle - LINE_BOX / 2, width, LINE_BOX};
}

void album_grid_keep_rows(BrowseScreen *browse)
{
    ListView *list = &browse->list;
    int row = list->selected / ALBUM_GRID_COLUMNS;
    int top = list->top / ALBUM_GRID_COLUMNS;
    int last = ((int)browse->count - 1) / ALBUM_GRID_COLUMNS;
    if (row < top) {
        top = row;
    } else if (row >= top + ALBUM_GRID_ROWS) {
        top = row - ALBUM_GRID_ROWS + 1;
    }
    if (top > last - ALBUM_GRID_ROWS + 1) {
        top = last - ALBUM_GRID_ROWS + 1;
    }
    list->top = (top > 0 ? top : 0) * ALBUM_GRID_COLUMNS;
}

void album_grid_draw(const BrowseScreen *browse, Canvas *canvas, Rect content)
{
    const Theme *theme = browse->pool->config.theme;
    const ListView *list = &browse->list;
    canvas_fill_rect(canvas, content, theme->colors.background);
    Canvas inside = canvas_clipped(canvas, content);
    int left = grid_left(theme);
    for (int slot = 0; slot < ALBUM_GRID_COLUMNS * ALBUM_GRID_ROWS; slot++) {
        int index = list->top + slot;
        if (index >= (int)browse->count) {
            break;
        }
        Rect cell = {left + slot % ALBUM_GRID_COLUMNS * (GRID_COVER + GRID_GAP),
                     content.y + GRID_TOP + slot / ALBUM_GRID_COLUMNS * GRID_PITCH, GRID_COVER,
                     GRID_COVER};
        bool selected = index == list->selected;
        if (selected) {
            theme_style_selection(&inside, theme,
                                  (Rect){cell.x - GRID_RING, cell.y - GRID_RING,
                                         cell.width + 2 * GRID_RING, cell.height + 2 * GRID_RING});
        }
        uint16_t album = 0;
        if (browse->pool->config.covers != NULL && browse_album_at(browse, index, &album)) {
            cover_cache_draw(browse->pool->config.covers, album, &inside, cell,
                             theme->colors.divider);
        } else {
            canvas_fill_rect(&inside, cell, theme->colors.divider);
        }
        TextStyle style = {.font = font_sized(GRID_NAME_SIZE, selected),
                           .color = selected ? theme->colors.text : theme->colors.text_secondary};
        const char *name = browse_row_label(browse, index);
        Rect line = line_box(cell.x - GRID_RING - 1, cell.y + cell.height + GRID_NAME_MIDDLE,
                             cell.width + 2 * GRID_RING + 2);
        label_draw(&inside, style,
                   (Label){.area = label_centered_area(style.font, line, name), .text = name});
    }
    if (list->top + ALBUM_GRID_COLUMNS * ALBUM_GRID_ROWS < (int)browse->count) {
        list_view_fade_more(&inside, theme, content, content.x + content.width - 14);
    }
}

void album_counter_draw(const BrowseScreen *browse, Canvas *canvas)
{
    const Theme *theme = browse->pool->config.theme;
    char text[24];
    StringBuilder builder = string_builder_make(text, sizeof text);
    string_builder_append_unsigned(&builder, (uint32_t)browse->list.selected + 1u);
    string_builder_append(&builder, " / ");
    string_builder_append_unsigned(&builder, browse->count);
    TextStyle style = {.font = font_sized(COUNTER_SIZE, false),
                       .color = theme->colors.header_muted};
    Rect header = theme_header_area(theme);
    int width = text_width(style.font, text);
    int right = header.x + header.width - COUNTER_RIGHT;
    label_draw(canvas, style,
               (Label){.area = line_box(right - width, header.y + (header.height - 1) / 2, width),
                       .text = text});
}

static int page_count(const void *context)
{
    const AlbumPage *page = context;
    return (int)page->track_count + 1;
}

static const char *page_label(const void *context, int index)
{
    const AlbumPage *page = context;
    if (index == 0) {
        return "Play all";
    }
    int row = index - page->labels_top;
    return row >= 0 && row < ALBUM_PAGE_ROWS ? page->labels[row] : "";
}

static int page_lead(const void *context, int index)
{
    (void)context;
    return index == 0 ? LIST_LEAD_PLAY : index;
}

static void load_labels(BrowseScreen *browse)
{
    AlbumPage *page = &browse->pool->album_page;
    const Library *library = browse->pool->config.library;
    int top = page->list.top;
    if (page->labels_top == top) {
        return;
    }
    page->labels_top = top;
    for (int row = 0; row < ALBUM_PAGE_ROWS; row++) {
        int index = top + row;
        page->labels[row][0] = '\0';
        BrowseItem item;
        char detail[BROWSE_DETAIL_CAPACITY];
        BrowseText text = {page->labels[row], BROWSE_LABEL_CAPACITY, detail, sizeof detail};
        if (index > 0 && browse_item(library, page->tracks, (uint32_t)index - 1u, &item)) {
            browse_item_text(library, page->tracks, item, text);
        }
    }
}

static void fill_line(BrowseScreen *browse, LibraryAlbum album)
{
    AlbumPage *page = &browse->pool->album_page;
    const Library *library = browse->pool->config.library;
    StringBuilder builder = string_builder_make(page->line, sizeof page->line);
    page->artist_length = 0;
    LibraryGroup artist;
    char name[64];
    if (album.album_artist != LIBRARY_NONE &&
        library_read_group(library, LIBRARY_ALBUM_ARTISTS, album.album_artist, &artist) &&
        library_read_string(library, artist.name, name, sizeof name)) {
        string_builder_append(&builder, name);
        page->artist_length = (uint8_t)builder.length;
        string_builder_append(&builder, SEPARATOR);
    }
    if (album.year > 0) {
        string_builder_append_unsigned(&builder, album.year);
        string_builder_append(&builder, SEPARATOR);
    }
    string_builder_append_unsigned(&builder, page->track_count);
    string_builder_append(&builder, page->track_count == 1 ? " song" : " songs");
}

void album_page_open(BrowseScreen *browse, uint32_t now_ms)
{
    AlbumPage *page = &browse->pool->album_page;
    const Library *library = browse->pool->config.library;
    const Theme *theme = browse->pool->config.theme;
    uint16_t album_index = 0;
    LibraryAlbum album;
    BrowseItem item = {.kind = BROWSE_ITEM_ALBUM};
    if (!browse_album_at(browse, browse->list.selected, &album_index) ||
        !library_read_album(library, album_index, &album)) {
        return;
    }
    item.index = album_index;
    if (!browse_child(browse->query, item, &page->tracks)) {
        return;
    }
    page->album = album_index;
    page->track_count = browse_count(library, page->tracks);
    page->title[0] = '\0';
    library_read_string(library, album.title, page->title, sizeof page->title);
    fill_line(browse, album);
    page->model = (ListModel){.context = page,
                              .count = page_count,
                              .label = page_label,
                              .lead = page_lead,
                              .arrow_at_edge = true};
    list_view_init(&page->list, &page->model, theme, now_ms);
    Rect content = theme_content_area(theme);
    page->list.row_height = PAGE_ROW;
    page->list.visible_rows = (content.height - PAGE_LIST_TOP + 2) / PAGE_ROW;
    page->labels_top = -1;
    load_labels(browse);
    if (browse->pool->config.covers != NULL) {
        cover_cache_load_full(browse->pool->config.covers, album_index);
    }
    browse->page_open = true;
}

static ScreenAction select_track(void *context, int index)
{
    BrowseScreen *browse = context;
    AlbumPage *page = &browse->pool->album_page;
    return browse_start_list(browse, page->tracks, index > 0 ? (uint32_t)index - 1u : 0u);
}

ScreenAction album_page_handle_key(BrowseScreen *browse, KeyEvent event, uint32_t now_ms)
{
    AlbumPage *page = &browse->pool->album_page;
    if (navigation_from_key(event).intent == NAVIGATION_BACK) {
        browse->page_open = false;
        return screen_action_none();
    }
    ListSelection selection = {.select = select_track, .hold = NULL, .context = browse};
    ScreenAction action = list_navigation_handle(&page->list, event, now_ms, selection);
    load_labels(browse);
    return action;
}

static Rect page_list_area(Rect content)
{
    int left = PAGE_LEFT + PAGE_COVER + PAGE_GAP - PAGE_LIST_LEFT;
    return (Rect){left, content.y + PAGE_LIST_TOP, content.x + content.width - left,
                  content.height - PAGE_LIST_TOP};
}

static void draw_dots(const BrowseScreen *browse, Canvas *canvas, Rect cover)
{
    const Theme *theme = browse->pool->config.theme;
    int count = (int)browse->count;
    int index = browse->list.selected;
    int shown = count < DOT_COUNT ? count : DOT_COUNT;
    int lit = index < DOT_COUNT / 2               ? index
              : index > count - 1 - DOT_COUNT / 2 ? shown - (count - index)
                                                  : DOT_COUNT / 2;
    int width = (shown - 1) * DOT_PITCH;
    int left = cover.x + (cover.width - width) / 2;
    int y = cover.y + cover.height + DOT_GAP;
    for (int dot = 0; dot < shown; dot++) {
        Color color = dot == lit ? theme_style_mark_color(theme) : theme->colors.text_secondary;
        header_fill_disc(canvas, (Point){left + dot * DOT_PITCH, y}, dot == lit ? 2 : 1, color);
    }
}

static void draw_cover(const BrowseScreen *browse, Canvas *canvas, Rect cover)
{
    const AlbumPage *page = &browse->pool->album_page;
    const CoverCache *covers = browse->pool->config.covers;
    Image image;
    if (covers != NULL && cover_cache_full(covers, page->album, &image) == COVER_LOADED) {
        image_draw_quad(canvas, image,
                        (Quad){cover.x, cover.width, cover.y, cover.height, cover.y, cover.height});
        return;
    }
    canvas_fill_rect(canvas, cover, browse->pool->config.theme->colors.divider);
}

static void draw_line(const BrowseScreen *browse, Canvas *canvas, Rect area)
{
    const Theme *theme = browse->pool->config.theme;
    const AlbumPage *page = &browse->pool->album_page;
    Canvas clipped = canvas_clipped(canvas, area);
    TextStyle artist = {.font = font_sized(PAGE_ARTIST_SIZE, false),
                        .color = theme->colors.header_text};
    TextStyle rest = {.font = font_sized(PAGE_LINE_SIZE, false),
                      .color = theme->colors.text_secondary};
    int pen = text_draw_span(&clipped, artist, (Point){area.x, label_baseline(artist.font, area)},
                             (TextSpan){page->line, page->artist_length});
    label_draw(canvas, rest,
               (Label){.area = {pen, area.y, area.x + area.width - pen, area.height},
                       .text = page->line + page->artist_length});
}

void album_page_draw(const BrowseScreen *browse, Canvas *canvas, Rect content)
{
    const Theme *theme = browse->pool->config.theme;
    const AlbumPage *page = &browse->pool->album_page;
    canvas_fill_rect(canvas, content, theme->colors.background);
    Rect cover = {PAGE_LEFT, content.y + PAGE_TOP, PAGE_COVER, PAGE_COVER};
    draw_cover(browse, canvas, cover);
    draw_dots(browse, canvas, cover);
    int left = PAGE_LEFT + PAGE_COVER + PAGE_GAP;
    int width = content.x + content.width - left - PAGE_RIGHT;
    TextStyle title = {.font = font_sized(PAGE_TITLE_SIZE, true), .color = theme->colors.text};
    label_draw(
        canvas, title,
        (Label){.area = line_box(left, content.y + PAGE_TITLE_MIDDLE, width), .text = page->title});
    draw_line(browse, canvas, line_box(left, content.y + PAGE_LINE_MIDDLE, width));
    canvas_fill_rect(canvas, (Rect){left, content.y + PAGE_DIVIDER, width, 1},
                     theme->colors.divider);
    list_view_draw(&page->list, canvas, page_list_area(content), browse->now_ms);
}

uint32_t album_page_picture(const BrowseScreen *browse)
{
    const AlbumPage *page = &browse->pool->album_page;
    uint32_t hash = screen_picture_text(screen_picture_mix(0x5A17u, page->album), page->title);
    return list_view_picture(&page->list,
                             page_list_area(theme_content_area(browse->pool->config.theme)),
                             browse->now_ms, page->title) ^
           hash;
}
