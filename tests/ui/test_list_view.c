#include "check.h"
#include "ui/list_view.h"
#include "ui_test_canvas.h"
#include "ui_test_theme.h"

typedef struct FakeItems {
    int count;
    const char *detail_at_one;
    bool opens_at_one;
    const char *first_label;
} FakeItems;

static int fake_count(const void *context)
{
    const FakeItems *items = context;
    return items->count;
}

static const char *fake_label(const void *context, int index)
{
    const FakeItems *items = context;
    if (index == 0 && items->first_label != NULL) {
        return items->first_label;
    }
    return index % 2 == 0 ? "A" : "B";
}

static const char *fake_detail(const void *context, int index)
{
    const FakeItems *items = context;
    return index == 1 ? items->detail_at_one : NULL;
}

static bool fake_opens(const void *context, int index)
{
    const FakeItems *items = context;
    return index == 1 && items->opens_at_one;
}

static FakeItems items;
static ListModel model;

static ListView list_of(int count)
{
    items = (FakeItems){.count = count, .detail_at_one = NULL};
    model = (ListModel){.context = &items,
                        .count = fake_count,
                        .label = fake_label,
                        .detail = fake_detail,
                        .opens = fake_opens};
    ListView view;
    list_view_init(&view, &model, &TEST_THEME, 0);
    return view;
}

static void move_times(ListView *view, int delta, int times)
{
    for (int step = 0; step < times; step++) {
        list_view_move(view, delta, false, 100);
    }
}

static Rect row_rect(int row)
{
    Rect content = theme_content_area(&TEST_THEME);
    return (Rect){LIST_PILL_LEFT, content.y + LIST_ROWS_TOP + row * TEST_ROW_HEIGHT + 1, 1,
                  TEST_ROW_HEIGHT - 2};
}

enum {
    WIDE_WIDTH = 160
};

static Color wide_pixels[WIDE_WIDTH * TEST_SCREEN_HEIGHT];
static Theme wide_theme;

static Canvas wide_list(ListView *view)
{
    wide_theme = TEST_THEME;
    wide_theme.metrics.screen_width = WIDE_WIDTH;
    view->theme = &wide_theme;
    Canvas canvas = canvas_make(wide_pixels, (Rect){0, 0, WIDE_WIDTH, TEST_SCREEN_HEIGHT});
    canvas_clear(&canvas, 0x0821);
    return canvas;
}

static Rect wide_row(int row, int left, int width)
{
    Rect content = theme_content_area(&TEST_THEME);
    return (Rect){left, content.y + LIST_ROWS_TOP + row * TEST_ROW_HEIGHT, width, TEST_ROW_HEIGHT};
}

static void a_new_list_selects_the_first_item(void)
{
    ListView view = list_of(10);
    CHECK_EQUAL(0, view.selected);
    CHECK_EQUAL(0, view.top);
    CHECK_EQUAL(TEST_VISIBLE_ROWS, view.visible_rows);
}

static void moving_within_the_page_keeps_the_top(void)
{
    ListView view = list_of(10);
    move_times(&view, 1, 1);
    CHECK_EQUAL(1, view.selected);
    CHECK_EQUAL(0, view.top);
}

static void moving_past_the_last_visible_row_scrolls_down(void)
{
    ListView view = list_of(10);
    move_times(&view, 1, 3);
    CHECK_EQUAL(3, view.selected);
    CHECK_EQUAL(2, view.top);
}

static void moving_above_the_first_visible_row_scrolls_up(void)
{
    ListView view = list_of(10);
    move_times(&view, 1, 5);
    move_times(&view, -1, 3);
    CHECK_EQUAL(2, view.selected);
    CHECK_EQUAL(1, view.top);
}

static void wrapping_forward_from_the_last_item_returns_to_the_top(void)
{
    ListView view = list_of(10);
    move_times(&view, 1, 9);
    list_view_move(&view, 1, true, 100);
    CHECK_EQUAL(0, view.selected);
    CHECK_EQUAL(0, view.top);
}

