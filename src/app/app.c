#include "app/app.h"

#include <stddef.h>

#include "app_internal.h"
#include "app_screens.h"
#include "bluetooth_link.h"
#include "board_layout.h"
#include "card_files.h"
#include "cassette.h"
#include "core/deadline.h"
#include "core/key_tracker.h"
#include "core/low_battery.h"
#include "core/power_policy.h"
#include "core/string_builder.h"
#include "cover_cache.h"
#include "default_theme.h"
#include "draw_profile.h"
#include "favorites_store.h"
#include "frame_overlay.h"
#include "gfx/frame_diff.h"
#include "gfx/strip_renderer.h"
#include "hal/audio.h"
#include "hal/bluetooth.h"
#include "hal/clock.h"
#include "hal/display.h"
#include "hal/input.h"
#include "hal/power.h"
#include "home_art.h"
#include "idle_track.h"
#include "library_rescan.h"
#include "library_store.h"
#include "motion.h"
#include "music_check.h"
#include "player.h"
#include "screens/charge_screen.h"
#include "settings_store.h"
#include "ui/fade.h"
#include "ui/header.h"
#include "ui/label.h"
#include "ui/message_screen.h"
#include "ui/screen_stack.h"
#include "ui/status_bar.h"
#include "ui/toast.h"

enum {
    RESCAN_PROGRESS_MS = 250,
    BATTERY_CHECK_MS = 250,
    CHARGE_LIT_MS = 5000
};

static const KeyConfig KEY_CONFIG = {
    .timing =
        {
            .long_press_ms = 600,
            .repeat_delay_ms = 400,
            .repeat_interval_ms = 120,
        },
    .behaviors =
        {
            [KEY_VOLUME_UP] = KEY_BEHAVIOR_REPEAT,
            [KEY_VOLUME_DOWN] = KEY_BEHAVIOR_REPEAT,
            [KEY_PLAY] = KEY_BEHAVIOR_TAP_OR_HOLD,
            [KEY_MENU] = KEY_BEHAVIOR_TAP_OR_HOLD,
            [KEY_BACK] = KEY_BEHAVIOR_TAP_OR_HOLD,
            [KEY_POWER] = KEY_BEHAVIOR_TAP_OR_HOLD,
        },
    .long_press_ms =
        {
            [KEY_VOLUME_UP] = 1000,
            [KEY_VOLUME_DOWN] = 1000,
            [KEY_POWER] = 2000,
        },
};

typedef struct AppState {
    KeyTracker keys;
    FrameStats frame_stats;
    FrameStats update_stats;
    FrameStats draw_stats;
    FrameStats send_stats;
    uint32_t send_us;
    ScreenStack stack;
    LibraryStore library;
    Player player;
    SettingsStore settings;
    FavoritesStore favorites;
    SpokenStore spoken;
    CoverCache covers;
    PowerPolicy power;
    LowBattery low_battery;
    uint32_t battery_check_ms;
    bool usb_power;
    bool rescan_requested;
    bool music_check_requested;
    bool prepare_pending;
    size_t prepare_index;
    CardFilesReport prepare_report;
    bool light_after_frame;
    const char *goodbye_text;
    Toast toast;
    BluetoothLink bluetooth;
    uint32_t shown_refusals;
    StatusBarState status_bar;
    AppScreens screens;
    MessageScreen notice;
    FrameDiff frame_diff;
    Fade splash;
    Fade goodbye;
    Motion intro;
    Motion afk;
    bool afk_tried;
    IdleTrack idle_track;
    bool saying_goodbye;
    bool goodbye_art;
    char goodbye_path[24];
    uint32_t now_ms;
    AppOptions options;
    bool display_on;
    bool powered_off;
    bool charging;
    uint32_t charge_lit_until_ms;
    uint32_t next_frame_ms;
    HalBattery charge_battery;
    const Theme *theme;
    Screen *still_screen;
    uint32_t still_picture;
    StatusBarState still_status;
    bool volume_window;
    uint32_t volume_tap_ms;
} AppState;

static AppState app;

static const FadeTiming SPLASH_TIMING = {.fade_ms = 600, .hold_ms = 500, .rising = true};
static const uint32_t SCREEN_ON_CLOCK_MHZ = 200;
static const uint32_t IDLE_CLOCK_MHZ = 96;
static const uint32_t REDRAW_PERIOD_MS = 16;
static const uint32_t BOOT_FRAME_MS = 70;
static const uint32_t AFK_FRAME_MS = 200;
static const uint32_t AFK_AFTER_MS = 10000;
static const uint32_t CLOCK_READ_MS = 1000;
static const FadeTiming GOODBYE_TIMING = {.fade_ms = 600, .hold_ms = 400, .rising = false};
static const FadeTiming GOODBYE_ART_TIMING = {.fade_ms = 0, .hold_ms = 1500, .rising = false};
static const char SPLASH_TEXT[] = "Graysand";
static const char GOODBYE_TEXT[] = "Graysand";
static const char BATTERY_LOW_TEXT[] = "Battery low";
static Color strip_pixels[HAL_DISPLAY_WIDTH * BOARD_STRIP_HEIGHT];
static uint32_t row_hashes[HAL_DISPLAY_HEIGHT];

