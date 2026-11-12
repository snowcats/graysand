#include "ui/list_view.h"

#include <stddef.h>
#include <stdint.h>

#include "core/string_builder.h"
#include "gfx/font_sized.h"
#include "gfx/text.h"
#include "ui/label.h"
#include "ui/screen.h"
#include "ui/theme_style.h"

enum {
    FADE_ALPHA = 235,
    END_RULE_INSET = 40,
    END_RULE_GAP = 12,
    ARROW_HALF = 6,
    TWO_LINE_EXTRA = 7,
    THUMB_EXTRA = 16,
    SOFT_DETAIL = 166,
    ROW_SLACK = 4,
    LINE_BOX = 20,
    TITLE_SIZE = 15,
    SUBTITLE_SIZE = 12,
    LEAD_SIZE = 13,
    COMPACT_SIZE = 14,
    COMPACT_LEAD_SIZE = 12,
    END_SIZE = 11
};

static const char END_TEXT[] = "the end";
static const char SECTION_LABELS[][2] = {"#", "A", "B", "C", "D", "E", "F", "G", "H", "I",
                                         "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S",
                                         "T", "U", "V", "W", "X", "Y", "Z", "#"};

typedef struct Row {
    int index;
    Rect area;
    Rect text;
    bool selected;
    int icon;
} Row;

enum {
    ICON_CENTER = 26,
    TEXT_WITH_ICON = 44,
    TEXT_PLAIN = 20,
    LEAD_CENTER = 28,
    TEXT_WITH_LEAD = 46,
    COMPACT_LEAD_CENTER = 21,
    COMPACT_TEXT = 36,
    THUMB_LEFT = 14,
    THUMB_INSET = 6,
    THUMB_GAP = 12,
    DETAIL_RIGHT = 30,
    PLAY_HALF = 5
};

static const uint8_t *icon_masks;

void list_view_set_icons(const uint8_t *masks)
{
    icon_masks = masks;
}

static int icon_of(const ListView *view, int index)
{
    const ListModel *model = view->model;
    if (icon_masks == NULL || !model->icons) {
        return 0;
    }
    const char *label = model->label(model->context, index);
    const char *name = (const char *)icon_masks + LIST_ICON_BYTES;
    while (*name != '\0') {
        int length = 0;
        while (name[length] != '\0') {
            length++;
        }
        if (string_same(name, label)) {
            return (uint8_t)name[length + 1];
        }
        name += length + 2;
    }
    return 0;
}

static void draw_icon(const ListView *view, Canvas *canvas, Row row)
{
    if (row.icon == 0) {
        return;
    }
    const uint8_t *mask = icon_masks + (row.icon - 1) * LIST_ICON_SIZE * LIST_ICON_SIZE;
    Color color =
        row.selected ? view->theme->colors.selection_text : view->theme->colors.header_text;
    int left = row.area.x + ICON_CENTER - LIST_ICON_SIZE / 2;
    int top = row.area.y + (row.area.height - LIST_ICON_SIZE) / 2;
    Rect shown = rect_intersection(canvas->clip, (Rect){left, top, LIST_ICON_SIZE, LIST_ICON_SIZE});
    for (int y = shown.y; y < shown.y + shown.height; y++) {
        const uint8_t *alphas = &mask[(y - top) * LIST_ICON_SIZE + (shown.x - left)];
        Color *pixel = &canvas->pixels[(y - canvas->bounds.y) * canvas->bounds.width +
                                       (shown.x - canvas->bounds.x)];
        for (int x = 0; x < shown.width; x++) {
            if (alphas[x] != 0) {
                pixel[x] = color_blend(pixel[x], color, alphas[x]);
            }
        }
    }
}

static int item_count(const ListView *view)
{
    return view->model->count(view->model->context);
}

static int max_of(int first, int second)
{
    return first > second ? first : second;
}

static void keep_selection_visible(ListView *view)
{
    int count = item_count(view);
    int above = view->selected > 0 ? 1 : 0;
    int below = view->selected < count - 1 ? 1 : 0;
    if (view->selected - above < view->top) {
        view->top = view->selected - above;
    } else if (view->selected + below >= view->top + view->visible_rows) {
        view->top = view->selected + below - view->visible_rows + 1;
    }
    if (view->top > count - view->visible_rows) {
        view->top = count - view->visible_rows;
    }
    if (view->top < 0) {
        view->top = 0;
    }
}

static int wrapped_target(int target, int count, bool wrap)
{
    if (target < 0) {
        return wrap ? count - 1 : 0;
    }
    if (target >= count) {
        return wrap ? 0 : count - 1;
    }
    return target;
}

