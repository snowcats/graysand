#include "settings/settings.h"

#include "core/hash.h"

enum {
    VERSION = 7,
    FIRST_VERSION = 1,
    AUTO_OFF_VERSION = 2,
    USB_MODE_VERSION = 3,
    DAC_VOLUME_VERSION = 4,
    MUSIC_VERSION = 6,
    LOOK_VERSION = 7,
    STYLE_BITS = 3,
    ALBUM_VIEW_BIT = 2,
    FORMAT_TAG_BIT = 3,
    VERTICAL_BIT = 4,
    LOOK_BITS = 0x1F,
    MAGIC_SIZE = 4,
    VERSION_FIELD = 4,
    BRIGHTNESS_FIELD = 6,
    TIMEOUT_FIELD = 7,
    GAIN_FIELD = 8,
    OUTPUT_FIELD = 9,
    START_FIELD = 10,
    SHUFFLE_FIELD = 11,
    REPEAT_FIELD = 12,
    VOLUME_FIELD = 13,
    AUTO_OFF_FIELD = 14,
    USB_MODE_FIELD = 15,
    DAC_VOLUME_FIELD = 16,
    THEME_FIELD = 17,
    HOME_ART_FIELD = 18,
    PODCASTS_FIELD = 19,
    CHECKSUM_FIELD = 20,
    VOLUME_LIMIT_FIELD = 24,
    BALANCE_FIELD = 25,
    SWAP_KEYS_FIELD = 26,
    EQUALIZER_FIELD = 27,
    DAC_FILTER_FIELD = 28,
    LYRICS_FIELD = 29,
    SKIP_FIELD = 30,
    PLAYER_STYLE_FIELD = 31,
    MUSIC_FIELDS = SETTINGS_FILE_SIZE - VOLUME_LIMIT_FIELD,
    DEFAULT_BRIGHTNESS = 7,
    DEFAULT_TIMEOUT_OPTION = 2,
    DEFAULT_VOLUME = 12,
    DEFAULT_AUTO_OFF_OPTION = 2,
    MILLISECONDS_PER_MINUTE = 60000,
    MILLISECONDS_PER_SECOND = 1000,
    BITS_PER_BYTE = 8
};

static const uint8_t MAGIC[MAGIC_SIZE] = {'E', 'M', 'S', 'T'};
static const uint16_t TIMEOUT_SECONDS[SCREEN_TIMEOUT_OPTION_COUNT] = {10,  20,  30, 60,
                                                                      120, 300, 0,  0};
static const uint8_t SLEEP_MINUTES[SLEEP_TIMER_OPTION_COUNT] = {0, 15, 30, 45, 60, 90, 120};
static const uint8_t AUTO_OFF_MINUTES[AUTO_OFF_OPTION_COUNT] = {0, 1, 5, 10};
static const uint8_t VOLUME_LIMITS[VOLUME_LIMIT_OPTION_COUNT] = {SETTINGS_VOLUME_MAX, 40, 30, 20};

Settings settings_defaults(void)
{
    return (Settings){
        .brightness = DEFAULT_BRIGHTNESS,
        .screen_timeout = DEFAULT_TIMEOUT_OPTION,
        .gain = GAIN_LOW,
        .output = OUTPUT_AUTO,
        .start_screen = START_MAIN_MENU,
        .shuffle = SHUFFLE_OFF,
        .repeat = REPEAT_OFF,
        .volume = DEFAULT_VOLUME,
        .auto_off = DEFAULT_AUTO_OFF_OPTION,
        .usb_mode = USB_MODE_STORAGE,
        .dac_volume = SETTINGS_DEFAULT_DAC_VOLUME,
        .theme = THEME_PODIUM,
        .home_art = HOME_ART_COVERS,
        .balance = BALANCE_CENTER,
        .lyrics = true,
        .player_style = PLAYER_STYLE_CASSETTE,
    };
}

static uint8_t look_byte(const Settings *settings)
{
    unsigned look = (unsigned)settings->player_style | (unsigned)settings->album_view
                                                           << ALBUM_VIEW_BIT;
    look |= (settings->format_tag ? 1u : 0u) << FORMAT_TAG_BIT;
    return (uint8_t)(look | (settings->vertical ? 1u : 0u) << VERTICAL_BIT);
}

static bool look_valid(const uint8_t *bytes)
{
    uint8_t look = bytes[PLAYER_STYLE_FIELD];
    if (bytes[VERSION_FIELD] < LOOK_VERSION) {
        return look <= 1;
    }
    return (look & STYLE_BITS) < PLAYER_STYLE_COUNT && (look & ~(unsigned)LOOK_BITS) == 0;
}