static void moving_past_the_end_without_wrap_stays_on_the_last_item(void)
{
    ListView view = list_of(10);
    move_times(&view, 1, 12);
    CHECK_EQUAL(9, view.selected);
    CHECK_EQUAL(7, view.top);
}

static void wrapping_back_from_the_first_item_shows_the_last_page(void)
{
    ListView view = list_of(10);
    list_view_move(&view, -1, true, 100);
    CHECK_EQUAL(9, view.selected);
    CHECK_EQUAL(7, view.top);
}

static void a_short_list_never_scrolls(void)
{
    ListView view = list_of(2);
    list_view_move(&view, -1, true, 100);
    CHECK_EQUAL(1, view.selected);
    CHECK_EQUAL(0, view.top);
}

static void an_empty_list_ignores_moves(void)
{
    ListView view = list_of(0);
    list_view_move(&view, 1, true, 100);
    list_view_move(&view, -1, true, 100);
    CHECK_EQUAL(0, view.selected);
    CHECK_EQUAL(0, view.top);
}

static void the_marquee_restarts_only_when_the_selection_changes(void)
{
    ListView view = list_of(3);
    list_view_move(&view, 1, false, 500);
    CHECK_EQUAL(500, view.selection_changed_ms);
    move_times(&view, 1, 5);
    list_view_move(&view, 1, false, 900);
    CHECK_EQUAL(100, view.selection_changed_ms);
    list_view_restart_marquee(&view, 1200);
    CHECK_EQUAL(1200, view.selection_changed_ms);
}

static void selecting_an_index_scrolls_it_into_view(void)
{
    ListView view = list_of(10);
    list_view_select(&view, 8, 300);
    CHECK_EQUAL(8, view.selected);
    CHECK_EQUAL(7, view.top);
    CHECK_EQUAL(300, view.selection_changed_ms);
    list_view_select(&view, 1, 400);
    CHECK_EQUAL(0, view.top);
}

static void selecting_past_the_end_picks_the_last_item(void)
{
    ListView view = list_of(4);
    list_view_select(&view, 9, 300);
    CHECK_EQUAL(3, view.selected);
    list_view_select(&view, -2, 300);
    CHECK_EQUAL(0, view.selected);
}

static void the_selected_row_is_highlighted(void)
{
    ListView view = list_of(10);
    move_times(&view, 1, 1);
    Canvas canvas = ui_test_canvas();
    list_view_draw(&view, &canvas, theme_content_area(&TEST_THEME), 100);
    CHECK(ui_test_area_has_only(&canvas, row_rect(1), TEST_THEME.colors.selection_background));
    CHECK(ui_test_area_has_only(&canvas, row_rect(0), TEST_THEME.colors.background));
    CHECK(ui_test_area_has_only(&canvas, row_rect(2), TEST_THEME.colors.background));
}

static void the_selected_row_has_a_shine_line_and_shades_down(void)
{
    Theme theme = TEST_THEME;
    theme.colors.selection_top = 0xFFFF;
    theme.colors.selection_shine = 0x07E0;
    ListView view = list_of(10);
    view.theme = &theme;
    move_times(&view, 1, 1);
    Canvas canvas = ui_test_canvas();
    list_view_draw(&view, &canvas, theme_content_area(&theme), 100);
    Rect row = row_rect(1);
    CHECK_EQUAL(0x07E0, canvas_pixel_at(&canvas, row.x, row.y));
    CHECK_EQUAL(theme.colors.selection_background,
                canvas_pixel_at(&canvas, row.x, row.y + row.height - 1));
    Color below_shine = canvas_pixel_at(&canvas, row.x, row.y + 1);
    CHECK(below_shine != 0x07E0 && below_shine != theme.colors.selection_background);
}

