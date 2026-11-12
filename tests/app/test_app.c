#include <string.h>

#include "app/app.h"
#include "app_internal.h"
#include "board_layout.h"
#include "card_files.h"
#include "cassette.h"
#include "check.h"
#include "default_theme.h"
#include "fake_hal.h"
#include "fixture_library.h"
#include "frame_overlay.h"
#include "gfx/canvas.h"
#include "gfx/text.h"
#include "hal/display.h"
#include "hal/system.h"
#include "library/cover_build.h"
#include "library/covers.h"
#include "library_rescan.h"
#include "screens/now_playing_screen.h"
#include "settings/settings.h"
#include "settings_store.h"
#include "spoken_library.h"
#include "ui/header.h"
#include "ui/list_view.h"
#include "ui/status_bar.h"
#include "ui/toast.h"

enum {
    START_MS = 1000,
    KEY_HOLD_MS = 30,
    FULL_FRAME_PIXELS = HAL_DISPLAY_WIDTH * HAL_DISPLAY_HEIGHT,
    DIAGNOSTICS_FRAME_TIME_INDEX = 3,
    DIAGNOSTICS_FIRMWARE_INDEX = 4,
    MUSIC_FAVORITES_INDEX = 0,
    MUSIC_PLAYLISTS_INDEX = 1,
    MUSIC_ALBUMS_INDEX = 2,
    MUSIC_ARTISTS_INDEX = 3,
    MUSIC_SONGS_INDEX = 5,
    BJORK_POSITION = 3,
    HOME_QURAN_INDEX = 1,
    HOME_BOOKS_INDEX = 1,
    HOME_BLUETOOTH_INDEX = 2,
    HOME_SETTINGS_INDEX = 3,
    SETTINGS_SCREEN_GROUP = 0,
    SETTINGS_SHUTDOWN_GROUP = 1,
    SETTINGS_SYSTEM_GROUP = 3,
    SETTINGS_MUSIC_GROUP = 4,
    SETTINGS_RESCAN_GROUP = 5,
    SETTINGS_TIMEOUT_INDEX = 0,
    SETTINGS_BRIGHTNESS_INDEX = 1,
    SETTINGS_SLEEP_INDEX = 1,
    SETTINGS_REPEAT_INDEX = 1,
    SETTINGS_GAIN_INDEX = 2,
    SETTINGS_START_INDEX = 2,
    SETTINGS_PODCASTS_INDEX = 3,
    SETTINGS_EQUALIZER_INDEX = 4,
    SETTINGS_FILTER_INDEX = 5,
    SETTINGS_DIAGNOSTICS_INDEX = 5,
    SETTINGS_FILES_INDEX = 6,
    SETTINGS_BALANCE_INDEX = 6,
    SETTINGS_VOLUME_LIMIT_INDEX = 7,
    SETTINGS_SWAP_KEYS_INDEX = 8,
    SETTINGS_PLAYER_STYLE_INDEX = 11,
    SETTINGS_ALBUM_VIEW_INDEX = 12,
    SETTINGS_HOME_ART_INDEX = 1,
    SETTINGS_THEME_INDEX = 0,
    SETTLE_STEPS = 30,
    RED_COVER = 0xF800,
    BLUE_COVER = 0x001F,
    DEFAULT_TIMEOUT_MS = 30000,
    SHORT_TIMEOUT_MS = 10000,
    IDLE_MS = 20000,
    DIAGNOSTICS_KEY_TEST_INDEX = 0,
    DIAGNOSTICS_CPU_CLOCK_INDEX = 5,
    DIAGNOSTICS_STOPWATCH_INDEX = 6,
    DIAGNOSTICS_TEST_TONE_INDEX = 7,
    DIAGNOSTICS_DMA_TONE_INDEX = 8,
    DIAGNOSTICS_ACCEL_INDEX = 24,
    DIAGNOSTICS_HIGH_RATE_TONE_INDEX = 25,
    DIAGNOSTICS_WATCHDOG_TEST_INDEX = 26,
    DIAGNOSTICS_TONE_176_INDEX = 27,
    DIAGNOSTICS_LOOP_CYCLES_INDEX = 11,
    DIAGNOSTICS_CORE1_INDEX = 12,
    DIAGNOSTICS_STALL_TEST_INDEX = 19,
    DIAGNOSTICS_POWER_TAPS_INDEX = 20,
    FIFTEEN_MINUTES_MS = 15 * 60 * 1000,
    GOODBYE_MS = 1000,
    SPLASH_FADE_MS = 600,
    SPLASH_MS = 1100,
    HALF_BATTERY = 50
};

static const char HELLS_BELLS_PATH[] = "/AC DC/Back In Black/1-01 - Hells Bells.flac";
static const char SHOOT_TO_THRILL_PATH[] = "/AC DC/Back In Black/1-02 - Shoot to Thrill.flac";
static const char SHOOTS_PATH[] = "/acloudyskye/This Won't Be The Last Time/01 - Shoots.flac";

static uint32_t now_ms;

static void prefill_player_files(void)
{
    for (size_t index = 0; index < card_files_count(); index++) {
        HalFile file;
        if (!card_files_slow(index)) {
            continue;
        }
        if (hal_file_open(&file, card_files_path(index))) {
            hal_file_close(&file);
        } else {
            fake_hal_add_file(card_files_path(index), NULL, 4096);
        }
    }
}

static void start_app(void)
{
    fake_hal_reset();
    prefill_player_files();
    now_ms = START_MS;
    fake_hal_set_milliseconds(now_ms);
    app_start();
}

static void start_app_with_database(const uint8_t *bytes, uint32_t size)
{
    uint32_t covers_size = 0;
    const uint8_t *covers = fixture_covers_bytes(&covers_size);
    fake_hal_reset();
    fake_hal_add_file("/.echo/library.db", bytes, size);
    fake_hal_add_file("/.echo/covers.bin", covers, covers_size);
    fake_hal_add_file("/.echo/build.tmp", NULL, 16 * 1024 * 1024);
    prefill_player_files();
    now_ms = START_MS;
    fake_hal_set_milliseconds(now_ms);
    app_start();
}

static void start_app_with_library(void)
{
    uint32_t size = 0;
    const uint8_t *bytes = fixture_bytes(&size);
    start_app_with_database(bytes, size);
}

static void start_app_with_cue_library(void)
{
    static FixtureLibrary cue;
    fixture_cue_library_open(&cue);
    start_app_with_database(cue.bytes, cue.size);
}

static void step(void)
{
    fake_hal_set_milliseconds(now_ms);
    app_step();
}

static void tap(Key key)
{
    fake_hal_queue_transition(key, true, now_ms);
    fake_hal_queue_transition(key, false, now_ms + KEY_HOLD_MS);
    now_ms += KEY_HOLD_MS + 10;
    step();
}

static void hold_for(Key key, uint32_t duration_ms)
{
    fake_hal_queue_transition(key, true, now_ms);
    now_ms += duration_ms;
    step();
    fake_hal_queue_transition(key, false, now_ms);
    now_ms += 10;
    step();
}

static void hold(Key key)
{
    hold_for(key, 1000);
}

static void hold_power(void)
{
    hold_for(KEY_POWER, 2000);
}

static void hold_back(void)
{
    hold(KEY_BACK);
}

static void wait_ms(uint32_t duration_ms)
{
    now_ms += duration_ms;
    step();
}

static void restart_app(void)
{
    now_ms += 10;
    fake_hal_set_milliseconds(now_ms);
    app_start();
}

static void move_down(int times)
{
    for (int count = 0; count < times; count++) {
        tap(KEY_VOLUME_UP);
    }
}

static void move_up(int times)
{
    for (int count = 0; count < times; count++) {
        tap(KEY_VOLUME_DOWN);
    }
}

static bool title_is(const char *expected)
{
    return strcmp(expected, app_screen_title()) == 0;
}

static void open_diagnostics(void)
{
    move_down(HOME_SETTINGS_INDEX);
    tap(KEY_PLAY);
    move_down(SETTINGS_SYSTEM_GROUP);
    tap(KEY_PLAY);
    move_down(SETTINGS_DIAGNOSTICS_INDEX);
    tap(KEY_PLAY);
}

static bool overlay_visible(void)
{
    Rect area = frame_overlay_area();
    return fake_hal_pixel(area.x, area.y) == frame_overlay_background();
}

static void audio_gets_a_turn_after_every_strip(void)
{
    start_app();
    int before = fake_hal_audio_service_count();
    step();
    CHECK(fake_hal_audio_service_count() - before >= HAL_DISPLAY_HEIGHT / BOARD_STRIP_HEIGHT + 1);
}

static void bluetooth_gets_a_turn_after_every_strip(void)
{
    start_app();
    int before = fake_hal_bluetooth_service_count();
    step();
    CHECK(fake_hal_bluetooth_service_count() - before >=
          HAL_DISPLAY_HEIGHT / BOARD_STRIP_HEIGHT + 1);
}

static void a_step_writes_one_full_frame_and_presents_it(void)
{
    start_app();
    step();
    CHECK_EQUAL(FULL_FRAME_PIXELS, fake_hal_pixels_written());
    CHECK_EQUAL(1, fake_hal_present_count());
}

static void the_home_menu_is_the_root(void)
{
    start_app();
    step();
    CHECK(title_is("Home"));
    CHECK_EQUAL(1, app_screen_depth());
}

static void selecting_music_opens_the_music_menu(void)
{
    start_app();
    tap(KEY_PLAY);
    CHECK(title_is("Music"));
    CHECK_EQUAL(2, app_screen_depth());
}

static void back_returns_to_the_previous_screen(void)
{
    start_app();
    tap(KEY_PLAY);
    tap(KEY_BACK);
    CHECK(title_is("Home"));
}

static void a_long_back_returns_home_from_deep_screens(void)
{
    start_app();
    tap(KEY_PLAY);
    tap(KEY_PLAY);
    CHECK_EQUAL(3, app_screen_depth());
    hold_back();
    CHECK_EQUAL(1, app_screen_depth());
}

static void the_frame_overlay_is_off_until_enabled_in_diagnostics(void)
{
    start_app();
    step();
    CHECK(!overlay_visible());
    open_diagnostics();
    CHECK(title_is("Diagnostics"));
    move_down(DIAGNOSTICS_FRAME_TIME_INDEX);
    tap(KEY_PLAY);
    CHECK(overlay_visible());
}

static void the_frame_overlay_splits_the_frame_time(void)
{
    char text[FRAME_OVERLAY_TEXT_CAPACITY];
    FrameBreakdown breakdown = {
        .total_us = 190400, .update_us = 12000, .draw_us = 119600, .send_us = 58200};
    frame_overlay_format(breakdown, text, sizeof text);
    CHECK(strcmp("190 u12 d120 s58", text) == 0);
}

static void the_frame_overlay_splits_the_drawing(void)
{
    char text[FRAME_OVERLAY_TEXT_CAPACITY];
    uint32_t parts[DRAW_PART_COUNT] = {[DRAW_PART_COVERS] = 9400, [DRAW_PART_REFLECTIONS] = 3100,
                                       [DRAW_PART_TEXT] = 2000,   [DRAW_PART_BACKDROP] = 600,
                                       [DRAW_PART_HASH] = 1500,   [DRAW_PART_AUDIO] = 0};
    frame_overlay_format_parts(parts, text, sizeof text);
    CHECK(strcmp("c9 r3 t2 b1 h2 a0", text) == 0);
}

static void the_frame_overlay_shows_the_audio_numbers(void)
{
    char text[FRAME_OVERLAY_TEXT_CAPACITY];
    HalAudioStats stats = {.buffered_ms = 340,
                           .underruns = 2,
                           .output_underruns = 7,
                           .decode_total_us = 100000,
                           .busy_us = 640000,
                           .elapsed_us = 1000000};
    frame_overlay_format_audio(stats, text, sizeof text);
    CHECK(strcmp("b340 x2 o7 L64%", text) == 0);
}

static void the_frame_overlay_shows_average_and_longest_block_times(void)
{
    char text[FRAME_OVERLAY_TEXT_CAPACITY];
    HalAudioStats stats = {.blocks = 4,
                           .decode_total_us = 92000,
                           .decode_max_us = 38400,
                           .read_total_us = 20000,
                           .read_max_us = 11600};
    frame_overlay_format_block_times(stats, text, sizeof text);
    CHECK(strcmp("d23/38 c5/12", text) == 0);
}

static void the_frame_overlay_shows_zero_before_any_block(void)
{
    char text[FRAME_OVERLAY_TEXT_CAPACITY];
    frame_overlay_format_block_times((HalAudioStats){0}, text, sizeof text);
    CHECK(strcmp("d0/0 c0/0", text) == 0);
    frame_overlay_format_audio((HalAudioStats){0}, text, sizeof text);
    CHECK(strcmp("b0 x0 o0 L0%", text) == 0);
}

static void open_music_entry(int index)
{
    tap(KEY_PLAY);
    move_down(index);
    tap(KEY_PLAY);
}