static Screen *top_screen(void)
{
    return screen_stack_top(&app.stack);
}

static void read_input(void)
{
    HalKeyTransition transition;
    while (hal_input_next_transition(&transition)) {
        app.next_frame_ms = app.now_ms;
        if (app.settings.settings.swap_volume_keys &&
            (transition.key == KEY_VOLUME_UP || transition.key == KEY_VOLUME_DOWN)) {
            transition.key = transition.key == KEY_VOLUME_UP ? KEY_VOLUME_DOWN : KEY_VOLUME_UP;
        }
        if (transition.pressed) {
            key_tracker_key_down(&app.keys, transition.key, transition.time_ms);
        } else {
            key_tracker_key_up(&app.keys, transition.key, transition.time_ms);
        }
    }
}

static void use_screen_key_behaviors(void);

static uint32_t wanted_clock_mhz(void)
{
    if (app.display_on || !hal_audio_status().idle) {
        return SCREEN_ON_CLOCK_MHZ;
    }
    return IDLE_CLOCK_MHZ;
}

static void use_wanted_clock(void)
{
    uint32_t wanted = wanted_clock_mhz();
    if (hal_system_clock_mhz() != wanted) {
        hal_system_set_clock_mhz(wanted);
    }
}

static void light_after_first_frame(void)
{
    if (app.light_after_frame) {
        app.light_after_frame = false;
        hal_display_set_power(true);
    }
}

static void set_display(bool on)
{
    app.still_screen = NULL;
    app.display_on = on;
    app.light_after_frame = on;
    app.next_frame_ms = app.now_ms;
    if (!on) {
        hal_display_set_power(false);
    }
    use_wanted_clock();
    if (screen_stack_depth(&app.stack) > 0) {
        use_screen_key_behaviors();
    }
}

static void save_settings(void)
{
    settings_store_capture(&app.settings, player_status(&app.player));
    settings_store_save(&app.settings);
}

static void save_session(void)
{
    spoken_store_flush(&app.spoken, player_status(&app.player), hal_clock_milliseconds());
    player_save(&app.player);
    save_settings();
}

static void power_off_now(void)
{
    set_display(false);
    app.powered_off = true;
    hal_power_off();
}

static void light_charge_screen(uint32_t now_ms)
{
    app.charge_lit_until_ms = now_ms + CHARGE_LIT_MS;
    if (!app.display_on) {
        set_display(true);
    }
}

static void start_charging(uint32_t now_ms)
{
    if (player_status(&app.player).state == PLAYER_PLAYING) {
        player_toggle_pause(&app.player);
    }
    player_release_output(&app.player);
    bluetooth_link_sleep(&app.bluetooth);
    app.charging = true;
    app.charge_battery = hal_power_battery();
    light_charge_screen(now_ms);
}

static void stop_charging(uint32_t now_ms)
{
    app.charging = false;
    bluetooth_link_start(&app.bluetooth, &app.toast, now_ms);
    power_policy_wake(&app.power, now_ms);
    if (!app.display_on) {
        set_display(true);
    }
    app_show_splash();
}

static void finish_shut_down(void)
{
    app.saying_goodbye = false;
    if (hal_power_battery().usb_power) {
        start_charging(app.now_ms);
        return;
    }
    power_off_now();
}

static bool goodbye_art_ready(const char *text)
{
    StringBuilder builder = string_builder_make(app.goodbye_path, sizeof app.goodbye_path);
    string_builder_append(&builder, "/.echo/bye/");
    string_builder_append(&builder, home_art_theme_folder(app.settings.settings.theme));
    string_builder_append(&builder, ".bin");
    HalFile file;
    if (text != GOODBYE_TEXT || builder.overflowed || !hal_file_open(&file, app.goodbye_path)) {
        return false;
    }
    bool whole = file.size == (uint32_t)HAL_DISPLAY_WIDTH * HAL_DISPLAY_HEIGHT * sizeof(Color);
    hal_file_close(&file);
    return whole;
}

static void shut_down(const char *text)
{
    app.goodbye_text = text;
    save_session();
    if (!app.display_on) {
        finish_shut_down();
        return;
    }
    app.goodbye_art = goodbye_art_ready(text);
    fade_start(&app.goodbye, app.now_ms, app.goodbye_art ? GOODBYE_ART_TIMING : GOODBYE_TIMING);
    app.saying_goodbye = true;
}

static bool fade_running(void)
{
    return app.saying_goodbye || !fade_finished(&app.splash, app.now_ms);
}

static uint32_t screen_bytes(void)
{
    return (uint32_t)HAL_DISPLAY_WIDTH * (uint32_t)HAL_DISPLAY_HEIGHT * (uint32_t)sizeof(Color);
}

static Motion *shown_motion(void)
{
    if (motion_running(&app.intro, app.now_ms)) {
        return &app.intro;
    }
    return motion_running(&app.afk, app.now_ms) ? &app.afk : NULL;
}

