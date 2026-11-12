#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ui/list_view.h"
#include "ui/screen.h"
#include "ui/theme.h"

enum {
    FILES_ROWS = 8,
    FILES_PATH_CAPACITY = 256,
    FILES_LABEL_CAPACITY = 96,
    FILES_DETAIL_CAPACITY = 16,
    FILES_MAX_ENTRIES = 4000
};

typedef struct FilesScreen {
    Screen screen;
    const Theme *theme;
    ListModel model;
    ListView list;
    int count;
    int rows_first;
    int rows_count;
    char path[FILES_PATH_CAPACITY];
    char labels[FILES_ROWS][FILES_LABEL_CAPACITY];
    char details[FILES_ROWS][FILES_DETAIL_CAPACITY];
    bool folders[FILES_ROWS];
    uint32_t now_ms;
} FilesScreen;

Screen *files_screen_init(FilesScreen *files, const Theme *theme);