void list_view_init(ListView *view, const ListModel *model, const Theme *theme, uint32_t now_ms)
{
    int row_height = theme->metrics.row_height + (model->thumb != NULL ? THUMB_EXTRA
                                                  : model->two_line    ? TWO_LINE_EXTRA
                                                                       : 0);
    *view = (ListView){
        .model = model,
        .theme = theme,
        .row_height = row_height,
        .visible_rows = (theme_content_area(theme).height - LIST_ROWS_TOP + ROW_SLACK) / row_height,
        .marked = true,
        .selection_changed_ms = now_ms,
    };
}

static int resting_y(const ListView *view)
{
    return (view->selected - view->top) * view->row_height;
}

void list_view_move(ListView *view, int delta, bool wrap, uint32_t now_ms)
{
    int count = item_count(view);
    if (count == 0) {
        return;
    }
    int target = wrapped_target(view->selected + delta, count, wrap);
    if (target == view->selected) {
        return;
    }
    view->selected = target;
    view->selection_changed_ms = now_ms;
    keep_selection_visible(view);
}

void list_view_start_scroll(ListView *view, int direction, uint32_t now_ms)
{
    view->scroll_started_ms = now_ms;
    list_view_move(view, direction, true, now_ms);
}

static int section_at(const ListView *view, int index)
{
    return view->model->section(view->model->context, index);
}

static int first_of_section_at_least(const ListView *view, int section, int low, int high)
{
    while (low < high) {
        int middle = low + (high - low) / 2;
        if (section_at(view, middle) >= section) {
            high = middle;
        } else {
            low = middle + 1;
        }
    }
    return low;
}

static int section_jump_target(const ListView *view, int direction)
{
    int count = item_count(view);
    int current = view->selected;
    int section = section_at(view, current);
    if (direction > 0) {
        return first_of_section_at_least(view, section + 1, current + 1, count);
    }
    int start = first_of_section_at_least(view, section, 0, current);
    if (start < current || current == 0) {
        return start;
    }
    return first_of_section_at_least(view, section_at(view, current - 1), 0, current - 1);
}

void list_view_keep_scrolling(ListView *view, int direction, uint32_t now_ms)
{
    uint32_t held_ms = now_ms - view->scroll_started_ms;
    if (held_ms < LIST_FAST_SCROLL_AFTER_MS) {
        list_view_move(view, direction, false, now_ms);
        return;
    }
    bool sectioned = view->model->section != NULL;
    if (sectioned) {
        view->section_shown_until_ms = now_ms + LIST_SECTION_SHOWN_MS;
    }
    if (held_ms < LIST_SECTION_JUMP_AFTER_MS) {
        list_view_move(view, direction * LIST_FAST_SCROLL_ROWS, false, now_ms);
    } else if (sectioned) {
        list_view_select(view, section_jump_target(view, direction), now_ms);
    } else {
        list_view_move(view, direction * LIST_FASTEST_SCROLL_ROWS, false, now_ms);
    }
}

int list_section_of_key(const char *key)
{
    unsigned char first = (unsigned char)key[0];
    if (first < 'a') {
        return LIST_SECTION_BEFORE_LETTERS;
    }
    if (first > 'z') {
        return LIST_SECTION_AFTER_LETTERS;
    }
    return LIST_SECTION_FIRST_LETTER + (first - 'a');
}

const char *list_section_label(int section)
{
    if (section < LIST_SECTION_BEFORE_LETTERS || section > LIST_SECTION_AFTER_LETTERS) {
        return SECTION_LABELS[LIST_SECTION_BEFORE_LETTERS];
    }
    return SECTION_LABELS[section];
}

void list_view_select(ListView *view, int index, uint32_t now_ms)
{
    int count = item_count(view);
    if (count == 0) {
        return;
    }
    int target = index < 0 ? 0 : index >= count ? count - 1 : index;
    view->selected = target;
    view->selection_changed_ms = now_ms;
    keep_selection_visible(view);
}

void list_view_restart_marquee(ListView *view, uint32_t now_ms)
{
    view->selection_changed_ms = now_ms;
}

static bool compact(const ListView *view)
{
    return view->row_height < view->theme->metrics.row_height;
}

static bool plays(const ListView *view, int index)
{
    return view->model->lead != NULL &&
           view->model->lead(view->model->context, index) == LIST_LEAD_PLAY;
}

static const Font *label_font(const ListView *view, bool bold)
{
    if (view->model->two_line) {
        return font_sized(TITLE_SIZE, bold);
    }
    if (compact(view)) {
        return font_sized(COMPACT_SIZE, bold);
    }
    return bold ? view->theme->fonts.heading : view->theme->fonts.row;
}

