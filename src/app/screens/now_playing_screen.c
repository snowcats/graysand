#include "now_playing_screen.h"

#include <stddef.h>

#include "cassette.h"
#include "core/format.h"
#include "core/string_builder.h"
#include "gfx/font_sized.h"
#include "gfx/image.h"
#include "hal/clock.h"
#include "hal/display.h"
#include "hal/power.h"
#include "home_art.h"
#include "settings/settings.h"
#include "ui/header.h"
#include "ui/icons.h"
#include "ui/label.h"
#include "ui/marquee.h"
#include "ui/status_bar.h"
#include "ui/theme_style.h"
#include "volume_blur.h"

enum {
    TIME_CAPACITY = 16,
    CASSETTE_PATH_CAPACITY = 32,
    BADGE_ICON_SIZE = 28,
    COVER_LEFT = 10,
    COVER_TOP = 39,
    COVER_SIZE = 146,
    COVER_CORNER = 5,
    SCANLINE_STEP = 3,
    COLUMN_LEFT = COVER_LEFT + COVER_SIZE + 16,
    COLUMN_RIGHT = 468,
    LINE_HEIGHT = 20,
    NAME_MIDDLE = 48,
    MIDDLE_TOP = 63,
    MIDDLE_HEIGHT = 100,
    INFO_MIDDLE = 178,
    INFO_TEXT = 15,
    INFO_SPACE = 16,
    FOOT_TOP = 198,
    FOOT_HEIGHT = 24,
    TIME_INSET = 9,
    TRACK_LEFT = 44,
    TRACK_RIGHT = 430,
    TRACK_MIDDLE = 211,
    TRACK_SEGMENTS = 56,
    TRIM_FIRST = 30,
    TRIM_STEP = 60,
    KNOB = 10,
    KNOB_MARK = 14,
    VOLUME_SEGMENTS = 12,
    VOLUME_SIDE = 16,
    VOLUME_TEXT_HEIGHT = 28,
    VOLUME_BAR_TOP = 58,
    VOLUME_BAR_HEIGHT = 10,
    VOLUME_BAR_GAP = 3,
    POSITION_STEP_MS = 250,
    STATUS_CAPACITY = 64,
    BITRATE_CAPACITY = 24,
    RATE_CAPACITY = 16,
    TAG_HEIGHT = 16,
    TAG_PAD = 7,
    TAG_SPLIT = 7,
    TAG_GAP = 10
};

typedef struct FooterLook {
    Color top;
    Color bottom;
    Color line;
    Color track;
    Color inner;
    Color fill;
    Color fill_bottom;
    uint8_t line_height;
    uint8_t track_height;
    uint8_t fill_height;
    uint8_t inset;
} FooterLook;

static const FooterLook FOOTERS[] = {
    [THEME_STYLE_PLAIN] = {COLOR_RGB(16, 16, 16), COLOR_RGB(30, 30, 30), COLOR_RGB(88, 88, 88),
                           COLOR_RGB(52, 54, 62), COLOR_RGB(52, 54, 62), COLOR_RGB(245, 245, 245),
                           COLOR_RGB(245, 245, 245), 1, 4, 4, 0},
    [THEME_STYLE_POKEDEX] = {COLOR_RGB(40, 34, 38), COLOR_RGB(40, 34, 38), COLOR_RGB(24, 20, 24),
                             COLOR_RGB(14, 12, 14), COLOR_RGB(70, 66, 72), COLOR_RGB(170, 255, 180),
                             COLOR_RGB(76, 220, 99), 2, 8, 5, 2},
    [THEME_STYLE_WIRED] = {COLOR_RGB(22, 12, 30), COLOR_RGB(22, 12, 30), COLOR_RGB(240, 140, 184),
                           COLOR_RGB(32, 48, 42), COLOR_RGB(32, 48, 42), COLOR_RGB(140, 255, 178),
                           COLOR_RGB(140, 255, 178), 1, 7, 7, 0},
    [THEME_STYLE_HYRULE] = {COLOR_RGB(14, 30, 21), COLOR_RGB(28, 52, 38), COLOR_RGB(226, 188, 100),
                            COLOR_RGB(71, 68, 40), COLOR_RGB(71, 68, 40), COLOR_RGB(226, 188, 100),
                            COLOR_RGB(226, 188, 100), 1, 1, 2, 0},
    [THEME_STYLE_ICE] = {COLOR_RGB(10, 24, 64), COLOR_RGB(24, 70, 170), COLOR_RGB(30, 66, 112),
                         COLOR_RGB(24, 48, 78), COLOR_RGB(24, 48, 78), COLOR_RGB(30, 110, 235),
                         COLOR_RGB(70, 160, 255), 1, 3, 4, 0},
};

static const uint8_t CORNER_CUT[COVER_CORNER] = {3, 2, 1, 1, 1};
static const uint8_t LYRIC_MIDDLE[NOW_PLAYING_LYRIC_LINES] = {73, 92, 113, 134, 153};

static const char SEPARATOR[] = " \xC2\xB7 ";

static const char NOTHING_PLAYING[] = "Nothing playing";
static const char PLAYER_ART[] = "/.echo/player/";
static const char VOLUME_ART[] = "/.echo/volume/";
static const char FULL_ART[] = "/.echo/full/";
static const char *const SHUFFLE_TEXT[SHUFFLE_MODE_COUNT] = {"", "Shuffle Songs", "Shuffle Albums"};
static const char *const REPEAT_TEXT[REPEAT_MODE_COUNT] = {"", "Repeat One", "Repeat All"};

const char *now_playing_codec_name(HalAudioCodec codec)
{
    switch (codec) {
    case HAL_AUDIO_CODEC_FLAC:
        return "FLAC";
    case HAL_AUDIO_CODEC_MP3:
        return "MP3";
    case HAL_AUDIO_CODEC_WAV:
        return "WAV";
    case HAL_AUDIO_CODEC_AAC:
        return "AAC";
    case HAL_AUDIO_CODEC_ALAC:
        return "ALAC";
    case HAL_AUDIO_CODEC_UNKNOWN:
        return "";
    }
    return "";
}

