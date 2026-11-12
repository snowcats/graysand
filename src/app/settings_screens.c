#include "settings_screens.h"

#include "core/string_builder.h"
#include "hal/audio.h"
#include "hal/clock.h"
#include "hal/power.h"
#include "hal/storage.h"
#include "hal/system.h"
#include "ui/list_navigation.h"
#include "ui/navigation.h"

enum {
    BRIGHTNESS_LEVEL_COUNT = SETTINGS_BRIGHTNESS_MAX - SETTINGS_BRIGHTNESS_MIN + 1,
    MILLISECONDS_PER_MINUTE = 60000
};

static const char *const SHUFFLE_OPTIONS[SHUFFLE_MODE_COUNT] = {"Off", "Songs", "Albums"};
static const char *const REPEAT_OPTIONS[REPEAT_MODE_COUNT] = {"Off", "One", "All"};
static const char *const SLEEP_OPTIONS[SLEEP_TIMER_OPTION_COUNT] = {
    "Off", "15 min", "30 min", "45 min", "60 min", "90 min", "120 min"};
static const char *const SLEEP_MESSAGES[SLEEP_TIMER_OPTION_COUNT] = {
    "Sleep timer off",       "Turning off in 15 min", "Turning off in 30 min",
    "Turning off in 45 min", "Turning off in 60 min", "Turning off in 90 min",
    "Turning off in 120 min"};
static const char *const AUTO_OFF_OPTIONS[AUTO_OFF_OPTION_COUNT] = {"Off", "1 min", "5 min",
                                                                    "10 min"};
static const char *const TIMEOUT_OPTIONS[SCREEN_TIMEOUT_OPTION_COUNT] = {
    "10 s", "20 s", "30 s", "1 min", "2 min", "5 min", "Never", "Idle screen"};
static const char *const BRIGHTNESS_OPTIONS[BRIGHTNESS_LEVEL_COUNT] = {"1", "2", "3", "4", "5",
                                                                       "6", "7", "8", "9", "10"};
static const char *const GAIN_OPTIONS[GAIN_LEVEL_COUNT] = {"Low", "High"};
static const char *const OUTPUT_OPTIONS[OUTPUT_JACK_COUNT] = {"Auto", "3.5 mm", "4.4 mm"};
static const char *const OUTPUT_MESSAGES[OUTPUT_JACK_COUNT] = {
    "Volume follows the plugged jack", "Using 3.5 mm volume levels", "Using 4.4 mm volume levels"};
static const char *const START_OPTIONS[START_SCREEN_COUNT] = {
    "Home", "Now Playing", "Albums", "Artists", "Favorites", "Playlists", "Songs"};
static const char *const USB_MODE_OPTIONS[USB_MODE_COUNT] = {"Computer", "Normal"};
const char *const SETTINGS_THEME_NAMES[THEME_CHOICE_COUNT] = {"Graysand", "Pokemon", "Zelda",
                                                              "Lain", "Noah"};
static const char *const HOME_ART_OPTIONS[HOME_ART_MODE_COUNT] = {"Album covers", "Theme emblem"};
static const char *const PLAYER_STYLE_OPTIONS[PLAYER_STYLE_COUNT] = {
    "Album cover", "Cassette, full screen", "Cassette, minimal"};
static const char *const ALBUM_VIEW_OPTIONS[ALBUM_VIEW_COUNT] = {"List", "Grid"};
static const char *const PODCASTS_OPTIONS[2] = {"With Books", "Separate"};
static const char *const EQUALIZER_OPTIONS[EQUALIZER_OPTION_COUNT] = {
    "Off", "Retro", "Bass", "Heavy", "Pop", "Jazz", "Unique"};
static const char *const FILTER_OPTIONS[DAC_FILTER_OPTION_COUNT] = {
    "Fast minimum", "Fast linear", "Slow minimum", "Slow linear", "No oversampling"};
static const char *const BALANCE_OPTIONS[BALANCE_OPTION_COUNT] = {
    "L 10 dB", "L 8 dB", "L 6 dB", "L 4 dB", "L 2 dB", "Center",
    "R 2 dB",  "R 4 dB", "R 6 dB", "R 8 dB", "R 10 dB"};