static void skip_intro(uint32_t now_ms)
{
    KeyEvent event;
    while (key_tracker_next_event(&app.keys, &event)) {
        motion_stop(&app.intro);
    }
    power_policy_wake(&app.power, now_ms);
}

static void update_afk(uint32_t now_ms)
{
    bool idle = app.display_on && !fade_running() && !motion_running(&app.intro, now_ms) &&
                settings_shows_idle_screen(&app.settings.settings) &&
                app.settings.settings.theme != THEME_PODIUM &&
                now_ms - app.power.last_activity_ms >= AFK_AFTER_MS;
    if (!idle) {
        motion_stop(&app.afk);
        app.afk_tried = false;
        return;
    }
    if (!app.afk_tried) {
        app.afk_tried = true;
        motion_start(&app.afk, "afk", app.settings.settings.theme, AFK_FRAME_MS, true,
                     screen_bytes(), now_ms);
    }
    if (app.library.available) {
        idle_track_update(&app.idle_track, &app.library.library, player_status(&app.player));
    }
}

static void drain_key_events(void)
{
    KeyEvent event;
    while (key_tracker_next_event(&app.keys, &event)) {
    }
}

static bool volume_keys_hold(void)
{
    return !app.display_on || top_screen()->hold_volume_keys;
}

static void use_screen_key_behaviors(void)
{
    bool hold = volume_keys_hold() && !app.volume_window;
    KeyBehavior volume = hold ? KEY_BEHAVIOR_TAP_HOLD_REPEAT : KEY_BEHAVIOR_REPEAT;
    key_tracker_set_behavior(&app.keys, KEY_VOLUME_UP, volume);
    key_tracker_set_behavior(&app.keys, KEY_VOLUME_DOWN, volume);
}

static bool opens_now_playing(KeyEvent event)
{
    return event.key == KEY_MENU && event.gesture == KEY_GESTURE_PRESS &&
           player_status(&app.player).has_track &&
           top_screen() != &app.screens.now_playing.screen && !top_screen()->takes_every_key;
}

static ScreenAction now_playing_action(void)
{
    return top_screen() == &app.screens.up_next.screen
               ? screen_action_pop()
               : screen_action_push(&app.screens.now_playing.screen);
}

static void dispatch_to_screen(KeyEvent event, uint32_t now_ms)
{
    Screen *screen = top_screen();
    ScreenAction action = opens_now_playing(event)
                              ? now_playing_action()
                              : screen->handle_key(screen->context, event, now_ms);
    if (screen_stack_apply(&app.stack, action, now_ms)) {
        use_screen_key_behaviors();
    }
}

static bool may_show_now_playing(void)
{
    PlayerStatus status = player_status(&app.player);
    Screen *top = top_screen();
    return status.has_track && status.state == PLAYER_PLAYING &&
           top != &app.screens.now_playing.screen && !top->stays_on_wake;
}

static void show_now_playing(uint32_t now_ms)
{
    if (may_show_now_playing() &&
        screen_stack_apply(&app.stack, screen_action_push(&app.screens.now_playing.screen),
                           now_ms)) {
        use_screen_key_behaviors();
    }
}

static void run_background(KeyEvent event)
{
    switch (now_playing_command_for(event, false)) {
    case NOW_PLAYING_TOGGLE_PAUSE:
        player_toggle_pause(&app.player);
        break;
    case NOW_PLAYING_NEXT:
        player_skip(&app.player, 1);
        break;
    case NOW_PLAYING_PREVIOUS:
        player_skip(&app.player, -1);
        break;
    case NOW_PLAYING_VOLUME_UP:
        player_change_volume(&app.player, 1);
        break;
    case NOW_PLAYING_VOLUME_DOWN:
        player_change_volume(&app.player, -1);
        break;
    default:
        break;
    }
}

static bool ignored_power_tap(KeyEvent event)
{
    return app.options.ignore_power_taps && event.key == KEY_POWER &&
           event.gesture == KEY_GESTURE_PRESS;
}

static bool volume_key(Key key)
{
    return key == KEY_VOLUME_UP || key == KEY_VOLUME_DOWN;
}

static KeyEvent follow_volume_window(KeyEvent event, uint32_t now_ms)
{
    if (!volume_key(event.key) && app.volume_window) {
        app.volume_window = false;
        use_screen_key_behaviors();
    }
    if (!volume_key(event.key) || event.gesture == KEY_GESTURE_LONG_PRESS) {
        return event;
    }
    if (app.volume_window) {
        event.gesture = KEY_GESTURE_PRESS;
    } else if (event.gesture != KEY_GESTURE_PRESS || !volume_keys_hold()) {
        return event;
    }
    app.volume_tap_ms = now_ms;
    if (!app.volume_window) {
        app.volume_window = true;
        use_screen_key_behaviors();
    }
    return event;
}

static void close_volume_window(uint32_t now_ms)
{
    bool held = app.keys.states[KEY_VOLUME_UP].held || app.keys.states[KEY_VOLUME_DOWN].held;
    if (app.volume_window && !held &&
        now_ms - app.volume_tap_ms >= NOW_PLAYING_VOLUME_FEEDBACK_MS) {
        app.volume_window = false;
        use_screen_key_behaviors();
    }
}