void now_playing_stream_text(HalAudioFormat format, char *buffer, size_t capacity)
{
    StringBuilder builder = string_builder_make(buffer, capacity);
    if (format.codec == HAL_AUDIO_CODEC_UNKNOWN || format.sample_rate == 0) {
        return;
    }
    if (format.bits_per_sample > 0) {
        string_builder_append_unsigned(&builder, format.bits_per_sample);
        string_builder_append(&builder, "-bit");
        string_builder_append(&builder, SEPARATOR);
    }
    char rate[RATE_CAPACITY];
    format_sample_rate(format.sample_rate, rate, sizeof rate);
    string_builder_append(&builder, rate);
}

void now_playing_tag_text(HalAudioFormat format, char *buffer, size_t capacity)
{
    StringBuilder builder = string_builder_make(buffer, capacity);
    if (format.bits_per_sample > 0) {
        string_builder_append_unsigned(&builder, format.bits_per_sample);
        string_builder_append_char(&builder, '/');
    }
    string_builder_append_unsigned(&builder, format.sample_rate / 1000u);
    if (format.sample_rate % 1000u >= 100u) {
        string_builder_append_char(&builder, '.');
        string_builder_append_unsigned(&builder, format.sample_rate % 1000u / 100u);
    }
}

void now_playing_detail_text(const PlayerStatus *status, char *buffer, size_t capacity)
{
    StringBuilder builder = string_builder_make(buffer, capacity);
    bool has_bitrate =
        status->format.codec != HAL_AUDIO_CODEC_UNKNOWN && status->format.bitrate_kbps > 0;
    if (has_bitrate) {
        char bitrate[BITRATE_CAPACITY];
        format_bitrate(status->format.bitrate_kbps, bitrate, sizeof bitrate);
        string_builder_append(&builder, bitrate);
    }
    if (status->place.total == 0) {
        return;
    }
    if (has_bitrate) {
        string_builder_append(&builder, SEPARATOR);
    }
    string_builder_append_unsigned(&builder, status->place.number);
    string_builder_append(&builder, " of ");
    string_builder_append_unsigned(&builder, status->place.total);
}

static NowPlayingCommand by_gesture(KeyGesture gesture, NowPlayingCommand press,
                                    NowPlayingCommand hold)
{
    switch (gesture) {
    case KEY_GESTURE_PRESS:
        return press;
    case KEY_GESTURE_LONG_PRESS:
        return hold;
    case KEY_GESTURE_REPEAT:
        return NOW_PLAYING_NONE;
    }
    return NOW_PLAYING_NONE;
}

uint32_t now_playing_scrub_target(uint32_t position_ms, uint32_t duration_ms, int32_t delta_ms)
{
    int64_t target = (int64_t)position_ms + delta_ms;
    if (target < 0) {
        return 0;
    }
    return target > duration_ms ? duration_ms : (uint32_t)target;
}

static NowPlayingCommand scrub_command_for(KeyEvent event)
{
    bool held = event.gesture != KEY_GESTURE_PRESS;
    switch (event.key) {
    case KEY_VOLUME_UP:
        return held ? NOW_PLAYING_SCRUB_FORWARD_FAST : NOW_PLAYING_SCRUB_FORWARD;
    case KEY_VOLUME_DOWN:
        return held ? NOW_PLAYING_SCRUB_BACK_FAST : NOW_PLAYING_SCRUB_BACK;
    case KEY_PLAY:
        return by_gesture(event.gesture, NOW_PLAYING_SCRUB_COMMIT, NOW_PLAYING_NONE);
    case KEY_BACK:
        return by_gesture(event.gesture, NOW_PLAYING_SCRUB_CANCEL, NOW_PLAYING_HOME);
    case KEY_MENU:
    case KEY_POWER:
        return NOW_PLAYING_NONE;
    }
    return NOW_PLAYING_NONE;
}

NowPlayingCommand now_playing_command_for(KeyEvent event, bool scrubbing)
{
    if (scrubbing) {
        return scrub_command_for(event);
    }
    switch (event.key) {
    case KEY_VOLUME_UP:
        return by_gesture(event.gesture, NOW_PLAYING_VOLUME_UP, NOW_PLAYING_NEXT);
    case KEY_VOLUME_DOWN:
        return by_gesture(event.gesture, NOW_PLAYING_VOLUME_DOWN, NOW_PLAYING_PREVIOUS);
    case KEY_PLAY:
        return by_gesture(event.gesture, NOW_PLAYING_TOGGLE_PAUSE, NOW_PLAYING_SCRUB_START);
    case KEY_BACK:
        return by_gesture(event.gesture, NOW_PLAYING_LEAVE, NOW_PLAYING_HOME);
    case KEY_MENU:
        return by_gesture(event.gesture, NOW_PLAYING_UP_NEXT, NOW_PLAYING_NONE);
    case KEY_POWER:
        return NOW_PLAYING_NONE;
    }
    return NOW_PLAYING_NONE;
}

int now_playing_progress_width(uint32_t position_ms, uint32_t duration_ms, int width)
{
    if (duration_ms == 0) {
        return 0;
    }
    if (position_ms >= duration_ms) {
        return width;
    }
    return (int)((uint64_t)position_ms * (uint64_t)width / duration_ms);
}

NowPlayingFeedback now_playing_feedback_for(NowPlayingCommand command, PlayerState after)
{
    switch (command) {
    case NOW_PLAYING_VOLUME_UP:
    case NOW_PLAYING_VOLUME_DOWN:
        return NOW_PLAYING_FEEDBACK_VOLUME;
    case NOW_PLAYING_NEXT:
        return NOW_PLAYING_FEEDBACK_NEXT;
    case NOW_PLAYING_PREVIOUS:
        return NOW_PLAYING_FEEDBACK_PREVIOUS;
    case NOW_PLAYING_TOGGLE_PAUSE:
        return after == PLAYER_PAUSED ? NOW_PLAYING_FEEDBACK_PAUSE : NOW_PLAYING_FEEDBACK_PLAY;
    default:
        return NOW_PLAYING_FEEDBACK_NONE;
    }
}

bool now_playing_feedback_visible(NowPlayingFeedback feedback, uint32_t started_ms, uint32_t now_ms)
{
    if (feedback == NOW_PLAYING_FEEDBACK_NONE) {
        return false;
    }
    uint32_t duration = feedback == NOW_PLAYING_FEEDBACK_VOLUME ? NOW_PLAYING_VOLUME_FEEDBACK_MS
                                                                : NOW_PLAYING_BADGE_MS;
    return now_ms - started_ms < duration;
}