static void decode_look(const uint8_t *bytes, Settings *settings)
{
    uint8_t look = bytes[PLAYER_STYLE_FIELD];
    if (bytes[VERSION_FIELD] < LOOK_VERSION) {
        settings->player_style = look == 0 ? PLAYER_STYLE_CASSETTE : PLAYER_STYLE_COVER;
        return;
    }
    settings->player_style = (uint8_t)(look & STYLE_BITS);
    settings->album_view = (uint8_t)(look >> ALBUM_VIEW_BIT & 1u);
    settings->format_tag = (look >> FORMAT_TAG_BIT & 1u) == 1;
    settings->vertical = (look >> VERTICAL_BIT & 1u) == 1;
}

uint32_t settings_timeout_option_seconds(int option)
{
    return option >= 0 && option < SCREEN_TIMEOUT_OPTION_COUNT ? TIMEOUT_SECONDS[option] : 0;
}

uint32_t settings_sleep_option_minutes(int option)
{
    return option >= 0 && option < SLEEP_TIMER_OPTION_COUNT ? SLEEP_MINUTES[option] : 0;
}

uint32_t settings_screen_timeout_ms(const Settings *settings)
{
    return settings_timeout_option_seconds(settings->screen_timeout) * MILLISECONDS_PER_SECOND;
}

bool settings_shows_idle_screen(const Settings *settings)
{
    return settings->screen_timeout == SCREEN_TIMEOUT_IDLE_SCREEN;
}

uint32_t settings_auto_off_option_minutes(int option)
{
    return option >= 0 && option < AUTO_OFF_OPTION_COUNT ? AUTO_OFF_MINUTES[option] : 0;
}

uint32_t settings_auto_off_ms(const Settings *settings)
{
    return settings_auto_off_option_minutes(settings->auto_off) * MILLISECONDS_PER_MINUTE;
}

uint8_t settings_volume_limit(const Settings *settings)
{
    return VOLUME_LIMITS[settings->volume_limit];
}

uint32_t settings_skip_ms(const Settings *settings)
{
    uint8_t option = settings->skip_seconds < SKIP_OPTION_COUNT ? settings->skip_seconds : 0;
    return (uint32_t)(SKIP_SECONDS_MIN + option) * MILLISECONDS_PER_SECOND;
}

static uint32_t checksum_of(const uint8_t *bytes)
{
    uint32_t hash = hash_update(HASH_START, bytes, CHECKSUM_FIELD);
    return bytes[VERSION_FIELD] < MUSIC_VERSION
               ? hash
               : hash_update(hash, bytes + VOLUME_LIMIT_FIELD, MUSIC_FIELDS);
}

static void put_u32(uint8_t *bytes, int offset, uint32_t value)
{
    for (int index = 0; index < 4; index++) {
        bytes[offset + index] = (uint8_t)(value >> (BITS_PER_BYTE * index));
    }
}

static uint32_t get_u32(const uint8_t *bytes, int offset)
{
    uint32_t value = 0;
    for (int index = 3; index >= 0; index--) {
        value = value << BITS_PER_BYTE | bytes[offset + index];
    }
    return value;
}

void settings_encode(const Settings *settings, uint8_t *bytes)
{
    for (int index = 0; index < SETTINGS_FILE_SIZE; index++) {
        bytes[index] = index < MAGIC_SIZE ? MAGIC[index] : 0;
    }
    bytes[VERSION_FIELD] = VERSION;
    bytes[BRIGHTNESS_FIELD] = settings->brightness;
    bytes[TIMEOUT_FIELD] = settings->screen_timeout;
    bytes[GAIN_FIELD] = (uint8_t)settings->gain;
    bytes[OUTPUT_FIELD] = (uint8_t)settings->output;
    bytes[START_FIELD] = (uint8_t)settings->start_screen;
    bytes[SHUFFLE_FIELD] = (uint8_t)settings->shuffle;
    bytes[REPEAT_FIELD] = (uint8_t)settings->repeat;
    bytes[VOLUME_FIELD] = settings->volume;
    bytes[AUTO_OFF_FIELD] = settings->auto_off;
    bytes[USB_MODE_FIELD] = (uint8_t)settings->usb_mode;
    bytes[DAC_VOLUME_FIELD] = settings->dac_volume;
    bytes[THEME_FIELD] = (uint8_t)settings->theme;
    bytes[HOME_ART_FIELD] = (uint8_t)settings->home_art;
    bytes[PODCASTS_FIELD] = settings->podcasts_separate ? 1 : 0;
    bytes[VOLUME_LIMIT_FIELD] = settings->volume_limit;
    bytes[BALANCE_FIELD] = settings->balance;
    bytes[SWAP_KEYS_FIELD] = settings->swap_volume_keys ? 1 : 0;
    bytes[EQUALIZER_FIELD] = settings->equalizer;
    bytes[DAC_FILTER_FIELD] = settings->dac_filter;
    bytes[LYRICS_FIELD] = settings->lyrics ? 1 : 0;
    bytes[SKIP_FIELD] = settings->skip_seconds;
    bytes[PLAYER_STYLE_FIELD] = look_byte(settings);
    put_u32(bytes, CHECKSUM_FIELD, checksum_of(bytes));
}