static const char *const LIMIT_OPTIONS[VOLUME_LIMIT_OPTION_COUNT] = {"Off", "40", "30", "20"};
static const char *const SKIP_OPTIONS[SKIP_OPTION_COUNT] = {
    "5 s", "6 s", "7 s", "8 s", "9 s", "10 s", "11 s", "12 s", "13 s", "14 s", "15 s"};
static const char *const OFF_ON_OPTIONS[2] = {"Off", "On"};
static const char *const USB_MODE_MESSAGES[USB_MODE_COUNT] = {"USB shows the player's drives",
                                                              "USB plays the computer's sound"};

static const Settings *settings_of(const void *context)
{
    const SettingsContext *settings = context;
    return &settings->store->settings;
}

static Settings *editable_settings(void *context)
{
    SettingsContext *settings = context;
    return &settings->store->settings;
}

static void save(SettingsContext *context)
{
    settings_store_capture(context->store, player_status(context->player));
    if (!settings_store_save(context->store)) {
        toast_show(context->toast, "Could not save settings", hal_clock_milliseconds());
    }
}

static void apply_and_save(SettingsContext *context)
{
    settings_store_apply_hardware(&context->store->settings);
    save(context);
}

static int current_shuffle(const void *context)
{
    const SettingsContext *settings = context;
    return (int)player_status(settings->player).shuffle;
}

static void choose_shuffle(void *context, int index)
{
    SettingsContext *settings = context;
    player_set_shuffle(settings->player, (ShuffleMode)index, hal_clock_microseconds());
    save(settings);
}

static int current_repeat(const void *context)
{
    const SettingsContext *settings = context;
    return (int)player_status(settings->player).repeat;
}

static void choose_repeat(void *context, int index)
{
    SettingsContext *settings = context;
    player_set_repeat(settings->player, (RepeatMode)index);
    save(settings);
}

static int current_sleep(const void *context)
{
    const SettingsContext *settings = context;
    return settings->power->sleep_timer_active ? settings->sleep_option : 0;
}

static void choose_sleep(void *context, int index)
{
    SettingsContext *settings = context;
    uint32_t now_ms = hal_clock_milliseconds();
    uint32_t minutes = settings_sleep_option_minutes(index);
    settings->sleep_option = index;
    if (minutes == 0) {
        power_policy_cancel_sleep_timer(settings->power);
    } else {
        power_policy_start_sleep_timer(settings->power, minutes * MILLISECONDS_PER_MINUTE, now_ms);
    }
    toast_show(settings->toast, SLEEP_MESSAGES[index], now_ms);
}

static int current_timeout(const void *context)
{
    return settings_of(context)->screen_timeout;
}

static void choose_timeout(void *context, int index)
{
    SettingsContext *settings = context;
    editable_settings(context)->screen_timeout = (uint8_t)index;
    power_policy_set_timeout(settings->power, settings_screen_timeout_ms(settings_of(context)));
    save(settings);
}

static int current_auto_off(const void *context)
{
    return settings_of(context)->auto_off;
}

static void choose_auto_off(void *context, int index)
{
    SettingsContext *settings = context;
    editable_settings(context)->auto_off = (uint8_t)index;
    power_policy_set_auto_off(settings->power, settings_auto_off_ms(settings_of(context)),
                              hal_clock_milliseconds());
    save(settings);
}

static int current_brightness(const void *context)
{
    return settings_of(context)->brightness - SETTINGS_BRIGHTNESS_MIN;
}

static void choose_brightness(void *context, int index)
{
    editable_settings(context)->brightness = (uint8_t)(index + SETTINGS_BRIGHTNESS_MIN);
    apply_and_save(context);
}

static int current_gain(const void *context)
{
    return (int)settings_of(context)->gain;
}

static void choose_gain(void *context, int index)
{
    editable_settings(context)->gain = (GainLevel)index;
    apply_and_save(context);
}

static int current_output(const void *context)
{
    return (int)settings_of(context)->output;
}

static void choose_output(void *context, int index)
{
    SettingsContext *settings = context;
    editable_settings(context)->output = (OutputJack)index;
    apply_and_save(context);
    toast_show(settings->toast, OUTPUT_MESSAGES[index], hal_clock_milliseconds());
}