static void dispatch_key_events(uint32_t now_ms)
{
    KeyEvent event;
    while (!app.powered_off && key_tracker_next_event(&app.keys, &event)) {
        event = follow_volume_window(event, now_ms);
        if (ignored_power_tap(event)) {
            continue;
        }
        PowerKeyOutcome outcome = power_policy_key(&app.power, event, now_ms);
        if (app.afk.active && !outcome.consumed) {
            motion_stop(&app.afk);
            continue;
        }
        if (outcome.shutdown) {
            shut_down(GOODBYE_TEXT);
        } else if (outcome.background) {
            run_background(event);
        } else if (!outcome.consumed) {
            dispatch_to_screen(event, now_ms);
        }
    }
}

static bool battery_empty(uint32_t now_ms)
{
    if (!deadline_reached(now_ms, app.battery_check_ms)) {
        return false;
    }
    app.battery_check_ms = now_ms + BATTERY_CHECK_MS;
    HalBattery battery = hal_power_battery();
    app.usb_power = battery.usb_power;
    LowBatteryReading reading = {
        .known = battery.known, .millivolts = battery.millivolts, .usb_power = battery.usb_power};
    return low_battery_update(&app.low_battery, reading, now_ms);
}

static void keep_awake_while_busy(uint32_t now_ms)
{
    if (app.usb_power || player_status(&app.player).state == PLAYER_PLAYING) {
        power_policy_stay_awake(&app.power, now_ms);
    }
}

static void update_power(uint32_t now_ms)
{
    if (!app.saying_goodbye && battery_empty(now_ms)) {
        shut_down(BATTERY_LOW_TEXT);
        return;
    }
    keep_awake_while_busy(now_ms);
    if (power_policy_update(&app.power, now_ms)) {
        shut_down(GOODBYE_TEXT);
        return;
    }
    if (app.power.display_on == app.display_on) {
        return;
    }
    set_display(app.power.display_on);
    if (!app.display_on) {
        save_settings();
    } else {
        show_now_playing(now_ms);
    }
}

static void update_screen(uint32_t now_ms)
{
    Screen *screen = top_screen();
    if (screen->update != NULL) {
        screen->update(screen->context, now_ms);
    }
}

static const Theme *board_theme(void)
{
    if (hal_system_restart_pending()) {
        return &NOTICE_THEME;
    }
    return app.theme != NULL ? app.theme : &BOARD_THEME;
}

static void draw_fade(Canvas *canvas, const Fade *fade, const char *text)
{
    const Theme *theme = board_theme();
    const Font *font = theme->fonts.title;
    Color color =
        color_blend(theme->colors.background, theme->colors.text, fade_alpha(fade, app.now_ms));
    int mark = 36;
    int gap = 8;
    int text_height = font_line_height(font);
    int mark_height = mark * 3 / 4;
    int top = (HAL_DISPLAY_HEIGHT - (mark_height + gap + text_height)) / 2;
    header_mark(canvas, (Rect){(HAL_DISPLAY_WIDTH - mark) / 2, top, mark, mark_height},
                THEME_STYLE_PLAIN, color);
    TextStyle style = {.font = font, .color = color};
    Rect line = {0, top + mark_height + gap, HAL_DISPLAY_WIDTH, text_height};
    label_draw(canvas, style, (Label){.area = label_centered_area(font, line, text), .text = text});
}

static bool draw_running_fade(Canvas *canvas)
{
    if (app.saying_goodbye && app.goodbye_art) {
        home_art_draw_file(app.goodbye_path, canvas,
                           (Rect){0, 0, HAL_DISPLAY_WIDTH, HAL_DISPLAY_HEIGHT});
        return true;
    }
    if (app.saying_goodbye) {
        draw_fade(canvas, &app.goodbye, app.goodbye_text);
        return true;
    }
    if (!fade_finished(&app.splash, app.now_ms)) {
        draw_fade(canvas, &app.splash, SPLASH_TEXT);
        return true;
    }
    return false;
}

static void draw_frame(void *context, Canvas *canvas)
{
    const AppState *state = context;
    if (draw_running_fade(canvas)) {
        return;
    }
    Motion *shown = shown_motion();
    if (shown != NULL) {
        motion_draw(shown, canvas, (Rect){0, 0, HAL_DISPLAY_WIDTH, HAL_DISPLAY_HEIGHT}, app.now_ms);
        if (shown == &app.afk) {
            idle_track_draw(&app.idle_track, canvas, board_theme(), player_status(&app.player));
        }
        return;
    }
    Screen *screen = screen_stack_top(&state->stack);
    header_set_parent(screen_stack_parent_title(&state->stack, screen));
    screen->draw(screen->context, canvas);
    header_set_parent(NULL);
    if (!screen->full_screen) {
        status_bar_draw(canvas, board_theme(), state->status_bar);
    }
    toast_draw(&state->toast, canvas, board_theme());
    if (state->options.show_frame_time) {
        FrameBreakdown breakdown = {
            .total_us = frame_stats_average_us(&state->frame_stats),
            .update_us = frame_stats_average_us(&state->update_stats),
            .draw_us = frame_stats_average_us(&state->draw_stats),
            .send_us = frame_stats_average_us(&state->send_stats),
        };
        frame_overlay_draw(canvas, board_theme(), breakdown, hal_audio_stats());
    }
}