static Rect bottom_band(void)
{
    Rect content = theme_content_area(&TEST_THEME);
    return (Rect){TEST_SCREEN_WIDTH / 4, content.y + content.height - 3, TEST_SCREEN_WIDTH / 2, 3};
}

static void a_row_that_opens_something_shows_no_chevron(void)
{
    ListView view = list_of(3);
    items.opens_at_one = true;
    Canvas canvas = ui_test_canvas();
    list_view_draw(&view, &canvas, theme_content_area(&TEST_THEME), 100);
    Rect right_of_row_one = {TEST_SCREEN_WIDTH - 8, row_rect(1).y, 8, TEST_ROW_HEIGHT};
    CHECK(ui_test_area_has_only(&canvas, right_of_row_one, TEST_THEME.colors.background));
}

static void a_long_list_fades_out_with_an_arrow_while_more_is_below(void)
{
    ListView long_list = list_of(10);
    Canvas canvas = ui_test_canvas();
    list_view_draw(&long_list, &canvas, theme_content_area(&TEST_THEME), 100);
    CHECK(ui_test_area_has_color(&canvas, bottom_band(), TEST_THEME.colors.selection_background));
    ListView short_list = list_of(2);
    canvas = ui_test_canvas();
    list_view_draw(&short_list, &canvas, theme_content_area(&TEST_THEME), 100);
    CHECK(ui_test_area_has_only(&canvas, bottom_band(), TEST_THEME.colors.background));
}

static void at_the_bottom_the_rows_lift_above_the_end_mark(void)
{
    ListView view = list_of(10);
    list_view_select(&view, 9, 100);
    Rect content = theme_content_area(&TEST_THEME);
    Canvas canvas = ui_test_canvas();
    list_view_draw(&view, &canvas, content, 100);
    int lift = TEST_ROW_HEIGHT * 13 / 16;
    CHECK_EQUAL(TEST_THEME.colors.selection_background,
                canvas_pixel_at(&canvas, LIST_PILL_LEFT, content.y + content.height - lift - 2));
    CHECK_EQUAL(TEST_THEME.colors.background,
                canvas_pixel_at(&canvas, 0, content.y + content.height - 1));
    CHECK(!ui_test_area_has_color(&canvas, bottom_band(), TEST_THEME.colors.selection_background));
}

static void a_detail_is_drawn_in_the_secondary_color_at_the_right(void)
{
    ListView view = list_of(2);
    items.detail_at_one = "B";
    Canvas canvas = wide_list(&view);
    list_view_draw(&view, &canvas, theme_content_area(&wide_theme), 100);
    CHECK(ui_test_area_has_color(&canvas, wide_row(1, WIDE_WIDTH / 2, WIDE_WIDTH / 2),
                                 TEST_THEME.colors.text_secondary));
}

static void a_long_detail_stops_at_its_maximum_width(void)
{
    ListView view = list_of(2);
    items.detail_at_one = "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB";
    model.detail_max_width = WIDE_WIDTH / 4;
    Canvas canvas = wide_list(&view);
    list_view_draw(&view, &canvas, theme_content_area(&wide_theme), 100);
    CHECK(!ui_test_area_has_color(&canvas, wide_row(1, 0, WIDE_WIDTH / 2),
                                  TEST_THEME.colors.text_secondary));
    CHECK(ui_test_area_has_color(&canvas, wide_row(1, WIDE_WIDTH / 2, WIDE_WIDTH / 2),
                                 TEST_THEME.colors.text_secondary));
}