static const char *describe_output(int current)
{
    HalAudioJack jack = hal_audio_jack();
    if (current != OUTPUT_AUTO || jack == HAL_AUDIO_JACK_NONE) {
        return OUTPUT_OPTIONS[current];
    }
    return jack == HAL_AUDIO_JACK_BALANCED ? "Auto (4.4 mm)" : "Auto (3.5 mm)";
}

static int current_start(const void *context)
{
    return (int)settings_of(context)->start_screen;
}

static void choose_start(void *context, int index)
{
    editable_settings(context)->start_screen = (StartScreen)index;
    save(context);
}

static int current_usb_mode(const void *context)
{
    return (int)settings_of(context)->usb_mode;
}

static void choose_usb_mode(void *context, int index)
{
    SettingsContext *settings = context;
    editable_settings(context)->usb_mode = (UsbMode)index;
    apply_and_save(context);
    toast_show(settings->toast, USB_MODE_MESSAGES[index], hal_clock_milliseconds());
}

static int current_theme(const void *context)
{
    return (int)settings_of(context)->theme;
}

static void choose_theme(void *context, int index)
{
    editable_settings(context)->theme = (ThemeChoice)index;
    save(context);
}

static int current_home_art(const void *context)
{
    return (int)settings_of(context)->home_art;
}

static void choose_home_art(void *context, int index)
{
    editable_settings(context)->home_art = (HomeArtMode)index;
    save(context);
}

static int current_podcasts(const void *context)
{
    return settings_of(context)->podcasts_separate ? 1 : 0;
}

static void choose_podcasts(void *context, int index)
{
    editable_settings(context)->podcasts_separate = index == 1;
    save(context);
}

static int current_equalizer(const void *context)
{
    return settings_of(context)->equalizer;
}

static void choose_equalizer(void *context, int index)
{
    editable_settings(context)->equalizer = (uint8_t)index;
    apply_and_save(context);
}

static int current_filter(const void *context)
{
    return settings_of(context)->dac_filter;
}

static void choose_filter(void *context, int index)
{
    editable_settings(context)->dac_filter = (uint8_t)index;
    apply_and_save(context);
}

static int current_balance(const void *context)
{
    return settings_of(context)->balance;
}

static void choose_balance(void *context, int index)
{
    editable_settings(context)->balance = (uint8_t)index;
    apply_and_save(context);
}

static int current_limit(const void *context)
{
    return settings_of(context)->volume_limit;
}

static void choose_limit(void *context, int index)
{
    SettingsContext *settings = context;
    editable_settings(context)->volume_limit = (uint8_t)index;
    player_set_volume_limit(settings->player, settings_volume_limit(settings_of(context)));
    save(settings);
}

static int current_swap(const void *context)
{
    return settings_of(context)->swap_volume_keys ? 1 : 0;
}

static void choose_swap(void *context, int index)
{
    editable_settings(context)->swap_volume_keys = index == 1;
    save(context);
}

static int current_skip(const void *context)
{
    return settings_of(context)->skip_seconds;
}

static void choose_skip(void *context, int index)
{
    editable_settings(context)->skip_seconds = (uint8_t)index;
    save(context);
}

static int current_lyrics(const void *context)
{
    return settings_of(context)->lyrics ? 1 : 0;
}

static void choose_lyrics(void *context, int index)
{
    editable_settings(context)->lyrics = index == 1;
    save(context);
}

static int current_player_style(const void *context)
{
    return settings_of(context)->player_style;
}

static void choose_player_style(void *context, int index)
{
    editable_settings(context)->player_style = (uint8_t)index;
    save(context);
}

static int current_album_view(const void *context)
{
    return settings_of(context)->album_view;
}

static void choose_album_view(void *context, int index)
{
    editable_settings(context)->album_view = (uint8_t)index;
    save(context);
}

static int current_format_tag(const void *context)
{
    return settings_of(context)->format_tag ? 1 : 0;
}

static void choose_format_tag(void *context, int index)
{
    editable_settings(context)->format_tag = index == 1;
    save(context);
}