static void write_to_display(void *context, Rect area, const Color *pixels)
{
    (void)context;
    Rect changed;
    uint32_t audio_started_us = hal_clock_microseconds();
    hal_audio_service();
    hal_bluetooth_service();
    uint32_t hash_started_us = hal_clock_microseconds();
    draw_profile_add(DRAW_PART_AUDIO, hash_started_us - audio_started_us);
    bool any_changed = frame_diff_changed(&app.frame_diff, area, pixels, &changed);
    draw_profile_add(DRAW_PART_HASH, hal_clock_microseconds() - hash_started_us);
    if (!any_changed) {
        return;
    }
    const Color *first_row = pixels + (size_t)(changed.y - area.y) * (size_t)area.width;
    uint32_t started_us = hal_clock_microseconds();
    hal_display_write(changed.x, changed.y, changed.width, changed.height, first_row);
    app.send_us += hal_clock_microseconds() - started_us;
}

static StatusPlayback playback_of(PlayerState state)
{
    switch (state) {
    case PLAYER_PLAYING:
        return STATUS_PLAYBACK_PLAYING;
    case PLAYER_PAUSED:
        return STATUS_PLAYBACK_PAUSED;
    case PLAYER_IDLE:
        return STATUS_PLAYBACK_NONE;
    }
    return STATUS_PLAYBACK_NONE;
}

static StatusBluetooth bluetooth_of(const BluetoothLink *link)
{
    if (!link->saved.enabled || link->dozing) {
        return STATUS_BLUETOOTH_OFF;
    }
    return link->connected ? STATUS_BLUETOOTH_CONNECTED : STATUS_BLUETOOTH_ON;
}

static void refresh_clock(uint32_t now_ms)
{
    if (now_ms - app.idle_track.clock_ms < CLOCK_READ_MS) {
        return;
    }
    app.idle_track.clock_known = hal_clock_wall_time(&app.idle_track.clock);
    app.idle_track.clock_ms = now_ms;
}

static StatusBarState current_status_bar(void)
{
    HalBattery battery = hal_power_battery();
    WallTime clock = app.idle_track.clock;
    return (StatusBarState){
        .playback = playback_of(player_status(&app.player).state),
        .bluetooth = bluetooth_of(&app.bluetooth),
        .battery_known = battery.known,
        .battery_percent = battery.percent,
        .charging = battery.charging,
        .clock_known = app.idle_track.clock_known,
        .hour = clock.hour,
        .minute = clock.minute,
        .theme_name = SETTINGS_THEME_NAMES[app.settings.settings.theme],
    };
}

static void render(FrameDrawer drawer, bool drawer_paints_everything)
{
    app.still_screen = NULL;
    StripRenderer renderer = {
        .strip_pixels = strip_pixels,
        .screen_width = HAL_DISPLAY_WIDTH,
        .screen_height = HAL_DISPLAY_HEIGHT,
        .strip_height = BOARD_STRIP_HEIGHT,
        .background = board_theme()->colors.background,
        .drawer_paints_everything = drawer_paints_everything,
    };
    FrameSink sink = {.write = write_to_display, .context = NULL};
    strip_renderer_render(&renderer, drawer, sink);
    hal_display_present();
}

static bool same_status(StatusBarState first, StatusBarState second)
{
    return first.playback == second.playback && first.bluetooth == second.bluetooth &&
           first.battery_known == second.battery_known &&
           first.battery_percent == second.battery_percent && first.charging == second.charging &&
           first.clock_known == second.clock_known && first.hour == second.hour &&
           first.minute == second.minute;
}

static uint32_t still_picture(Screen *screen)
{
    bool moving = fade_running() || app.toast.visible || app.options.show_frame_time;
    Motion *shown = shown_motion();
    if (!moving && shown != NULL) {
        uint32_t frame = screen_picture_mix(shown == &app.intro ? 1u : 2u,
                                            (uint32_t)motion_frame(shown, app.now_ms));
        if (shown == &app.afk) {
            frame = screen_picture_mix(
                frame, idle_track_picture(&app.idle_track, player_status(&app.player)));
        }
        return frame != 0 ? frame : 1;
    }
    return moving || screen->picture == NULL ? 0 : screen->picture(screen->context);
}

static bool frame_unchanged(Screen *screen, uint32_t picture)
{
    return picture != 0 && screen == app.still_screen && picture == app.still_picture &&
           same_status(app.status_bar, app.still_status);
}