static bool columns_have_ink(int row, int first, int last)
{
    int top = BOARD_THEME.metrics.header_height + row * BOARD_THEME.metrics.row_height;
    for (int y = top; y < top + BOARD_THEME.metrics.row_height; y++) {
        for (int x = first; x < last; x++) {
            if (fake_hal_pixel(x, y) != BOARD_THEME.colors.background) {
                return true;
            }
        }
    }
    return false;
}

static bool row_shows_a_chevron(int row)
{
    int right =
        HAL_DISPLAY_WIDTH - BOARD_THEME.metrics.padding - BOARD_THEME.metrics.selection_inset;
    int chevron = text_width(BOARD_THEME.fonts.row, "\xE2\x80\xBA");
    int gap = BOARD_THEME.metrics.detail_gap;
    return columns_have_ink(row, right - chevron, right) &&
           !columns_have_ink(row, right - chevron - gap, right - chevron);
}

static void album_and_track_rows_show_no_chevron(void)
{
    start_app_with_library();
    open_music_entry(MUSIC_ALBUMS_INDEX);
    step();
    CHECK(!row_shows_a_chevron(1));
    tap(KEY_PLAY);
    step();
    CHECK(!row_shows_a_chevron(1));
}

static void albums_open_from_the_library(void)
{
    start_app_with_library();
    open_music_entry(MUSIC_ALBUMS_INDEX);
    CHECK(title_is("Albums"));
    tap(KEY_PLAY);
    CHECK(title_is("Back In Black"));
    CHECK_EQUAL(4, app_screen_depth());
}

static void a_cue_album_lists_its_cue_tracks_and_each_plays_its_own_part(void)
{
    start_app_with_cue_library();
    open_music_entry(MUSIC_ALBUMS_INDEX);
    move_down(1);
    tap(KEY_PLAY);
    CHECK(title_is("Cue Album"));
    tap(KEY_PLAY);
    CHECK_EQUAL(0, fake_hal_audio_play_start());
    CHECK_EQUAL(1000, fake_hal_audio_play_end());
    tap(KEY_BACK);
    move_down(2);
    tap(KEY_PLAY);
    CHECK_EQUAL(2493, fake_hal_audio_play_start());
    CHECK_EQUAL(0, fake_hal_audio_play_end());
    tap(KEY_BACK);
    move_down(1);
    tap(KEY_PLAY);
    CHECK_EQUAL(0, fake_hal_audio_play_start());
    CHECK_EQUAL(1000, fake_hal_audio_play_end());
}

static void an_artist_leads_to_their_albums(void)
{
    start_app_with_library();
    open_music_entry(MUSIC_ARTISTS_INDEX);
    move_down(BJORK_POSITION);
    tap(KEY_PLAY);
    CHECK(title_is("Björk"));
    tap(KEY_PLAY);
    CHECK(title_is("Homogenic"));
}

static void back_returns_to_the_previous_list(void)
{
    start_app_with_library();
    open_music_entry(MUSIC_ALBUMS_INDEX);
    tap(KEY_PLAY);
    tap(KEY_BACK);
    CHECK(title_is("Albums"));
}

static void without_a_library_the_lists_explain_what_is_missing(void)
{
    start_app();
    open_music_entry(MUSIC_ARTISTS_INDEX);
    CHECK(title_is("No library"));
}

static bool playing_path_is(const char *expected)
{
    return strcmp(expected, fake_hal_audio_playing_path()) == 0;
}

static void play_first_album_track(void)
{
    open_music_entry(MUSIC_ALBUMS_INDEX);
    tap(KEY_PLAY);
    tap(KEY_PLAY);
}

static void selecting_a_track_plays_it_and_shows_now_playing(void)
{
    start_app_with_library();
    play_first_album_track();
    CHECK(title_is("Now Playing"));
    CHECK(playing_path_is(HELLS_BELLS_PATH));
}

static void now_playing_keys_control_the_player(void)
{
    start_app_with_library();
    play_first_album_track();
    uint8_t volume = fake_hal_audio_volume();
    tap(KEY_VOLUME_UP);
    CHECK_EQUAL(volume + 1, fake_hal_audio_volume());
    tap(KEY_PLAY);
    CHECK_EQUAL(PLAYER_PAUSED, app_player_status().state);
    hold(KEY_VOLUME_UP);
    CHECK(playing_path_is(SHOOT_TO_THRILL_PATH));
    CHECK_EQUAL(volume + 1, fake_hal_audio_volume());
    hold(KEY_VOLUME_DOWN);
    CHECK(playing_path_is(HELLS_BELLS_PATH));
}

static void lists_still_move_as_soon_as_a_volume_key_goes_down(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_BACK);
    tap(KEY_BACK);
    CHECK(title_is("Albums"));
    fake_hal_queue_transition(KEY_VOLUME_UP, true, now_ms);
    now_ms += 10;
    step();
    tap(KEY_PLAY);
    CHECK(!title_is("Back In Black"));
}

enum {
    HOME_ART_CYCLE_MS = 5000
};

static uint16_t home_art_probe(void)
{
    return fake_hal_pixel(HAL_DISPLAY_WIDTH - 72, 88);
}

static void home_art_shows_a_library_cover_when_idle(void)
{
    start_app_with_library();
    step();
    step();
    CHECK_EQUAL(RED_COVER, home_art_probe());
}

static void home_art_moves_on_and_skips_albums_without_a_cover(void)
{
    start_app_with_library();
    step();
    wait_ms(HOME_ART_CYCLE_MS);
    step();
    CHECK_EQUAL(BLUE_COVER, home_art_probe());
}

static void home_art_keeps_the_playing_album(void)
{
    start_app_with_library();
    play_first_album_track();
    hold_back();
    step();
    for (int cycle = 0; cycle < 3; cycle++) {
        wait_ms(HOME_ART_CYCLE_MS);
    }
    step();
    CHECK(title_is("Home"));
    CHECK_EQUAL(RED_COVER, home_art_probe());
}

static void open_setting(int group, int item)
{
    move_down(HOME_SETTINGS_INDEX);
    tap(KEY_PLAY);
    move_down(group);
    tap(KEY_PLAY);
    if (group == SETTINGS_RESCAN_GROUP) {
        return;
    }
    move_down(item);
    tap(KEY_PLAY);
}

static void settings_choose_shuffle_and_repeat(void)
{
    start_app_with_library();
    open_setting(SETTINGS_MUSIC_GROUP, 0);
    CHECK(title_is("Shuffle"));
    move_down(1);
    tap(KEY_PLAY);
    CHECK(title_is("Music"));
    CHECK_EQUAL(SHUFFLE_SONGS, app_player_status().shuffle);
    move_down(SETTINGS_REPEAT_INDEX);
    tap(KEY_PLAY);
    CHECK(title_is("Repeat"));
    move_down(2);
    tap(KEY_PLAY);
    CHECK_EQUAL(REPEAT_ALL, app_player_status().repeat);
}

static bool saved_settings(Settings *settings)
{
    uint32_t size = 0;
    const uint8_t *bytes = fake_hal_internal_file_bytes(SETTINGS_STORE_PATH, &size);
    return bytes != NULL && settings_decode(bytes, size, settings);
}

static void home_art_can_show_the_theme_emblem_instead_of_covers(void)
{
    start_app_with_library();
    open_setting(SETTINGS_SYSTEM_GROUP, SETTINGS_HOME_ART_INDEX);
    CHECK(title_is("Home art"));
    move_down(1);
    tap(KEY_PLAY);
    hold_back();
    step();
    step();
    CHECK(title_is("Home"));
    CHECK(home_art_probe() != RED_COVER);
    CHECK(home_art_probe() != BLUE_COVER);
}

static void a_new_theme_applies_at_once_and_keeps_settings_open(void)
{
    start_app_with_library();
    open_setting(SETTINGS_SYSTEM_GROUP, SETTINGS_THEME_INDEX);
    CHECK(title_is("Theme"));
    move_down(THEME_NOAH);
    tap(KEY_PLAY);
    step();
    CHECK(title_is("Settings"));
    tap(KEY_PLAY);
    tap(KEY_PLAY);
    CHECK(title_is("Theme"));
    Settings settings;
    CHECK(saved_settings(&settings));
    CHECK_EQUAL(THEME_NOAH, settings.theme);
}

static void brightness_is_applied_and_saved(void)
{
    start_app_with_library();
    CHECK_EQUAL(settings_defaults().brightness, fake_hal_display_brightness());
    open_setting(SETTINGS_SCREEN_GROUP, SETTINGS_BRIGHTNESS_INDEX);
    CHECK(title_is("Brightness"));
    move_down(1);
    tap(KEY_PLAY);
    CHECK_EQUAL(settings_defaults().brightness + 1, fake_hal_display_brightness());
    Settings settings;
    CHECK(saved_settings(&settings));
    CHECK_EQUAL(settings_defaults().brightness + 1, settings.brightness);
}

static void gain_is_applied(void)
{
    start_app_with_library();
    open_setting(SETTINGS_MUSIC_GROUP, SETTINGS_GAIN_INDEX);
    move_down(1);
    tap(KEY_PLAY);
    CHECK_EQUAL(HAL_AUDIO_GAIN_HIGH, fake_hal_audio_gain());
}

static void sound_settings_reach_the_audio_path_and_survive_a_restart(void)
{
    start_app_with_library();
    open_setting(SETTINGS_MUSIC_GROUP, SETTINGS_EQUALIZER_INDEX);
    CHECK(title_is("Equalizer"));
    move_down(2);
    tap(KEY_PLAY);
    CHECK_EQUAL(2, fake_hal_audio_sound().equalizer);
    restart_app();
    CHECK_EQUAL(2, fake_hal_audio_sound().equalizer);
    open_setting(SETTINGS_MUSIC_GROUP, SETTINGS_BALANCE_INDEX);
    move_up(3);
    tap(KEY_PLAY);
    CHECK_EQUAL(-3, fake_hal_audio_sound().balance);
    restart_app();
    open_setting(SETTINGS_MUSIC_GROUP, SETTINGS_FILTER_INDEX);
    move_down(4);
    tap(KEY_PLAY);
    CHECK_EQUAL(4, fake_hal_audio_sound().filter);
    CHECK_EQUAL(-3, fake_hal_audio_sound().balance);
    CHECK_EQUAL(2, fake_hal_audio_sound().equalizer);
}

static uint16_t screen_art[HAL_DISPLAY_WIDTH * HAL_DISPLAY_HEIGHT];

static void add_screen_art(const char *path, uint16_t color)
{
    for (int index = 0; index < HAL_DISPLAY_WIDTH * HAL_DISPLAY_HEIGHT; index++) {
        screen_art[index] = color;
    }
    fake_hal_add_file(path, (const uint8_t *)screen_art, sizeof screen_art);
}

static void the_theme_cassette_plays_until_cover_style_is_chosen(void)
{
    start_app_with_library();
    add_screen_art("/.echo/player/graysand.bin", 0x1234);
    play_first_album_track();
    wait_ms(DEFAULT_TIMEOUT_MS);
    CHECK_EQUAL(0x1234, fake_hal_pixel(HAL_DISPLAY_WIDTH / 2, 100));
    restart_app();
    open_setting(SETTINGS_MUSIC_GROUP, SETTINGS_PLAYER_STYLE_INDEX);
    CHECK(title_is("Player style"));
    move_up(1);
    tap(KEY_PLAY);
    restart_app();
    play_first_album_track();
    wait_ms(DEFAULT_TIMEOUT_MS);
    CHECK(title_is("Now Playing"));
    CHECK(fake_hal_pixel(HAL_DISPLAY_WIDTH / 2, 100) != 0x1234);
}

static void the_theme_volume_art_shows_while_the_volume_changes(void)
{
    start_app_with_library();
    add_screen_art("/.echo/volume/graysand/0.bin", 0x5678);
    play_first_album_track();
    wait_ms(2 * NOW_PLAYING_VOLUME_FEEDBACK_MS);
    CHECK(fake_hal_pixel(HAL_DISPLAY_WIDTH / 2, 100) != 0x5678);
    for (int press = 0; press < HAL_AUDIO_VOLUME_MAX; press++) {
        tap(KEY_VOLUME_DOWN);
    }
    CHECK_EQUAL(0, app_player_status().volume);
    CHECK_EQUAL(0x5678, fake_hal_pixel(HAL_DISPLAY_WIDTH / 2, 100));
    wait_ms(2 * NOW_PLAYING_VOLUME_FEEDBACK_MS);
    CHECK(fake_hal_pixel(HAL_DISPLAY_WIDTH / 2, 100) != 0x5678);
}

static void files_browses_the_folders_on_the_card(void)
{
    start_app();
    fake_hal_add_dir_entry("/", "Music", true, 0);
    fake_hal_add_dir_entry("/", "notes.txt", false, 2048);
    fake_hal_add_dir_entry("/Music", "song.flac", false, 3u << 20);
    open_setting(SETTINGS_SYSTEM_GROUP, SETTINGS_FILES_INDEX);
    CHECK(title_is("SD card"));
    tap(KEY_PLAY);
    CHECK(title_is("Music"));
    tap(KEY_PLAY);
    CHECK(title_is("Music"));
    tap(KEY_BACK);
    CHECK(title_is("SD card"));
    move_down(1);
    tap(KEY_PLAY);
    CHECK(title_is("SD card"));
    tap(KEY_BACK);
    CHECK(title_is("System"));
}