int now_playing_volume_width(uint8_t volume, int width)
{
    uint8_t level = volume < SETTINGS_VOLUME_MAX ? volume : SETTINGS_VOLUME_MAX;
    return level * width / SETTINGS_VOLUME_MAX;
}

static int32_t skip_step_ms(const NowPlayingScreen *now_playing)
{
    const Settings *settings = now_playing->config.settings;
    if (settings == NULL) {
        return NOW_PLAYING_SCRUB_STEP_MS;
    }
    return (int32_t)settings_skip_ms(settings);
}

static void move_scrub(NowPlayingScreen *now_playing, int32_t delta_ms)
{
    now_playing->scrub_position_ms = now_playing_scrub_target(
        now_playing->scrub_position_ms, now_playing->status.duration_ms, delta_ms);
}

static void start_scrub(NowPlayingScreen *now_playing)
{
    now_playing->scrubbing = now_playing->status.has_track;
    now_playing->scrub_position_ms = now_playing->status.position_ms;
}

static void commit_scrub(NowPlayingScreen *now_playing)
{
    player_seek(now_playing->config.player, now_playing->scrub_position_ms);
    now_playing->scrubbing = false;
}

static ScreenAction run_scrub_command(NowPlayingScreen *now_playing, NowPlayingCommand command)
{
    switch (command) {
    case NOW_PLAYING_SCRUB_START:
        start_scrub(now_playing);
        break;
    case NOW_PLAYING_SCRUB_FORWARD:
    case NOW_PLAYING_SCRUB_FORWARD_FAST:
        move_scrub(now_playing, skip_step_ms(now_playing));
        break;
    case NOW_PLAYING_SCRUB_BACK:
    case NOW_PLAYING_SCRUB_BACK_FAST:
        move_scrub(now_playing, -skip_step_ms(now_playing));
        break;
    case NOW_PLAYING_SCRUB_COMMIT:
        commit_scrub(now_playing);
        break;
    default:
        now_playing->scrubbing = false;
        break;
    }
    return screen_action_none();
}

static ScreenAction run_command(NowPlayingScreen *now_playing, NowPlayingCommand command)
{
    Player *player = now_playing->config.player;
    switch (command) {
    case NOW_PLAYING_TOGGLE_PAUSE:
        player_toggle_pause(player);
        break;
    case NOW_PLAYING_NEXT:
        player_skip(player, 1);
        break;
    case NOW_PLAYING_PREVIOUS:
        player_skip(player, -1);
        break;
    case NOW_PLAYING_VOLUME_UP:
        player_change_volume(player, 1);
        break;
    case NOW_PLAYING_VOLUME_DOWN:
        player_change_volume(player, -1);
        break;
    case NOW_PLAYING_LEAVE:
        return screen_action_pop();
    case NOW_PLAYING_UP_NEXT:
        return now_playing->config.up_next != NULL ? screen_action_push(now_playing->config.up_next)
                                                   : screen_action_none();
    case NOW_PLAYING_HOME:
        now_playing->scrubbing = false;
        return screen_action_pop_to_root();
    case NOW_PLAYING_SCRUB_START:
    case NOW_PLAYING_SCRUB_FORWARD:
    case NOW_PLAYING_SCRUB_FORWARD_FAST:
    case NOW_PLAYING_SCRUB_BACK:
    case NOW_PLAYING_SCRUB_BACK_FAST:
    case NOW_PLAYING_SCRUB_COMMIT:
    case NOW_PLAYING_SCRUB_CANCEL:
        return run_scrub_command(now_playing, command);
    case NOW_PLAYING_NONE:
        break;
    }
    return screen_action_none();
}

static bool lyrics_wanted(const NowPlayingScreen *now_playing)
{
    const Settings *settings = now_playing->config.settings;
    return settings != NULL && settings->lyrics && now_playing->status.has_track;
}

static void load_lyrics_file(NowPlayingScreen *now_playing, uint16_t track_index)
{
    const Library *library = now_playing->config.library;
    LibraryTrack track;
    char relative[PLAYER_PATH_CAPACITY];
    char path[PLAYER_PATH_CAPACITY];
    lyrics_clear();
    if (library == NULL || !library_read_track(library, track_index, &track) ||
        !library_read_string(library, track.path, relative, sizeof relative)) {
        return;
    }
    StringBuilder builder = string_builder_make(path, sizeof path);
    string_builder_append_char(&builder, '/');
    string_builder_append(&builder, relative);
    if (!builder.overflowed) {
        lyrics_load(path);
    }
}

static uint32_t shown_position(const NowPlayingScreen *now_playing)
{
    return now_playing->scrubbing ? now_playing->scrub_position_ms
                                  : now_playing->status.position_ms;
}

static void remember_lyrics(NowPlayingScreen *now_playing)
{
    if (!lyrics_wanted(now_playing)) {
        if (now_playing->lyrics_ready) {
            lyrics_clear();
            now_playing->lyrics_ready = false;
        }
        now_playing->has_lyrics = false;
        return;
    }
    if (!now_playing->lyrics_ready) {
        load_lyrics_file(now_playing, now_playing->status.track);
        now_playing->lyrics_ready = true;
        now_playing->lyric_from = 1;
        now_playing->lyric_until = 0;
    }
    uint32_t position = shown_position(now_playing);
    if (position >= now_playing->lyric_from && position < now_playing->lyric_until) {
        return;
    }
    now_playing->has_lyrics = lyrics_window(position, now_playing->lyrics, NOW_PLAYING_LYRIC_LINES,
                                            &now_playing->lyric_from, &now_playing->lyric_until);
}