static Color soft_color(const ListView *view, bool selected)
{
    const ThemeColors *colors = &view->theme->colors;
    return selected ? color_blend(colors->selection_background, colors->selection_text, SOFT_DETAIL)
                    : colors->text_secondary;
}

static Rect line_box(Rect text, int middle)
{
    return (Rect){text.x, middle - LINE_BOX / 2, text.width, LINE_BOX};
}

static Row draw_detail(const ListView *view, Canvas *canvas, Row row)
{
    const ListModel *model = view->model;
    const char *detail = model->detail != NULL ? model->detail(model->context, row.index) : NULL;
    if (detail == NULL) {
        return row;
    }
    Color color = row.selected           ? view->theme->colors.selection_text
                  : model->accent_detail ? theme_style_mark_color(view->theme)
                                         : view->theme->colors.text_secondary;
    TextStyle style = {.font = view->theme->fonts.row, .color = color};
    int width = text_width(style.font, detail);
    if (model->detail_max_width > 0 && width > model->detail_max_width) {
        width = model->detail_max_width;
    }
    Rect area = {row.text.x + row.text.width - width, row.text.y, width, row.text.height};
    label_draw(canvas, style, (Label){.area = area, .text = detail});
    row.text.width -= width + view->theme->metrics.detail_gap;
    return row;
}

static void draw_label(const ListView *view, Canvas *canvas, Row row, uint32_t now_ms)
{
    const char *text = view->model->label(view->model->context, row.index);
    TextStyle style = {.font = label_font(view, row.selected || plays(view, row.index)),
                       .color = row.selected ? view->theme->colors.selection_text
                                             : view->theme->colors.text};
    Label label = {.area = row.text, .text = text};
    int width = text_width(style.font, text);
    if (!row.selected || width <= row.text.width) {
        label_draw(canvas, style, label);
        return;
    }
    MarqueeTiming timing = view->theme->marquee;
    int offset = marquee_offset(timing, width, row.text.width, now_ms - view->selection_changed_ms);
    label_draw_scrolling(canvas, style, label, (Marquee){.offset = offset, .gap = timing.gap_px});
}

static void draw_two_lines(const ListView *view, Canvas *canvas, Row row, uint32_t now_ms)
{
    const char *detail =
        view->model->detail != NULL ? view->model->detail(view->model->context, row.index) : NULL;
    Row top = row;
    top.text = line_box(row.text, row.area.y + (row.area.height - TITLE_SIZE) / 2);
    draw_label(view, canvas, top, now_ms);
    if (detail == NULL) {
        return;
    }
    TextStyle style = {.font = font_sized(SUBTITLE_SIZE, false),
                       .color = soft_color(view, row.selected)};
    Rect area = line_box(row.text, row.area.y + (row.area.height + 18) / 2);
    label_draw(canvas, style, (Label){.area = area, .text = detail});
}

static void draw_lead(const ListView *view, Canvas *canvas, Row row)
{
    int lead = view->model->lead(view->model->context, row.index);
    int center = row.area.x + (compact(view) ? COMPACT_LEAD_CENTER : LEAD_CENTER);
    int middle = row.area.y + row.area.height / 2;
    if (lead == LIST_LEAD_PLAY) {
        Color color =
            row.selected ? view->theme->colors.selection_text : theme_style_mark_color(view->theme);
        for (int step = 0; step < 2 * PLAY_HALF; step++) {
            int half = PLAY_HALF - 1 - step / 2;
            canvas_fill_rect(canvas, (Rect){center - 4 + step, middle - half, 1, 2 * half + 1},
                             color);
        }
        return;
    }
    if (lead <= 0) {
        return;
    }
    char number[8];
    StringBuilder builder = string_builder_make(number, sizeof number);
    string_builder_append_unsigned(&builder, (uint32_t)lead);
    TextStyle style = {.font = font_sized(compact(view) ? COMPACT_LEAD_SIZE : LEAD_SIZE, false),
                       .color = soft_color(view, row.selected)};
    Rect area = {center - LEAD_CENTER, row.area.y, 2 * LEAD_CENTER, row.area.height};
    label_draw(canvas, style,
               (Label){.area = label_centered_area(style.font, area, number), .text = number});
}

static void draw_thumb(const ListView *view, Canvas *canvas, Row row)
{
    int side = row.area.height - 2 * THUMB_INSET;
    Rect cell = {row.area.x + THUMB_LEFT, row.area.y + THUMB_INSET, side, side};
    if (!rect_is_empty(rect_intersection(cell, canvas->clip))) {
        view->model->thumb(view->model->context, row.index, canvas, cell);
    }
}

