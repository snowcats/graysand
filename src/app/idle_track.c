#include "idle_track.h"

#include "gfx/text.h"
#include "hal/display.h"
#include "ui/label.h"

enum {
    LEFT = 14,
    WIDTH = 240,
    SHADE_TOP = 138,
    SHADE_ALPHA = 150,
    STATE_TOP = 146,
    TITLE_TOP = 162,
    TITLE_HEIGHT = 26,
    ARTIST_TOP = 188,
    ARTIST_HEIGHT = 18,
    BAR_TOP = 210,
    BAR_WIDTH = 180,
    BAR_HEIGHT = 3,
    CLOCK_TOP = 26
};

static const Color WHITE = 0xFFFF;
static const Color BLACK = 0x0000;
static const Color DIM = 0xBDF7;
static const Color TRACK = 0x4208;

void idle_track_update(IdleTrack *idle, const Library *library, PlayerStatus status)
{
    bool has_track = status.has_track && !status.spoken;
    if (has_track == idle->has_track && status.track == idle->track) {
        return;
    }
    idle->has_track = has_track;
    idle->track = status.track;
    idle->title[0] = '\0';
    idle->artist[0] = '\0';
    LibraryTrack track;
    if (!has_track || !library_read_track(library, status.track, &track)) {
        return;
    }
    library_read_string(library, track.title, idle->title, sizeof idle->title);
    LibraryGroup artist;
    if (library_read_group(library, LIBRARY_ARTISTS, track.artist, &artist)) {
        library_read_string(library, artist.name, idle->artist, sizeof idle->artist);
    }
}

static int bar_filled(PlayerStatus status)
{
    if (status.duration_ms == 0) {
        return 0;
    }
    uint32_t position =
        status.position_ms < status.duration_ms ? status.position_ms : status.duration_ms;
    return (int)((uint64_t)position * BAR_WIDTH / status.duration_ms);
}

static void shade(Canvas *canvas, Rect area)
{
    Rect visible = rect_intersection(canvas->clip, area);
    for (int y = visible.y; y < visible.y + visible.height; y++) {
        int strength = SHADE_ALPHA * (y - area.y) / (area.height / 2);
        uint8_t alpha = (uint8_t)(strength < SHADE_ALPHA ? strength : SHADE_ALPHA);
        for (int x = visible.x; x < visible.x + visible.width; x++) {
            canvas_blend_pixel(canvas, (Point){x, y}, BLACK, alpha);
        }
    }
}

static void shadowed(Canvas *canvas, const Font *font, Color color, Rect area, const char *text)
{
    Rect below = {area.x + 1, area.y + 1, area.width, area.height};
    label_draw(canvas, (TextStyle){.font = font, .color = BLACK}, (Label){below, text});
    label_draw(canvas, (TextStyle){.font = font, .color = color}, (Label){area, text});
}

static void put_two_digits(char *text, int value)
{
    text[0] = (char)('0' + value / 10);
    text[1] = (char)('0' + value % 10);
}

static void draw_clock(const IdleTrack *idle, Canvas *canvas, const Theme *theme)
{
    if (!idle->clock_known) {
        return;
    }
    WallTime now = idle->clock;
    char time[] = "00:00";
    put_two_digits(time, now.hour);
    put_two_digits(time + 3, now.minute);
    int time_left = HAL_DISPLAY_WIDTH - LEFT - text_width(theme->fonts.title, time);
    shadowed(canvas, theme->fonts.title, WHITE,
             (Rect){time_left, CLOCK_TOP, HAL_DISPLAY_WIDTH - time_left, TITLE_HEIGHT}, time);
}

void idle_track_draw(const IdleTrack *idle, Canvas *canvas, const Theme *theme, PlayerStatus status)
{
    draw_clock(idle, canvas, theme);
    if (idle->title[0] == '\0') {
        return;
    }
    shade(canvas, (Rect){0, SHADE_TOP, WIDTH + LEFT, HAL_DISPLAY_HEIGHT - SHADE_TOP});
    const char *state = status.state == PLAYER_PLAYING ? "now playing" : "paused";
    shadowed(canvas, theme->fonts.small, DIM, (Rect){LEFT, STATE_TOP, WIDTH, ARTIST_HEIGHT}, state);
    shadowed(canvas, theme->fonts.row, WHITE, (Rect){LEFT, TITLE_TOP, WIDTH, TITLE_HEIGHT},
             idle->title);
    shadowed(canvas, theme->fonts.small, DIM, (Rect){LEFT, ARTIST_TOP, WIDTH, ARTIST_HEIGHT},
             idle->artist);
    canvas_fill_rect(canvas, (Rect){LEFT, BAR_TOP, BAR_WIDTH, BAR_HEIGHT}, TRACK);
    canvas_fill_rect(canvas, (Rect){LEFT, BAR_TOP, bar_filled(status), BAR_HEIGHT}, WHITE);
}

uint32_t idle_track_picture(const IdleTrack *idle, PlayerStatus status)
{
    uint32_t picture = 0;
    if (idle->clock_known) {
        picture = (uint32_t)(idle->clock.hour * 60 + idle->clock.minute) << 16;
    }
    if (idle->title[0] == '\0') {
        return picture;
    }
    return picture ^ (uint32_t)idle->track << 16 ^ (uint32_t)status.state << 8 ^
           (uint32_t)bar_filled(status);
}