static void load_texts(NowPlayingScreen *now_playing, uint16_t track_index)
{
    const Library *library = now_playing->config.library;
    LibraryTrack track;
    LibraryAlbum album;
    LibraryGroup artist;
    now_playing->title[0] = '\0';
    now_playing->artist[0] = '\0';
    now_playing->album[0] = '\0';
    if (!library_read_track(library, track_index, &track)) {
        return;
    }
    library_read_string(library, track.title, now_playing->title, NOW_PLAYING_TEXT_CAPACITY);
    if (library_read_group(library, LIBRARY_ARTISTS, track.artist, &artist)) {
        library_read_string(library, artist.name, now_playing->artist, NOW_PLAYING_TEXT_CAPACITY);
    }
    now_playing->album_index = track.album;
    char path[PLAYER_PATH_CAPACITY];
    now_playing->file_name[0] = '\0';
    if (library_read_string(library, track.path, path, sizeof path)) {
        const char *name = path;
        for (const char *letter = path; *letter != '\0'; letter++) {
            name = *letter == '/' ? letter + 1 : name;
        }
        StringBuilder builder =
            string_builder_make(now_playing->file_name, NOW_PLAYING_TEXT_CAPACITY);
        string_builder_append(&builder, name);
    }
    if (library_read_album(library, track.album, &album)) {
        library_read_string(library, album.title, now_playing->album, NOW_PLAYING_TEXT_CAPACITY);
    }
}

static bool showing(const NowPlayingScreen *now_playing, NowPlayingFeedback feedback)
{
    return now_playing->feedback == feedback &&
           now_playing_feedback_visible(feedback, now_playing->feedback_started_ms,
                                        now_playing->now_ms);
}

static void refresh(NowPlayingScreen *now_playing, uint32_t now_ms)
{
    now_playing->now_ms = now_ms;
    now_playing->status = player_status(now_playing->config.player);
    bool changed = !now_playing->has_text || now_playing->text_track != now_playing->status.track;
    if (now_playing->status.has_track && changed) {
        load_texts(now_playing, now_playing->status.track);
        now_playing->has_text = true;
        now_playing->text_track = now_playing->status.track;
        now_playing->text_changed_ms = now_ms;
        now_playing->lyrics_ready = false;
    }
    if (!now_playing->status.has_track) {
        now_playing->lyrics_ready = false;
    }
    remember_lyrics(now_playing);
    now_playing->screen.full_screen =
        now_playing->status.has_track &&
        (now_playing->full ||
         (now_playing->volume_art && showing(now_playing, NOW_PLAYING_FEEDBACK_VOLUME)));
    if (!now_playing->full) {
        return;
    }
    WallTime time;
    StringBuilder builder = string_builder_make(now_playing->clock, sizeof now_playing->clock);
    if (hal_clock_wall_time(&time)) {
        string_builder_append_char(&builder, (char)('0' + time.hour / 10));
        string_builder_append_char(&builder, (char)('0' + time.hour % 10));
        string_builder_append_char(&builder, ':');
        string_builder_append_char(&builder, (char)('0' + time.minute / 10));
        string_builder_append_char(&builder, (char)('0' + time.minute % 10));
    }
    HalBattery battery = hal_power_battery();
    status_bar_percent_text(battery.percent, now_playing->battery, sizeof now_playing->battery);
    now_playing->battery_permille = battery.known ? (uint16_t)(battery.percent * 10u) : 0;
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    NowPlayingScreen *now_playing = context;
    NowPlayingCommand command = now_playing_command_for(event, now_playing->scrubbing);
    ScreenAction action = run_command(now_playing, command);
    refresh(now_playing, now_ms);
    NowPlayingFeedback feedback = now_playing_feedback_for(command, now_playing->status.state);
    if (feedback != NOW_PLAYING_FEEDBACK_NONE) {
        now_playing->feedback = feedback;
        now_playing->feedback_started_ms = now_ms;
    }
    return action;
}

static void art_path(const NowPlayingScreen *now_playing, const char *kind, int level, char *path)
{
    StringBuilder builder = string_builder_make(path, CASSETTE_PATH_CAPACITY);
    string_builder_append(&builder, kind);
    string_builder_append(&builder, home_art_theme_folder(now_playing->config.settings->theme));
    if (level >= 0) {
        string_builder_append(&builder, "/");
        string_builder_append_unsigned(&builder, (uint32_t)level);
    }
    string_builder_append(&builder, ".bin");
}

static bool art_ready(const NowPlayingScreen *now_playing, const char *kind, int level)
{
    char path[CASSETTE_PATH_CAPACITY];
    art_path(now_playing, kind, level, path);
    HalFile file;
    if (!hal_file_open(&file, path)) {
        return false;
    }
    bool whole = file.size >= (uint32_t)HAL_DISPLAY_WIDTH * HAL_DISPLAY_HEIGHT * sizeof(Color);
    hal_file_close(&file);
    return whole;
}

static void enter(void *context, uint32_t now_ms)
{
    NowPlayingScreen *now_playing = context;
    const Settings *settings = now_playing->config.settings;
    now_playing->cassette = settings != NULL && settings->player_style != PLAYER_STYLE_COVER &&
                            art_ready(now_playing, PLAYER_ART, -1);
    now_playing->full = settings != NULL && settings->player_style == PLAYER_STYLE_CASSETTE &&
                        art_ready(now_playing, FULL_ART, -1);
    now_playing->slot_count = 0;
    if (now_playing->full || now_playing->cassette) {
        char path[CASSETTE_PATH_CAPACITY];
        art_path(now_playing, now_playing->full ? FULL_ART : PLAYER_ART, -1, path);
        now_playing->slot_count =
            art_slots_load(path, (uint32_t)HAL_DISPLAY_WIDTH * HAL_DISPLAY_HEIGHT * sizeof(Color),
                           now_playing->slots, ART_SLOT_CAPACITY);
    }
    now_playing->volume_art = settings != NULL && art_ready(now_playing, VOLUME_ART, 0);
    now_playing->has_text = false;
    now_playing->lyrics_ready = false;
    now_playing->scrubbing = false;
    now_playing->feedback = NOW_PLAYING_FEEDBACK_NONE;
    refresh(now_playing, now_ms);
}

static void update(void *context, uint32_t now_ms)
{
    NowPlayingScreen *now_playing = context;
    refresh(now_playing, now_ms);
    if (now_playing->config.covers != NULL && now_playing->status.has_track) {
        cover_cache_load_full(now_playing->config.covers, now_playing->album_index);
    }
}

static bool current_cover(const NowPlayingScreen *now_playing, Image *image)
{
    const CoverCache *covers = now_playing->config.covers;
    return covers != NULL && now_playing->has_text &&
           cover_cache_full(covers, now_playing->album_index, image) == COVER_LOADED;
}

