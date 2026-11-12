#include "default_theme.h"

#include "fonts/fonts.h"
#include "hal/display.h"

#define PALETTE(bg_, text_, text2_, text3_, sel_, sel_top_, shine_, sel_text_, head_, head_top_,   \
                head_text_, muted_, divider_, thumb_, danger_, success_, chevron_, accent_)        \
    {                                                                                              \
        .background = bg_,                                                                         \
        .text = text_,                                                                             \
        .text_secondary = text2_,                                                                  \
        .text_tertiary = text3_,                                                                   \
        .selection_background = sel_,                                                              \
        .selection_top = sel_top_,                                                                 \
        .selection_shine = shine_,                                                                 \
        .selection_text = sel_text_,                                                               \
        .header_background = head_,                                                                \
        .header_top = head_top_,                                                                   \
        .header_text = head_text_,                                                                 \
        .header_muted = muted_,                                                                    \
        .divider = divider_,                                                                       \
        .accent = accent_,                                                                         \
        .scrollbar_thumb = thumb_,                                                                 \
        .danger = danger_,                                                                         \
        .success = success_,                                                                       \
        .chevron = chevron_,                                                                       \
    }

#define PODIUM_COLORS                                                                              \
    PALETTE(COLOR_RGB(10, 10, 10), COLOR_RGB(236, 236, 236), COLOR_RGB(140, 140, 140),             \
            COLOR_RGB(96, 96, 96), COLOR_RGB(240, 240, 240), COLOR_RGB(240, 240, 240),             \
            COLOR_RGB(240, 240, 240), COLOR_RGB(12, 12, 12), COLOR_RGB(10, 10, 10),                \
            COLOR_RGB(10, 10, 10), COLOR_RGB(245, 245, 245), COLOR_RGB(110, 110, 110),             \
            COLOR_RGB(30, 30, 30), COLOR_RGB(70, 70, 70), COLOR_RGB(235, 77, 61),                  \
            COLOR_RGB(52, 199, 89), COLOR_RGB(90, 90, 90), COLOR_RGB(240, 240, 240))

#define POKEMON_COLORS                                                                             \
    PALETTE(COLOR_RGB(31, 26, 30), COLOR_RGB(244, 242, 240), COLOR_RGB(150, 144, 150),             \
            COLOR_RGB(100, 94, 100), COLOR_RGB(200, 36, 22), COLOR_RGB(226, 50, 32),               \
            COLOR_RGB(240, 90, 70), COLOR_RGB(255, 255, 255), COLOR_RGB(196, 35, 21),              \
            COLOR_RGB(230, 57, 39), COLOR_RGB(255, 255, 255), COLOR_RGB(255, 196, 186),            \
            COLOR_RGB(83, 25, 22), COLOR_RGB(150, 144, 150), COLOR_RGB(255, 206, 18),              \
            COLOR_RGB(52, 199, 89), COLOR_RGB(150, 144, 150), COLOR_RGB(100, 228, 120))

#define ZELDA_COLORS                                                                               \
    PALETTE(COLOR_RGB(14, 27, 19), COLOR_RGB(238, 228, 200), COLOR_RGB(140, 154, 124),             \
            COLOR_RGB(96, 108, 86), COLOR_RGB(222, 180, 92), COLOR_RGB(234, 204, 134),             \
            COLOR_RGB(246, 226, 170), COLOR_RGB(30, 22, 8), COLOR_RGB(14, 30, 21),                 \
            COLOR_RGB(28, 52, 38), COLOR_RGB(238, 210, 130), COLOR_RGB(140, 154, 124),             \
            COLOR_RGB(130, 116, 64), COLOR_RGB(140, 154, 124), COLOR_RGB(204, 72, 52),             \
            COLOR_RGB(126, 200, 128), COLOR_RGB(140, 154, 124), COLOR_RGB(224, 186, 100))

