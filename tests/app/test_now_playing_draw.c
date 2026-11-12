#include "check.h"
#include "default_theme.h"
#include "fake_hal.h"
#include "fixture_library.h"
#include "hal/display.h"
#include "player.h"
#include "screens/now_playing_screen.h"

enum {
    BACK_IN_BLACK = 0,
    SEED = 7,
    FOOT_LINE = 198,
    TRACK_ROW = 210,
    TRACK_LEFT = 44,
    COVER_LEFT = 10,
    COVER_TOP = 39
};

static FixtureLibrary fixture;
static Player player;
static NowPlayingScreen now_playing;
static Settings settings;
static Color pixels[HAL_DISPLAY_WIDTH * HAL_DISPLAY_HEIGHT];

static const Color FOOT_LINES[THEME_CHOICE_COUNT] = {
    COLOR_RGB(88, 88, 88), COLOR_RGB(24, 20, 24), COLOR_RGB(226, 188, 100),
    COLOR_RGB(240, 140, 184), COLOR_RGB(30, 66, 112)};

static const Color EMPTY_TRACKS[THEME_CHOICE_COUNT] = {COLOR_RGB(52, 54, 62), COLOR_RGB(70, 66, 72),
                                                       COLOR_RGB(71, 68, 40), COLOR_RGB(32, 48, 42),
                                                       COLOR_RGB(24, 48, 78)};

static Color pixel(int x, int y)
{
    return pixels[y * HAL_DISPLAY_WIDTH + x];
}

static void draw_playing(ThemeChoice theme)
{
    fake_hal_reset();
    player_init(&player, fixture_library_open(&fixture));
    CHECK(player_play_list(&player,
                           (BrowseQuery){.kind = BROWSE_ALBUM_TRACKS,
                                         .group = LIBRARY_ARTISTS,
                                         .subject = BACK_IN_BLACK,
                                         .artist = LIBRARY_NONE},
                           0, SEED));
    settings = settings_defaults();
    settings.theme = theme;
    settings.player_style = PLAYER_STYLE_COVER;
    settings.format_tag = true;
    Screen *screen =
        now_playing_screen_init(&now_playing, (NowPlayingConfig){.player = &player,
                                                                 .library = &fixture.library,
                                                                 .theme = default_theme_for(theme),
                                                                 .settings = &settings});
    screen->enter(screen->context, 100);
    Canvas canvas = canvas_make(pixels, (Rect){0, 0, HAL_DISPLAY_WIDTH, HAL_DISPLAY_HEIGHT});
    canvas_clear(&canvas, 0);
    screen->draw(screen->context, &canvas);
}

static void the_footer_has_the_theme_line_and_an_empty_track(void)
{
    for (int theme = 0; theme < THEME_CHOICE_COUNT; theme++) {
        draw_playing((ThemeChoice)theme);
        CHECK_EQUAL(FOOT_LINES[theme], pixel(1, FOOT_LINE));
        CHECK_EQUAL(EMPTY_TRACKS[theme], pixel(TRACK_LEFT + 200, TRACK_ROW));
    }
}

static void rounded_themes_cut_the_cover_corners(void)
{
    draw_playing(THEME_PODIUM);
    Color background = default_theme_for(THEME_PODIUM)->colors.background;
    CHECK_EQUAL(background, pixel(COVER_LEFT, COVER_TOP));
    CHECK(pixel(COVER_LEFT + 4, COVER_TOP + 4) != background);
    draw_playing(THEME_ZELDA);
    CHECK(pixel(COVER_LEFT, COVER_TOP) != default_theme_for(THEME_ZELDA)->colors.background);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_footer_has_the_theme_line_and_an_empty_track),
        TEST_CASE(rounded_themes_cut_the_cover_corners),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