static void draw_row(const ListView *view, Canvas *canvas, Row row, uint32_t now_ms)
{
    draw_icon(view, canvas, row);
    if (view->model->lead != NULL) {
        draw_lead(view, canvas, row);
    }
    if (view->model->thumb != NULL) {
        draw_thumb(view, canvas, row);
    }
    if (view->model->two_line) {
        draw_two_lines(view, canvas, row, now_ms);
        return;
    }
    draw_label(view, canvas, draw_detail(view, canvas, row), now_ms);
}

static int end_height(const ListView *view)
{
    return view->theme->metrics.row_height * 13 / 16;
}

void list_view_fade_more(Canvas *canvas, const Theme *theme, Rect area, int arrow_x)
{
    int height = theme->metrics.row_height - 2;
    Color background = theme->colors.background;
    int bottom = area.y + area.height;
    for (int line = 0; line < height; line++) {
        uint8_t alpha =
            (uint8_t)(FADE_ALPHA * (line * line + line * height) / (2 * height * height));
        for (int x = area.x; x < area.x + area.width; x++) {
            canvas_blend_pixel(canvas, (Point){x, bottom - height + line}, background, alpha);
        }
    }
    Color mark = theme_style_mark_color(theme);
    int top = bottom - height * 13 / 30;
    for (int step = 0; step <= ARROW_HALF; step++) {
        canvas_fill_rect(canvas, (Rect){arrow_x - ARROW_HALF + step, top + step, 2, 2}, mark);
        canvas_fill_rect(canvas, (Rect){arrow_x + ARROW_HALF - step - 1, top + step, 2, 2}, mark);
    }
}

static void draw_fade(const ListView *view, Canvas *canvas, Rect area)
{
    int arrow_x = view->model->arrow_at_edge ? area.x + area.width - 14 : area.x + area.width / 2;
    list_view_fade_more(canvas, view->theme, area, arrow_x);
}

static void draw_end(const ListView *view, Canvas *canvas, Rect area)
{
    const ThemeColors *colors = &view->theme->colors;
    TextStyle style = {.font = font_sized(END_SIZE, false), .color = colors->text_secondary};
    Color rule = color_blend(colors->background, colors->text_secondary, 100);
    int middle = area.y + area.height - end_height(view) * 6 / 13;
    int half = text_width(style.font, END_TEXT) / 2 + END_RULE_GAP;
    int center = area.x + area.width / 2;
    int left = area.x + END_RULE_INSET;
    canvas_fill_rect(canvas, (Rect){left, middle, center - half - left, 1}, rule);
    canvas_fill_rect(canvas, (Rect){center + half, middle, center - half - left, 1}, rule);
    Rect band = line_box((Rect){area.x, 0, area.width, 0}, middle);
    label_draw(canvas, style,
               (Label){.area = label_centered_area(style.font, band, END_TEXT), .text = END_TEXT});
}

static Row make_row(const ListView *view, Rect rows, int position)
{
    const ListModel *model = view->model;
    int index = view->top + position;
    Rect area = {rows.x, rows.y + position * view->row_height, rows.width, view->row_height};
    int icon = index < item_count(view) ? icon_of(view, index) : 0;
    int left = model->thumb != NULL  ? THUMB_LEFT + area.height - 2 * THUMB_INSET + THUMB_GAP
               : model->lead != NULL ? (compact(view) ? COMPACT_TEXT : TEXT_WITH_LEAD)
               : model->icons && icon_masks != NULL ? TEXT_WITH_ICON
                                                    : TEXT_PLAIN;
    Rect text = {area.x + left, area.y, area.width - left - DETAIL_RIGHT, area.height};
    return (Row){.index = index,
                 .area = area,
                 .text = text,
                 .selected = view->marked && index == view->selected,
                 .icon = icon};
}

static void draw_section(const ListView *view, Canvas *canvas, Rect area, uint32_t now_ms)
{
    if (view->model->section == NULL || item_count(view) == 0 ||
        (int32_t)(now_ms - view->section_shown_until_ms) >= 0) {
        return;
    }
    const Theme *theme = view->theme;
    int side = area.height * 2 / 3 < area.width ? area.height * 2 / 3 : area.width;
    Rect box = {area.x + (area.width - side) / 2, area.y + (area.height - side) / 2, side, side};
    Rect inner = {box.x + 1, box.y + 1, side - 2, side - 2};
    canvas_fill_rect(canvas, box, theme->colors.divider);
    canvas_fill_rect(canvas, inner, theme->colors.header_background);
    TextStyle style = {.font = theme->fonts.title, .color = theme->colors.header_text};
    int scale = max_of(1, side / (font_line_height(style.font) + 4));
    const char *label = list_section_label(section_at(view, view->selected));
    text_draw_glyph_scaled(canvas, style, inner, (uint32_t)(unsigned char)label[0], scale);
}