static void list_rows_show_the_card_icons(void)
{
    static const char NAMES[] = "Playlists\0\2";
    static uint8_t icons[LIST_ICON_BYTES + sizeof NAMES + 1];
    memset(icons, 0xFF, LIST_ICON_BYTES);
    memcpy(icons + LIST_ICON_BYTES, NAMES, sizeof NAMES);
    start_app_with_library();
    fake_hal_add_file("/.echo/icons.bin", icons, sizeof icons);
    restart_app();
    tap(KEY_PLAY);
    CHECK(title_is("Music"));
    CHECK_EQUAL(default_theme_for(THEME_PODIUM)->colors.header_text,
                fake_hal_pixel(24, BOARD_THEME.metrics.header_height + LIST_ROWS_TOP + 32 + 16));
}

static void the_theme_goodbye_art_shows_when_switching_off(void)
{
    start_app_with_library();
    add_screen_art("/.echo/bye/graysand.bin", 0x4321);
    hold_power();
    CHECK_EQUAL(0x4321, fake_hal_pixel(HAL_DISPLAY_WIDTH / 2, HAL_DISPLAY_HEIGHT / 2));
}

static void the_volume_limit_caps_the_volume_keys(void)
{
    start_app_with_library();
    open_setting(SETTINGS_MUSIC_GROUP, SETTINGS_VOLUME_LIMIT_INDEX);
    move_down(3);
    tap(KEY_PLAY);
    hold_back();
    play_first_album_track();
    wait_ms(DEFAULT_TIMEOUT_MS);
    for (int press = 0; press < HAL_AUDIO_VOLUME_MAX; press++) {
        tap(KEY_VOLUME_UP);
    }
    CHECK_EQUAL(20, app_player_status().volume);
}

static void swapped_volume_keys_turn_up_with_the_down_key(void)
{
    start_app_with_library();
    open_setting(SETTINGS_MUSIC_GROUP, SETTINGS_SWAP_KEYS_INDEX);
    move_down(1);
    tap(KEY_PLAY);
    hold_back();
    play_first_album_track();
    wait_ms(DEFAULT_TIMEOUT_MS);
    uint8_t volume = app_player_status().volume;
    tap(KEY_VOLUME_DOWN);
    CHECK_EQUAL(volume + 1, app_player_status().volume);
}

static void saved_settings_are_restored_at_start(void)
{
    start_app_with_library();
    open_setting(SETTINGS_SCREEN_GROUP, SETTINGS_BRIGHTNESS_INDEX);
    move_down(2);
    tap(KEY_PLAY);
    restart_app();
    CHECK_EQUAL(settings_defaults().brightness + 2, fake_hal_display_brightness());
}

static uint16_t selection_probe(void)
{
    return fake_hal_pixel(HAL_DISPLAY_WIDTH / 2 - 20, BOARD_THEME.metrics.header_height + 4);
}

static void a_new_screen_appears_at_once(void)
{
    start_app();
    step();
    move_down(HOME_SETTINGS_INDEX);
    tap(KEY_PLAY);
    CHECK(title_is("Settings"));
    CHECK_EQUAL(BOARD_THEME.colors.selection_background, selection_probe());
}

static void going_back_shows_the_previous_screen_at_once(void)
{
    start_app();
    step();
    move_down(HOME_SETTINGS_INDEX);
    tap(KEY_PLAY);
    tap(KEY_BACK);
    CHECK(title_is("Home"));
    CHECK(selection_probe() != BOARD_THEME.colors.selection_background);
}

static void waking_draws_a_frame_before_the_backlight_comes_on(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_POWER);
    CHECK(!fake_hal_display_on());
    int presents = fake_hal_present_count();
    tap(KEY_POWER);
    CHECK(fake_hal_display_on());
    CHECK(fake_hal_presents_at_power_on() > presents);
}

static void the_clock_never_changes_with_the_screen_while_audio_is_up(void)
{
    start_app_with_library();
    play_first_album_track();
    step();
    CHECK_EQUAL(200, hal_system_clock_mhz());
    tap(KEY_POWER);
    CHECK(!fake_hal_display_on());
    CHECK_EQUAL(200, hal_system_clock_mhz());
    tap(KEY_POWER);
    CHECK_EQUAL(200, hal_system_clock_mhz());
}

static void with_the_screen_off_the_cpu_drops_to_96_mhz_once_audio_is_down(void)
{
    start_app_with_library();
    tap(KEY_POWER);
    CHECK_EQUAL(96, hal_system_clock_mhz());
    tap(KEY_POWER);
    CHECK_EQUAL(200, hal_system_clock_mhz());
    play_first_album_track();
    tap(KEY_POWER);
    CHECK_EQUAL(200, hal_system_clock_mhz());
    fake_hal_audio_set_idle(true);
    step();
    CHECK_EQUAL(96, hal_system_clock_mhz());
    fake_hal_audio_set_idle(false);
    step();
    CHECK_EQUAL(200, hal_system_clock_mhz());
}

static void the_player_turns_itself_off_after_five_idle_minutes_on_battery(void)
{
    start_app_with_library();
    wait_ms(5 * 60000 - 1000);
    CHECK_EQUAL(0, fake_hal_power_off_count());
    wait_ms(1000);
    wait_ms(GOODBYE_MS);
    CHECK_EQUAL(1, fake_hal_power_off_count());
}

static void playing_or_usb_power_keeps_the_player_on(void)
{
    start_app_with_library();
    play_first_album_track();
    for (int minute = 0; minute < 12; minute++) {
        wait_ms(60000);
    }
    CHECK_EQUAL(0, fake_hal_power_off_count());
    tap(KEY_PLAY);
    fake_hal_set_battery(
        (HalBattery){.known = true, .percent = 80, .millivolts = 3900, .usb_power = true});
    for (int minute = 0; minute < 12; minute++) {
        wait_ms(60000);
    }
    CHECK_EQUAL(0, fake_hal_power_off_count());
}

static void the_screen_turns_off_after_the_timeout(void)
{
    start_app();
    step();
    CHECK(fake_hal_display_on());
    wait_ms(DEFAULT_TIMEOUT_MS);
    CHECK(!fake_hal_display_on());
    int presents = fake_hal_present_count();
    step();
    CHECK_EQUAL(presents, fake_hal_present_count());
}

static void after_a_timeout_volume_changes_without_waking(void)
{
    start_app_with_library();
    play_first_album_track();
    uint8_t volume = app_player_status().volume;
    wait_ms(DEFAULT_TIMEOUT_MS);
    CHECK(!fake_hal_display_on());
    tap(KEY_VOLUME_UP);
    CHECK_EQUAL(volume + 1, app_player_status().volume);
    CHECK(!fake_hal_display_on());
}

static void the_timeout_setting_is_used(void)
{
    start_app();
    open_setting(SETTINGS_SCREEN_GROUP, SETTINGS_TIMEOUT_INDEX);
    CHECK(title_is("Screen timeout"));
    move_up(2);
    tap(KEY_PLAY);
    wait_ms(SHORT_TIMEOUT_MS);
    CHECK(!fake_hal_display_on());
}

static void with_the_screen_off_media_keys_keep_working(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_BACK);
    CHECK(!title_is("Now Playing"));
    tap(KEY_POWER);
    CHECK(!fake_hal_display_on());
    uint8_t volume = app_player_status().volume;
    tap(KEY_VOLUME_UP);
    CHECK_EQUAL(volume + 1, app_player_status().volume);
    tap(KEY_PLAY);
    CHECK_EQUAL(PLAYER_PAUSED, app_player_status().state);
    tap(KEY_PLAY);
    CHECK_EQUAL(PLAYER_PLAYING, app_player_status().state);
    hold(KEY_VOLUME_UP);
    CHECK(playing_path_is(SHOOT_TO_THRILL_PATH));
    const char *title = app_screen_title();
    tap(KEY_MENU);
    tap(KEY_BACK);
    CHECK(title_is(title));
    CHECK(!fake_hal_display_on());
    tap(KEY_POWER);
    CHECK(fake_hal_display_on());
}

static void holding_power_saves_and_turns_the_player_off(void)
{
    start_app_with_library();
    play_first_album_track();
    int writes = fake_hal_write_count();
    hold_power();
    CHECK(fake_hal_write_count() > writes);
    CHECK_EQUAL(0, fake_hal_power_off_count());
    CHECK(fake_hal_display_on());
    wait_ms(GOODBYE_MS);
    CHECK_EQUAL(1, fake_hal_power_off_count());
    CHECK(!fake_hal_display_on());
    tap(KEY_PLAY);
    CHECK_EQUAL(1, fake_hal_power_off_count());
}

static void a_one_second_power_hold_only_turns_the_screen_off(void)
{
    start_app_with_library();
    play_first_album_track();
    hold_for(KEY_POWER, 1000);
    wait_ms(GOODBYE_MS);
    CHECK_EQUAL(0, fake_hal_power_off_count());
    CHECK(!fake_hal_display_on());
}

static void a_slightly_long_press_wakes_the_screen_without_switching_off(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_POWER);
    CHECK(!fake_hal_display_on());
    hold_for(KEY_POWER, 1000);
    CHECK_EQUAL(0, fake_hal_power_off_count());
    CHECK(fake_hal_display_on());
}

static void a_volume_hold_under_a_second_changes_the_volume(void)
{
    start_app_with_library();
    play_first_album_track();
    uint8_t volume = fake_hal_audio_volume();
    hold_for(KEY_VOLUME_UP, 800);
    CHECK_EQUAL(volume + 1, fake_hal_audio_volume());
    CHECK(playing_path_is(HELLS_BELLS_PATH));
}

static void a_tap_then_a_hold_keeps_changing_the_volume_but_a_lone_hold_skips(void)
{
    start_app_with_library();
    play_first_album_track();
    uint8_t volume = fake_hal_audio_volume();
    tap(KEY_VOLUME_DOWN);
    CHECK_EQUAL(volume - 1, fake_hal_audio_volume());
    fake_hal_queue_transition(KEY_VOLUME_DOWN, true, now_ms);
    for (int index = 0; index < 30; index++) {
        now_ms += 50;
        step();
    }
    fake_hal_queue_transition(KEY_VOLUME_DOWN, false, now_ms);
    now_ms += 10;
    step();
    CHECK(fake_hal_audio_volume() < volume - 3);
    CHECK(playing_path_is(HELLS_BELLS_PATH));
    wait_ms(2000);
    hold_for(KEY_VOLUME_UP, 1200);
    CHECK(!playing_path_is(HELLS_BELLS_PATH));
}

static void keys_are_ignored_while_saying_goodbye(void)
{
    start_app_with_library();
    hold_power();
    const char *title = app_screen_title();
    tap(KEY_PLAY);
    CHECK(title_is(title));
    wait_ms(GOODBYE_MS);
    CHECK_EQUAL(1, fake_hal_power_off_count());
}

static void the_splash_ignores_keys_until_it_ends(void)
{
    start_app();
    app_show_splash();
    step();
    tap(KEY_PLAY);
    CHECK(title_is("Home"));
    wait_ms(SPLASH_MS);
    tap(KEY_PLAY);
    CHECK(title_is("Music"));
}

static void the_splash_draws_over_the_home_screen(void)
{
    start_app();
    app_show_splash();
    wait_ms(SPLASH_FADE_MS);
    Rect content = theme_content_area(&BOARD_THEME);
    CHECK_EQUAL(BOARD_THEME.colors.background, fake_hal_pixel(content.x + 2, content.y + 4));
}

static bool toast_is(const char *expected)
{
    return app_toast_text() != NULL && strcmp(expected, app_toast_text()) == 0;
}

static void a_refused_track_says_why_it_was_skipped(void)
{
    start_app_with_library();
    fake_hal_audio_set_refusal(
        (HalAudioRefusal){.problem = HAL_AUDIO_PROBLEM_RATE, .sample_rate = 192000});
    fake_hal_audio_refuse_plays(1);
    play_first_album_track();
    CHECK(playing_path_is(SHOOT_TO_THRILL_PATH));
    CHECK(toast_is("192 kHz isn't supported yet"));
}

static void rescanning_without_a_card_says_so(void)
{
    start_app_with_library();
    open_setting(SETTINGS_RESCAN_GROUP, 0);
    step();
    CHECK_EQUAL(0, fake_hal_scratch_borrow_count());
    CHECK(toast_is("No music found"));
}

static void a_rescan_of_an_empty_card_writes_an_empty_library(void)
{
    start_app_with_library();
    fake_hal_set_empty_card(true);
    app_request_rescan();
    CHECK_EQUAL(0, fake_hal_scratch_borrow_count());
    step();
    CHECK_EQUAL(1, fake_hal_scratch_borrow_count());
    CHECK_EQUAL(0, fake_hal_scratch_outstanding());
    CHECK(fake_hal_library_writes() > 0);
    CHECK(toast_is("Library: 0 tracks"));
}

static void start_on_a_fresh_card(HalReserveResult result)
{
    fake_hal_reset();
    fake_hal_set_empty_card(true);
    fake_hal_set_reserve_result(result);
    now_ms = START_MS;
    fake_hal_set_milliseconds(now_ms);
    app_start();
    while (app_preparing()) {
        step();
    }
}

