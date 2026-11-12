#pragma once

#include <stdint.h>

#include "core/power_policy.h"
#include "info_screen.h"
#include "library_store.h"
#include "player.h"
#include "screens/bluetooth_screen.h"
#include "settings_store.h"
#include "ui/choice_screen.h"
#include "ui/menu_screen.h"
#include "ui/theme.h"
#include "ui/toast.h"

enum {
    SETTINGS_CHOICE_COUNT = 23,
    SETTINGS_GROUP_COUNT = 5,
    SETTINGS_MENU_COUNT = 7,
    SETTINGS_GROUP_ITEM_CAPACITY = 14
};

extern const char *const SETTINGS_THEME_NAMES[THEME_CHOICE_COUNT];

typedef struct SettingsContext {
    SettingsStore *store;
    Player *player;
    PowerPolicy *power;
    Toast *toast;
    bool *rescan_requested;
    BluetoothScreen *bluetooth;
    Screen *diagnostics;
    Screen *files;
    int sleep_option;
    const LibraryStore *library;
} SettingsContext;

typedef struct SettingChoice {
    const char *title;
    const char *const *options;
    int option_count;
    int (*current)(const void *context);
    void (*choose)(void *context, int index);
    const char *(*describe)(int current);
} SettingChoice;

typedef struct SettingsScreens SettingsScreens;

typedef struct SettingsEntry {
    const SettingChoice *choice;
    SettingsScreens *owner;
} SettingsEntry;

struct SettingsScreens {
    SettingsContext context;
    const Theme *theme;
    MenuScreen menu;
    ChoiceScreen choice;
    MenuScreen group;
    SettingsEntry entries[SETTINGS_CHOICE_COUNT];
    MenuItem items[SETTINGS_MENU_COUNT];
    MenuItem group_items[SETTINGS_GROUP_ITEM_CAPACITY];
    InfoScreen info;
};

Screen *settings_screens_init(SettingsScreens *screens, SettingsContext context,
                              const Theme *theme);
