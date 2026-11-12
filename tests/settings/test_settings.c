#include "check.h"
#include "core/hash.h"
#include "settings/settings.h"

enum {
    VERSION_FIELD = 4,
    BRIGHTNESS_FIELD = 6,
    AUTO_OFF_FIELD = 14,
    USB_MODE_FIELD = 15,
    DAC_VOLUME_FIELD = 16,
    THEME_FIELD = 17,
    HOME_ART_FIELD = 18,
    PODCASTS_FIELD = 19,
    CHECKSUM_FIELD = 20,
    VOLUME_LIMIT_FIELD = 24,
    LYRICS_FIELD = 29,
    SKIP_FIELD = 30,
    PLAYER_STYLE_FIELD = 31,
    OLD_CLOCK_FIELD = 31,
    OLD_FILE_SIZE = 32,
    NEVER_OPTION = 6
};

static Settings custom_settings(void)
{
    Settings settings = settings_defaults();
    settings.brightness = 3;
    settings.screen_timeout = 4;
    settings.gain = GAIN_HIGH;
    settings.output = OUTPUT_BALANCED;
    settings.start_screen = START_FAVORITES;
    settings.shuffle = SHUFFLE_ALBUMS;
    settings.repeat = REPEAT_ONE;
    settings.volume = 42;
    settings.auto_off = 3;
    settings.usb_mode = USB_MODE_DAC;
    settings.dac_volume = 37;
    settings.theme = THEME_LAIN;
    settings.home_art = HOME_ART_EMBLEM;
    settings.podcasts_separate = true;
    settings.volume_limit = 2;
    settings.balance = 8;
    settings.swap_volume_keys = true;
    settings.equalizer = 3;
    settings.dac_filter = 4;
    settings.lyrics = false;
    settings.skip_seconds = 2;
    settings.player_style = PLAYER_STYLE_CASSETTE_MINIMAL;
    settings.album_view = ALBUM_VIEW_GRID;
    settings.format_tag = true;
    settings.vertical = true;
    return settings;
}

static void rewrite_checksum(uint8_t *bytes)
{
    uint32_t checksum = hash_update(HASH_START, bytes, CHECKSUM_FIELD);
    if (bytes[VERSION_FIELD] >= 6) {
        checksum = hash_update(checksum, bytes + VOLUME_LIMIT_FIELD,
                               SETTINGS_FILE_SIZE - VOLUME_LIMIT_FIELD);
    }
    for (int index = 0; index < 4; index++) {
        bytes[CHECKSUM_FIELD + index] = (uint8_t)(checksum >> (8 * index));
    }
}

static void defaults_are_comfortable(void)
{
    Settings settings = settings_defaults();
    CHECK_EQUAL(7, settings.brightness);
    CHECK_EQUAL(30000, settings_screen_timeout_ms(&settings));
    CHECK_EQUAL(GAIN_LOW, settings.gain);
    CHECK_EQUAL(OUTPUT_AUTO, settings.output);
    CHECK_EQUAL(START_MAIN_MENU, settings.start_screen);
    CHECK_EQUAL(SHUFFLE_OFF, settings.shuffle);
    CHECK_EQUAL(REPEAT_OFF, settings.repeat);
    CHECK_EQUAL(12, settings.volume);
    CHECK_EQUAL(5u * 60000u, settings_auto_off_ms(&settings));
    CHECK_EQUAL(USB_MODE_STORAGE, settings.usb_mode);
    CHECK_EQUAL(30, settings.dac_volume);
}

static void auto_off_options_are_stocks(void)
{
    CHECK_EQUAL(0, settings_auto_off_option_minutes(0));
    CHECK_EQUAL(1, settings_auto_off_option_minutes(1));
    CHECK_EQUAL(5, settings_auto_off_option_minutes(2));
    CHECK_EQUAL(10, settings_auto_off_option_minutes(AUTO_OFF_OPTION_COUNT - 1));
}

static void a_version_one_file_keeps_its_values_and_gets_the_default_auto_off(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[VERSION_FIELD] = 1;
    bytes[AUTO_OFF_FIELD] = 0;
    rewrite_checksum(bytes);
    Settings decoded;
    CHECK(settings_decode(bytes, sizeof bytes, &decoded));
    CHECK_EQUAL(42, decoded.volume);
    CHECK_EQUAL(settings_defaults().auto_off, decoded.auto_off);
    CHECK_EQUAL(USB_MODE_STORAGE, decoded.usb_mode);
}