Rect now_playing_cover_area(const Theme *theme)
{
    (void)theme;
    return (Rect){COVER_LEFT, COVER_TOP, COVER_SIZE, COVER_SIZE};
}

static Rect column(int middle)
{
    return (Rect){COLUMN_LEFT, middle - LINE_HEIGHT / 2, COLUMN_RIGHT - COLUMN_LEFT, LINE_HEIGHT};
}

static Color highlight(const Theme *theme)
{
    return theme->style == THEME_STYLE_WIRED ? theme->colors.header_text
                                             : theme_style_mark_color(theme);
}

static Color accent(const Theme *theme)
{
    return theme->style == THEME_STYLE_ICE ? theme->colors.accent : highlight(theme);
}

static bool draw_text_line(const NowPlayingScreen *now_playing, Canvas *canvas, Label label,
                           TextStyle style)
{
    int width = text_width(style.font, label.text);
    if (width <= label.area.width) {
        label_draw(canvas, style, label);
        return false;
    }
    MarqueeTiming timing = now_playing->config.theme->marquee;
    uint32_t elapsed = now_playing->now_ms - now_playing->text_changed_ms;
    int offset = marquee_offset(timing, width, label.area.width, elapsed);
    label_draw_scrolling(canvas, style, label, (Marquee){.offset = offset, .gap = timing.gap_px});
    return true;
}

static void draw_right_aligned(Canvas *canvas, TextStyle style, Rect area, const char *text)
{
    int width = text_width(style.font, text);
    Rect right = {area.x + area.width - width, area.y, width, area.height};
    label_draw(canvas, style, (Label){.area = right, .text = text});
}

static Color quiet(const Theme *theme, uint8_t alpha)
{
    return color_blend(theme->colors.background, theme->colors.header_text, alpha);
}

static void finish_cover(Canvas *canvas, const Theme *theme, Rect area)
{
    if (theme->style == THEME_STYLE_WIRED) {
        for (int y = area.y; y < area.y + area.height; y += SCANLINE_STEP) {
            for (int x = area.x; x < area.x + area.width; x++) {
                canvas_blend_pixel(canvas, (Point){x, y}, COLOR_RGB(20, 8, 26), 9);
            }
        }
        return;
    }
    if (theme->style == THEME_STYLE_HYRULE) {
        return;
    }
    Color outside = theme->colors.background;
    for (int row = 0; row < COVER_CORNER; row++) {
        int cut = CORNER_CUT[row];
        int right = area.x + area.width - cut;
        int top = area.y + row;
        int bottom = area.y + area.height - 1 - row;
        canvas_fill_rect(canvas, (Rect){area.x, top, cut, 1}, outside);
        canvas_fill_rect(canvas, (Rect){right, top, cut, 1}, outside);
        canvas_fill_rect(canvas, (Rect){area.x, bottom, cut, 1}, outside);
        canvas_fill_rect(canvas, (Rect){right, bottom, cut, 1}, outside);
    }
}

static void draw_art(const NowPlayingScreen *now_playing, Canvas *canvas)
{
    const Theme *theme = now_playing->config.theme;
    Rect area = now_playing_cover_area(theme);
    Image cover;
    if (now_playing->status.has_track && current_cover(now_playing, &cover)) {
        image_draw_quad(canvas, cover,
                        (Quad){area.x, area.width, area.y, area.height, area.y, area.height});
    } else {
        canvas_fill_rect(canvas, area, quiet(theme, 3));
        canvas_fill_rect(canvas, (Rect){area.x + 1, area.y + 1, area.width - 2, area.height - 2},
                         quiet(theme, 1));
        int size = area.width * 2 / 5;
        header_mark(
            canvas,
            (Rect){area.x + (area.width - size) / 2, area.y + (area.height - size) / 2, size, size},
            theme->style, quiet(theme, 9));
    }
    finish_cover(canvas, theme, area);
}

static void modes_text(const PlayerStatus *status, char *buffer, size_t capacity)
{
    StringBuilder builder = string_builder_make(buffer, capacity);
    string_builder_append(&builder, SHUFFLE_TEXT[status->shuffle]);
    if (status->shuffle != SHUFFLE_OFF && status->repeat != REPEAT_OFF) {
        string_builder_append(&builder, SEPARATOR);
    }
    string_builder_append(&builder, REPEAT_TEXT[status->repeat]);
}

static void draw_badge_icon(Canvas *canvas, NowPlayingFeedback feedback, Rect area, Color color)
{
    switch (feedback) {
    case NOW_PLAYING_FEEDBACK_NEXT:
        icon_draw_next(canvas, area, color);
        return;
    case NOW_PLAYING_FEEDBACK_PREVIOUS:
        icon_draw_previous(canvas, area, color);
        return;
    case NOW_PLAYING_FEEDBACK_PAUSE:
        icon_draw_pause(canvas, area, color);
        return;
    case NOW_PLAYING_FEEDBACK_PLAY:
        icon_draw_play(canvas, area, color);
        return;
    default:
        return;
    }
}

static void draw_lyrics(const NowPlayingScreen *now_playing, Canvas *canvas)
{
    const Theme *theme = now_playing->config.theme;
    int middle = NOW_PLAYING_LYRIC_LINES / 2;
    for (int index = 0; index < NOW_PLAYING_LYRIC_LINES; index++) {
        int away = index < middle ? middle - index : index - middle;
        TextStyle style = {.font = away == 0 ? font_sized(14, true) : font_sized(12, false),
                           .color = away == 0
                                        ? highlight(theme)
                                        : color_blend(theme->colors.background, theme->colors.text,
                                                      away == 1 ? 10 : 6)};
        label_draw(canvas, style, (Label){column(LYRIC_MIDDLE[index]), now_playing->lyrics[index]});
    }
}