static void a_fresh_card_gets_the_player_files_and_a_first_scan(void)
{
    start_on_a_fresh_card(HAL_RESERVE_CREATED);
    CHECK_EQUAL(4096, fake_hal_reserved_size("/.echo/settings.bin"));
    CHECK_EQUAL(4096, fake_hal_reserved_size("/.echo/resume.bin"));
    CHECK_EQUAL(4096, fake_hal_reserved_size("/.echo/favorites.bin"));
    CHECK_EQUAL(4 * 1024 * 1024, fake_hal_reserved_size("/.echo/library.db"));
    uint32_t covers_size = fake_hal_reserved_size("/.echo/covers.bin");
    CHECK_EQUAL(80 * 1024 * 1024, covers_size);
    CHECK((covers_size - COVERS_HEADER_SIZE) / (COVERS_ENTRY_SIZE + COVER_BUILD_RECORD_BYTES) >=
          2500);
    CHECK_EQUAL(4096, fake_hal_reserved_size("/.echo/scan.bin"));
    CHECK_EQUAL(4096, fake_hal_reserved_size("/.echo/upnext.bin"));
    CHECK_EQUAL(4096, fake_hal_reserved_size("/.echo/bluetooth.bin"));
    step();
    CHECK_EQUAL(1, fake_hal_scratch_borrow_count());
    CHECK(toast_is("Library: 0 tracks"));
}

static void start_on_a_fresh_card_without_stepping(void)
{
    fake_hal_reset();
    fake_hal_set_empty_card(true);
    fake_hal_set_reserve_result(HAL_RESERVE_CREATED);
    now_ms = START_MS;
    fake_hal_set_milliseconds(now_ms);
    app_start();
}

static void starting_makes_no_files_before_the_first_step(void)
{
    start_on_a_fresh_card_without_stepping();
    CHECK_EQUAL(0, fake_hal_reserve_count());
    CHECK(app_preparing());
    CHECK(!fake_hal_display_on());
}

static void each_big_file_is_made_in_its_own_step_after_the_screen_is_lit(void)
{
    start_on_a_fresh_card_without_stepping();
    step();
    CHECK(fake_hal_display_on());
    CHECK(app_preparing());
    CHECK_EQUAL(1, fake_hal_reserve_count());
    step();
    CHECK_EQUAL(2, fake_hal_reserve_count());
    step();
    CHECK_EQUAL(3, fake_hal_reserve_count());
    CHECK(app_preparing());
    step();
    CHECK_EQUAL(12, fake_hal_reserve_count());
    CHECK_EQUAL(1, fake_hal_scratch_borrow_count());
}

static void a_card_with_the_files_is_prepared_within_the_first_step(void)
{
    start_app();
    CHECK(app_preparing());
    step();
    CHECK(!app_preparing());
    CHECK(fake_hal_display_on());
}

static void a_card_that_has_the_files_is_not_rescanned(void)
{
    start_on_a_fresh_card(HAL_RESERVE_EXISTS);
    CHECK_EQUAL(12, fake_hal_reserve_count());
    step();
    CHECK_EQUAL(0, fake_hal_scratch_borrow_count());
}

static void unchanged_music_is_not_rescanned_after_usb(void)
{
    start_on_a_fresh_card(HAL_RESERVE_CREATED);
    step();
    CHECK_EQUAL(1, fake_hal_scratch_borrow_count());
    app_request_rescan_if_music_changed();
    step();
    CHECK_EQUAL(1, fake_hal_scratch_borrow_count());
}

static void a_new_firmware_checks_the_music_once_without_rescanning(void)
{
    start_on_a_fresh_card(HAL_RESERVE_CREATED);
    step();
    CHECK_EQUAL(1, fake_hal_scratch_borrow_count());
    fake_hal_set_version("newer");
    int walks_before = fake_hal_dir_open_count();
    restart_app();
    step();
    CHECK(fake_hal_dir_open_count() > walks_before);
    CHECK_EQUAL(1, fake_hal_scratch_borrow_count());
    int walks_after = fake_hal_dir_open_count();
    restart_app();
    step();
    CHECK_EQUAL(walks_after, fake_hal_dir_open_count());
}

static void the_music_check_after_usb_says_so_before_walking_the_card(void)
{
    start_on_a_fresh_card(HAL_RESERVE_CREATED);
    step();
    int presents = fake_hal_present_count();
    app_request_rescan_if_music_changed();
    step();
    CHECK(fake_hal_presents_at_last_dir_open() > presents);
}

static void a_new_firmware_rescans_music_that_changed(void)
{
    start_on_a_fresh_card(HAL_RESERVE_CREATED);
    step();
    static const uint8_t BLANK[16] = {0};
    CHECK(hal_file_write_at("/.echo/scan.bin", 0, BLANK, sizeof BLANK));
    fake_hal_set_version("newer");
    restart_app();
    step();
    CHECK_EQUAL(2, fake_hal_scratch_borrow_count());
}

static void a_new_firmware_rebuilds_a_library_it_cannot_read(void)
{
    start_on_a_fresh_card(HAL_RESERVE_CREATED);
    step();
    static const uint8_t OLD_VERSION[2] = {2, 0};
    CHECK(hal_file_write_at("/.echo/library.db", 4, OLD_VERSION, sizeof OLD_VERSION));
    fake_hal_set_version("newer");
    restart_app();
    step();
    CHECK_EQUAL(2, fake_hal_scratch_borrow_count());
}

static void a_different_stored_fingerprint_is_rescanned_after_usb(void)
{
    start_on_a_fresh_card(HAL_RESERVE_CREATED);
    step();
    static const uint8_t BLANK[16] = {0};
    CHECK(hal_file_write_at("/.echo/scan.bin", 0, BLANK, sizeof BLANK));
    app_request_rescan_if_music_changed();
    step();
    CHECK_EQUAL(2, fake_hal_scratch_borrow_count());
}

static void a_song_added_over_usb_is_rescanned_after_eject(void)
{
    start_on_a_fresh_card(HAL_RESERVE_CREATED);
    step();
    CHECK_EQUAL(1, fake_hal_scratch_borrow_count());
    fake_hal_add_dir_entry("/", "New Song.flac", false, 4321);
    app_request_rescan_if_music_changed();
    step();
    CHECK_EQUAL(2, fake_hal_scratch_borrow_count());
}

static void a_card_that_cannot_be_walked_is_rescanned_after_usb(void)
{
    start_on_a_fresh_card(HAL_RESERVE_CREATED);
    step();
    fake_hal_set_empty_card(false);
    app_request_rescan_if_music_changed();
    step();
    CHECK(toast_is("No music found"));
}

static void a_full_card_says_it_is_full(void)
{
    start_on_a_fresh_card(HAL_RESERVE_NO_SPACE);
    step();
    CHECK_EQUAL(0, fake_hal_scratch_borrow_count());
    CHECK(toast_is("Storage is full"));
}

static void a_rescan_after_an_installed_update_asks_for_a_restart(void)
{
    start_app_with_library();
    fake_hal_set_empty_card(true);
    fake_hal_set_restart_pending(true);
    app_request_rescan();
    step();
    CHECK_EQUAL(0, fake_hal_scratch_borrow_count());
    CHECK_EQUAL(0, fake_hal_library_writes());
    CHECK(toast_is("Restart the player to finish the update"));
}

static void the_music_check_after_an_installed_update_skips_the_walk(void)
{
    start_app_with_library();
    fake_hal_set_empty_card(true);
    fake_hal_set_restart_pending(true);
    int before = fake_hal_dir_open_count();
    app_request_rescan_if_music_changed();
    step();
    CHECK_EQUAL(before, fake_hal_dir_open_count());
    CHECK(toast_is("Restart the player to finish the update"));
}

static void a_rescan_without_scratch_memory_says_so(void)
{
    start_app_with_library();
    fake_hal_set_empty_card(true);
    fake_hal_refuse_scratch(true);
    app_request_rescan();
    step();
    fake_hal_refuse_scratch(false);
    CHECK(toast_is("Couldn't free memory for the scan"));
}

static bool rescan_message_is(ScanResult result, const char *expected)
{
    char text[48];
    RescanOutcome outcome = {.result = result};
    return strcmp(expected, library_rescan_message(outcome, text, sizeof text)) == 0;
}

static void each_way_a_scan_runs_out_has_its_own_message(void)
{
    CHECK(rescan_message_is(SCAN_OUT_OF_MEMORY, "Library too big for the player's memory"));
    CHECK(rescan_message_is(SCAN_TOO_MANY, "Too many tracks for the player"));
}

static void a_rescan_borrows_the_idle_audio_buffer_for_its_listing(void)
{
    start_app_with_library();
    fake_hal_set_empty_card(true);
    app_request_rescan();
    step();
    CHECK_EQUAL(1, fake_hal_listing_borrow_count());
}

static void a_rescan_rebuilds_the_covers_file(void)
{
    start_app_with_library();
    fake_hal_set_empty_card(true);
    app_request_rescan();
    step();
    CHECK(fake_hal_covers_writes() > 0);
    CHECK(toast_is("Library: 0 tracks"));
}

static void repeated_rescans_do_not_leave_files_open(void)
{
    start_app_with_library();
    fake_hal_set_empty_card(true);
    app_request_rescan();
    step();
    int open_after_one = fake_hal_open_file_count();
    for (int round = 0; round < 6; round++) {
        app_request_rescan();
        step();
    }
    CHECK_EQUAL(open_after_one, fake_hal_open_file_count());
}

static void rescans_that_fail_do_not_leave_files_open(void)
{
    start_app_with_library();
    int open_before = fake_hal_open_file_count();
    for (int round = 0; round < 6; round++) {
        app_request_rescan();
        step();
    }
    CHECK(toast_is("No music found"));
    CHECK_EQUAL(open_before, fake_hal_open_file_count());
    fake_hal_set_empty_card(true);
    fake_hal_refuse_scratch(true);
    for (int round = 0; round < 6; round++) {
        app_request_rescan();
        step();
    }
    fake_hal_refuse_scratch(false);
    CHECK(toast_is("Couldn't free memory for the scan"));
    CHECK_EQUAL(open_before, fake_hal_open_file_count());
    fake_hal_set_restart_pending(true);
    for (int round = 0; round < 6; round++) {
        app_request_rescan();
        step();
    }
    fake_hal_set_restart_pending(false);
    CHECK_EQUAL(open_before, fake_hal_open_file_count());
}

static void a_rescan_while_a_song_plays_then_more_rescans_keep_the_same_files_open(void)
{
    start_app_with_library();
    play_first_album_track();
    fake_hal_set_empty_card(true);
    app_request_rescan();
    step();
    int open_after_one = fake_hal_open_file_count();
    for (int round = 0; round < 6; round++) {
        app_request_rescan_if_music_changed();
        step();
        app_request_rescan();
        step();
    }
    CHECK_EQUAL(open_after_one, fake_hal_open_file_count());
}

static void a_music_check_does_not_leave_the_library_open_twice(void)
{
    start_on_a_fresh_card(HAL_RESERVE_CREATED);
    step();
    int open_before = fake_hal_open_file_count();
    for (int round = 0; round < 6; round++) {
        app_request_rescan_if_music_changed();
        step();
    }
    CHECK_EQUAL(1, fake_hal_scratch_borrow_count());
    CHECK_EQUAL(open_before, fake_hal_open_file_count());
}

static void a_rescan_keeps_the_stall_watch_fed(void)
{
    start_app_with_library();
    fake_hal_set_empty_card(true);
    app_request_rescan();
    int before = fake_hal_keep_alive_count();
    step();
    CHECK(fake_hal_keep_alive_count() > before);
}

static void the_sleep_timer_turns_the_player_off(void)
{
    start_app_with_library();
    fake_hal_set_battery(
        (HalBattery){.known = true, .percent = 80, .millivolts = 3900, .usb_power = true});
    open_setting(SETTINGS_SHUTDOWN_GROUP, SETTINGS_SLEEP_INDEX);
    CHECK(title_is("Sleep timer"));
    move_down(1);
    tap(KEY_PLAY);
    CHECK(app_toast_text() != NULL);
    wait_ms(FIFTEEN_MINUTES_MS - 1000);
    CHECK(!app_charging());
    wait_ms(1000);
    wait_ms(GOODBYE_MS);
    CHECK_EQUAL(0, fake_hal_power_off_count());
    CHECK(app_charging());
}

static const HalBattery ON_USB = {
    .known = true, .percent = 64, .millivolts = 3900, .usb_power = true, .charging = true};

static void switching_off_on_usb_power_shows_the_charge_screen(void)
{
    start_app_with_library();
    play_first_album_track();
    fake_hal_set_battery(ON_USB);
    hold_power();
    wait_ms(GOODBYE_MS);
    CHECK_EQUAL(0, fake_hal_power_off_count());
    CHECK(app_charging());
    CHECK(fake_hal_display_on());
    CHECK_EQUAL(PLAYER_PAUSED, app_player_status().state);
}