static bool more_below(const ListView *view)
{
    return view->top + view->visible_rows < item_count(view);
}

static bool at_end(const ListView *view)
{
    return !more_below(view) && item_count(view) > view->visible_rows;
}

static Rect rows_area(const ListView *view, Rect area)
{
    if (at_end(view)) {
        area.y += area.height - end_height(view) - view->visible_rows * view->row_height;
    } else {
        area.y += view->model->thumb != NULL ? LIST_ROWS_TOP - 1 : LIST_ROWS_TOP;
    }
    return area;
}

void list_view_draw(const ListView *view, Canvas *canvas, Rect area, uint32_t now_ms)
{
    canvas_fill_rect(canvas, area, view->theme->colors.background);
    Canvas inside = canvas_clipped(canvas, area);
    Rect rows = rows_area(view, area);
    if (view->marked && item_count(view) > 0) {
        theme_style_selection(&inside, view->theme,
                              (Rect){rows.x + LIST_PILL_LEFT, rows.y + resting_y(view) + 1,
                                     rows.width - LIST_PILL_LEFT - LIST_PILL_RIGHT,
                                     view->row_height - 2});
    }
    for (int position = 0; position < view->visible_rows; position++) {
        Row row = make_row(view, rows, position);
        if (row.index >= item_count(view)) {
            break;
        }
        if (!rect_is_empty(rect_intersection(row.area, inside.clip))) {
            draw_row(view, &inside, row, now_ms);
        }
    }
    if (more_below(view)) {
        draw_fade(view, &inside, area);
    } else if (at_end(view)) {
        draw_end(view, &inside, area);
    }
    draw_section(view, canvas, area, now_ms);
}

static int label_room(const ListView *view, Row row)
{
    const ListModel *model = view->model;
    const Theme *theme = view->theme;
    int room = row.text.width;
    const char *detail =
        model->detail != NULL && !model->two_line ? model->detail(model->context, row.index) : NULL;
    if (detail != NULL) {
        int width = text_width(theme->fonts.row, detail);
        if (model->detail_max_width > 0 && width > model->detail_max_width) {
            width = model->detail_max_width;
        }
        room -= width + theme->metrics.detail_gap;
    }
    return room;
}

static uint32_t mix_marquee(uint32_t hash, const ListView *view, Row row, uint32_t now_ms)
{
    const char *text = view->model->label(view->model->context, row.index);
    int width = text_width(label_font(view, true), text);
    int room = label_room(view, row);
    if (width <= room) {
        return hash;
    }
    MarqueeTiming timing = view->theme->marquee;
    return screen_picture_mix(
        hash, (uint32_t)marquee_offset(timing, width, room, now_ms - view->selection_changed_ms));
}

static uint32_t picture_at_rest(const ListView *view, Rect area, uint32_t now_ms, const char *title)
{
    const ListModel *model = view->model;
    int count = item_count(view);
    uint32_t hash = screen_picture_text(
        screen_picture_mix((uint32_t)(uintptr_t)view->theme, (uint32_t)count), title);
    hash =
        screen_picture_mix(screen_picture_mix(hash, (uint32_t)view->selected), (uint32_t)view->top);
    Rect rows = rows_area(view, area);
    for (int position = 0; position < view->visible_rows; position++) {
        Row row = make_row(view, rows, position);
        if (row.index >= count) {
            break;
        }
        hash = screen_picture_text(hash, model->label(model->context, row.index));
        hash = screen_picture_text(
            hash, model->detail != NULL ? model->detail(model->context, row.index) : NULL);
        hash = screen_picture_mix(hash,
                                  model->opens != NULL && model->opens(model->context, row.index));
        if (row.selected) {
            hash = mix_marquee(hash, view, row, now_ms);
        }
    }
    bool section =
        model->section != NULL && count > 0 && (int32_t)(now_ms - view->section_shown_until_ms) < 0;
    hash = screen_picture_mix(hash, section ? (uint32_t)section_at(view, view->selected) + 1u : 0u);
    return hash != 0 ? hash : 1;
}

uint32_t list_view_picture(const ListView *view, Rect area, uint32_t now_ms, const char *title)
{
    return picture_at_rest(view, area, now_ms, title);
}