static void draw_details(const NowPlayingScreen *now_playing, Canvas *canvas)
{
    const Theme *theme = now_playing->config.theme;
    char lines[3][STATUS_CAPACITY];
    char rate[STATUS_CAPACITY];
    HalAudioFormat format = now_playing->status.format;
    const char *codec = now_playing_codec_name(format.codec);
    now_playing_stream_text(format, rate, sizeof rate);
    StringBuilder builder = string_builder_make(lines[0], sizeof lines[0]);
    string_builder_append(&builder, codec);
    if (codec[0] != '\0' && rate[0] != '\0') {
        string_builder_append(&builder, SEPARATOR);
    }
    string_builder_append(&builder, rate);
    now_playing_detail_text(&now_playing->status, lines[1], sizeof lines[1]);
    modes_text(&now_playing->status, lines[2], sizeof lines[2]);
    TextStyle style = {.font = font_sized(12, false), .color = theme->colors.text_secondary};
    int row = 1;
    for (int index = 0; index < 3; index++) {
        if (lines[index][0] != '\0') {
            label_draw(canvas, style, (Label){column(LYRIC_MIDDLE[row]), lines[index]});
            row++;
        }
    }
}

static void draw_middle(const NowPlayingScreen *now_playing, Canvas *canvas)
{
    const Theme *theme = now_playing->config.theme;
    Rect area = {COLUMN_LEFT, MIDDLE_TOP, COLUMN_RIGHT - COLUMN_LEFT, MIDDLE_HEIGHT};
    NowPlayingFeedback feedback = now_playing->feedback;
    if (feedback != NOW_PLAYING_FEEDBACK_NONE && feedback != NOW_PLAYING_FEEDBACK_VOLUME &&
        showing(now_playing, feedback)) {
        Rect icon = {area.x + (area.width - BADGE_ICON_SIZE) / 2,
                     area.y + (area.height - BADGE_ICON_SIZE) / 2, BADGE_ICON_SIZE,
                     BADGE_ICON_SIZE};
        draw_badge_icon(canvas, feedback, icon, theme->colors.header_text);
    } else if (now_playing->has_lyrics) {
        draw_lyrics(now_playing, canvas);
    } else {
        draw_details(now_playing, canvas);
    }
}

static void draw_mic(Canvas *canvas, int x, int y, Color color)
{
    canvas_fill_rect(canvas, (Rect){x + 3, y + 1, 4, 7}, color);
    canvas_fill_rect(canvas, (Rect){x + 1, y + 6, 1, 3}, color);
    canvas_fill_rect(canvas, (Rect){x + 8, y + 6, 1, 3}, color);
    canvas_fill_rect(canvas, (Rect){x + 2, y + 9, 6, 1}, color);
    canvas_fill_rect(canvas, (Rect){x + 4, y + 10, 2, 2}, color);
    canvas_fill_rect(canvas, (Rect){x + 2, y + 12, 6, 1}, color);
}

static void draw_disc(Canvas *canvas, int x, int y, const Theme *theme, Color color)
{
    Point center = {x + 6, y + 7};
    header_fill_disc(canvas, center, 6, color);
    header_fill_disc(canvas, center, 1, theme->colors.background);
}

static bool draw_info(const NowPlayingScreen *now_playing, Canvas *canvas)
{
    const Theme *theme = now_playing->config.theme;
    Rect row = column(INFO_MIDDLE);
    if (now_playing->scrubbing) {
        TextStyle seek = {.font = font_sized(11, true), .color = theme->colors.header_text};
        label_draw(canvas, seek, (Label){row, "Seeking \xC2\xB7 Play to jump"});
        return false;
    }
    int icon_y = INFO_MIDDLE - 7;
    Color tint = accent(theme);
    TextStyle artist = {.font = font_sized(11, true), .color = theme->colors.text};
    TextStyle album = {.font = font_sized(11, false), .color = theme->colors.text_secondary};
    int artist_width = text_width(artist.font, now_playing->artist);
    if (artist_width > row.width * 2 / 5) {
        artist_width = row.width * 2 / 5;
    }
    draw_mic(canvas, row.x, icon_y, tint);
    int x = row.x + INFO_TEXT;
    bool artist_scrolls = draw_text_line(
        now_playing, canvas,
        (Label){(Rect){x, row.y, artist_width, row.height}, now_playing->artist}, artist);
    x += artist_width + INFO_SPACE;
    draw_disc(canvas, x - 1, icon_y, theme, tint);
    x += INFO_TEXT;
    bool album_scrolls = draw_text_line(
        now_playing, canvas,
        (Label){(Rect){x, row.y, row.x + row.width - x, row.height}, now_playing->album}, album);
    return artist_scrolls || album_scrolls;
}

static int draw_format_tag(const NowPlayingScreen *now_playing, Canvas *canvas, Rect line)
{
    const Theme *theme = now_playing->config.theme;
    HalAudioFormat format = now_playing->status.format;
    const char *codec = now_playing_codec_name(format.codec);
    if (now_playing->config.settings == NULL || !now_playing->config.settings->format_tag ||
        codec[0] == '\0' || format.sample_rate == 0) {
        return 0;
    }
    char rest[RATE_CAPACITY];
    now_playing_tag_text(format, rest, sizeof rest);
    TextStyle strong = {.font = font_sized(10, true), .color = accent(theme)};
    TextStyle soft = {.font = font_sized(10, false), .color = theme->colors.text_secondary};
    int codec_width = text_width(strong.font, codec);
    int rest_width = text_width(soft.font, rest);
    int width = TAG_PAD + codec_width + TAG_SPLIT + rest_width + TAG_PAD;
    Rect box = {line.x + line.width - width, NAME_MIDDLE - TAG_HEIGHT / 2, width, TAG_HEIGHT};
    Color edge = color_blend(theme->colors.background, strong.color, 7);
    canvas_fill_rect(canvas, (Rect){box.x + 1, box.y, box.width - 2, 1}, edge);
    canvas_fill_rect(canvas, (Rect){box.x + 1, box.y + box.height - 1, box.width - 2, 1}, edge);
    canvas_fill_rect(canvas, (Rect){box.x, box.y + 1, 1, box.height - 2}, edge);
    canvas_fill_rect(canvas, (Rect){box.x + box.width - 1, box.y + 1, 1, box.height - 2}, edge);
    int x = box.x + TAG_PAD;
    label_draw(canvas, strong, (Label){(Rect){x, box.y, codec_width, box.height}, codec});
    x += codec_width + TAG_SPLIT / 2;
    canvas_fill_rect(canvas, (Rect){x, box.y + 4, 1, box.height - 8}, edge);
    x += TAG_SPLIT - TAG_SPLIT / 2;
    label_draw(canvas, soft, (Label){(Rect){x, box.y, rest_width, box.height}, rest});
    return width + TAG_GAP;
}

