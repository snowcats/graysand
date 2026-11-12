#include <string.h>

#include "check.h"
#include "fake_hal.h"
#include "lyrics.h"

static bool same(const char *left, const char *right)
{
    return strcmp(left, right) == 0;
}

static void the_line_follows_the_latest_stamp(void)
{
    const char *text = "[00:01.00]one\n[00:02.50][00:10.00]two\n[00:05.00]three\n";
    char line[32];
    CHECK(!lyrics_line_at(text, 999, line, sizeof line));
    CHECK(lyrics_line_at(text, 1000, line, sizeof line));
    CHECK(same(line, "one"));
    CHECK(lyrics_line_at(text, 5000, line, sizeof line));
    CHECK(same(line, "three"));
    CHECK(lyrics_line_at(text, 10000, line, sizeof line));
    CHECK(same(line, "two"));
}

static void an_empty_stamp_clears_the_line(void)
{
    char line[32] = "kept";
    CHECK(!lyrics_line_at("[00:01.00]one\n[00:02.00]\n", 2000, line, sizeof line));
    CHECK(line[0] == '\0');
}

static void tags_and_a_bom_are_ignored(void)
{
    const char *text = "\xEF\xBB\xBF[ar:Someone]\r\n[01:00.005] hello \n";
    char line[32];
    CHECK(!lyrics_line_at(text, 59999, line, sizeof line));
    CHECK(lyrics_line_at(text, 60005, line, sizeof line));
    CHECK(same(line, "hello"));
}

static void the_sidecar_uses_the_song_name(void)
{
    const char lyric[] = "[00:03.20]Heart to heart\n";
    char line[32] = "kept";
    fake_hal_reset();
    fake_hal_add_file("/music/Heart To Heart.lrc", (const uint8_t *)lyric, sizeof lyric - 1);
    lyrics_load("/music/Heart To Heart.flac");
    CHECK(!lyrics_line(3199, line, sizeof line));
    CHECK(lyrics_line(3200, line, sizeof line));
    CHECK(same(line, "Heart to heart"));
    lyrics_load("/music/missing.mp3");
    CHECK(!lyrics_line(3200, line, sizeof line));
    CHECK(line[0] == '\0');
}

static void the_window_holds_the_lines_around_the_current_one(void)
{
    const char lyric[] = "[00:01.00]one\n[00:02.00]two\n[00:03.00]three\n[00:04.00]four\n";
    char lines[3][LYRICS_LINE_CAPACITY];
    uint32_t from = 0;
    uint32_t until = 0;
    fake_hal_reset();
    fake_hal_add_file("/music/song.lrc", (const uint8_t *)lyric, sizeof lyric - 1);
    lyrics_load("/music/song.mp3");
    CHECK(lyrics_window(2500, lines, 3, &from, &until));
    CHECK(same(lines[0], "one"));
    CHECK(same(lines[1], "two"));
    CHECK(same(lines[2], "three"));
    CHECK_EQUAL(2000, from);
    CHECK_EQUAL(3000, until);
    CHECK(lyrics_window(500, lines, 3, &from, &until));
    CHECK(lines[0][0] == '\0');
    CHECK(lines[1][0] == '\0');
    CHECK(same(lines[2], "one"));
    CHECK(lyrics_window(4500, lines, 3, &from, &until));
    CHECK(same(lines[1], "four"));
    CHECK(lines[2][0] == '\0');
    lyrics_load("/music/missing.mp3");
    CHECK(!lyrics_window(2500, lines, 3, &from, &until));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_line_follows_the_latest_stamp),
        TEST_CASE(the_window_holds_the_lines_around_the_current_one),
        TEST_CASE(an_empty_stamp_clears_the_line),
        TEST_CASE(tags_and_a_bom_are_ignored),
        TEST_CASE(the_sidecar_uses_the_song_name),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