static void the_charge_screen_goes_dark_and_a_key_lights_it(void)
{
    start_app_with_library();
    fake_hal_set_battery(ON_USB);
    hold_power();
    wait_ms(GOODBYE_MS);
    wait_ms(5000);
    CHECK(!fake_hal_display_on());
    tap(KEY_PLAY);
    CHECK(fake_hal_display_on());
    CHECK(app_charging());
}

static void holding_power_on_the_charge_screen_turns_the_player_on(void)
{
    start_app_with_library();
    fake_hal_set_battery(ON_USB);
    hold_power();
    wait_ms(GOODBYE_MS);
    hold_power();
    CHECK(!app_charging());
    CHECK(fake_hal_display_on());
    CHECK_EQUAL(0, fake_hal_power_off_count());
}

static void unplugging_on_the_charge_screen_turns_the_player_off(void)
{
    start_app_with_library();
    fake_hal_set_battery(ON_USB);
    hold_power();
    wait_ms(GOODBYE_MS);
    fake_hal_set_battery((HalBattery){.known = true, .percent = 64, .millivolts = 3900});
    wait_ms(500);
    CHECK_EQUAL(1, fake_hal_power_off_count());
}

static void a_cable_start_opens_the_charge_screen(void)
{
    fake_hal_reset();
    fake_hal_set_battery(ON_USB);
    fake_hal_set_usb_start(true);
    now_ms = START_MS;
    fake_hal_set_milliseconds(now_ms);
    app_start();
    CHECK(app_charging());
    wait_ms(100);
    CHECK(fake_hal_display_on());
    CHECK(!fake_hal_bluetooth_powered());
}

static void an_empty_battery_saves_and_turns_the_player_off(void)
{
    start_app_with_library();
    play_first_album_track();
    int writes = fake_hal_write_count();
    fake_hal_set_battery((HalBattery){.known = true, .percent = 0, .millivolts = 3400});
    wait_ms(300);
    wait_ms(1900);
    CHECK_EQUAL(0, fake_hal_power_off_count());
    wait_ms(300);
    CHECK(fake_hal_write_count() > writes);
    wait_ms(GOODBYE_MS);
    CHECK_EQUAL(1, fake_hal_power_off_count());
}

static void an_empty_battery_on_usb_power_keeps_playing(void)
{
    start_app_with_library();
    play_first_album_track();
    fake_hal_set_battery(
        (HalBattery){.known = true, .percent = 0, .millivolts = 3400, .usb_power = true});
    wait_ms(5000);
    wait_ms(GOODBYE_MS);
    CHECK_EQUAL(0, fake_hal_power_off_count());
}

static void open_albums_and_hold_the_first(void)
{
    open_music_entry(MUSIC_ALBUMS_INDEX);
    hold(KEY_PLAY);
}

static void open_first_album(void)
{
    open_music_entry(MUSIC_ALBUMS_INDEX);
    tap(KEY_PLAY);
}

static void holding_play_on_a_song_offers_up_next(void)
{
    start_app_with_library();
    open_first_album();
    hold(KEY_PLAY);
    CHECK(title_is("Hells Bells"));
    tap(KEY_BACK);
    CHECK(title_is("Back In Black"));
}

static void play_next_from_a_list_queues_the_song(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_BACK);
    move_down(1);
    hold(KEY_PLAY);
    tap(KEY_PLAY);
    CHECK(toast_is("Playing next"));
    CHECK(title_is("Back In Black"));
    CHECK(playing_path_is(HELLS_BELLS_PATH));
}

static void add_to_up_next_from_a_list_queues_the_song(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_BACK);
    move_down(1);
    hold(KEY_PLAY);
    move_down(1);
    tap(KEY_PLAY);
    CHECK(toast_is("Added to Up Next"));
    CHECK(title_is("Back In Black"));
}

static void play_next_with_nothing_playing_starts_the_song(void)
{
    start_app_with_library();
    open_first_album();
    hold(KEY_PLAY);
    tap(KEY_PLAY);
    CHECK(playing_path_is(HELLS_BELLS_PATH));
    CHECK(toast_is("Playing"));
}

static void a_paused_song_is_not_sent_again_until_it_moves(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_PLAY);
    wait_ms(4000);
    int presents = fake_hal_present_count();
    wait_ms(500);
    CHECK_EQUAL(presents, fake_hal_present_count());
    fake_hal_audio_set_position(9000);
    wait_ms(500);
    CHECK(fake_hal_present_count() > presents);
}

static void menu_on_now_playing_opens_up_next(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_MENU);
    CHECK(title_is("Up Next"));
    tap(KEY_MENU);
    CHECK(title_is("Now Playing"));
    tap(KEY_MENU);
    tap(KEY_BACK);
    CHECK(title_is("Now Playing"));
}

static void choosing_a_song_in_up_next_plays_it(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_MENU);
    move_down(1);
    tap(KEY_PLAY);
    CHECK(title_is("Now Playing"));
    CHECK(playing_path_is(SHOOT_TO_THRILL_PATH));
}

static void holding_play_on_an_album_adds_it_to_favorites(void)
{
    start_app_with_library();
    open_albums_and_hold_the_first();
    CHECK(title_is("Albums"));
    CHECK(toast_is("Added to Favorites"));
    tap(KEY_BACK);
    move_up(MUSIC_ALBUMS_INDEX - MUSIC_FAVORITES_INDEX);
    tap(KEY_PLAY);
    CHECK(title_is("Favorites"));
    tap(KEY_PLAY);
    CHECK(title_is("Back In Black"));
}

static void holding_play_in_favorites_removes_the_album(void)
{
    start_app_with_library();
    open_albums_and_hold_the_first();
    tap(KEY_BACK);
    move_up(MUSIC_ALBUMS_INDEX - MUSIC_FAVORITES_INDEX);
    tap(KEY_PLAY);
    hold(KEY_PLAY);
    CHECK(toast_is("Removed from Favorites"));
    tap(KEY_PLAY);
    CHECK(title_is("Favorites"));
}

static void favorites_survive_a_restart(void)
{
    start_app_with_library();
    open_albums_and_hold_the_first();
    restart_app();
    open_music_entry(MUSIC_FAVORITES_INDEX);
    tap(KEY_PLAY);
    CHECK(title_is("Back In Black"));
}

static void a_toast_disappears_after_a_while(void)
{
    start_app_with_library();
    open_albums_and_hold_the_first();
    wait_ms(TOAST_DURATION_MS);
    CHECK(app_toast_text() == NULL);
}

static void favorite_the_first_song(void)
{
    open_first_album();
    hold(KEY_PLAY);
    move_down(2);
    tap(KEY_PLAY);
}

static void a_song_can_be_added_to_favorites(void)
{
    start_app_with_library();
    favorite_the_first_song();
    CHECK(toast_is("Added to Favorites"));
    CHECK(title_is("Back In Black"));
    restart_app();
    open_music_entry(MUSIC_FAVORITES_INDEX);
    tap(KEY_PLAY);
    CHECK(title_is("Favorite Songs"));
    tap(KEY_PLAY);
    CHECK(title_is("Now Playing"));
    CHECK(playing_path_is(HELLS_BELLS_PATH));
}

static void a_favorite_song_can_be_removed(void)
{
    start_app_with_library();
    favorite_the_first_song();
    hold(KEY_PLAY);
    move_down(2);
    tap(KEY_PLAY);
    CHECK(toast_is("Removed from Favorites"));
    restart_app();
    open_music_entry(MUSIC_FAVORITES_INDEX);
    tap(KEY_PLAY);
    CHECK(title_is("Favorites"));
}

static void playlists_open_from_the_music_menu(void)
{
    start_app_with_library();
    open_music_entry(MUSIC_PLAYLISTS_INDEX);
    CHECK(title_is("Playlists"));
    move_down(1);
    tap(KEY_PLAY);
    CHECK(title_is("Road Trip"));
    tap(KEY_PLAY);
    CHECK(playing_path_is(SHOOTS_PATH));
}

static void favorites_are_pinned_at_the_top_of_playlists(void)
{
    start_app_with_library();
    open_albums_and_hold_the_first();
    restart_app();
    open_music_entry(MUSIC_PLAYLISTS_INDEX);
    CHECK(title_is("Playlists"));
    tap(KEY_PLAY);
    CHECK(title_is("Favorites"));
    tap(KEY_PLAY);
    CHECK(title_is("Back In Black"));
}

static void start_with(StartScreen start)
{
    start_app_with_library();
    open_setting(SETTINGS_SYSTEM_GROUP, SETTINGS_START_INDEX);
    CHECK(title_is("Start screen"));
    move_down((int)start);
    tap(KEY_PLAY);
    restart_app();
}

static void the_start_screen_setting_opens_albums(void)
{
    start_with(START_ALBUMS);
    CHECK(title_is("Albums"));
    tap(KEY_BACK);
    CHECK(title_is("Music"));
    tap(KEY_BACK);
    CHECK(title_is("Home"));
}

static void the_start_screen_setting_opens_now_playing(void)
{
    start_with(START_NOW_PLAYING);
    CHECK(title_is("Now Playing"));
    CHECK_EQUAL(2, app_screen_depth());
}

static void settle(void)
{
    for (int count = 0; count < SETTLE_STEPS; count++) {
        wait_ms(16);
    }
}

static bool header_title_matches_the_notice_theme(void)
{
    static Color expected[HAL_DISPLAY_WIDTH * 32];
    int height = NOTICE_THEME.metrics.header_height;
    Canvas canvas = canvas_make(expected, (Rect){0, 0, HAL_DISPLAY_WIDTH, height});
    header_draw(&canvas, &NOTICE_THEME, app_screen_title());
    for (int y = 0; y < height; y++) {
        for (int x = NOTICE_THEME.metrics.padding; x < HAL_DISPLAY_WIDTH / 4; x++) {
            if (fake_hal_pixel(x, y) != canvas_pixel_at(&canvas, x, y)) {
                return false;
            }
        }
    }
    return true;
}

static void after_an_installed_update_screens_use_the_resident_font(void)
{
    start_app_with_library();
    settle();
    CHECK(!header_title_matches_the_notice_theme());
    fake_hal_set_restart_pending(true);
    app_start();
    settle();
    CHECK(header_title_matches_the_notice_theme());
}

static bool area_has_color(Rect area, Color color)
{
    for (int y = area.y; y < area.y + area.height; y++) {
        for (int x = area.x; x < area.x + area.width; x++) {
            if (fake_hal_pixel(x, y) == color) {
                return true;
            }
        }
    }
    return false;
}

static void the_header_shows_the_play_state_and_battery(void)
{
    start_app_with_library();
    fake_hal_set_battery((HalBattery){.known = true, .percent = HALF_BATTERY, .charging = false});
    Color icon = BOARD_THEME.colors.header_text;
    step();
    CHECK(!area_has_color(status_bar_playback_area(&BOARD_THEME), icon));
    CHECK(area_has_color(status_bar_battery_area(&BOARD_THEME), icon));
    play_first_album_track();
    CHECK(title_is("Now Playing"));
    CHECK(!area_has_color(status_bar_playback_area(&BOARD_THEME), icon));
    tap(KEY_BACK);
    CHECK(!title_is("Now Playing"));
    CHECK(!area_has_color(status_bar_playback_area(&BOARD_THEME), icon));
}

static void stopping_the_app_saves_the_session(void)
{
    start_app_with_library();
    play_first_album_track();
    int writes = fake_hal_write_count();
    app_stop();
    Settings settings;
    CHECK_EQUAL(writes + 3, fake_hal_write_count());
    CHECK(saved_settings(&settings));
}

static void each_step_records_its_duration(void)
{
    start_app();
    fake_hal_set_microseconds_per_read(4000);
    uint32_t before_us = fake_hal_microseconds();
    step();
    uint32_t first_read_us = before_us + 4000;
    CHECK_EQUAL(fake_hal_microseconds() - first_read_us, frame_stats_latest_us(app_frame_stats()));
    CHECK(frame_stats_latest_us(app_frame_stats()) > 0);
}

static void steps_inside_one_redraw_period_present_once(void)
{
    start_app();
    step();
    int presents = fake_hal_present_count();
    now_ms += 5;
    step();
    now_ms += 5;
    step();
    CHECK_EQUAL(presents, fake_hal_present_count());
    now_ms += 6;
    step();
    CHECK_EQUAL(presents + 1, fake_hal_present_count());
}

static void a_key_press_redraws_at_once(void)
{
    start_app();
    step();
    int presents = fake_hal_present_count();
    now_ms += 1;
    fake_hal_queue_transition(KEY_VOLUME_DOWN, true, now_ms);
    step();
    CHECK_EQUAL(presents + 1, fake_hal_present_count());
}

static void a_notice_is_drawn_at_once_over_the_current_screen(void)
{
    start_app();
    step();
    int presents = fake_hal_present_count();
    long pixels = fake_hal_pixels_written();
    app_show_notice("USB", "Connected to a computer");
    CHECK_EQUAL(presents + 1, fake_hal_present_count());
    CHECK(fake_hal_pixels_written() > pixels);
    CHECK(title_is("Home"));
}