static void an_accent_detail_takes_the_mark_color_until_selected(void)
{
    ListView view = list_of(2);
    items.detail_at_one = "B";
    model.accent_detail = true;
    Canvas canvas = wide_list(&view);
    Rect right = wide_row(1, WIDE_WIDTH / 2, WIDE_WIDTH / 2);
    list_view_draw(&view, &canvas, theme_content_area(&wide_theme), 100);
    CHECK(ui_test_area_has_color(&canvas, right, TEST_THEME.colors.selection_background));
    CHECK(!ui_test_area_has_color(&canvas, right, TEST_THEME.colors.text_secondary));
    list_view_move(&view, 1, false, 100);
    list_view_draw(&view, &canvas, theme_content_area(&wide_theme), 100);
    CHECK(ui_test_area_has_color(&canvas, right, TEST_THEME.colors.selection_text));
}

static void text_starts_at_the_plain_inset_and_the_pill_stops_short_of_the_edge(void)
{
    ListView view = list_of(2);
    Canvas canvas = wide_list(&view);
    list_view_draw(&view, &canvas, theme_content_area(&wide_theme), 100);
    Rect row = wide_row(0, 0, WIDE_WIDTH);
    CHECK(ui_test_area_has_only(&canvas, (Rect){0, row.y, LIST_PILL_LEFT, row.height},
                                TEST_THEME.colors.background));
    CHECK(!ui_test_area_has_color(&canvas, (Rect){LIST_PILL_LEFT, row.y, 12, row.height},
                                  TEST_THEME.colors.selection_text));
    CHECK(ui_test_area_has_color(&canvas, (Rect){20, row.y, 12, row.height},
                                 TEST_THEME.colors.selection_text));
    CHECK(ui_test_area_has_only(
        &canvas, (Rect){WIDE_WIDTH - LIST_PILL_RIGHT, row.y, LIST_PILL_RIGHT, row.height},
        TEST_THEME.colors.background));
}

static void fake_thumb(const void *context, int index, Canvas *canvas, Rect cell)
{
    (void)context;
    (void)index;
    canvas_fill_rect(canvas, cell, TEST_THEME.colors.accent);
}

static void two_line_and_cover_rows_are_taller(void)
{
    ListView view = list_of(3);
    CHECK_EQUAL(TEST_ROW_HEIGHT, view.row_height);
    CHECK_EQUAL(TEST_VISIBLE_ROWS, view.visible_rows);
    model.two_line = true;
    list_view_init(&view, &model, &TEST_THEME, 0);
    CHECK_EQUAL(TEST_ROW_HEIGHT + 7, view.row_height);
    model.thumb = fake_thumb;
    list_view_init(&view, &model, &TEST_THEME, 0);
    CHECK_EQUAL(TEST_ROW_HEIGHT + 16, view.row_height);
    view.visible_rows = 1;
    view.marked = false;
    Canvas canvas = wide_list(&view);
    list_view_draw(&view, &canvas, theme_content_area(&wide_theme), 100);
    Rect content = theme_content_area(&TEST_THEME);
    CHECK_EQUAL(TEST_THEME.colors.accent, canvas_pixel_at(&canvas, 14, content.y + 1 + 6));
    CHECK(canvas_pixel_at(&canvas, 13, content.y + 1 + 6) != TEST_THEME.colors.accent);
}

static uint32_t picture_at(const ListView *view, uint32_t now_ms, const char *title)
{
    return list_view_picture(view, theme_content_area(&TEST_THEME), now_ms, title);
}

static void a_still_list_keeps_its_picture(void)
{
    ListView view = list_of(3);
    uint32_t first = picture_at(&view, 100, "T");
    CHECK(first != 0);
    CHECK_EQUAL(first, picture_at(&view, 9000, "T"));
}

static void moving_or_retitling_changes_the_picture(void)
{
    ListView view = list_of(3);
    uint32_t first = picture_at(&view, 100, "T");
    CHECK(first != picture_at(&view, 100, "U"));
    list_view_move(&view, 1, false, 100);
    CHECK(first != picture_at(&view, 100, "T"));
}

static void a_new_detail_changes_the_picture(void)
{
    ListView view = list_of(3);
    uint32_t first = picture_at(&view, 100, "T");
    items.detail_at_one = "9";
    CHECK(first != picture_at(&view, 100, "T"));
}