static const SettingChoice CHOICES[SETTINGS_CHOICE_COUNT] = {
    {"Shuffle", SHUFFLE_OPTIONS, SHUFFLE_MODE_COUNT, current_shuffle, choose_shuffle, NULL},
    {"Repeat", REPEAT_OPTIONS, REPEAT_MODE_COUNT, current_repeat, choose_repeat, NULL},
    {"Sleep timer", SLEEP_OPTIONS, SLEEP_TIMER_OPTION_COUNT, current_sleep, choose_sleep, NULL},
    {"Power saving", AUTO_OFF_OPTIONS, AUTO_OFF_OPTION_COUNT, current_auto_off, choose_auto_off,
     NULL},
    {"Screen timeout", TIMEOUT_OPTIONS, SCREEN_TIMEOUT_OPTION_COUNT, current_timeout,
     choose_timeout, NULL},
    {"Brightness", BRIGHTNESS_OPTIONS, BRIGHTNESS_LEVEL_COUNT, current_brightness,
     choose_brightness, NULL},
    {"Gain", GAIN_OPTIONS, GAIN_LEVEL_COUNT, current_gain, choose_gain, NULL},
    {"Output", OUTPUT_OPTIONS, OUTPUT_JACK_COUNT, current_output, choose_output, describe_output},
    {"Start screen", START_OPTIONS, START_SCREEN_COUNT, current_start, choose_start, NULL},
    {"Mode", USB_MODE_OPTIONS, USB_MODE_COUNT, current_usb_mode, choose_usb_mode, NULL},
    {"Theme", SETTINGS_THEME_NAMES, THEME_CHOICE_COUNT, current_theme, choose_theme, NULL},
    {"Home art", HOME_ART_OPTIONS, HOME_ART_MODE_COUNT, current_home_art, choose_home_art, NULL},
    {"Podcasts", PODCASTS_OPTIONS, 2, current_podcasts, choose_podcasts, NULL},
    {"Equalizer", EQUALIZER_OPTIONS, EQUALIZER_OPTION_COUNT, current_equalizer, choose_equalizer,
     NULL},
    {"Filter", FILTER_OPTIONS, DAC_FILTER_OPTION_COUNT, current_filter, choose_filter, NULL},
    {"Balance", BALANCE_OPTIONS, BALANCE_OPTION_COUNT, current_balance, choose_balance, NULL},
    {"Volume limit", LIMIT_OPTIONS, VOLUME_LIMIT_OPTION_COUNT, current_limit, choose_limit, NULL},
    {"Swap volume keys", OFF_ON_OPTIONS, 2, current_swap, choose_swap, NULL},
    {"Lyrics", OFF_ON_OPTIONS, 2, current_lyrics, choose_lyrics, NULL},
    {"Fast forward", SKIP_OPTIONS, SKIP_OPTION_COUNT, current_skip, choose_skip, NULL},
    {"Player style", PLAYER_STYLE_OPTIONS, PLAYER_STYLE_COUNT, current_player_style,
     choose_player_style, NULL},
    {"Album view", ALBUM_VIEW_OPTIONS, ALBUM_VIEW_COUNT, current_album_view, choose_album_view,
     NULL},
    {"Format info", OFF_ON_OPTIONS, 2, current_format_tag, choose_format_tag, NULL},
};

static ScreenAction open_choice(void *context)
{
    SettingsEntry *entry = context;
    SettingsScreens *screens = entry->owner;
    const SettingChoice *choice = entry->choice;
    ChoiceConfig config = {
        .title = choice->title,
        .options = choice->options,
        .option_count = choice->option_count,
        .current = choice->current,
        .choose = choice->choose,
        .context = &screens->context,
        .theme = screens->theme,
    };
    return screen_action_push(choice_screen_init(&screens->choice, config));
}

static const char *choice_value(const void *context)
{
    const SettingsEntry *entry = context;
    int current = entry->choice->current(&entry->owner->context);
    if (current < 0 || current >= entry->choice->option_count) {
        return "";
    }
    if (entry->choice->describe != NULL) {
        return entry->choice->describe(current);
    }
    return entry->choice->options[current];
}

static ScreenAction request_rescan(void *context)
{
    SettingsContext *settings = context;
    if (settings->rescan_requested != NULL) {
        *settings->rescan_requested = true;
    }
    return screen_action_none();
}