static void render_frame(void)
{
    uint32_t started_us = hal_clock_microseconds();
    app.send_us = 0;
    app.status_bar = current_status_bar();
    Screen *screen = top_screen();
    uint32_t picture = still_picture(screen);
    if (frame_unchanged(screen, picture)) {
        return;
    }
    bool screen_paints = screen->paints_every_pixel && !fade_running() && shown_motion() == NULL;
    render((FrameDrawer){.draw = draw_frame, .context = &app}, screen_paints);
    app.still_screen = screen;
    app.still_picture = picture;
    app.still_status = app.status_bar;
    uint32_t render_us = hal_clock_microseconds() - started_us;
    frame_stats_record(&app.send_stats, app.send_us);
    frame_stats_record(&app.draw_stats, render_us - app.send_us);
    draw_profile_finish_frame();
}

static void draw_notice(void *context, Canvas *canvas)
{
    Screen *screen = context;
    screen->draw(screen->context, canvas);
}

static uint8_t list_icons[LIST_ICON_FILE_CAPACITY];

static void load_list_icons(void)
{
    HalFile file;
    if (!hal_file_open(&file, "/.echo/icons.bin")) {
        return;
    }
    if (file.size > LIST_ICON_BYTES && file.size < sizeof list_icons - 1 &&
        hal_file_read(file, 0, list_icons, file.size)) {
        list_view_set_icons(list_icons);
    }
    hal_file_close(&file);
}

static void start_playback(void)
{
    library_store_open(&app.library);
    player_init(&app.player, &app.library.library);
    settings_store_restore_playback(&app.settings, &app.player, hal_clock_microseconds());
    player_restore(&app.player);
    favorites_store_load(&app.favorites, &app.library.library);
    spoken_store_load(&app.spoken, &app.library.library);
    player_set_resume(&app.player, spoken_store_resume(&app.spoken));
    cover_cache_open(&app.covers, &app.library.library);
}

static void start_hardware(uint32_t now_ms)
{
    power_policy_init(&app.power, settings_screen_timeout_ms(&app.settings.settings), now_ms);
    power_policy_set_auto_off(&app.power, settings_auto_off_ms(&app.settings.settings), now_ms);
    settings_store_apply_hardware(&app.settings.settings);
    set_display(true);
}

static void open_start_screen(uint32_t now_ms)
{
    Screen *path[START_PATH_CAPACITY];
    int count = app_screens_start_path(&app.screens, app.settings.settings.start_screen, path);
    for (int index = 0; index < count; index++) {
        screen_stack_apply(&app.stack, screen_action_push(path[index]), now_ms);
    }
}

static void build_screens(uint32_t now_ms)
{
    app.still_screen = NULL;
    AppScreensContext context = {
        .options = &app.options,
        .theme = board_theme(),
        .store = &app.library,
        .player = &app.player,
        .spoken = &app.spoken,
        .settings = &app.settings,
        .favorites = &app.favorites,
        .power = &app.power,
        .toast = &app.toast,
        .covers = &app.covers,
        .rescan_requested = &app.rescan_requested,
        .bluetooth = &app.bluetooth,
    };
    Screen *root = app_screens_init(&app.screens, context);
    screen_stack_init(&app.stack, root, now_ms);
}

static void start_screens(uint32_t now_ms)
{
    build_screens(now_ms);
    open_start_screen(now_ms);
    use_screen_key_behaviors();
}

static void apply_theme_change(uint32_t now_ms)
{
    const Theme *wanted = default_theme_for(app.settings.settings.theme);
    if (wanted == app.theme) {
        return;
    }
    int selected = app.screens.settings.menu.list.selected;
    app.theme = wanted;
    build_screens(now_ms);
    screen_stack_apply(&app.stack, screen_action_push(&app.screens.settings.menu.screen), now_ms);
    list_view_select(&app.screens.settings.menu.list, selected, now_ms);
    use_screen_key_behaviors();
}

static void report_card_files(CardFilesReport files, uint32_t now_ms)
{
    if (files.card_full) {
        toast_show(&app.toast, "Storage is full", now_ms);
    }
    if (files.library_created) {
        app.rescan_requested = true;
    } else if (music_check_firmware_is_new()) {
        app.music_check_requested = true;
    }
}

void app_start(void)
{
    uint32_t now_ms = hal_clock_milliseconds();
    frame_diff_init(&app.frame_diff, row_hashes, HAL_DISPLAY_HEIGHT);
    key_tracker_init(&app.keys, &KEY_CONFIG);
    frame_stats_reset(&app.frame_stats);
    frame_stats_reset(&app.update_stats);
    frame_stats_reset(&app.draw_stats);
    frame_stats_reset(&app.send_stats);
    app.options = (AppOptions){.show_frame_time = false, .ignore_power_taps = false};
    app.toast = (Toast){.visible = false};
    app.powered_off = false;
    app.charging = false;
    app.saying_goodbye = false;
    app.goodbye_text = GOODBYE_TEXT;
    low_battery_init(&app.low_battery);
    app.battery_check_ms = now_ms;
    app.splash = (Fade){.active = false};
    motion_stop(&app.intro);
    motion_stop(&app.afk);
    app.afk_tried = false;
    app.idle_track = (IdleTrack){.has_track = false};
    app.now_ms = now_ms;
    app.prepare_pending = true;
    app.prepare_index = 0;
    app.prepare_report = (CardFilesReport){.library_created = false, .card_full = false};
    settings_store_load(&app.settings);
    load_list_icons();
    app.theme = default_theme_for(app.settings.settings.theme);
    start_playback();
    start_hardware(now_ms);
    bluetooth_link_start(&app.bluetooth, &app.toast, now_ms);
    start_screens(now_ms);
    if (hal_power_take_usb_start()) {
        start_charging(now_ms);
    }
}