static bool header_valid(const uint8_t *bytes)
{
    for (int index = 0; index < MAGIC_SIZE; index++) {
        if (bytes[index] != MAGIC[index]) {
            return false;
        }
    }
    uint8_t version = bytes[VERSION_FIELD];
    return version >= FIRST_VERSION && version <= VERSION && bytes[VERSION_FIELD + 1] == 0 &&
           get_u32(bytes, CHECKSUM_FIELD) == checksum_of(bytes);
}

static bool values_valid(const uint8_t *bytes)
{
    return bytes[BRIGHTNESS_FIELD] >= SETTINGS_BRIGHTNESS_MIN &&
           bytes[BRIGHTNESS_FIELD] <= SETTINGS_BRIGHTNESS_MAX &&
           bytes[TIMEOUT_FIELD] < SCREEN_TIMEOUT_OPTION_COUNT &&
           bytes[GAIN_FIELD] < GAIN_LEVEL_COUNT && bytes[OUTPUT_FIELD] < OUTPUT_JACK_COUNT &&
           bytes[START_FIELD] < START_SCREEN_COUNT && bytes[SHUFFLE_FIELD] < SHUFFLE_MODE_COUNT &&
           bytes[REPEAT_FIELD] < REPEAT_MODE_COUNT && bytes[VOLUME_FIELD] <= SETTINGS_VOLUME_MAX &&
           bytes[AUTO_OFF_FIELD] < AUTO_OFF_OPTION_COUNT &&
           bytes[USB_MODE_FIELD] < USB_MODE_COUNT &&
           bytes[DAC_VOLUME_FIELD] <= SETTINGS_VOLUME_MAX &&
           bytes[THEME_FIELD] < THEME_CHOICE_COUNT && bytes[HOME_ART_FIELD] < HOME_ART_MODE_COUNT &&
           bytes[PODCASTS_FIELD] <= 1 &&
           (bytes[VERSION_FIELD] < MUSIC_VERSION ||
            (bytes[VOLUME_LIMIT_FIELD] < VOLUME_LIMIT_OPTION_COUNT &&
             bytes[BALANCE_FIELD] < BALANCE_OPTION_COUNT && bytes[SWAP_KEYS_FIELD] <= 1 &&
             bytes[EQUALIZER_FIELD] < EQUALIZER_OPTION_COUNT &&
             bytes[DAC_FILTER_FIELD] < DAC_FILTER_OPTION_COUNT && bytes[LYRICS_FIELD] <= 1 &&
             bytes[SKIP_FIELD] < SKIP_OPTION_COUNT && look_valid(bytes)));
}

bool settings_decode(const uint8_t *bytes, uint32_t length, Settings *settings)
{
    if (length < SETTINGS_FILE_SIZE || !header_valid(bytes) || !values_valid(bytes)) {
        return false;
    }
    *settings = (Settings){
        .brightness = bytes[BRIGHTNESS_FIELD],
        .screen_timeout = bytes[TIMEOUT_FIELD],
        .gain = (GainLevel)bytes[GAIN_FIELD],
        .output = (OutputJack)bytes[OUTPUT_FIELD],
        .start_screen = (StartScreen)bytes[START_FIELD],
        .shuffle = (ShuffleMode)bytes[SHUFFLE_FIELD],
        .repeat = (RepeatMode)bytes[REPEAT_FIELD],
        .volume = bytes[VOLUME_FIELD],
        .auto_off = bytes[VERSION_FIELD] < AUTO_OFF_VERSION ? (uint8_t)DEFAULT_AUTO_OFF_OPTION
                                                            : bytes[AUTO_OFF_FIELD],
        .usb_mode = bytes[VERSION_FIELD] < USB_MODE_VERSION ? USB_MODE_STORAGE
                                                            : (UsbMode)bytes[USB_MODE_FIELD],
        .dac_volume = bytes[VERSION_FIELD] < DAC_VOLUME_VERSION
                          ? (uint8_t)SETTINGS_DEFAULT_DAC_VOLUME
                          : bytes[DAC_VOLUME_FIELD],
        .theme = (ThemeChoice)bytes[THEME_FIELD],
        .home_art = (HomeArtMode)bytes[HOME_ART_FIELD],
        .podcasts_separate = bytes[PODCASTS_FIELD] == 1,
        .balance = BALANCE_CENTER,
        .lyrics = true,
        .player_style = PLAYER_STYLE_CASSETTE,
    };
    if (bytes[VERSION_FIELD] >= MUSIC_VERSION) {
        settings->volume_limit = bytes[VOLUME_LIMIT_FIELD];
        settings->balance = bytes[BALANCE_FIELD];
        settings->swap_volume_keys = bytes[SWAP_KEYS_FIELD] == 1;
        settings->equalizer = bytes[EQUALIZER_FIELD];
        settings->dac_filter = bytes[DAC_FILTER_FIELD];
        settings->lyrics = bytes[LYRICS_FIELD] == 1;
        settings->skip_seconds = bytes[SKIP_FIELD];
        decode_look(bytes, settings);
    }
    return true;
}