static void a_version_two_file_keeps_its_auto_off_and_uses_usb_storage(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[VERSION_FIELD] = 2;
    bytes[USB_MODE_FIELD] = 0;
    rewrite_checksum(bytes);
    Settings decoded;
    CHECK(settings_decode(bytes, sizeof bytes, &decoded));
    CHECK_EQUAL(3, decoded.auto_off);
    CHECK_EQUAL(USB_MODE_STORAGE, decoded.usb_mode);
}

static void the_usb_mode_and_dac_volume_are_saved_at_bytes_fifteen_and_sixteen(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    CHECK_EQUAL(7, bytes[VERSION_FIELD]);
    CHECK_EQUAL(USB_MODE_DAC, bytes[USB_MODE_FIELD]);
    CHECK_EQUAL(37, bytes[DAC_VOLUME_FIELD]);
}

static void a_version_three_file_keeps_its_usb_mode_and_gets_the_default_dac_volume(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[VERSION_FIELD] = 3;
    bytes[DAC_VOLUME_FIELD] = 0;
    rewrite_checksum(bytes);
    Settings decoded;
    CHECK(settings_decode(bytes, sizeof bytes, &decoded));
    CHECK_EQUAL(USB_MODE_DAC, decoded.usb_mode);
    CHECK_EQUAL(SETTINGS_DEFAULT_DAC_VOLUME, decoded.dac_volume);
}

static void a_dac_volume_above_the_top_is_rejected(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[DAC_VOLUME_FIELD] = SETTINGS_VOLUME_MAX + 1;
    rewrite_checksum(bytes);
    CHECK(!settings_decode(bytes, sizeof bytes, &settings));
}

static void an_unknown_usb_mode_is_rejected(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[USB_MODE_FIELD] = USB_MODE_COUNT;
    rewrite_checksum(bytes);
    CHECK(!settings_decode(bytes, sizeof bytes, &settings));
}

static void a_version_four_file_keeps_its_dac_volume_and_gets_the_default_theme(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[VERSION_FIELD] = 4;
    bytes[THEME_FIELD] = 0;
    bytes[HOME_ART_FIELD] = 0;
    rewrite_checksum(bytes);
    Settings decoded;
    CHECK(settings_decode(bytes, sizeof bytes, &decoded));
    CHECK_EQUAL(37, decoded.dac_volume);
    CHECK_EQUAL(THEME_PODIUM, decoded.theme);
    CHECK_EQUAL(HOME_ART_COVERS, decoded.home_art);
}

static void an_unknown_theme_is_rejected(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[THEME_FIELD] = THEME_CHOICE_COUNT;
    rewrite_checksum(bytes);
    CHECK(!settings_decode(bytes, sizeof bytes, &settings));
}

static void an_unknown_home_art_is_rejected(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[HOME_ART_FIELD] = HOME_ART_MODE_COUNT;
    rewrite_checksum(bytes);
    CHECK(!settings_decode(bytes, sizeof bytes, &settings));
}

static void podcasts_sit_in_byte_nineteen_and_older_files_keep_them_with_books(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    CHECK_EQUAL(7, bytes[VERSION_FIELD]);
    CHECK_EQUAL(1, bytes[PODCASTS_FIELD]);
    bytes[PODCASTS_FIELD] = 0;
    rewrite_checksum(bytes);
    Settings decoded;
    CHECK(settings_decode(bytes, sizeof bytes, &decoded));
    CHECK(!decoded.podcasts_separate);
    CHECK_EQUAL(THEME_LAIN, decoded.theme);
    bytes[PODCASTS_FIELD] = 2;
    rewrite_checksum(bytes);
    CHECK(!settings_decode(bytes, sizeof bytes, &decoded));
}

static void a_newer_version_is_rejected(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[VERSION_FIELD] = 8;
    rewrite_checksum(bytes);
    CHECK(!settings_decode(bytes, sizeof bytes, &settings));
}

static void timeout_options_end_with_never(void)
{
    CHECK_EQUAL(10, settings_timeout_option_seconds(0));
    CHECK_EQUAL(300, settings_timeout_option_seconds(5));
    CHECK_EQUAL(0, settings_timeout_option_seconds(NEVER_OPTION));
    Settings settings = settings_defaults();
    settings.screen_timeout = NEVER_OPTION;
    CHECK_EQUAL(0, settings_screen_timeout_ms(&settings));
}

static void the_idle_screen_choice_comes_last_and_keeps_the_screen_on(void)
{
    Settings settings = settings_defaults();
    CHECK(!settings_shows_idle_screen(&settings));
    settings.screen_timeout = SCREEN_TIMEOUT_IDLE_SCREEN;
    CHECK(settings_shows_idle_screen(&settings));
    CHECK_EQUAL(0, settings_screen_timeout_ms(&settings));
    CHECK_EQUAL(SCREEN_TIMEOUT_OPTION_COUNT - 1, SCREEN_TIMEOUT_IDLE_SCREEN);
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    Settings decoded;
    CHECK(settings_decode(bytes, sizeof bytes, &decoded));
    CHECK(settings_shows_idle_screen(&decoded));
}

