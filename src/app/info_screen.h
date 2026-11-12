#pragma once

#include "art_slots.h"
#include "hal/storage.h"
#include "library_store.h"
#include "settings_store.h"
#include "ui/screen.h"
#include "ui/theme.h"

enum {
    INFO_TEXT_CAPACITY = 48,
    INFO_TEXT_COUNT = 6
};

typedef struct InfoScreen {
    Screen screen;
    const Theme *theme;
    const SettingsStore *settings;
    const LibraryStore *library;
    int slot_count;
    ArtSlot slots[ART_SLOT_CAPACITY];
    char texts[INFO_TEXT_COUNT][INFO_TEXT_CAPACITY];
    const char *build;
    uint16_t memory_permille;
    uint16_t card_permille;
} InfoScreen;

uint16_t info_screen_space_texts(HalSpace space, char *used, char *total, size_t capacity);
const char *info_screen_version_text(const char *version, char *text, size_t capacity);
Screen *info_screen_init(InfoScreen *info, const Theme *theme, const SettingsStore *settings,
                         const LibraryStore *library);