static uint16_t cassette_label_probe(void)
{
    const NowPlayingLayout *layout = &BOARD_LAYOUT.now_playing;
    Rect area = {(HAL_DISPLAY_WIDTH - layout->cassette_width) / 2, layout->cassette_top,
                 layout->cassette_width, layout->cassette_height};
    Rect label = cassette_parts(area).label;
    return fake_hal_pixel(label.x + 4, label.y + 8);
}

static void usb_notices_show_a_cassette(void)
{
    start_app();
    step();
    app_show_notice("USB", "Connected to a computer");
    CHECK_EQUAL(cassette_colors(BOARD_THEME.style).label, cassette_label_probe());
    app_show_notice("USB DAC", "Computer found. Starting...");
    CHECK_EQUAL(cassette_colors(BOARD_THEME.style).label, cassette_label_probe());
    app_show_notice("Scanning library", "Starting...");
    CHECK(cassette_colors(BOARD_THEME.style).label != cassette_label_probe());
}

static void a_notice_turns_the_display_on(void)
{
    start_app();
    wait_ms(DEFAULT_TIMEOUT_MS);
    CHECK(!fake_hal_display_on());
    app_show_notice("USB", "Connected to a computer");
    CHECK(fake_hal_display_on());
}

static void waking_keeps_the_screen_on_for_a_full_timeout(void)
{
    start_app();
    step();
    now_ms += DEFAULT_TIMEOUT_MS * 4;
    fake_hal_set_milliseconds(now_ms);
    app_wake();
    wait_ms(DEFAULT_TIMEOUT_MS - 1000);
    CHECK(fake_hal_display_on());
    wait_ms(1000);
    CHECK(!fake_hal_display_on());
}

static void diagnostics_shows_the_firmware_version_from_the_hal(void)
{
    start_app();
    fake_hal_set_version("1e8bebb");
    const char *value = app_diagnostics_value(DIAGNOSTICS_FIRMWARE_INDEX);
    CHECK(value != NULL);
    CHECK(strcmp("1e8bebb", value) == 0);
}

static uint16_t cover_probe(void)
{
    Rect cover = now_playing_cover_area(&BOARD_THEME);
    return fake_hal_pixel(cover.x + cover.width / 2, cover.y + cover.height / 2);
}

static void now_playing_shows_the_album_cover(void)
{
    start_app_with_library();
    play_first_album_track();
    step();
    step();
    CHECK_EQUAL(0xF800, cover_probe());
}

static void now_playing_without_a_cover_shows_a_plain_placeholder(void)
{
    start_app_with_library();
    open_music_entry(MUSIC_ALBUMS_INDEX);
    move_down(1);
    tap(KEY_PLAY);
    tap(KEY_PLAY);
    step();
    step();
    CHECK(title_is("Now Playing"));
    Rect cover = now_playing_cover_area(&BOARD_THEME);
    Color plain = color_blend(BOARD_THEME.colors.background, BOARD_THEME.colors.header_text, 1);
    CHECK_EQUAL(plain, fake_hal_pixel(cover.x + 4, cover.y + 4));
    CHECK(cover_probe() != 0xF800);
}

static void an_unchanged_frame_sends_no_pixels(void)
{
    start_app();
    step();
    long pixels = fake_hal_pixels_written();
    step();
    CHECK_EQUAL(pixels, fake_hal_pixels_written());
}

static void moving_the_selection_sends_only_part_of_the_screen(void)
{
    start_app();
    step();
    long pixels = fake_hal_pixels_written();
    tap(KEY_VOLUME_UP);
    long sent = fake_hal_pixels_written() - pixels;
    CHECK(sent > 0);
    CHECK(sent < FULL_FRAME_PIXELS / 2);
}

static void restarting_the_app_sends_a_whole_frame_again(void)
{
    start_app();
    step();
    restart_app();
    long pixels = fake_hal_pixels_written();
    step();
    CHECK_EQUAL(pixels + FULL_FRAME_PIXELS, fake_hal_pixels_written());
}

static void play_on_the_playing_track_opens_now_playing_without_restarting(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_BACK);
    CHECK(!title_is("Now Playing"));
    int plays = fake_hal_audio_play_count();
    tap(KEY_PLAY);
    CHECK(title_is("Now Playing"));
    CHECK_EQUAL(plays, fake_hal_audio_play_count());
}

static void play_on_the_paused_track_resumes_it_without_restarting(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_PLAY);
    CHECK_EQUAL(PLAYER_PAUSED, app_player_status().state);
    tap(KEY_BACK);
    int plays = fake_hal_audio_play_count();
    tap(KEY_PLAY);
    CHECK(title_is("Now Playing"));
    CHECK_EQUAL(PLAYER_PLAYING, app_player_status().state);
    CHECK_EQUAL(plays, fake_hal_audio_play_count());
    CHECK(playing_path_is(HELLS_BELLS_PATH));
}

static void play_on_the_song_restored_by_a_restart_of_the_app_plays_it(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_BACK);
    app_start();
    CHECK_EQUAL(PLAYER_PAUSED, app_player_status().state);
    play_first_album_track();
    CHECK(title_is("Now Playing"));
    CHECK_EQUAL(PLAYER_PLAYING, app_player_status().state);
    CHECK(playing_path_is(HELLS_BELLS_PATH));
}

static void play_on_another_track_starts_it(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_BACK);
    move_down(1);
    int plays = fake_hal_audio_play_count();
    tap(KEY_PLAY);
    CHECK(title_is("Now Playing"));
    CHECK_EQUAL(plays + 1, fake_hal_audio_play_count());
    CHECK(playing_path_is(SHOOT_TO_THRILL_PATH));
}

static void idling_in_the_menus_stays_put_while_playing(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_BACK);
    for (uint32_t waited = 0; waited < IDLE_MS; waited += 1000) {
        wait_ms(1000);
    }
    CHECK(fake_hal_display_on());
    CHECK(!title_is("Now Playing"));
}

static void the_key_test_stays_open_after_waking(void)
{
    start_app_with_library();
    play_first_album_track();
    hold_back();
    open_diagnostics();
    move_down(DIAGNOSTICS_KEY_TEST_INDEX);
    tap(KEY_PLAY);
    CHECK(title_is("Key test"));
    tap(KEY_POWER);
    tap(KEY_POWER);
    CHECK(title_is("Key test"));
}

static void menu_stays_in_the_key_test_while_playing(void)
{
    start_app_with_library();
    play_first_album_track();
    hold_back();
    open_diagnostics();
    move_down(DIAGNOSTICS_KEY_TEST_INDEX);
    tap(KEY_PLAY);
    tap(KEY_MENU);
    CHECK(title_is("Key test"));
}

static void waking_the_screen_shows_now_playing_while_playing(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_BACK);
    tap(KEY_POWER);
    CHECK(!fake_hal_display_on());
    tap(KEY_POWER);
    CHECK(fake_hal_display_on());
    CHECK(title_is("Now Playing"));
}

static void waking_while_paused_keeps_the_screen(void)
{
    start_app_with_library();
    play_first_album_track();
    tap(KEY_PLAY);
    tap(KEY_BACK);
    tap(KEY_POWER);
    tap(KEY_POWER);
    CHECK(!title_is("Now Playing"));
}

static void open_diagnostics_row(int index)
{
    open_diagnostics();
    move_down(index);
}

static void the_cpu_clock_cycles_through_24_96_and_200_mhz(void)
{
    start_app();
    open_diagnostics_row(DIAGNOSTICS_CPU_CLOCK_INDEX);
    CHECK_EQUAL(200, hal_system_clock_mhz());
    tap(KEY_PLAY);
    CHECK_EQUAL(24, hal_system_clock_mhz());
    tap(KEY_PLAY);
    CHECK_EQUAL(96, hal_system_clock_mhz());
    tap(KEY_PLAY);
    CHECK_EQUAL(200, hal_system_clock_mhz());
}

static void the_test_tone_plays_once_per_press(void)
{
    start_app();
    open_diagnostics_row(DIAGNOSTICS_TEST_TONE_INDEX);
    CHECK_EQUAL(0, fake_hal_test_tone_count());
    tap(KEY_PLAY);
    CHECK_EQUAL(1, fake_hal_test_tone_count());
    CHECK(title_is("Diagnostics"));
}

static void the_stall_test_row_freezes_the_main_loop_on_purpose(void)
{
    start_app();
    open_diagnostics_row(DIAGNOSTICS_STALL_TEST_INDEX);
    CHECK_EQUAL(0, fake_hal_stall_test_count());
    tap(KEY_PLAY);
    CHECK_EQUAL(1, fake_hal_stall_test_count());
}

static void the_watchdog_test_row_stops_everything_on_purpose(void)
{
    start_app();
    open_diagnostics_row(DIAGNOSTICS_WATCHDOG_TEST_INDEX);
    CHECK_EQUAL(0, fake_hal_watchdog_test_count());
    tap(KEY_PLAY);
    CHECK_EQUAL(1, fake_hal_watchdog_test_count());
}

static void ignored_power_taps_leave_the_screen_on_but_a_hold_still_switches_off(void)
{
    start_app();
    open_diagnostics_row(DIAGNOSTICS_POWER_TAPS_INDEX);
    tap(KEY_PLAY);
    tap(KEY_POWER);
    CHECK(fake_hal_display_on());
    hold_power();
    wait_ms(GOODBYE_MS);
    CHECK_EQUAL(1, fake_hal_power_off_count());
}

static void the_dma_tone_starts_and_stops_from_one_row(void)
{
    start_app();
    open_diagnostics_row(DIAGNOSTICS_DMA_TONE_INDEX);
    CHECK(!hal_audio_dma_tone_status().running);
    tap(KEY_PLAY);
    CHECK(hal_audio_dma_tone_status().running);
    tap(KEY_PLAY);
    CHECK(!hal_audio_dma_tone_status().running);
    CHECK(title_is("Diagnostics"));
}

static void the_high_rate_tone_plays_at_192_khz(void)
{
    start_app();
    open_diagnostics_row(DIAGNOSTICS_HIGH_RATE_TONE_INDEX);
    tap(KEY_PLAY);
    CHECK(hal_audio_dma_tone_status().running);
    CHECK_EQUAL(192000u, fake_hal_dma_tone_rate());
    tap(KEY_PLAY);
    CHECK(!hal_audio_dma_tone_status().running);
}

static void the_176_khz_tone_plays_at_176_4_khz(void)
{
    start_app();
    open_diagnostics_row(DIAGNOSTICS_TONE_176_INDEX);
    tap(KEY_PLAY);
    CHECK(hal_audio_dma_tone_status().running);
    CHECK_EQUAL(176400u, fake_hal_dma_tone_rate());
    tap(KEY_PLAY);
    CHECK(!hal_audio_dma_tone_status().running);
}

static void the_dma_tone_plays_at_44_1_khz(void)
{
    start_app();
    open_diagnostics_row(DIAGNOSTICS_DMA_TONE_INDEX);
    tap(KEY_PLAY);
    CHECK_EQUAL(44100u, fake_hal_dma_tone_rate());
}

static void the_loop_benchmark_runs_once_per_press(void)
{
    start_app();
    open_diagnostics_row(DIAGNOSTICS_LOOP_CYCLES_INDEX);
    CHECK_EQUAL(0, fake_hal_loop_bench_count());
    tap(KEY_PLAY);
    CHECK_EQUAL(1, fake_hal_loop_bench_count());
    CHECK(title_is("Diagnostics"));
}

static void the_core_1_test_runs_once_per_press(void)
{
    start_app();
    open_diagnostics_row(DIAGNOSTICS_CORE1_INDEX);
    CHECK_EQUAL(0, fake_hal_core1_test_count());
    tap(KEY_PLAY);
    CHECK_EQUAL(1, fake_hal_core1_test_count());
    CHECK(title_is("Diagnostics"));
}

static void the_accelerator_test_runs_once_per_press(void)
{
    start_app();
    open_diagnostics_row(DIAGNOSTICS_ACCEL_INDEX);
    CHECK_EQUAL(0, fake_hal_core1_accel_count());
    tap(KEY_PLAY);
    CHECK_EQUAL(1, fake_hal_core1_accel_count());
    CHECK(title_is("Diagnostics"));
}

static void a_clock_that_does_not_lock_stays_where_it_was(void)
{
    start_app();
    fake_hal_refuse_clock_switch();
    open_diagnostics_row(DIAGNOSTICS_CPU_CLOCK_INDEX);
    tap(KEY_PLAY);
    CHECK_EQUAL(200, hal_system_clock_mhz());
    CHECK(title_is("Diagnostics"));
}

static void the_stopwatch_stays_open_after_waking(void)
{
    start_app_with_library();
    play_first_album_track();
    hold_back();
    open_diagnostics_row(DIAGNOSTICS_STOPWATCH_INDEX);
    tap(KEY_PLAY);
    CHECK(title_is("Stopwatch"));
    tap(KEY_POWER);
    tap(KEY_POWER);
    CHECK(title_is("Stopwatch"));
}

static void menu_opens_now_playing_from_anywhere_while_playing(void)
{
    start_app_with_library();
    play_first_album_track();
    hold_back();
    CHECK(title_is("Home"));
    tap(KEY_MENU);
    CHECK(title_is("Now Playing"));
    int depth = app_screen_depth();
    tap(KEY_MENU);
    CHECK(title_is("Up Next"));
    CHECK_EQUAL(depth + 1, app_screen_depth());
}