static void fill_bar(Canvas *canvas, int x, int width, int height, Color top, Color bottom)
{
    canvas_fill_gradient(canvas, (Rect){x, TRACK_MIDDLE - (height + 1) / 2, width, height}, top,
                         bottom);
}

static void draw_track(const Theme *theme, Canvas *canvas, int filled)
{
    const FooterLook *look = &FOOTERS[theme->style];
    int width = TRACK_RIGHT - TRACK_LEFT;
    if (theme->style == THEME_STYLE_WIRED) {
        int lit = filled * TRACK_SEGMENTS / width;
        for (int segment = 0; segment < TRACK_SEGMENTS; segment++) {
            int x = TRACK_LEFT + segment * width / TRACK_SEGMENTS;
            int next = TRACK_LEFT + (segment + 1) * width / TRACK_SEGMENTS;
            Color color = segment < lit ? look->fill : look->track;
            fill_bar(canvas, x, next - x - 2, look->track_height, color, color);
        }
        return;
    }
    fill_bar(canvas, TRACK_LEFT, width, look->track_height, look->track, look->track);
    fill_bar(canvas, TRACK_LEFT + 1, width - 2, look->track_height - 2, look->inner, look->inner);
    fill_bar(canvas, TRACK_LEFT + look->inset, filled - look->inset, look->fill_height, look->fill,
             look->fill_bottom);
    Point knob = {TRACK_LEFT + filled, TRACK_MIDDLE};
    if (theme->style == THEME_STYLE_PLAIN) {
        header_fill_disc(canvas, knob, KNOB / 2, COLOR_RGB(255, 255, 255));
    } else if (theme->style != THEME_STYLE_POKEDEX) {
        header_mark(
            canvas,
            (Rect){knob.x - KNOB_MARK / 2, knob.y - KNOB_MARK / 2 - 1, KNOB_MARK, KNOB_MARK},
            theme->style, look->fill_bottom);
    }
}

static void draw_timeline(const NowPlayingScreen *now_playing, Canvas *canvas)
{
    const Theme *theme = now_playing->config.theme;
    const FooterLook *look = &FOOTERS[theme->style];
    uint32_t position_ms = shown_position(now_playing);
    uint32_t duration_ms = now_playing->status.duration_ms;
    uint32_t remaining_ms = duration_ms > position_ms ? duration_ms - position_ms : 0;
    char elapsed[TIME_CAPACITY];
    char remaining[TIME_CAPACITY] = "-";
    format_duration(position_ms, elapsed, sizeof elapsed);
    format_duration(remaining_ms, remaining + 1, sizeof remaining - 1);
    canvas_fill_gradient(canvas, (Rect){0, FOOT_TOP, HAL_DISPLAY_WIDTH, FOOT_HEIGHT}, look->top,
                         look->bottom);
    canvas_fill_rect(canvas, (Rect){0, FOOT_TOP, HAL_DISPLAY_WIDTH, look->line_height}, look->line);
    for (int x = TRIM_FIRST; theme->style == THEME_STYLE_HYRULE && x < HAL_DISPLAY_WIDTH;
         x += TRIM_STEP) {
        canvas_fill_rect(canvas, (Rect){x - 1, FOOT_TOP - 1, 3, 3}, look->line);
        canvas_fill_rect(canvas, (Rect){x, FOOT_TOP - 2, 1, 5}, look->line);
        canvas_fill_rect(canvas, (Rect){x, FOOT_TOP, 1, 1}, look->top);
    }
    Rect row = {TIME_INSET, FOOT_TOP, HAL_DISPLAY_WIDTH - 2 * TIME_INSET, FOOT_HEIGHT};
    TextStyle times = {.font = font_sized(11, true), .color = theme->colors.text};
    label_draw(canvas, times, (Label){.area = row, .text = elapsed});
    draw_right_aligned(canvas, times, row, remaining);
    int width = TRACK_RIGHT - TRACK_LEFT;
    draw_track(theme, canvas, now_playing_progress_width(position_ms, duration_ms, width));
}

static void draw_cassette(NowPlayingScreen *now_playing, Canvas *canvas)
{
    const PlayerStatus *status = &now_playing->status;
    const Theme *theme = now_playing->config.theme;
    char path[CASSETTE_PATH_CAPACITY];
    art_path(now_playing, now_playing->full ? FULL_ART : PLAYER_ART, -1, path);
    home_art_draw_file(path, canvas, (Rect){0, 0, HAL_DISPLAY_WIDTH, HAL_DISPLAY_HEIGHT});
    if (!now_playing->full) {
        header_draw_centered(canvas, theme, now_playing->screen.title);
        draw_timeline(now_playing, canvas);
    }
    uint32_t position_ms = shown_position(now_playing);
    uint32_t remaining_ms =
        status->duration_ms > position_ms ? status->duration_ms - position_ms : 0;
    char elapsed[TIME_CAPACITY];
    char remaining[TIME_CAPACITY] = "-";
    char stream[NOW_PLAYING_TEXT_CAPACITY];
    char rate[RATE_CAPACITY];
    char format[NOW_PLAYING_TEXT_CAPACITY];
    format_duration(position_ms, elapsed, sizeof elapsed);
    format_duration(remaining_ms, remaining + 1, sizeof remaining - 1);
    bool known = status->format.codec != HAL_AUDIO_CODEC_UNKNOWN;
    now_playing_stream_text(status->format, stream, sizeof stream);
    now_playing_tag_text(status->format, rate, sizeof rate);
    const char *codec = known ? now_playing_codec_name(status->format.codec) : NULL;
    StringBuilder builder = string_builder_make(format, sizeof format);
    string_builder_append(&builder, codec != NULL ? codec : "");
    string_builder_append(&builder, SEPARATOR);
    string_builder_append(&builder, stream);
    ArtSlotValues values = {
        .texts = {[ART_FILE] = now_playing->file_name[0] != '\0' ? now_playing->file_name
                                                                 : now_playing->title,
                  [ART_CLOCK] = now_playing->clock,
                  [ART_BATTERY] = now_playing->battery,
                  [ART_ELAPSED] = elapsed,
                  [ART_REMAINING] = remaining,
                  [ART_FORMAT] = known ? format : NULL,
                  [ART_CODEC] = codec,
                  [ART_RATE] = known ? rate : NULL,
                  [ART_ARTIST] = now_playing->artist,
                  [ART_ALBUM] = now_playing->album},
        .permille = {[ART_PROGRESS - ART_TEXT_COUNT] = (uint16_t)now_playing_progress_width(
                         position_ms, status->duration_ms, ART_PERMILLE),
                     [ART_BATTERY_LEVEL - ART_TEXT_COUNT] = now_playing->battery_permille}};
    art_slots_draw(canvas, theme, now_playing->slots, now_playing->slot_count, &values);
    now_playing->scrolling = false;
}

