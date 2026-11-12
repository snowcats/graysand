#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "playback/play_queue.h"

typedef enum GainLevel {
    GAIN_LOW,
    GAIN_HIGH
} GainLevel;

typedef enum OutputJack {
    OUTPUT_AUTO,
    OUTPUT_SINGLE_ENDED,
    OUTPUT_BALANCED
} OutputJack;

typedef enum StartScreen {
    START_MAIN_MENU,
    START_NOW_PLAYING,
    START_ALBUMS,
    START_ARTISTS,
    START_FAVORITES,
    START_PLAYLISTS,
    START_COVER_FLOW
} StartScreen;

typedef enum ThemeChoice {
    THEME_PODIUM,
    THEME_POKEMON,
    THEME_ZELDA,
    THEME_LAIN,
    THEME_NOAH
} ThemeChoice;

typedef enum PlayerStyle {
    PLAYER_STYLE_COVER,
    PLAYER_STYLE_CASSETTE,
    PLAYER_STYLE_CASSETTE_MINIMAL,
    PLAYER_STYLE_COUNT
} PlayerStyle;

typedef enum AlbumView {
    ALBUM_VIEW_LIST,
    ALBUM_VIEW_GRID,
    ALBUM_VIEW_COUNT
} AlbumView;

typedef enum HomeArtMode {
    HOME_ART_COVERS,
    HOME_ART_EMBLEM
} HomeArtMode;

typedef enum UsbMode {
    USB_MODE_STORAGE,
    USB_MODE_DAC
} UsbMode;

enum {
    GAIN_LEVEL_COUNT = GAIN_HIGH + 1,
    OUTPUT_JACK_COUNT = OUTPUT_BALANCED + 1,
    START_SCREEN_COUNT = START_COVER_FLOW + 1,
    USB_MODE_COUNT = USB_MODE_DAC + 1,
    THEME_CHOICE_COUNT = THEME_NOAH + 1,
    HOME_ART_MODE_COUNT = HOME_ART_EMBLEM + 1,
    SETTINGS_BRIGHTNESS_MIN = 1,
    SETTINGS_BRIGHTNESS_MAX = 10,
    SETTINGS_VOLUME_MAX = 50,
    SETTINGS_DEFAULT_DAC_VOLUME = 30,
    SCREEN_TIMEOUT_IDLE_SCREEN = 7,
    SCREEN_TIMEOUT_OPTION_COUNT = 8,
    SLEEP_TIMER_OPTION_COUNT = 7,
    AUTO_OFF_OPTION_COUNT = 4,
    VOLUME_LIMIT_OPTION_COUNT = 4,
    SKIP_OPTION_COUNT = 11,
    SKIP_SECONDS_MIN = 5,
    BALANCE_OPTION_COUNT = 11,
    BALANCE_CENTER = 5,
    EQUALIZER_OPTION_COUNT = 7,
    DAC_FILTER_OPTION_COUNT = 5,
    SETTINGS_FILE_SIZE = 32
};

typedef struct Settings {
    uint8_t brightness;
    uint8_t screen_timeout;
    GainLevel gain;
    OutputJack output;
    StartScreen start_screen;
    ShuffleMode shuffle;
    RepeatMode repeat;
    uint8_t volume;
    uint8_t auto_off;
    UsbMode usb_mode;
    uint8_t dac_volume;
    ThemeChoice theme;
    HomeArtMode home_art;
    bool podcasts_separate;
    uint8_t volume_limit;
    uint8_t balance;
    bool swap_volume_keys;
    uint8_t equalizer;
    uint8_t dac_filter;
    bool lyrics;
    uint8_t skip_seconds;
    uint8_t player_style;
    uint8_t album_view;
    bool format_tag;
    bool vertical;
} Settings;

Settings settings_defaults(void);
uint32_t settings_timeout_option_seconds(int option);
uint32_t settings_sleep_option_minutes(int option);
uint32_t settings_screen_timeout_ms(const Settings *settings);
bool settings_shows_idle_screen(const Settings *settings);
uint32_t settings_auto_off_option_minutes(int option);
uint32_t settings_auto_off_ms(const Settings *settings);
uint8_t settings_volume_limit(const Settings *settings);
uint32_t settings_skip_ms(const Settings *settings);
void settings_encode(const Settings *settings, uint8_t *bytes);
bool settings_decode(const uint8_t *bytes, uint32_t length, Settings *settings);
