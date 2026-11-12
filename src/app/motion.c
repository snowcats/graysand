#include "motion.h"

#include "core/string_builder.h"
#include "hal/storage.h"
#include "home_art.h"

void motion_frame_path(const Motion *motion, int frame, char *path, int capacity)
{
    StringBuilder builder = string_builder_make(path, (size_t)capacity);
    string_builder_append(&builder, motion->folder);
    if (frame < 10) {
        string_builder_append(&builder, "0");
    }
    string_builder_append_unsigned(&builder, (uint32_t)frame);
    string_builder_append(&builder, ".bin");
}

static bool frame_present(const Motion *motion, int frame)
{
    char path[MOTION_PATH_CAPACITY];
    motion_frame_path(motion, frame, path, sizeof path);
    HalFile file;
    if (!hal_file_open(&file, path)) {
        return false;
    }
    bool right_size = file.size == motion->frame_bytes;
    hal_file_close(&file);
    return right_size;
}

bool motion_start(Motion *motion, const char *kind, ThemeChoice theme, uint32_t frame_ms,
                  bool loops, uint32_t frame_bytes, uint32_t now_ms)
{
    *motion = (Motion){
        .frame_ms = frame_ms, .started_ms = now_ms, .frame_bytes = frame_bytes, .loops = loops};
    StringBuilder builder = string_builder_make(motion->folder, sizeof motion->folder);
    string_builder_append(&builder, "/.echo/");
    string_builder_append(&builder, kind);
    string_builder_append(&builder, "/");
    string_builder_append(&builder, home_art_theme_folder(theme));
    string_builder_append(&builder, "/");
    while (motion->frames < MOTION_MAX_FRAMES && frame_present(motion, motion->frames)) {
        motion->frames++;
    }
    motion->active = motion->frames > 0;
    return motion->active;
}

void motion_stop(Motion *motion)
{
    motion->active = false;
}

bool motion_running(const Motion *motion, uint32_t now_ms)
{
    if (!motion->active) {
        return false;
    }
    return motion->loops ||
           now_ms - motion->started_ms < (uint32_t)motion->frames * motion->frame_ms;
}

int motion_frame(const Motion *motion, uint32_t now_ms)
{
    uint32_t step = (now_ms - motion->started_ms) / motion->frame_ms;
    if (motion->loops) {
        return (int)(step % (uint32_t)motion->frames);
    }
    return step < (uint32_t)motion->frames ? (int)step : motion->frames - 1;
}

void motion_draw(const Motion *motion, Canvas *canvas, Rect area, uint32_t now_ms)
{
    char path[MOTION_PATH_CAPACITY];
    motion_frame_path(motion, motion_frame(motion, now_ms), path, sizeof path);
    home_art_draw_file(path, canvas, area);
}