static void sleep_options_start_with_off(void)
{
    CHECK_EQUAL(0, settings_sleep_option_minutes(0));
    CHECK_EQUAL(15, settings_sleep_option_minutes(1));
    CHECK_EQUAL(120, settings_sleep_option_minutes(SLEEP_TIMER_OPTION_COUNT - 1));
}

static void settings_survive_a_round_trip(void)
{
    Settings original = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    Settings decoded;
    settings_encode(&original, bytes);
    CHECK(settings_decode(bytes, sizeof bytes, &decoded));
    CHECK_EQUAL(original.brightness, decoded.brightness);
    CHECK_EQUAL(original.player_style, decoded.player_style);
    CHECK_EQUAL(original.screen_timeout, decoded.screen_timeout);
    CHECK_EQUAL(original.gain, decoded.gain);
    CHECK_EQUAL(original.output, decoded.output);
    CHECK_EQUAL(original.start_screen, decoded.start_screen);
    CHECK_EQUAL(original.shuffle, decoded.shuffle);
    CHECK_EQUAL(original.repeat, decoded.repeat);
    CHECK_EQUAL(original.volume, decoded.volume);
    CHECK_EQUAL(original.auto_off, decoded.auto_off);
    CHECK_EQUAL(original.usb_mode, decoded.usb_mode);
    CHECK_EQUAL(original.dac_volume, decoded.dac_volume);
    CHECK_EQUAL(original.theme, decoded.theme);
    CHECK_EQUAL(original.home_art, decoded.home_art);
    CHECK(decoded.podcasts_separate);
    CHECK_EQUAL(30, settings_volume_limit(&decoded));
    CHECK_EQUAL(8, decoded.balance);
    CHECK(decoded.swap_volume_keys);
    CHECK_EQUAL(3, decoded.equalizer);
    CHECK_EQUAL(4, decoded.dac_filter);
    CHECK(!decoded.lyrics);
    CHECK_EQUAL(7000, settings_skip_ms(&decoded));
    CHECK_EQUAL(ALBUM_VIEW_GRID, decoded.album_view);
    CHECK(decoded.format_tag);
    CHECK(decoded.vertical);
}

static void a_version_six_file_keeps_its_player_style_and_gets_the_new_defaults(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    Settings decoded;
    for (uint8_t old = 0; old < 2; old++) {
        settings_encode(&settings, bytes);
        bytes[VERSION_FIELD] = 6;
        bytes[PLAYER_STYLE_FIELD] = old;
        rewrite_checksum(bytes);
        CHECK(settings_decode(bytes, sizeof bytes, &decoded));
        CHECK_EQUAL(old == 0 ? PLAYER_STYLE_CASSETTE : PLAYER_STYLE_COVER, decoded.player_style);
        CHECK_EQUAL(ALBUM_VIEW_LIST, decoded.album_view);
        CHECK(!decoded.format_tag);
        CHECK(!decoded.vertical);
    }
}

static void music_settings_default_to_no_limit_centred_and_lyrics_on(void)
{
    Settings settings = settings_defaults();
    CHECK_EQUAL(50, settings_volume_limit(&settings));
    CHECK_EQUAL(BALANCE_CENTER, settings.balance);
    CHECK(!settings.swap_volume_keys);
    CHECK_EQUAL(0, settings.equalizer);
    CHECK_EQUAL(0, settings.dac_filter);
    CHECK(settings.lyrics);
    CHECK_EQUAL(5000, settings_skip_ms(&settings));
    CHECK_EQUAL(PLAYER_STYLE_CASSETTE, settings.player_style);
    CHECK_EQUAL(ALBUM_VIEW_LIST, settings.album_view);
    CHECK(!settings.format_tag);
    CHECK(!settings.vertical);
}

static void a_version_five_file_keeps_its_values_and_gets_the_music_defaults(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[VERSION_FIELD] = 5;
    for (int index = CHECKSUM_FIELD; index < SETTINGS_FILE_SIZE; index++) {
        bytes[index] = 0;
    }
    rewrite_checksum(bytes);
    Settings decoded;
    CHECK(settings_decode(bytes, sizeof bytes, &decoded));
    CHECK_EQUAL(THEME_LAIN, decoded.theme);
    CHECK_EQUAL(50, settings_volume_limit(&decoded));
    CHECK_EQUAL(BALANCE_CENTER, decoded.balance);
    CHECK(decoded.lyrics);
    CHECK_EQUAL(5000, settings_skip_ms(&decoded));
}

