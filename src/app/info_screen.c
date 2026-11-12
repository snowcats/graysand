#include "info_screen.h"

#include "core/string_builder.h"
#include "hal/storage.h"
#include "hal/system.h"
#include "home_art.h"
#include "library_rescan.h"
#include "ui/header.h"
#include "ui/navigation.h"

enum {
    INFO_PATH_CAPACITY = 32,
    KILOBYTE = 1024,
    TENTHS = 10,
    WHOLE_GIGABYTES_FROM = 100
};

static const char INFO_ART[] = "/.echo/info/";
static const char SEPARATOR[] = "  \xC2\xB7  ";

const char *info_screen_version_text(const char *version, char *text, size_t capacity)
{
    StringBuilder builder = string_builder_make(text, capacity);
    string_builder_append(&builder, "Graysand ");
    for (const char *letter = version; *letter != '\0'; letter++) {
        if (*letter == '-') {
            string_builder_append(&builder, SEPARATOR);
            string_builder_append(&builder, "build ");
            return letter + 1;
        }
        string_builder_append_char(&builder, *letter);
    }
    return NULL;
}

static void art_path(const InfoScreen *info, char *path)
{
    StringBuilder builder = string_builder_make(path, INFO_PATH_CAPACITY);
    string_builder_append(&builder, INFO_ART);
    string_builder_append(&builder, home_art_theme_folder(info->settings->settings.theme));
    string_builder_append(&builder, ".bin");
}

static void append_count(StringBuilder *builder, uint32_t count, const char *noun)
{
    string_builder_append_unsigned(builder, count);
    string_builder_append(builder, noun);
}

static void append_gigabytes(StringBuilder *builder, uint32_t megabytes)
{
    uint32_t tenths = (megabytes * TENTHS + KILOBYTE / 2) / KILOBYTE;
    string_builder_append_unsigned(builder, tenths / TENTHS);
    if (tenths < WHOLE_GIGABYTES_FROM * TENTHS && tenths % TENTHS != 0) {
        string_builder_append_char(builder, '.');
        string_builder_append_unsigned(builder, tenths % TENTHS);
    }
    string_builder_append(builder, " GB");
}

uint16_t info_screen_space_texts(HalSpace space, char *used, char *total, size_t capacity)
{
    StringBuilder builder = string_builder_make(used, capacity);
    if (space.total_megabytes == 0) {
        return 0;
    }
    append_gigabytes(&builder, space.used_megabytes);
    builder = string_builder_make(total, capacity);
    string_builder_append(&builder, "of ");
    append_gigabytes(&builder, space.total_megabytes);
    uint32_t used_megabytes =
        space.used_megabytes < space.total_megabytes ? space.used_megabytes : space.total_megabytes;
    return (uint16_t)((uint64_t)used_megabytes * ART_PERMILLE / space.total_megabytes);
}

static void read_space(HalSpace space[HAL_SPACE_COUNT])
{
    HalFile file;
    if (hal_file_open(&file, RESCAN_TIMES_PATH)) {
        if (!hal_file_read(file, HAL_SPACE_OFFSET, space, HAL_SPACE_COUNT * sizeof *space)) {
            space[HAL_SPACE_INTERNAL] = space[HAL_SPACE_CARD] = (HalSpace){0, 0};
        }
        hal_file_close(&file);
    }
}

static void fill_texts(InfoScreen *info)
{
    info->build =
        info_screen_version_text(hal_system_version(), info->texts[0], INFO_TEXT_CAPACITY);
    HalSpace space[HAL_SPACE_COUNT] = {{0, 0}, {0, 0}};
    read_space(space);
    info->memory_permille = info_screen_space_texts(space[HAL_SPACE_INTERNAL], info->texts[1],
                                                    info->texts[4], INFO_TEXT_CAPACITY);
    if (info->texts[1][0] == '\0') {
        StringBuilder unknown = string_builder_make(info->texts[1], INFO_TEXT_CAPACITY);
        string_builder_append(&unknown, "Unknown");
    }
    if (space[HAL_SPACE_CARD].total_megabytes == 0) {
        space[HAL_SPACE_CARD].used_megabytes = hal_storage_card_megabytes();
    }
    info->card_permille = info_screen_space_texts(space[HAL_SPACE_CARD], info->texts[2],
                                                  info->texts[5], INFO_TEXT_CAPACITY);
    StringBuilder builder = string_builder_make(info->texts[2], INFO_TEXT_CAPACITY);
    if (space[HAL_SPACE_CARD].total_megabytes == 0 && space[HAL_SPACE_CARD].used_megabytes > 0) {
        append_gigabytes(&builder, space[HAL_SPACE_CARD].used_megabytes);
    } else if (space[HAL_SPACE_CARD].total_megabytes == 0) {
        string_builder_append(&builder, "No card");
    }
    const Library *library = &info->library->library;
    bool ready = info->library->available;
    builder = string_builder_make(info->texts[3], INFO_TEXT_CAPACITY);
    append_count(&builder, ready ? library_music_count(library, LIBRARY_TRACKS) : 0, " songs");
    string_builder_append(&builder, SEPARATOR);
    append_count(&builder, ready ? library_music_count(library, LIBRARY_ALBUMS) : 0, " albums");
    string_builder_append(&builder, SEPARATOR);
    append_count(&builder, ready ? library_music_count(library, LIBRARY_ARTISTS) : 0, " artists");
}

static void enter(void *context, uint32_t now_ms)
{
    (void)now_ms;
    InfoScreen *info = context;
    fill_texts(info);
    char path[INFO_PATH_CAPACITY];
    art_path(info, path);
    Rect area = theme_content_area(info->theme);
    info->slot_count = art_slots_load(path, (uint32_t)(area.width * area.height) * sizeof(Color),
                                      info->slots, ART_SLOT_CAPACITY);
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    (void)context;
    (void)now_ms;
    return navigation_exit_action(event);
}

static void draw(void *context, Canvas *canvas)
{
    InfoScreen *info = context;
    header_draw(canvas, info->theme, info->screen.title);
    char path[INFO_PATH_CAPACITY];
    art_path(info, path);
    home_art_draw_file(path, canvas, theme_content_area(info->theme));
    ArtSlotValues values = {
        .texts = {[ART_INFO_VERSION] = info->texts[0],
                  [ART_INFO_MEMORY] = info->texts[1],
                  [ART_INFO_CARD] = info->texts[2],
                  [ART_INFO_LIBRARY] = info->texts[3],
                  [ART_INFO_BUILD] = info->build,
                  [ART_INFO_MEMORY_TOTAL] = info->texts[4],
                  [ART_INFO_CARD_TOTAL] = info->texts[5]},
        .permille = {[ART_INFO_MEMORY_LEVEL - ART_TEXT_COUNT] = info->memory_permille,
                     [ART_INFO_CARD_LEVEL - ART_TEXT_COUNT] = info->card_permille}};
    art_slots_draw(canvas, info->theme, info->slots, info->slot_count, &values);
}

static uint32_t picture(const void *context)
{
    const InfoScreen *info = context;
    return screen_picture_mix((uint32_t)(uintptr_t)info->theme, (uint32_t)info->slot_count + 1u);
}

Screen *info_screen_init(InfoScreen *info, const Theme *theme, const SettingsStore *settings,
                         const LibraryStore *library)
{
    *info = (InfoScreen){.theme = theme, .settings = settings, .library = library};
    info->screen = (Screen){.title = "Information",
                            .context = info,
                            .enter = enter,
                            .handle_key = handle_key,
                            .draw = draw,
                            .picture = picture};
    return &info->screen;
}
