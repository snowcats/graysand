#include "files_screen.h"

#include "core/string_builder.h"
#include "hal/storage.h"
#include "ui/header.h"
#include "ui/list_navigation.h"

static const char *const UNITS[] = {" B", " KB", " MB", " GB"};

static void size_text(uint32_t bytes, char *text)
{
    int unit = 0;
    while (bytes >= 1024u && unit < 3) {
        bytes /= 1024u;
        unit++;
    }
    StringBuilder builder = string_builder_make(text, FILES_DETAIL_CAPACITY);
    string_builder_append_unsigned(&builder, bytes);
    string_builder_append(&builder, UNITS[unit]);
}

static int text_length(const char *text)
{
    int length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static int last_slash(const char *path)
{
    int found = -1;
    for (int index = 0; path[index] != '\0'; index++) {
        found = path[index] == '/' ? index : found;
    }
    return found;
}

static const char *folder_name(const FilesScreen *files)
{
    int slash = last_slash(files->path);
    if (slash < 0) {
        return hal_file_volume() == HAL_VOLUME_INTERNAL ? "Internal storage" : "SD card";
    }
    return files->path + slash + 1;
}

static void load(FilesScreen *files, int top, int rows)
{
    HalDir dir;
    HalDirEntry entry;
    int index = 0;
    files->rows_first = top;
    files->rows_count = 0;
    if (!hal_dir_open(&dir, files->path[0] != '\0' ? files->path : "/")) {
        files->count = 0;
        return;
    }
    while (index < FILES_MAX_ENTRIES && hal_dir_next(&dir, &entry)) {
        int row = index - top;
        if (row >= 0 && row < rows) {
            StringBuilder builder = string_builder_make(files->labels[row], FILES_LABEL_CAPACITY);
            string_builder_append(&builder, entry.name);
            files->folders[row] = entry.directory;
            files->details[row][0] = '\0';
            if (!entry.directory) {
                size_text(entry.size, files->details[row]);
            }
            files->rows_count = row + 1;
        }
        index++;
    }
    hal_dir_close(&dir);
    files->count = index;
}

static void refresh(FilesScreen *files)
{
    if (files->rows_first != files->list.top) {
        int rows = files->list.visible_rows < FILES_ROWS ? files->list.visible_rows : FILES_ROWS;
        load(files, files->list.top, rows);
    }
}

static void open_folder(FilesScreen *files, uint32_t now_ms)
{
    files->screen.title = folder_name(files);
    load(files, 0, 0);
    list_view_init(&files->list, &files->model, files->theme, now_ms);
    files->rows_first = -1;
    refresh(files);
}

static int row_index(const FilesScreen *files, int index)
{
    int row = index - files->rows_first;
    return files->rows_first < 0 || row < 0 || row >= files->rows_count ? -1 : row;
}

static int row_count(const void *context)
{
    const FilesScreen *files = context;
    return files->count;
}

static const char *row_label(const void *context, int index)
{
    const FilesScreen *files = context;
    int row = row_index(files, index);
    return row < 0 ? "" : files->labels[row];
}

static const char *row_detail(const void *context, int index)
{
    const FilesScreen *files = context;
    int row = row_index(files, index);
    return row < 0 || files->details[row][0] == '\0' ? NULL : files->details[row];
}

static bool row_opens(const void *context, int index)
{
    const FilesScreen *files = context;
    int row = row_index(files, index);
    return row >= 0 && files->folders[row];
}

static ScreenAction select_row(void *context, int index)
{
    FilesScreen *files = context;
    int row = row_index(files, index);
    int length = text_length(files->path);
    if (row < 0 || !files->folders[row] ||
        length + 1 + text_length(files->labels[row]) >= FILES_PATH_CAPACITY) {
        return screen_action_none();
    }
    StringBuilder builder =
        string_builder_make(files->path + length, (size_t)(FILES_PATH_CAPACITY - length));
    string_builder_append_char(&builder, '/');
    string_builder_append(&builder, files->labels[row]);
    open_folder(files, files->now_ms);
    return screen_action_none();
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    FilesScreen *files = context;
    files->now_ms = now_ms;
    ListSelection selection = {.select = select_row, .context = files};
    ScreenAction action = list_navigation_handle(&files->list, event, now_ms, selection);
    int slash = last_slash(files->path);
    if (action.type == SCREEN_ACTION_POP && slash >= 0) {
        files->path[slash] = '\0';
        open_folder(files, now_ms);
        return screen_action_none();
    }
    refresh(files);
    return action;
}

static void enter(void *context, uint32_t now_ms)
{
    FilesScreen *files = context;
    files->now_ms = now_ms;
    files->path[0] = '\0';
    open_folder(files, now_ms);
}

static void update(void *context, uint32_t now_ms)
{
    FilesScreen *files = context;
    files->now_ms = now_ms;
    refresh(files);
}

static void draw(void *context, Canvas *canvas)
{
    const FilesScreen *files = context;
    header_draw(canvas, files->theme, files->screen.title);
    list_view_draw(&files->list, canvas, theme_content_area(files->theme), files->now_ms);
}

Screen *files_screen_init(FilesScreen *files, const Theme *theme)
{
    files->theme = theme;
    files->count = 0;
    files->rows_first = -1;
    files->rows_count = 0;
    files->path[0] = '\0';
    files->now_ms = 0;
    files->model = (ListModel){.context = files,
                               .count = row_count,
                               .label = row_label,
                               .detail = row_detail,
                               .opens = row_opens};
    list_view_init(&files->list, &files->model, theme, 0);
    files->screen = (Screen){
        .title = "Files",
        .context = files,
        .enter = enter,
        .handle_key = handle_key,
        .update = update,
        .draw = draw,
    };
    return &files->screen;
}