static void an_old_clock_byte_still_decodes(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[OLD_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[OLD_CLOCK_FIELD] = 1;
    uint32_t checksum = hash_update(HASH_START, bytes, CHECKSUM_FIELD);
    checksum =
        hash_update(checksum, bytes + VOLUME_LIMIT_FIELD, OLD_FILE_SIZE - VOLUME_LIMIT_FIELD);
    for (int index = 0; index < 4; index++) {
        bytes[CHECKSUM_FIELD + index] = (uint8_t)(checksum >> (8 * index));
    }
    Settings decoded;
    CHECK(settings_decode(bytes, sizeof bytes, &decoded));
    CHECK_EQUAL(42, decoded.volume);
    CHECK_EQUAL(THEME_LAIN, decoded.theme);
}

static void an_unknown_music_setting_is_rejected(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[LYRICS_FIELD] = 2;
    rewrite_checksum(bytes);
    CHECK(!settings_decode(bytes, sizeof bytes, &settings));
    settings_encode(&settings, bytes);
    bytes[VOLUME_LIMIT_FIELD] = VOLUME_LIMIT_OPTION_COUNT;
    rewrite_checksum(bytes);
    CHECK(!settings_decode(bytes, sizeof bytes, &settings));
    settings_encode(&settings, bytes);
    bytes[SKIP_FIELD] = SKIP_OPTION_COUNT;
    rewrite_checksum(bytes);
    CHECK(!settings_decode(bytes, sizeof bytes, &settings));
    settings_encode(&settings, bytes);
    bytes[PLAYER_STYLE_FIELD] = PLAYER_STYLE_COUNT;
    rewrite_checksum(bytes);
    CHECK(!settings_decode(bytes, sizeof bytes, &settings));
    settings_encode(&settings, bytes);
    bytes[PLAYER_STYLE_FIELD] = 0x20;
    rewrite_checksum(bytes);
    CHECK(!settings_decode(bytes, sizeof bytes, &settings));
}

static void a_corrupted_file_is_rejected(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[BRIGHTNESS_FIELD] ^= 0x02;
    CHECK(!settings_decode(bytes, sizeof bytes, &settings));
    CHECK(!settings_decode(bytes, SETTINGS_FILE_SIZE - 1, &settings));
}

static void out_of_range_values_are_rejected(void)
{
    Settings settings = custom_settings();
    uint8_t bytes[SETTINGS_FILE_SIZE];
    settings_encode(&settings, bytes);
    bytes[BRIGHTNESS_FIELD] = SETTINGS_BRIGHTNESS_MAX + 1;
    rewrite_checksum(bytes);
    CHECK(!settings_decode(bytes, sizeof bytes, &settings));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(defaults_are_comfortable),
        TEST_CASE(timeout_options_end_with_never),
        TEST_CASE(the_idle_screen_choice_comes_last_and_keeps_the_screen_on),
        TEST_CASE(sleep_options_start_with_off),
        TEST_CASE(settings_survive_a_round_trip),
        TEST_CASE(a_version_six_file_keeps_its_player_style_and_gets_the_new_defaults),
        TEST_CASE(a_corrupted_file_is_rejected),
        TEST_CASE(out_of_range_values_are_rejected),
        TEST_CASE(auto_off_options_are_stocks),
        TEST_CASE(a_version_one_file_keeps_its_values_and_gets_the_default_auto_off),
        TEST_CASE(a_version_two_file_keeps_its_auto_off_and_uses_usb_storage),
        TEST_CASE(the_usb_mode_and_dac_volume_are_saved_at_bytes_fifteen_and_sixteen),
        TEST_CASE(a_version_three_file_keeps_its_usb_mode_and_gets_the_default_dac_volume),
        TEST_CASE(a_dac_volume_above_the_top_is_rejected),
        TEST_CASE(an_unknown_usb_mode_is_rejected),
        TEST_CASE(a_version_four_file_keeps_its_dac_volume_and_gets_the_default_theme),
        TEST_CASE(an_unknown_theme_is_rejected),
        TEST_CASE(an_unknown_home_art_is_rejected),
        TEST_CASE(a_newer_version_is_rejected),
        TEST_CASE(podcasts_sit_in_byte_nineteen_and_older_files_keep_them_with_books),
        TEST_CASE(music_settings_default_to_no_limit_centred_and_lyrics_on),
        TEST_CASE(a_version_five_file_keeps_its_values_and_gets_the_music_defaults),
        TEST_CASE(an_old_clock_byte_still_decodes),
        TEST_CASE(an_unknown_music_setting_is_rejected),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