static void show_rescan_progress(uint32_t files, uint32_t tracks)
{
    static uint32_t shown_ms;
    uint32_t now_ms = hal_clock_milliseconds();
    if (files > 1 && now_ms - shown_ms < RESCAN_PROGRESS_MS) {
        return;
    }
    shown_ms = now_ms;
    static char text[48];
    StringBuilder builder = string_builder_make(text, sizeof text);
    string_builder_append_unsigned(&builder, tracks);
    string_builder_append(&builder, " tracks so far");
    app_show_notice("Scanning library", text);
}

static void show_cover_progress(uint32_t done, uint32_t total)
{
    static uint32_t shown_ms;
    uint32_t now_ms = hal_clock_milliseconds();
    if (done > 0 && done < total && now_ms - shown_ms < RESCAN_PROGRESS_MS) {
        return;
    }
    shown_ms = now_ms;
    static char text[48];
    StringBuilder builder = string_builder_make(text, sizeof text);
    string_builder_append_unsigned(&builder, done);
    string_builder_append(&builder, " of ");
    string_builder_append_unsigned(&builder, total);
    string_builder_append(&builder, " albums");
    app_show_notice("Finding covers", text);
}

static void explain_refusals(uint32_t now_ms)
{
    static char text[48];
    uint32_t count = app.player.refusal_count;
    bool fresh = count > app.shown_refusals;
    app.shown_refusals = count;
    if (fresh) {
        player_refusal_text(app.player.refusal, text, sizeof text);
        toast_show(&app.toast, text, now_ms);
    }
}

static void run_rescan(void)
{
    app_show_notice("Scanning library", "Starting...");
    RescanProgress progress = {show_rescan_progress, show_cover_progress};
    RescanOutcome outcome = library_rescan_run(progress);
    static char message[48];
    const char *text = library_rescan_message(outcome, message, sizeof message);
    if (outcome.result == SCAN_OK) {
        music_check_remember(music_check_fingerprint());
        cover_cache_close(&app.covers);
        library_store_close(&app.library);
        app_start();
    }
    app_wake();
    toast_show(&app.toast, text, hal_clock_milliseconds());
}

void app_release_audio(void)
{
    spoken_store_flush(&app.spoken, player_status(&app.player), hal_clock_milliseconds());
    player_release_output(&app.player);
}

void app_remember_dac_volume(uint8_t level)
{
    app.settings.settings.dac_volume = level;
    settings_store_save(&app.settings);
}

void app_request_rescan(void)
{
    app.rescan_requested = true;
}

void app_request_rescan_if_music_changed(void)
{
    app.music_check_requested = true;
}

static void draw_charge(void *context, Canvas *canvas)
{
    (void)context;
    charge_screen_draw(canvas, board_theme(), app.charge_battery);
}

static void follow_charge_keys(uint32_t now_ms)
{
    KeyEvent event;
    while (app.charging && key_tracker_next_event(&app.keys, &event)) {
        if (event.key == KEY_POWER && event.gesture == KEY_GESTURE_LONG_PRESS) {
            stop_charging(now_ms);
        } else {
            light_charge_screen(now_ms);
        }
    }
}

static void charge_step(uint32_t now_ms)
{
    follow_charge_keys(now_ms);
    if (!app.charging) {
        return;
    }
    if (deadline_reached(now_ms, app.battery_check_ms)) {
        app.battery_check_ms = now_ms + BATTERY_CHECK_MS;
        app.charge_battery = hal_power_battery();
        if (!app.charge_battery.usb_power) {
            app.charging = false;
            power_off_now();
            return;
        }
    }
    if (app.display_on && deadline_reached(now_ms, app.charge_lit_until_ms)) {
        set_display(false);
    }
    if (app.display_on) {
        render((FrameDrawer){.draw = draw_charge, .context = NULL}, true);
        light_after_first_frame();
    } else {
        use_wanted_clock();
    }
}

static void show_prepare_progress(size_t index)
{
    static char text[48];
    StringBuilder builder = string_builder_make(text, sizeof text);
    string_builder_append(&builder, "File ");
    string_builder_append_unsigned(&builder, (uint32_t)index + 1);
    string_builder_append(&builder, " of ");
    string_builder_append_unsigned(&builder, (uint32_t)card_files_count());
    app_show_notice("Preparing storage", text);
}

static bool prepare_step(void)
{
    while (app.prepare_index < card_files_count()) {
        size_t index = app.prepare_index++;
        if (!card_files_missing(index)) {
            continue;
        }
        bool slow = card_files_slow(index);
        if (slow && !app.charging) {
            show_prepare_progress(index);
        }
        card_files_reserve(&app.prepare_report, index);
        if (slow) {
            return !app.charging;
        }
    }
    app.prepare_pending = false;
    report_card_files(app.prepare_report, hal_clock_milliseconds());
    return false;
}