#define LAIN_COLORS                                                                                \
    PALETTE(COLOR_RGB(13, 8, 18), COLOR_RGB(228, 212, 234), COLOR_RGB(132, 112, 142),              \
            COLOR_RGB(90, 76, 100), COLOR_RGB(27, 40, 36), COLOR_RGB(32, 48, 42),                  \
            COLOR_RGB(240, 140, 184), COLOR_RGB(140, 255, 178), COLOR_RGB(18, 10, 25),             \
            COLOR_RGB(22, 12, 30), COLOR_RGB(140, 255, 178), COLOR_RGB(132, 112, 142),             \
            COLOR_RGB(90, 40, 90), COLOR_RGB(132, 112, 142), COLOR_RGB(235, 77, 96),               \
            COLOR_RGB(120, 255, 160), COLOR_RGB(132, 112, 142), COLOR_RGB(140, 255, 178))

#define NOAH_COLORS                                                                                \
    PALETTE(COLOR_RGB(12, 17, 33), COLOR_RGB(222, 236, 252), COLOR_RGB(112, 142, 184),             \
            COLOR_RGB(76, 96, 130), COLOR_RGB(40, 116, 232), COLOR_RGB(72, 136, 235),              \
            COLOR_RGB(128, 180, 250), COLOR_RGB(255, 255, 255), COLOR_RGB(12, 32, 83),             \
            COLOR_RGB(22, 63, 157), COLOR_RGB(232, 244, 255), COLOR_RGB(150, 180, 230),            \
            COLOR_RGB(6, 17, 60), COLOR_RGB(112, 142, 184), COLOR_RGB(235, 77, 61),                \
            COLOR_RGB(52, 199, 89), COLOR_RGB(112, 142, 184), COLOR_RGB(110, 170, 245))

#define ECHO_METRICS                                                                               \
    {                                                                                              \
        .screen_width = HAL_DISPLAY_WIDTH,                                                         \
        .screen_height = HAL_DISPLAY_HEIGHT,                                                       \
        .header_height = 29,                                                                       \
        .header_inset = 132,                                                                       \
        .row_height = 32,                                                                          \
        .padding = 12,                                                                             \
        .scrollbar_width = 4,                                                                      \
        .scrollbar_inset = 2,                                                                      \
        .detail_gap = 10,                                                                          \
        .status_icon_size = 11,                                                                    \
        .selection_inset = 6,                                                                      \
        .selection_radius = 6,                                                                     \
    }

#define ECHO_MARQUEE {.pause_ms = 1200, .speed_px_per_s = 56, .gap_px = 40}

#define THEMED(colors_, style_)                                                                    \
    {                                                                                              \
        .colors = colors_,                                                                         \
        .fonts =                                                                                   \
            {                                                                                      \
                .title = &FONT_INTER_SEMIBOLD_22,                                                  \
                .body = &FONT_INTER_REGULAR_22,                                                    \
                .small = &FONT_INTER_REGULAR_16,                                                   \
                .heading = &FONT_INTER_SEMIBOLD_16,                                                \
                .row = &FONT_INTER_REGULAR_16,                                                     \
            },                                                                                     \
        .metrics = ECHO_METRICS,                                                                   \
        .marquee = ECHO_MARQUEE,                                                                   \
        .style = style_,                                                                           \
    }

const Theme ECHO_NOTICE_THEME = {
    .colors = PODIUM_COLORS,
    .fonts =
        {
            .title = &FONT_INTER_REGULAR_16,
            .body = &FONT_INTER_REGULAR_16,
            .small = &FONT_INTER_REGULAR_16,
            .heading = &FONT_INTER_REGULAR_16,
            .row = &FONT_INTER_REGULAR_16,
        },
    .metrics = ECHO_METRICS,
    .marquee = ECHO_MARQUEE,
};

static const Theme THEMES[THEME_CHOICE_COUNT] = {
    [THEME_PODIUM] = THEMED(PODIUM_COLORS, THEME_STYLE_PLAIN),
    [THEME_POKEMON] = THEMED(POKEMON_COLORS, THEME_STYLE_POKEDEX),
    [THEME_ZELDA] = THEMED(ZELDA_COLORS, THEME_STYLE_HYRULE),
    [THEME_LAIN] = THEMED(LAIN_COLORS, THEME_STYLE_WIRED),
    [THEME_NOAH] = THEMED(NOAH_COLORS, THEME_STYLE_ICE),
};

const Theme *default_theme_for(ThemeChoice choice)
{
    return &THEMES[choice];
}