static ScreenAction turn_off(void *context)
{
    (void)context;
    hal_power_off();
    return screen_action_none();
}

enum {
    KIND_CHOICE,
    KIND_POWER,
    KIND_BLUETOOTH,
    KIND_DIAGNOSTICS,
    KIND_FILES
};

enum {
    CHOICE_SHUFFLE,
    CHOICE_REPEAT,
    CHOICE_SLEEP,
    CHOICE_AUTO_OFF,
    CHOICE_TIMEOUT,
    CHOICE_BRIGHTNESS,
    CHOICE_GAIN,
    CHOICE_OUTPUT,
    CHOICE_START,
    CHOICE_USB,
    CHOICE_THEME,
    CHOICE_HOME_ART,
    CHOICE_PODCASTS,
    CHOICE_EQUALIZER,
    CHOICE_FILTER,
    CHOICE_BALANCE,
    CHOICE_LIMIT,
    CHOICE_SWAP,
    CHOICE_LYRICS,
    CHOICE_SKIP,
    CHOICE_PLAYER_STYLE,
    CHOICE_ALBUM_VIEW,
    CHOICE_FORMAT_TAG,
    CHOICE_COUNT
};

typedef struct GroupRow {
    uint8_t kind;
    uint8_t choice;
} GroupRow;

typedef struct SettingsGroup {
    const char *title;
    const GroupRow *rows;
    int count;
} SettingsGroup;

static const GroupRow SCREEN_ROWS[] = {
    {KIND_CHOICE, CHOICE_TIMEOUT},
    {KIND_CHOICE, CHOICE_BRIGHTNESS},
};
static const GroupRow SHUTDOWN_ROWS[] = {
    {KIND_POWER, 0},
    {KIND_CHOICE, CHOICE_SLEEP},
    {KIND_CHOICE, CHOICE_AUTO_OFF},
};
static const GroupRow CHARGING_ROWS[] = {{KIND_CHOICE, CHOICE_USB}};
static const GroupRow SYSTEM_ROWS[] = {
    {KIND_CHOICE, CHOICE_THEME},
    {KIND_CHOICE, CHOICE_HOME_ART},
    {KIND_CHOICE, CHOICE_START},
    {KIND_CHOICE, CHOICE_PODCASTS},
    {KIND_BLUETOOTH, 0},
    {KIND_DIAGNOSTICS, 0},
    {KIND_FILES, 0},
};
static const GroupRow MUSIC_ROWS[] = {
    {KIND_CHOICE, CHOICE_SHUFFLE},    {KIND_CHOICE, CHOICE_REPEAT},
    {KIND_CHOICE, CHOICE_GAIN},       {KIND_CHOICE, CHOICE_OUTPUT},
    {KIND_CHOICE, CHOICE_EQUALIZER},  {KIND_CHOICE, CHOICE_FILTER},
    {KIND_CHOICE, CHOICE_BALANCE},    {KIND_CHOICE, CHOICE_LIMIT},
    {KIND_CHOICE, CHOICE_SWAP},       {KIND_CHOICE, CHOICE_LYRICS},
    {KIND_CHOICE, CHOICE_SKIP},       {KIND_CHOICE, CHOICE_PLAYER_STYLE},
    {KIND_CHOICE, CHOICE_ALBUM_VIEW}, {KIND_CHOICE, CHOICE_FORMAT_TAG},
};

enum {
    MENU_RESCAN = SETTINGS_GROUP_COUNT,
    MENU_INFO
};

static const SettingsGroup GROUPS[SETTINGS_GROUP_COUNT] = {
    {"Screen", SCREEN_ROWS, (int)(sizeof SCREEN_ROWS / sizeof SCREEN_ROWS[0])},
    {"Shutdown", SHUTDOWN_ROWS, (int)(sizeof SHUTDOWN_ROWS / sizeof SHUTDOWN_ROWS[0])},
    {"Charging", CHARGING_ROWS, (int)(sizeof CHARGING_ROWS / sizeof CHARGING_ROWS[0])},
    {"System", SYSTEM_ROWS, (int)(sizeof SYSTEM_ROWS / sizeof SYSTEM_ROWS[0])},
    {"Music", MUSIC_ROWS, (int)(sizeof MUSIC_ROWS / sizeof MUSIC_ROWS[0])},
};