bool app_preparing(void)
{
    return app.prepare_pending;
}

void app_step(void)
{
    if (app.powered_off) {
        return;
    }
    if (app.prepare_pending && prepare_step()) {
        return;
    }
    if (app.music_check_requested) {
        app.music_check_requested = false;
        bool changed = hal_system_restart_pending();
        if (!changed) {
            app_show_notice("Music", "Checking for changes...");
            library_store_close(&app.library);
            changed = !music_check_unchanged() || !library_store_open(&app.library);
        }
        if (!changed) {
            music_check_note_firmware();
        }
        app.rescan_requested |= changed;
    }
    if (app.rescan_requested) {
        app.rescan_requested = false;
        run_rescan();
        return;
    }
    uint32_t started_us = hal_clock_microseconds();
    uint32_t now_ms = hal_clock_milliseconds();
    app.now_ms = now_ms;
    hal_audio_service();
    hal_bluetooth_service();
    read_input();
    key_tracker_update(&app.keys, now_ms);
    close_volume_window(now_ms);
    if (app.charging) {
        charge_step(now_ms);
        return;
    }
    if (fade_running()) {
        drain_key_events();
    } else if (motion_running(&app.intro, now_ms)) {
        skip_intro(now_ms);
    } else {
        dispatch_key_events(now_ms);
    }
    apply_theme_change(now_ms);
    if (app.saying_goodbye && fade_finished(&app.goodbye, now_ms)) {
        finish_shut_down();
    }
    if (!app.powered_off) {
        player_update(&app.player, now_ms);
        bluetooth_link_step(&app.bluetooth, &app.player, now_ms);
        spoken_store_follow(&app.spoken, player_status(&app.player), now_ms);
        explain_refusals(now_ms);
        update_power(now_ms);
    }
    if (app.powered_off) {
        return;
    }
    if (!app.display_on) {
        use_wanted_clock();
    }
    toast_update(&app.toast, now_ms);
    refresh_clock(now_ms);
    update_afk(now_ms);
    update_screen(now_ms);
    frame_stats_record(&app.update_stats, hal_clock_microseconds() - started_us);
    if (app.display_on && deadline_reached(now_ms, app.next_frame_ms)) {
        app.next_frame_ms = now_ms + REDRAW_PERIOD_MS;
        render_frame();
        light_after_first_frame();
        frame_stats_record(&app.frame_stats, hal_clock_microseconds() - started_us);
    }
}

bool app_charging(void)
{
    return app.charging;
}

typedef struct CassetteNotice {
    const char *heading;
    const char *message;
} CassetteNotice;

static const char *usb_heading(const char *title)
{
    if (string_same(title, "USB")) {
        return "USB DATA";
    }
    return string_same(title, "USB DAC") ? "USB DAC" : NULL;
}

static void draw_cassette_notice(void *context, Canvas *canvas)
{
    const CassetteNotice *notice = context;
    cassette_draw_notice(canvas, board_theme(), notice->heading, notice->message,
                         string_same(notice->heading, "USB DAC"));
}

void app_show_notice(const char *title, const char *message)
{
    const char *heading = usb_heading(title);
    if (heading != NULL && !hal_system_restart_pending()) {
        CassetteNotice notice = {.heading = heading, .message = message};
        if (!app.display_on) {
            set_display(true);
        }
        render((FrameDrawer){.draw = draw_cassette_notice, .context = &notice}, false);
        light_after_first_frame();
        return;
    }
    MessageConfig config = {.title = title, .message = message, .theme = &NOTICE_THEME};
    Screen *screen = message_screen_init(&app.notice, config);
    if (!app.display_on) {
        set_display(true);
    }
    render((FrameDrawer){.draw = draw_notice, .context = screen}, false);
    light_after_first_frame();
}

void app_show_splash(void)
{
    app.now_ms = hal_clock_milliseconds();
    motion_stop(&app.afk);
    if (!motion_start(&app.intro, "boot", app.settings.settings.theme, BOOT_FRAME_MS, false,
                      screen_bytes(), app.now_ms)) {
        fade_start(&app.splash, app.now_ms, SPLASH_TIMING);
    }
}

void app_wake(void)
{
    uint32_t now_ms = hal_clock_milliseconds();
    power_policy_wake(&app.power, now_ms);
    if (app.charging) {
        light_charge_screen(now_ms);
    }
}

const FrameStats *app_frame_stats(void)
{
    return &app.frame_stats;
}

const char *app_screen_title(void)
{
    return top_screen()->title;
}

int app_screen_depth(void)
{
    return screen_stack_depth(&app.stack);
}

void app_stop(void)
{
    if (!app.powered_off) {
        save_session();
    }
}

PlayerStatus app_player_status(void)
{
    return player_status(&app.player);
}

const char *app_toast_text(void)
{
    return app.toast.visible ? app.toast.text : NULL;
}

const char *app_diagnostics_value(int index)
{
    const MenuItem *item = &app.screens.diagnostics_items[index];
    return item->value != NULL ? item->value(item->context) : NULL;
}