static void draw_volume(const NowPlayingScreen *now_playing, Canvas *canvas)
{
    char path[CASSETTE_PATH_CAPACITY];
    art_path(now_playing, VOLUME_ART, now_playing->status.volume, path);
    HalFile file;
    if (!hal_file_open(&file, path)) {
        return;
    }
    Color row[HAL_DISPLAY_WIDTH];
    for (int y = 0; y < canvas->bounds.height; y++) {
        Color *out = canvas->pixels + y * canvas->bounds.width;
        hal_file_read(file, (uint32_t)(canvas->bounds.y + y) * sizeof row, row, sizeof row);
        for (int x = 0; x < HAL_DISPLAY_WIDTH; x++) {
            if (row[x] != 0) {
                out[x] = row[x];
            }
        }
    }
    hal_file_close(&file);
}

static void draw_page(NowPlayingScreen *now_playing, Canvas *canvas)
{
    const Theme *theme = now_playing->config.theme;
    if ((now_playing->full || now_playing->cassette) && now_playing->status.has_track) {
        draw_cassette(now_playing, canvas);
        return;
    }
    header_draw_centered(canvas, theme, now_playing->screen.title);
    draw_art(now_playing, canvas);
    now_playing->scrolling = false;
    TextStyle title = {.font = font_sized(16, true), .color = theme->colors.text};
    if (!now_playing->status.has_track) {
        label_draw(canvas, title, (Label){column(NAME_MIDDLE), NOTHING_PLAYING});
        return;
    }
    Rect name = column(NAME_MIDDLE);
    name.width -= draw_format_tag(now_playing, canvas, name);
    const char *shown =
        now_playing->file_name[0] != '\0' ? now_playing->file_name : now_playing->title;
    bool scrolling = draw_text_line(now_playing, canvas, (Label){name, shown}, title);
    draw_middle(now_playing, canvas);
    scrolling = draw_info(now_playing, canvas) || scrolling;
    draw_timeline(now_playing, canvas);
    now_playing->scrolling = scrolling;
}

static void draw(void *context, Canvas *canvas)
{
    NowPlayingScreen *now_playing = context;
    bool volume = now_playing->volume_art && now_playing->status.has_track &&
                  showing(now_playing, NOW_PLAYING_FEEDBACK_VOLUME);
    draw_page(now_playing, canvas);
    volume_blur_strip(canvas, volume);
    if (volume) {
        draw_volume(now_playing, canvas);
    }
}

static uint32_t picture(const void *context)
{
    const NowPlayingScreen *now_playing = context;
    const PlayerStatus *status = &now_playing->status;
    Image cover;
    char text[STATUS_CAPACITY];
    uint32_t hash = (uint32_t)(uintptr_t)now_playing->config.theme + now_playing->cassette;
    hash = screen_picture_mix(hash, status->has_track ? status->track + 1u : 0u);
    hash = screen_picture_mix(hash, now_playing->has_text ? now_playing->text_track + 1u : 0u);
    hash = screen_picture_mix(hash, current_cover(now_playing, &cover));
    hash = screen_picture_mix(hash, shown_position(now_playing) / POSITION_STEP_MS);
    hash = screen_picture_mix(hash, status->duration_ms);
    hash = screen_picture_mix(hash, status->volume);
    hash = screen_picture_mix(hash, (uint32_t)status->shuffle << 8 | (uint32_t)status->repeat);
    hash = screen_picture_mix(hash, now_playing->scrubbing);
    hash = screen_picture_mix(hash, showing(now_playing, NOW_PLAYING_FEEDBACK_VOLUME));
    hash = screen_picture_mix(hash, showing(now_playing, now_playing->feedback)
                                        ? (uint32_t)now_playing->feedback + 1u
                                        : 0u);
    now_playing_detail_text(status, text, sizeof text);
    hash = screen_picture_text(hash, text);
    now_playing_stream_text(status->format, text, sizeof text);
    hash = screen_picture_text(hash, text);
    hash = screen_picture_mix(hash, now_playing->has_lyrics ? now_playing->lyric_from + 1u : 0u);
    hash = screen_picture_text(hash, now_playing->lyrics[NOW_PLAYING_LYRIC_LINES / 2]);
    hash = screen_picture_mix(hash, (uint32_t)status->format.codec);
    hash = screen_picture_mix(hash, now_playing->config.settings != NULL &&
                                        now_playing->config.settings->format_tag);
    hash = screen_picture_text(hash, now_playing->clock);
    hash = screen_picture_mix(hash, now_playing->battery_permille + now_playing->full);
    if (now_playing->scrolling) {
        hash = screen_picture_mix(hash, now_playing->now_ms - now_playing->text_changed_ms);
    }
    return hash != 0 ? hash : 1;
}

Screen *now_playing_screen_init(NowPlayingScreen *now_playing, NowPlayingConfig config)
{
    now_playing->config = config;
    now_playing->has_text = false;
    now_playing->scrubbing = false;
    now_playing->scrub_position_ms = 0;
    now_playing->feedback = NOW_PLAYING_FEEDBACK_NONE;
    now_playing->feedback_started_ms = 0;
    now_playing->scrolling = true;
    now_playing->lyrics_ready = false;
    now_playing->has_lyrics = false;
    now_playing->status = (PlayerStatus){.state = PLAYER_IDLE};
    now_playing->screen = (Screen){
        .title = "Now Playing",
        .context = now_playing,
        .hold_volume_keys = true,
        .enter = enter,
        .handle_key = handle_key,
        .update = update,
        .draw = draw,
        .picture = picture,
    };
    return &now_playing->screen;
}
