#include "check.h"
#include "gfx/font_pack.h"
#include "gfx/font_sized.h"

static void each_size_has_inter_line_metrics(void)
{
    const Font *twelve = font_sized(12, false);
    CHECK(twelve->ascent == 12 && twelve->descent == 3);
    const Font *sixteen = font_sized(16, true);
    CHECK(sixteen->ascent == 16 && sixteen->descent == 4);
    CHECK(font_line_height(font_sized(7, false)) == 9);
}

static void faces_follow_the_pack_order(void)
{
    CHECK(font_sized(7, false)->pack_face == FONT_SIZED_FIRST_FACE + 1);
    CHECK(font_sized(7, true)->pack_face == FONT_SIZED_FIRST_FACE + 2);
    CHECK(font_sized(18, true)->pack_face == FONT_SIZED_FIRST_FACE + FONT_SIZED_COUNT);
    CHECK(FONT_SIZED_FIRST_FACE + FONT_SIZED_COUNT <= FONT_PACK_FACES);
}

static void sizes_outside_the_range_clamp(void)
{
    CHECK(font_sized(3, false) == font_sized(FONT_SIZED_SMALLEST, false));
    CHECK(font_sized(40, true) == font_sized(FONT_SIZED_LARGEST, true));
}

static void a_code_carries_size_and_weight(void)
{
    CHECK(font_sized_code(FONT_SIZED_BOLD | 12) == font_sized(12, true));
    CHECK(font_sized_code(9) == font_sized(9, false));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(each_size_has_inter_line_metrics),
        TEST_CASE(faces_follow_the_pack_order),
        TEST_CASE(sizes_outside_the_range_clamp),
        TEST_CASE(a_code_carries_size_and_weight),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