_Static_assert((int)CHOICE_COUNT == (int)SETTINGS_CHOICE_COUNT, "choice order");
_Static_assert((int)MENU_INFO + 1 == (int)SETTINGS_MENU_COUNT, "menu rows");
_Static_assert(sizeof MUSIC_ROWS / sizeof MUSIC_ROWS[0] <= SETTINGS_GROUP_ITEM_CAPACITY,
               "music rows fit");

static const char *row_label(GroupRow row)
{
    if (row.kind == KIND_CHOICE) {
        return CHOICES[row.choice].title;
    }
    if (row.kind == KIND_POWER) {
        return "Power off";
    }
    if (row.kind == KIND_BLUETOOTH) {
        return "Bluetooth";
    }
    return row.kind == KIND_FILES ? "Files" : "Diagnostics";
}

static Screen *open_group_page(SettingsScreens *screens, const char *title, const MenuItem *items,
                               int count)
{
    MenuConfig config = {.title = title,
                         .items = items,
                         .item_count = count,
                         .theme = screens->theme,
                         .two_line = true};
    return menu_screen_init(&screens->group, config);
}

static void fill_group(SettingsScreens *screens, int group)
{
    const SettingsGroup *spec = &GROUPS[group];
    for (int index = 0; index < spec->count; index++) {
        GroupRow row = spec->rows[index];
        if (row.kind == KIND_CHOICE) {
            screens->group_items[index] = (MenuItem){.label = CHOICES[row.choice].title,
                                                     .select = open_choice,
                                                     .context = &screens->entries[row.choice],
                                                     .value = choice_value,
                                                     .opens = true};
        } else if (row.kind == KIND_POWER) {
            screens->group_items[index] = (MenuItem){.label = "Power off", .select = turn_off};
        } else if (row.kind == KIND_BLUETOOTH) {
            screens->group_items[index] = (MenuItem){.label = "Bluetooth",
                                                     .select = bluetooth_screen_open,
                                                     .context = screens->context.bluetooth,
                                                     .value = bluetooth_screen_summary,
                                                     .opens = true};
        } else {
            screens->group_items[index] =
                (MenuItem){.label = row_label(row),
                           .select = menu_open_screen,
                           .context = row.kind == KIND_FILES ? screens->context.files
                                                             : screens->context.diagnostics,
                           .opens = true};
        }
    }
}

static ScreenAction open_menu_row(void *context)
{
    SettingsScreens *screens = context;
    int selected = screens->menu.list.selected;
    if (selected == MENU_RESCAN) {
        return request_rescan(&screens->context);
    }
    if (selected == MENU_INFO) {
        return screen_action_push(&screens->info.screen);
    }
    if (selected >= SETTINGS_GROUP_COUNT) {
        return screen_action_none();
    }
    fill_group(screens, selected);
    return screen_action_push(open_group_page(screens, GROUPS[selected].title, screens->group_items,
                                              GROUPS[selected].count));
}

Screen *settings_screens_init(SettingsScreens *screens, SettingsContext context, const Theme *theme)
{
    screens->context = context;
    screens->theme = theme;
    for (int index = 0; index < SETTINGS_CHOICE_COUNT; index++) {
        screens->entries[index] = (SettingsEntry){.choice = &CHOICES[index], .owner = screens};
    }
    for (int index = 0; index < SETTINGS_GROUP_COUNT; index++) {
        screens->items[index] = (MenuItem){.label = GROUPS[index].title,
                                           .select = open_menu_row,
                                           .context = screens,
                                           .opens = true};
    }
    screens->items[MENU_RESCAN] =
        (MenuItem){.label = "Rescan library", .select = open_menu_row, .context = screens};
    screens->items[MENU_INFO] =
        (MenuItem){.label = "Information", .select = open_menu_row, .context = screens};
    info_screen_init(&screens->info, theme, context.store, context.library);
    MenuConfig menu = {.title = "Settings",
                       .items = screens->items,
                       .item_count = SETTINGS_MENU_COUNT,
                       .theme = theme};
    return menu_screen_init(&screens->menu, menu);
}
