#include "board_layout.h"

#include "cover_cache.h"

const BoardLayout BOARD_LAYOUT = {
    .home_art_width = 180,
    .up_next_detail_max_width = 150,
    .now_playing =
        {
            .cassette_top = 34,
            .cassette_width = 392,
            .cassette_height = 184,
            .label_inset = 6,
            .cover_size = 48,
            .title_top = 4,
            .artist_top = 32,
            .footer_top = 104,
            .text_line_height = 28,
            .small_line_height = 20,
            .progress_top = 6,
            .progress_height = 3,
            .times_top = 12,
            .times_inset = 24,
        },
    .cover_flow =
        {
            .center_x = 240,
            .center_top = 8,
            .center_size = 128,
            .side_offset = 112,
            .side_spacing = 52,
            .side_width = 60,
            .side_near_height = 120,
            .side_far_height = 90,
            .side_near_light = 230,
            .side_far_light = 140,
            .fade_start = 2 * COVER_FLOW_ONE,
            .fade_end = 2 * COVER_FLOW_ONE + COVER_FLOW_ONE / 2,
        },
    .cover_flow_text =
        {
            .title_top = 142,
            .artist_top = 172,
            .line_height = 28,
            .reflection_rows = 40,
        },
};