static void menu_does_nothing_while_nothing_plays(void)
{
    start_app_with_library();
    tap(KEY_MENU);
    CHECK(title_is("Home"));
}

static uint16_t theme_probe(void)
{
    settle();
    return fake_hal_pixel(4, HAL_DISPLAY_HEIGHT - 4);
}

static void choose_noah_and_go_home(void)
{
    open_setting(SETTINGS_SYSTEM_GROUP, SETTINGS_THEME_INDEX);
    move_down(THEME_NOAH);
    tap(KEY_PLAY);
    hold_back();
    step();
}

static void the_theme_survives_the_battery_running_out(void)
{
    start_app_with_library();
    uint16_t graysand = theme_probe();
    choose_noah_and_go_home();
    uint16_t noah = theme_probe();
    CHECK(noah != graysand);
    fake_hal_set_battery((HalBattery){.known = true, .percent = 0, .millivolts = 3300});
    for (int second = 0; second < 5; second++) {
        wait_ms(1000);
    }
    wait_ms(GOODBYE_MS);
    CHECK_EQUAL(1, fake_hal_power_off_count());
    fake_hal_set_battery((HalBattery){.known = true, .percent = HALF_BATTERY, .millivolts = 3800});
    restart_app();
    CHECK(title_is("Home"));
    CHECK_EQUAL(noah, theme_probe());
}

static void a_torn_settings_write_falls_back_to_the_backup(void)
{
    static const uint8_t TORN[SETTINGS_FILE_SIZE] = {'E', 'M', 'S', 'T', 5};
    start_app_with_library();
    choose_noah_and_go_home();
    uint16_t noah = theme_probe();
    CHECK(hal_internal_file_write(SETTINGS_STORE_PATH, TORN, sizeof TORN));
    restart_app();
    CHECK_EQUAL(noah, theme_probe());
}

static bool home_row_opens(int index, const char *title)
{
    restart_app();
    move_down(index);
    tap(KEY_PLAY);
    return title_is(title);
}

static void the_home_menu_lists_music_books_bluetooth_and_settings(void)
{
    start_app_with_library();
    CHECK(home_row_opens(0, "Music"));
    CHECK(home_row_opens(HOME_BOOKS_INDEX, "Books"));
    tap(KEY_PLAY);
    CHECK(title_is("Audiobooks"));
    CHECK(home_row_opens(HOME_BLUETOOTH_INDEX, "Bluetooth"));
    CHECK(home_row_opens(HOME_SETTINGS_INDEX, "Settings"));
}

static void quran_shows_on_home_only_when_a_quran_folder_exists(void)
{
    static SpokenLibrary spoken;
    Library *library = spoken_library_open(&spoken);
    CHECK(library != NULL);
    start_app_with_database(spoken.bytes, spoken.size);
    CHECK(home_row_opens(HOME_QURAN_INDEX, "Quran"));
    CHECK(home_row_opens(HOME_QURAN_INDEX + 1, "Books"));
    CHECK(home_row_opens(HOME_SETTINGS_INDEX + 1, "Settings"));
    spoken_library_close(&spoken);
}

static void podcasts_can_get_their_own_home_row(void)
{
    start_app_with_library();
    open_setting(SETTINGS_SYSTEM_GROUP, SETTINGS_PODCASTS_INDEX);
    CHECK(title_is("Podcasts"));
    move_down(1);
    tap(KEY_PLAY);
    Settings settings;
    CHECK(saved_settings(&settings));
    CHECK(settings.podcasts_separate);
    CHECK(home_row_opens(HOME_BOOKS_INDEX, "Audiobooks"));
    CHECK(home_row_opens(HOME_BOOKS_INDEX + 1, "Podcasts"));
    CHECK(home_row_opens(HOME_BLUETOOTH_INDEX + 1, "Bluetooth"));
    CHECK(home_row_opens(HOME_SETTINGS_INDEX + 1, "Settings"));
}

enum {
    HOME_PAGE_BYTES = HAL_DISPLAY_WIDTH * HAL_DISPLAY_HEIGHT * 2
};

static const Color PAGE_MUSIC = 0x07E0;
static const Color PAGE_QURAN = 0xF81F;
static uint8_t music_page[HOME_PAGE_BYTES];
static uint8_t quran_page[HOME_PAGE_BYTES];

static void fill_page(uint8_t *page, Color color)
{
    for (int index = 0; index < HOME_PAGE_BYTES; index += 2) {
        page[index] = (uint8_t)(color & 0xFF);
        page[index + 1] = (uint8_t)(color >> 8);
    }
}

static void start_app_with_home_pages(uint32_t music_size)
{
    fill_page(music_page, PAGE_MUSIC);
    fill_page(quran_page, PAGE_QURAN);
    fake_hal_reset();
    fake_hal_add_file("/.echo/home/graysand/4-noquran/music.bin", music_page, music_size);
    fake_hal_add_file("/.echo/home/graysand/4-noquran/books.bin", quran_page, HOME_PAGE_BYTES);
    prefill_player_files();
    now_ms = START_MS;
    fake_hal_set_milliseconds(now_ms);
    app_start();
    wait_ms(SPLASH_MS);
    settle();
}

static void a_home_page_on_the_card_fills_the_screen(void)
{
    start_app_with_home_pages(HOME_PAGE_BYTES);
    CHECK_EQUAL(PAGE_MUSIC, fake_hal_pixel(240, 2));
    CHECK_EQUAL(PAGE_MUSIC, fake_hal_pixel(100, 100));
    move_down(1);
    settle();
    CHECK_EQUAL(PAGE_QURAN, fake_hal_pixel(100, 100));
}

static void a_still_home_page_is_not_sent_again(void)
{
    start_app_with_home_pages(HOME_PAGE_BYTES);
    int presents = fake_hal_present_count();
    settle();
    CHECK_EQUAL(presents, fake_hal_present_count());
    move_down(1);
    settle();
    CHECK(fake_hal_present_count() > presents);
}

static void a_still_list_is_not_sent_again(void)
{
    start_app_with_library();
    tap(KEY_PLAY);
    settle();
    int presents = fake_hal_present_count();
    settle();
    CHECK_EQUAL(presents, fake_hal_present_count());
    move_down(1);
    settle();
    CHECK(fake_hal_present_count() > presents);
}

static void a_home_page_of_the_wrong_size_is_ignored(void)
{
    start_app_with_home_pages(HOME_PAGE_BYTES - 2);
    CHECK(fake_hal_pixel(100, 100) != PAGE_MUSIC);
    move_down(1);
    settle();
    CHECK_EQUAL(PAGE_QURAN, fake_hal_pixel(100, 100));
}

static void start_app_with_motion(const char *first, const char *second)
{
    fill_page(music_page, PAGE_MUSIC);
    fill_page(quran_page, PAGE_QURAN);
    start_app();
    fake_hal_add_file(first, music_page, HOME_PAGE_BYTES);
    if (second != NULL) {
        fake_hal_add_file(second, quran_page, HOME_PAGE_BYTES);
    }
    wait_ms(16);
}

static void the_boot_frames_play_in_turn_then_the_home_screen(void)
{
    start_app_with_motion("/.echo/boot/graysand/00.bin", "/.echo/boot/graysand/01.bin");
    app_show_splash();
    wait_ms(16);
    CHECK_EQUAL(PAGE_MUSIC, fake_hal_pixel(100, 100));
    wait_ms(70);
    CHECK_EQUAL(PAGE_QURAN, fake_hal_pixel(100, 100));
    wait_ms(70);
    CHECK(fake_hal_pixel(100, 100) != PAGE_QURAN);
}

static void a_key_skips_the_boot_frames_and_goes_nowhere(void)
{
    start_app_with_motion("/.echo/boot/graysand/00.bin", "/.echo/boot/graysand/01.bin");
    app_show_splash();
    wait_ms(16);
    tap(KEY_PLAY);
    wait_ms(16);
    CHECK(fake_hal_pixel(100, 100) != PAGE_MUSIC);
    CHECK(!title_is("Music"));
}

static void choose_idle_screen(ThemeChoice theme)
{
    static uint8_t bytes[SETTINGS_FILE_SIZE];
    Settings settings = settings_defaults();
    settings.screen_timeout = SCREEN_TIMEOUT_IDLE_SCREEN;
    settings.theme = theme;
    settings_encode(&settings, bytes);
    fake_hal_add_file("/.echo/settings.bin", bytes, sizeof bytes);
    restart_app();
    wait_ms(16);
}

static bool idle_text_shown(void)
{
    for (int y = 140; y < HAL_DISPLAY_HEIGHT; y++) {
        for (int x = 0; x < HAL_DISPLAY_WIDTH / 2; x++) {
            if (fake_hal_pixel(x, y) == 0xFFFF) {
                return true;
            }
        }
    }
    return false;
}

static void without_the_idle_screen_choice_no_idle_screen_comes(void)
{
    start_app_with_motion("/.echo/afk/graysand/00.bin", NULL);
    wait_ms(10100);
    wait_ms(16);
    CHECK(fake_hal_display_on());
    CHECK(fake_hal_pixel(100, 100) != PAGE_MUSIC);
}

static void the_idle_screen_stays_on_instead_of_the_screen_turning_off(void)
{
    start_app_with_motion("/.echo/afk/graysand/00.bin", NULL);
    choose_idle_screen(THEME_PODIUM);
    wait_ms(10100);
    wait_ms(DEFAULT_TIMEOUT_MS * 4);
    CHECK(fake_hal_display_on());
    CHECK(fake_hal_pixel(100, 100) != PAGE_MUSIC);
}

static void the_idle_screen_shows_the_playing_track(void)
{
    start_app_with_library();
    fill_page(music_page, PAGE_MUSIC);
    fake_hal_add_file("/.echo/afk/pokemon/00.bin", music_page, HOME_PAGE_BYTES);
    choose_idle_screen(THEME_POKEMON);
    wait_ms(10100);
    wait_ms(16);
    CHECK_EQUAL(PAGE_MUSIC, fake_hal_pixel(100, 100));
    CHECK(!idle_text_shown());
    tap(KEY_PLAY);
    play_first_album_track();
    wait_ms(10100);
    wait_ms(16);
    CHECK(idle_text_shown());
    CHECK_EQUAL(PAGE_MUSIC, fake_hal_pixel(400, 100));
}

static uint32_t clock_corner(void)
{
    uint32_t white = 0;
    for (int y = 20; y < 70; y++) {
        for (int x = HAL_DISPLAY_WIDTH - 120; x < HAL_DISPLAY_WIDTH; x++) {
            white = white * 31u + (fake_hal_pixel(x, y) == 0xFFFF ? (uint32_t)x : 0u);
        }
    }
    return white;
}

static void the_idle_screen_shows_the_clock_and_it_keeps_time(void)
{
    start_app_with_motion("/.echo/afk/pokemon/00.bin", NULL);
    choose_idle_screen(THEME_POKEMON);
    wait_ms(10100);
    wait_ms(16);
    CHECK_EQUAL(0, clock_corner());
    tap(KEY_PLAY);
    fake_hal_set_wall_time((WallTime){2026, 11, 7, 21, 9, 45});
    wait_ms(10100);
    wait_ms(16);
    uint32_t before = clock_corner();
    CHECK(before != 0);
    wait_ms(5000);
    CHECK_EQUAL(before, clock_corner());
    fake_hal_set_wall_time((WallTime){2026, 11, 7, 21, 10, 0});
    wait_ms(1100);
    CHECK(clock_corner() != 0);
    CHECK(clock_corner() != before);
}

static void the_idle_screen_comes_after_ten_quiet_seconds_and_a_key_clears_it(void)
{
    start_app_with_motion("/.echo/afk/pokemon/00.bin", NULL);
    choose_idle_screen(THEME_POKEMON);
    wait_ms(9000);
    CHECK(fake_hal_pixel(100, 100) != PAGE_MUSIC);
    wait_ms(1100);
    wait_ms(16);
    CHECK_EQUAL(PAGE_MUSIC, fake_hal_pixel(100, 100));
    tap(KEY_PLAY);
    wait_ms(16);
    CHECK(fake_hal_pixel(100, 100) != PAGE_MUSIC);
    CHECK(!title_is("Music"));
}

static void the_album_grid_opens_an_album_page_that_plays_the_album(void)
{
    start_app_with_library();
    open_setting(SETTINGS_MUSIC_GROUP, SETTINGS_ALBUM_VIEW_INDEX);
    CHECK(title_is("Album view"));
    move_down(1);
    tap(KEY_PLAY);
    restart_app();
    open_music_entry(MUSIC_ALBUMS_INDEX);
    CHECK(title_is("Albums"));
    tap(KEY_PLAY);
    CHECK(title_is("Albums"));
    CHECK_EQUAL(3, app_screen_depth());
    tap(KEY_BACK);
    CHECK(title_is("Albums"));
    CHECK_EQUAL(3, app_screen_depth());
    tap(KEY_PLAY);
    tap(KEY_PLAY);
    CHECK(title_is("Now Playing"));
    CHECK_EQUAL(1, fake_hal_audio_play_count());
}

