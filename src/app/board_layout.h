#pragma once

#include "ui/cover_flow.h"

enum {
    BOARD_STRIP_HEIGHT = 23
};

typedef struct NowPlayingLayout {
    int cassette_top;
    int cassette_width;
    int cassette_height;
    int label_inset;
    int cover_size;
    int title_top;
    int artist_top;
    int footer_top;
    int text_line_height;
    int small_line_height;
    int progress_top;
    int progress_height;
    int times_top;
    int times_inset;
} NowPlayingLayout;

typedef struct CoverFlowTextLayout {
    int title_top;
    int artist_top;
    int line_height;
    int reflection_rows;
} CoverFlowTextLayout;

typedef struct BoardLayout {
    int home_art_width;
    int up_next_detail_max_width;
    NowPlayingLayout now_playing;
    CoverFlowLayout cover_flow;
    CoverFlowTextLayout cover_flow_text;
} BoardLayout;

extern const BoardLayout BOARD_LAYOUT;