static void a_scrolling_title_changes_the_picture_over_time(void)
{
    ListView view = list_of(3);
    items.first_label = "ABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABAB";
    uint32_t first = picture_at(&view, 0, "T");
    CHECK(first != picture_at(&view, 3000, "T"));
}

static uint8_t icon_file[LIST_ICON_FILE_CAPACITY];

static void a_row_icon_is_drawn_only_inside_the_clip(void)
{
    static const char names[] = "A\0\x01"
                                "B\0\x01";
    for (int index = 0; index < LIST_ICON_SIZE * LIST_ICON_SIZE; index++) {
        icon_file[index] = 255;
    }
    for (int index = 0; index < (int)sizeof names; index++) {
        icon_file[LIST_ICON_BYTES + index] = (uint8_t)names[index];
    }
    list_view_set_icons(icon_file);
    ListView view = list_of(4);
    model.icons = true;
    Canvas canvas = wide_list(&view);
    Rect content = theme_content_area(&wide_theme);
    Rect row = wide_row(0, content.x, 0);
    int split = content.x + 26;
    canvas_fill_rect(&canvas, canvas.bounds, 0x1234);
    Canvas left = canvas_clipped(&canvas, (Rect){0, 0, split, TEST_SCREEN_HEIGHT});
    list_view_draw(&view, &left, content, 100);
    list_view_set_icons(NULL);
    CHECK(ui_test_area_has_color(&canvas, (Rect){content.x + 18, row.y, 8, row.height},
                                 TEST_THEME.colors.header_text));
    CHECK(ui_test_area_has_only(&canvas, (Rect){split, 0, WIDE_WIDTH - split, TEST_SCREEN_HEIGHT},
                                0x1234));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_new_list_selects_the_first_item),
        TEST_CASE(a_row_icon_is_drawn_only_inside_the_clip),
        TEST_CASE(moving_within_the_page_keeps_the_top),
        TEST_CASE(moving_past_the_last_visible_row_scrolls_down),
        TEST_CASE(moving_above_the_first_visible_row_scrolls_up),
        TEST_CASE(wrapping_forward_from_the_last_item_returns_to_the_top),
        TEST_CASE(moving_past_the_end_without_wrap_stays_on_the_last_item),
        TEST_CASE(wrapping_back_from_the_first_item_shows_the_last_page),
        TEST_CASE(a_short_list_never_scrolls),
        TEST_CASE(an_empty_list_ignores_moves),
        TEST_CASE(the_marquee_restarts_only_when_the_selection_changes),
        TEST_CASE(selecting_an_index_scrolls_it_into_view),
        TEST_CASE(selecting_past_the_end_picks_the_last_item),
        TEST_CASE(the_selected_row_is_highlighted),
        TEST_CASE(a_long_list_fades_out_with_an_arrow_while_more_is_below),
        TEST_CASE(at_the_bottom_the_rows_lift_above_the_end_mark),
        TEST_CASE(the_selected_row_has_a_shine_line_and_shades_down),
        TEST_CASE(a_row_that_opens_something_shows_no_chevron),
        TEST_CASE(a_detail_is_drawn_in_the_secondary_color_at_the_right),
        TEST_CASE(a_long_detail_stops_at_its_maximum_width),
        TEST_CASE(an_accent_detail_takes_the_mark_color_until_selected),
        TEST_CASE(text_starts_at_the_plain_inset_and_the_pill_stops_short_of_the_edge),
        TEST_CASE(two_line_and_cover_rows_are_taller),
        TEST_CASE(a_still_list_keeps_its_picture),
        TEST_CASE(moving_or_retitling_changes_the_picture),
        TEST_CASE(a_new_detail_changes_the_picture),
        TEST_CASE(a_scrolling_title_changes_the_picture_over_time),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