static void the_music_menu_lists_the_six_ways_in(void)
{
    start_app_with_library();
    open_music_entry(MUSIC_SONGS_INDEX);
    CHECK(title_is("All Songs"));
    tap(KEY_BACK);
    move_up(1);
    tap(KEY_PLAY);
    CHECK(title_is("Genres"));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_notice_is_drawn_at_once_over_the_current_screen),
        TEST_CASE(usb_notices_show_a_cassette),
        TEST_CASE(a_notice_turns_the_display_on),
        TEST_CASE(waking_keeps_the_screen_on_for_a_full_timeout),
        TEST_CASE(a_step_writes_one_full_frame_and_presents_it),
        TEST_CASE(audio_gets_a_turn_after_every_strip),
        TEST_CASE(bluetooth_gets_a_turn_after_every_strip),
        TEST_CASE(an_unchanged_frame_sends_no_pixels),
        TEST_CASE(moving_the_selection_sends_only_part_of_the_screen),
        TEST_CASE(restarting_the_app_sends_a_whole_frame_again),
        TEST_CASE(the_home_menu_is_the_root),
        TEST_CASE(selecting_music_opens_the_music_menu),
        TEST_CASE(back_returns_to_the_previous_screen),
        TEST_CASE(a_long_back_returns_home_from_deep_screens),
        TEST_CASE(the_frame_overlay_is_off_until_enabled_in_diagnostics),
        TEST_CASE(the_frame_overlay_splits_the_frame_time),
        TEST_CASE(the_frame_overlay_splits_the_drawing),
        TEST_CASE(the_frame_overlay_shows_the_audio_numbers),
        TEST_CASE(the_frame_overlay_shows_average_and_longest_block_times),
        TEST_CASE(the_frame_overlay_shows_zero_before_any_block),
        TEST_CASE(diagnostics_shows_the_firmware_version_from_the_hal),
        TEST_CASE(albums_open_from_the_library),
        TEST_CASE(a_cue_album_lists_its_cue_tracks_and_each_plays_its_own_part),
        TEST_CASE(album_and_track_rows_show_no_chevron),
        TEST_CASE(an_artist_leads_to_their_albums),
        TEST_CASE(back_returns_to_the_previous_list),
        TEST_CASE(without_a_library_the_lists_explain_what_is_missing),
        TEST_CASE(selecting_a_track_plays_it_and_shows_now_playing),
        TEST_CASE(play_on_the_playing_track_opens_now_playing_without_restarting),
        TEST_CASE(play_on_the_paused_track_resumes_it_without_restarting),
        TEST_CASE(play_on_the_song_restored_by_a_restart_of_the_app_plays_it),
        TEST_CASE(play_on_another_track_starts_it),
        TEST_CASE(menu_opens_now_playing_from_anywhere_while_playing),
        TEST_CASE(idling_in_the_menus_stays_put_while_playing),
        TEST_CASE(the_key_test_stays_open_after_waking),
        TEST_CASE(menu_stays_in_the_key_test_while_playing),
        TEST_CASE(waking_the_screen_shows_now_playing_while_playing),
        TEST_CASE(waking_while_paused_keeps_the_screen),
        TEST_CASE(the_cpu_clock_cycles_through_24_96_and_200_mhz),
        TEST_CASE(the_test_tone_plays_once_per_press),
        TEST_CASE(the_dma_tone_starts_and_stops_from_one_row),
        TEST_CASE(the_high_rate_tone_plays_at_192_khz),
        TEST_CASE(the_176_khz_tone_plays_at_176_4_khz),
        TEST_CASE(the_dma_tone_plays_at_44_1_khz),
        TEST_CASE(the_stall_test_row_freezes_the_main_loop_on_purpose),
        TEST_CASE(the_watchdog_test_row_stops_everything_on_purpose),
        TEST_CASE(ignored_power_taps_leave_the_screen_on_but_a_hold_still_switches_off),
        TEST_CASE(the_loop_benchmark_runs_once_per_press),
        TEST_CASE(the_core_1_test_runs_once_per_press),
        TEST_CASE(the_accelerator_test_runs_once_per_press),
        TEST_CASE(a_clock_that_does_not_lock_stays_where_it_was),
        TEST_CASE(the_stopwatch_stays_open_after_waking),
        TEST_CASE(menu_does_nothing_while_nothing_plays),
        TEST_CASE(now_playing_shows_the_album_cover),
        TEST_CASE(now_playing_without_a_cover_shows_a_plain_placeholder),
        TEST_CASE(now_playing_keys_control_the_player),
        TEST_CASE(lists_still_move_as_soon_as_a_volume_key_goes_down),
        TEST_CASE(home_art_shows_a_library_cover_when_idle),
        TEST_CASE(home_art_moves_on_and_skips_albums_without_a_cover),
        TEST_CASE(home_art_keeps_the_playing_album),
        TEST_CASE(home_art_can_show_the_theme_emblem_instead_of_covers),
        TEST_CASE(a_new_theme_applies_at_once_and_keeps_settings_open),
        TEST_CASE(the_theme_survives_the_battery_running_out),
        TEST_CASE(a_torn_settings_write_falls_back_to_the_backup),
        TEST_CASE(the_home_menu_lists_music_books_bluetooth_and_settings),
        TEST_CASE(quran_shows_on_home_only_when_a_quran_folder_exists),
        TEST_CASE(podcasts_can_get_their_own_home_row),
        TEST_CASE(the_music_menu_lists_the_six_ways_in),
        TEST_CASE(the_album_grid_opens_an_album_page_that_plays_the_album),
        TEST_CASE(a_home_page_on_the_card_fills_the_screen),
        TEST_CASE(a_still_home_page_is_not_sent_again),
        TEST_CASE(a_still_list_is_not_sent_again),
        TEST_CASE(a_home_page_of_the_wrong_size_is_ignored),
        TEST_CASE(the_boot_frames_play_in_turn_then_the_home_screen),
        TEST_CASE(a_key_skips_the_boot_frames_and_goes_nowhere),
        TEST_CASE(the_idle_screen_comes_after_ten_quiet_seconds_and_a_key_clears_it),
        TEST_CASE(without_the_idle_screen_choice_no_idle_screen_comes),
        TEST_CASE(the_idle_screen_stays_on_instead_of_the_screen_turning_off),
        TEST_CASE(the_idle_screen_shows_the_playing_track),
        TEST_CASE(the_idle_screen_shows_the_clock_and_it_keeps_time),
        TEST_CASE(settings_choose_shuffle_and_repeat),
        TEST_CASE(brightness_is_applied_and_saved),
        TEST_CASE(gain_is_applied),
        TEST_CASE(sound_settings_reach_the_audio_path_and_survive_a_restart),
        TEST_CASE(the_volume_limit_caps_the_volume_keys),
        TEST_CASE(the_theme_cassette_plays_until_cover_style_is_chosen),
        TEST_CASE(the_theme_volume_art_shows_while_the_volume_changes),
        TEST_CASE(files_browses_the_folders_on_the_card),
        TEST_CASE(list_rows_show_the_card_icons),
        TEST_CASE(the_theme_goodbye_art_shows_when_switching_off),
        TEST_CASE(swapped_volume_keys_turn_up_with_the_down_key),
        TEST_CASE(saved_settings_are_restored_at_start),
        TEST_CASE(a_new_screen_appears_at_once),
        TEST_CASE(going_back_shows_the_previous_screen_at_once),
        TEST_CASE(waking_draws_a_frame_before_the_backlight_comes_on),
        TEST_CASE(the_clock_never_changes_with_the_screen_while_audio_is_up),
        TEST_CASE(with_the_screen_off_the_cpu_drops_to_96_mhz_once_audio_is_down),
        TEST_CASE(the_player_turns_itself_off_after_five_idle_minutes_on_battery),
        TEST_CASE(playing_or_usb_power_keeps_the_player_on),
        TEST_CASE(the_screen_turns_off_after_the_timeout),
        TEST_CASE(after_a_timeout_volume_changes_without_waking),
        TEST_CASE(the_timeout_setting_is_used),
        TEST_CASE(with_the_screen_off_media_keys_keep_working),
        TEST_CASE(holding_power_saves_and_turns_the_player_off),
        TEST_CASE(a_one_second_power_hold_only_turns_the_screen_off),
        TEST_CASE(a_slightly_long_press_wakes_the_screen_without_switching_off),
        TEST_CASE(a_volume_hold_under_a_second_changes_the_volume),
        TEST_CASE(a_tap_then_a_hold_keeps_changing_the_volume_but_a_lone_hold_skips),
        TEST_CASE(keys_are_ignored_while_saying_goodbye),
        TEST_CASE(the_splash_ignores_keys_until_it_ends),
        TEST_CASE(the_splash_draws_over_the_home_screen),
        TEST_CASE(the_sleep_timer_turns_the_player_off),
        TEST_CASE(a_refused_track_says_why_it_was_skipped),
        TEST_CASE(rescanning_without_a_card_says_so),
        TEST_CASE(a_rescan_of_an_empty_card_writes_an_empty_library),
        TEST_CASE(a_rescan_keeps_the_stall_watch_fed),
        TEST_CASE(a_rescan_rebuilds_the_covers_file),
        TEST_CASE(repeated_rescans_do_not_leave_files_open),
        TEST_CASE(a_music_check_does_not_leave_the_library_open_twice),
        TEST_CASE(rescans_that_fail_do_not_leave_files_open),
        TEST_CASE(a_rescan_while_a_song_plays_then_more_rescans_keep_the_same_files_open),
        TEST_CASE(a_rescan_after_an_installed_update_asks_for_a_restart),
        TEST_CASE(after_an_installed_update_screens_use_the_resident_font),
        TEST_CASE(a_rescan_without_scratch_memory_says_so),
        TEST_CASE(a_rescan_borrows_the_idle_audio_buffer_for_its_listing),
        TEST_CASE(each_way_a_scan_runs_out_has_its_own_message),
        TEST_CASE(the_music_check_after_an_installed_update_skips_the_walk),
        TEST_CASE(a_fresh_card_gets_the_player_files_and_a_first_scan),
        TEST_CASE(starting_makes_no_files_before_the_first_step),
        TEST_CASE(each_big_file_is_made_in_its_own_step_after_the_screen_is_lit),
        TEST_CASE(a_card_with_the_files_is_prepared_within_the_first_step),
        TEST_CASE(a_card_that_has_the_files_is_not_rescanned),
        TEST_CASE(unchanged_music_is_not_rescanned_after_usb),
        TEST_CASE(a_different_stored_fingerprint_is_rescanned_after_usb),
        TEST_CASE(a_new_firmware_checks_the_music_once_without_rescanning),
        TEST_CASE(the_music_check_after_usb_says_so_before_walking_the_card),
        TEST_CASE(a_new_firmware_rescans_music_that_changed),
        TEST_CASE(a_new_firmware_rebuilds_a_library_it_cannot_read),
        TEST_CASE(a_song_added_over_usb_is_rescanned_after_eject),
        TEST_CASE(a_card_that_cannot_be_walked_is_rescanned_after_usb),
        TEST_CASE(a_full_card_says_it_is_full),
        TEST_CASE(an_empty_battery_saves_and_turns_the_player_off),
        TEST_CASE(an_empty_battery_on_usb_power_keeps_playing),
        TEST_CASE(switching_off_on_usb_power_shows_the_charge_screen),
        TEST_CASE(the_charge_screen_goes_dark_and_a_key_lights_it),
        TEST_CASE(holding_power_on_the_charge_screen_turns_the_player_on),
        TEST_CASE(unplugging_on_the_charge_screen_turns_the_player_off),
        TEST_CASE(a_cable_start_opens_the_charge_screen),
        TEST_CASE(holding_play_on_an_album_adds_it_to_favorites),
        TEST_CASE(a_paused_song_is_not_sent_again_until_it_moves),
        TEST_CASE(menu_on_now_playing_opens_up_next),
        TEST_CASE(choosing_a_song_in_up_next_plays_it),
        TEST_CASE(holding_play_on_a_song_offers_up_next),
        TEST_CASE(play_next_from_a_list_queues_the_song),
        TEST_CASE(add_to_up_next_from_a_list_queues_the_song),
        TEST_CASE(play_next_with_nothing_playing_starts_the_song),
        TEST_CASE(holding_play_in_favorites_removes_the_album),
        TEST_CASE(favorites_survive_a_restart),
        TEST_CASE(a_toast_disappears_after_a_while),
        TEST_CASE(a_song_can_be_added_to_favorites),
        TEST_CASE(a_favorite_song_can_be_removed),
        TEST_CASE(playlists_open_from_the_music_menu),
        TEST_CASE(favorites_are_pinned_at_the_top_of_playlists),
        TEST_CASE(the_start_screen_setting_opens_albums),
        TEST_CASE(the_start_screen_setting_opens_now_playing),
        TEST_CASE(the_header_shows_the_play_state_and_battery),
        TEST_CASE(stopping_the_app_saves_the_session),
        TEST_CASE(each_step_records_its_duration),
        TEST_CASE(steps_inside_one_redraw_period_present_once),
        TEST_CASE(a_key_press_redraws_at_once),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
