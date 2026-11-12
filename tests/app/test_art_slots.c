#include <string.h>

#include "art_slots.h"
#include "check.h"
#include "fake_hal.h"
#include "info_screen.h"
#include "tiny_font.h"

enum {
    PIXEL_BYTES = 8,
    WIDE = 120,
    TALL = 2,
    RED = 0xF800,
    TEXT_CAPACITY = 48
};

static const char PATH[] = "/.echo/full/test.bin";
static const Theme THEME;
static Color pixels[WIDE * TALL];

static uint32_t art_with(const ArtSlot *slots, int count, uint8_t *file)
{
    memset(file, 0, PIXEL_BYTES);
    memcpy(file + PIXEL_BYTES, slots, (size_t)count * sizeof(ArtSlot));
    return PIXEL_BYTES + (uint32_t)count * sizeof(ArtSlot);
}

static Canvas blank_canvas(void)
{
    memset(pixels, 0, sizeof pixels);
    return canvas_make(pixels, (Rect){0, 0, WIDE, TALL});
}

static void the_slot_table_after_the_pixels_loads(void)
{
    fake_hal_reset();
    ArtSlot written[2] = {{.kind = ART_PROGRESS, .style = ART_BAR, .width = 100, .height = 1},
                          {.kind = ART_FILE, .style = ART_LEFT, .width = 40}};
    uint8_t file[PIXEL_BYTES + sizeof written];
    fake_hal_add_file(PATH, file, art_with(written, 2, file));
    ArtSlot slots[ART_SLOT_CAPACITY];
    CHECK(art_slots_load(PATH, PIXEL_BYTES, slots, ART_SLOT_CAPACITY) == 2);
    CHECK(slots[0].kind == ART_PROGRESS && slots[1].width == 40);
    CHECK(art_slots_load(PATH, PIXEL_BYTES, slots, 1) == 1);
    CHECK(art_slots_load(PATH, PIXEL_BYTES + sizeof written, slots, ART_SLOT_CAPACITY) == 0);
    CHECK(art_slots_load("/.echo/full/none.bin", PIXEL_BYTES, slots, ART_SLOT_CAPACITY) == 0);
}

static void a_bar_fills_in_proportion_to_its_level(void)
{
    ArtSlot bar = {.kind = ART_PROGRESS, .style = ART_BAR, .width = 100, .height = 1, .color = RED};
    ArtSlotValues values = {.permille = {[ART_PROGRESS - ART_TEXT_COUNT] = 500}};
    Canvas canvas = blank_canvas();
    art_slots_draw(&canvas, &THEME, &bar, 1, &values);
    CHECK(pixels[0] == RED && pixels[49] == RED);
    CHECK(pixels[50] == 0 && pixels[WIDE] == 0);
}

static void a_segmented_bar_lights_whole_segments(void)
{
    ArtSlot bar = {
        .kind = ART_PROGRESS, .style = ART_BAR, .width = 40, .height = 1, .font = 4, .color = RED};
    ArtSlotValues values = {.permille = {[ART_PROGRESS - ART_TEXT_COUNT] = 500}};
    Canvas canvas = blank_canvas();
    art_slots_draw(&canvas, &THEME, &bar, 1, &values);
    CHECK(pixels[0] == RED && pixels[7] == RED && pixels[8] == 0);
    CHECK(pixels[10] == RED && pixels[17] == RED && pixels[20] == 0);
}

static void slots_without_a_value_or_with_a_bad_kind_draw_nothing(void)
{
    ArtSlot slots[2] = {{.kind = ART_FILE, .style = ART_LEFT, .width = 40, .color = RED},
                        {.kind = ART_KIND_COUNT, .style = ART_BAR, .width = 40, .color = RED}};
    ArtSlotValues values = {0};
    Canvas canvas = blank_canvas();
    art_slots_draw(&canvas, &THEME, slots, 2, &values);
    for (int index = 0; index < WIDE * TALL; index++) {
        CHECK(pixels[index] == 0);
    }
}

static void a_slot_after_another_starts_where_that_text_ends(void)
{
    static const Theme tiny = {.fonts = {.small = &TINY_FONT}};
    ArtSlot slots[2] = {{.kind = ART_FILE, .style = ART_LEFT, .y = 1, .width = 40, .color = RED},
                        {.kind = ART_CLOCK, .x = 2, .y = 1, .width = 40, .after = 1, .color = RED}};
    ArtSlotValues values = {.texts = {[ART_FILE] = "B", [ART_CLOCK] = "B"}};
    Canvas canvas = blank_canvas();
    art_slots_draw(&canvas, &tiny, slots, 2, &values);
    CHECK(pixels[WIDE + 1] == RED && pixels[WIDE + 2] == 0 && pixels[WIDE + 4] == 0);
    CHECK(pixels[WIDE + 6] == RED && pixels[WIDE + 7] == 0);
}

static void the_version_line_names_the_build(void)
{
    char text[TEXT_CAPACITY];
    const char *build = info_screen_version_text("0.0.1-90bb1ac", text, sizeof text);
    CHECK(strcmp(text, "Graysand 0.0.1  \xC2\xB7  build ") == 0);
    CHECK(build != NULL && strcmp(build, "90bb1ac") == 0);
    CHECK(info_screen_version_text("simulator", text, sizeof text) == NULL);
    CHECK(strcmp(text, "Graysand simulator") == 0);
}

static void storage_reads_as_used_of_total(void)
{
    char used[TEXT_CAPACITY];
    char total[TEXT_CAPACITY];
    CHECK_EQUAL(612, info_screen_space_texts((HalSpace){10035, 16384}, used, total, TEXT_CAPACITY));
    CHECK(strcmp(used, "9.8 GB") == 0 && strcmp(total, "of 16 GB") == 0);
    CHECK_EQUAL(828,
                info_screen_space_texts((HalSpace){217088, 262144}, used, total, TEXT_CAPACITY));
    CHECK(strcmp(used, "212 GB") == 0 && strcmp(total, "of 256 GB") == 0);
    CHECK_EQUAL(0, info_screen_space_texts((HalSpace){0, 0}, used, total, TEXT_CAPACITY));
    CHECK(used[0] == '\0');
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_slot_table_after_the_pixels_loads),
        TEST_CASE(a_bar_fills_in_proportion_to_its_level),
        TEST_CASE(a_segmented_bar_lights_whole_segments),
        TEST_CASE(slots_without_a_value_or_with_a_bad_kind_draw_nothing),
        TEST_CASE(a_slot_after_another_starts_where_that_text_ends),
        TEST_CASE(the_version_line_names_the_build),
        TEST_CASE(storage_reads_as_used_of_total),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
