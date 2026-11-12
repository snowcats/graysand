#include "cassette.h"

#include "board_layout.h"
#include "ui/header.h"
#include "ui/label.h"

enum {
    BODY_RADIUS = 12,
    BODY_EDGE = 2,
    SCREW_INSET = 12,
    SCREW_RADIUS = 4,
    SCREW_SLOT = 1,
    LABEL_SIDE = 20,
    LABEL_RADIUS = 6,
    STRIPE_HEIGHT = 3,
    STRIPE_SIDE = 10,
    WINDOW_SIDE = 40,
    WINDOW_RADIUS = 8,
    REEL_INSET = 34,
    PACK_MIN = 13,
    PACK_SPAN = 7,
    HUB_RADIUS = 11,
    HUB_HOLE = 5,
    TOOTH_RADIUS = 2,
    TOOTH_COUNT = 3,
    BASE_TOP_INSET = 64,
    BASE_BOTTOM_INSET = 40,
    BASE_HOLE_RADIUS = 3,
    BASE_HOLE_INSET = 44,
    BASE_HOLE_BOTTOM = 6,
    TAPE_LINE = 2,
    MIDDLE_GAP = 26,
    CHIP_WIDTH = 76,
    CHIP_HEIGHT = 22,
    CHIP_GAP = 10,
    CHIP_TOP = 8,
    CHIP_RADIUS = 5
};

static const CassetteColors COLORS[] = {
    [THEME_STYLE_PLAIN] = {.shell = COLOR_RGB(36, 38, 44),
                           .edge = COLOR_RGB(70, 74, 86),
                           .label = COLOR_RGB(237, 230, 214),
                           .ink = COLOR_RGB(24, 24, 30),
                           .ink_soft = COLOR_RGB(88, 84, 80),
                           .stripe = COLOR_RGB(34, 96, 208),
                           .window = COLOR_RGB(12, 13, 16),
                           .reel = COLOR_RGB(216, 216, 222),
                           .tape = COLOR_RGB(66, 44, 34)},
    [THEME_STYLE_POKEDEX] = {.shell = COLOR_RGB(196, 40, 10),
                             .edge = COLOR_RGB(124, 24, 6),
                             .label = COLOR_RGB(244, 244, 240),
                             .ink = COLOR_RGB(18, 19, 26),
                             .ink_soft = COLOR_RGB(96, 96, 108),
                             .stripe = COLOR_RGB(255, 203, 5),
                             .window = COLOR_RGB(18, 19, 26),
                             .reel = COLOR_RGB(244, 244, 240),
                             .tape = COLOR_RGB(60, 40, 30)},
    [THEME_STYLE_WIRED] = {.shell = COLOR_RGB(42, 30, 46),
                           .edge = COLOR_RGB(226, 150, 178),
                           .label = COLOR_RGB(205, 179, 214),
                           .ink = COLOR_RGB(27, 16, 32),
                           .ink_soft = COLOR_RGB(84, 60, 92),
                           .stripe = COLOR_RGB(120, 255, 160),
                           .window = COLOR_RGB(10, 7, 14),
                           .reel = COLOR_RGB(232, 216, 236),
                           .tape = COLOR_RGB(70, 40, 56)},
    [THEME_STYLE_HYRULE] = {.shell = COLOR_RGB(27, 42, 34),
                            .edge = COLOR_RGB(176, 140, 64),
                            .label = COLOR_RGB(232, 217, 174),
                            .ink = COLOR_RGB(32, 24, 10),
                            .ink_soft = COLOR_RGB(96, 80, 48),
                            .stripe = COLOR_RGB(222, 186, 102),
                            .window = COLOR_RGB(9, 15, 13),
                            .reel = COLOR_RGB(222, 186, 102),
                            .tape = COLOR_RGB(64, 46, 26)},
    [THEME_STYLE_ICE] = {.shell = COLOR_RGB(20, 32, 74),
                         .edge = COLOR_RGB(44, 124, 236),
                         .label = COLOR_RGB(220, 238, 251),
                         .ink = COLOR_RGB(10, 18, 48),
                         .ink_soft = COLOR_RGB(60, 80, 120),
                         .stripe = COLOR_RGB(168, 216, 248),
                         .window = COLOR_RGB(8, 10, 22),
                         .reel = COLOR_RGB(220, 238, 251),
                         .tape = COLOR_RGB(40, 46, 70)},
};

static const Point TEETH[CASSETTE_TURN_STEPS] = {{8, 0},   {7, 4},  {4, 7},  {0, 8},
                                                 {-4, 7},  {-7, 4}, {-8, 0}, {-7, -4},
                                                 {-4, -7}, {0, -8}, {4, -7}, {7, -4}};

CassetteColors cassette_colors(ThemeStyle style)
{
    return COLORS[style];
}

static int half_chord(int radius, int offset)
{
    int half = radius;
    while (half > 0 && half * half > radius * radius - offset * offset) {
        half--;
    }
    return half;
}

static void fill_disc(Canvas *canvas, Point center, int radius, Color color)
{
    int top = canvas->clip.y - center.y;
    int bottom = canvas->clip.y + canvas->clip.height - 1 - center.y;
    int first = -radius > top ? -radius : top;
    int last = radius < bottom ? radius : bottom;
    for (int offset = first; offset <= last; offset++) {
        int half = half_chord(radius, offset);
        canvas_fill_rect(canvas, (Rect){center.x - half, center.y + offset, 2 * half + 1, 1},
                         color);
    }
}

static int corner_inset(int radius, int row, int height)
{
    int from_edge = row < radius ? row : height - 1 - row;
    if (from_edge >= radius) {
        return 0;
    }
    return radius - half_chord(radius, radius - from_edge);
}

static void fill_corner_rows(Canvas *canvas, Rect area, int radius, Color color)
{
    for (int row = 0; row < radius; row++) {
        int inset = corner_inset(radius, row, area.height);
        int width = area.width - 2 * inset;
        canvas_fill_rect(canvas, (Rect){area.x + inset, area.y + row, width, 1}, color);
        canvas_fill_rect(canvas, (Rect){area.x + inset, area.y + area.height - 1 - row, width, 1},
                         color);
    }
}

static void fill_round(Canvas *canvas, Rect area, int radius, Color color)
{
    canvas_fill_rect(canvas, (Rect){area.x, area.y + radius, area.width, area.height - 2 * radius},
                     color);
    fill_corner_rows(canvas, area, radius, color);
}

static void fill_shell(Canvas *canvas, Rect body, Rect label, Color color)
{
    int right = body.x + body.width;
    int label_right = label.x + label.width;
    int label_bottom = label.y + label.height;
    int middle_top = body.y + BODY_RADIUS;
    int middle_bottom = body.y + body.height - BODY_RADIUS;
    fill_corner_rows(canvas, body, BODY_RADIUS, color);
    canvas_fill_rect(
        canvas, (Rect){body.x, middle_top, label.x - body.x, middle_bottom - middle_top}, color);
    canvas_fill_rect(
        canvas, (Rect){label_right, middle_top, right - label_right, middle_bottom - middle_top},
        color);
    canvas_fill_rect(
        canvas, (Rect){label.x, label_bottom, label.width, middle_bottom - label_bottom}, color);
    canvas_fill_rect(canvas, (Rect){label.x, label.y, LABEL_RADIUS, LABEL_RADIUS}, color);
    canvas_fill_rect(
        canvas, (Rect){label_right - LABEL_RADIUS, label.y, LABEL_RADIUS, LABEL_RADIUS}, color);
    canvas_fill_rect(
        canvas, (Rect){label.x, label_bottom - LABEL_RADIUS, LABEL_RADIUS, LABEL_RADIUS}, color);
    canvas_fill_rect(
        canvas,
        (Rect){label_right - LABEL_RADIUS, label_bottom - LABEL_RADIUS, LABEL_RADIUS, LABEL_RADIUS},
        color);
}

static Rect inset_rect(Rect area, int by)
{
    return (Rect){area.x + by, area.y + by, area.width - 2 * by, area.height - 2 * by};
}

CassetteParts cassette_parts(Rect area)
{
    Rect label = {area.x + LABEL_SIDE, area.y + CASSETTE_LABEL_TOP, area.width - 2 * LABEL_SIDE,
                  CASSETTE_LABEL_HEIGHT};
    Rect window = {label.x + WINDOW_SIDE, label.y + CASSETTE_WINDOW_TOP,
                   label.width - 2 * WINDOW_SIDE, CASSETTE_WINDOW_HEIGHT};
    int base_top = label.y + label.height + CASSETTE_BASE_GAP;
    Rect base = {area.x + BASE_BOTTOM_INSET, base_top, area.width - 2 * BASE_BOTTOM_INSET,
                 area.y + area.height - base_top};
    int reel_y = window.y + window.height / 2;
    return (CassetteParts){.body = area,
                           .label = label,
                           .window = window,
                           .base = base,
                           .left_reel = {window.x + REEL_INSET, reel_y},
                           .right_reel = {window.x + window.width - REEL_INSET, reel_y}};
}

Rect cassette_window_middle(const CassetteParts *parts)
{
    int left = parts->left_reel.x + PACK_MIN + PACK_SPAN + MIDDLE_GAP / 2;
    int right = parts->right_reel.x - PACK_MIN - PACK_SPAN - MIDDLE_GAP / 2;
    return (Rect){left, parts->window.y, right - left, parts->window.height};
}

static void draw_screw(Canvas *canvas, Point center, const CassetteColors *colors)
{
    fill_disc(canvas, center, SCREW_RADIUS, colors->edge);
    canvas_fill_rect(
        canvas, (Rect){center.x - SCREW_RADIUS + 1, center.y, 2 * SCREW_RADIUS - 1, SCREW_SLOT},
        colors->shell);
}

static void draw_base(Canvas *canvas, const CassetteParts *parts, const CassetteColors *colors)
{
    Rect base = parts->base;
    int slant = BASE_TOP_INSET - BASE_BOTTOM_INSET;
    for (int row = 0; row < base.height; row++) {
        int inset = slant - slant * row / (base.height > 1 ? base.height - 1 : 1);
        Rect line = {base.x + inset, base.y + row, base.width - 2 * inset, 1};
        canvas_fill_rect(canvas, line, row == 0 ? colors->edge : colors->window);
    }
    int hole_y = base.y + base.height - BASE_HOLE_BOTTOM;
    fill_disc(canvas, (Point){base.x + BASE_HOLE_INSET, hole_y}, BASE_HOLE_RADIUS, colors->shell);
    fill_disc(canvas, (Point){base.x + base.width - BASE_HOLE_INSET, hole_y}, BASE_HOLE_RADIUS,
              colors->shell);
}

static void fill_backdrop(Canvas *canvas, Rect content, Rect body, Color color)
{
    int right = body.x + body.width;
    int bottom = body.y + body.height;
    canvas_fill_rect(canvas, (Rect){content.x, content.y, content.width, body.y - content.y},
                     color);
    canvas_fill_rect(canvas,
                     (Rect){content.x, bottom, content.width, content.y + content.height - bottom},
                     color);
    canvas_fill_rect(canvas, (Rect){content.x, body.y, body.x - content.x, body.height}, color);
    canvas_fill_rect(canvas, (Rect){right, body.y, content.x + content.width - right, body.height},
                     color);
    for (int corner = 0; corner < 4; corner++) {
        int x = corner % 2 == 0 ? body.x : right - BODY_RADIUS;
        int y = corner < 2 ? body.y : bottom - BODY_RADIUS;
        canvas_fill_rect(canvas, (Rect){x, y, BODY_RADIUS, BODY_RADIUS}, color);
    }
}

static void stroke_row(Canvas *canvas, Rect area, int row, int outer, int inner, Color color)
{
    canvas_fill_rect(canvas, (Rect){area.x + outer, area.y + row, inner - outer, 1}, color);
    canvas_fill_rect(canvas, (Rect){area.x + area.width - inner, area.y + row, inner - outer, 1},
                     color);
}

static void stroke_round(Canvas *canvas, Rect area, Color color)
{
    int inner_radius = BODY_RADIUS - BODY_EDGE;
    int inner_height = area.height - 2 * BODY_EDGE;
    for (int row = 0; row < BODY_RADIUS; row++) {
        int outer = corner_inset(BODY_RADIUS, row, area.height);
        int inner = row < BODY_EDGE
                        ? area.width / 2
                        : BODY_EDGE + corner_inset(inner_radius, row - BODY_EDGE, inner_height);
        stroke_row(canvas, area, row, outer, inner, color);
        stroke_row(canvas, area, area.height - 1 - row, outer, inner, color);
    }
    int middle = area.height - 2 * BODY_RADIUS;
    canvas_fill_rect(canvas, (Rect){area.x, area.y + BODY_RADIUS, BODY_EDGE, middle}, color);
    canvas_fill_rect(
        canvas, (Rect){area.x + area.width - BODY_EDGE, area.y + BODY_RADIUS, BODY_EDGE, middle},
        color);
}

void cassette_draw_body(Canvas *canvas, const CassetteParts *parts, const CassetteColors *colors,
                        CassetteBackdrop backdrop)
{
    Rect body = parts->body;
    fill_backdrop(canvas, backdrop.area, body, backdrop.color);
    fill_shell(canvas, body, parts->label, colors->shell);
    stroke_round(canvas, body, colors->edge);
    int right = body.x + body.width - 1 - SCREW_INSET;
    int bottom = body.y + body.height - 1 - SCREW_INSET;
    draw_screw(canvas, (Point){body.x + SCREW_INSET, body.y + SCREW_INSET}, colors);
    draw_screw(canvas, (Point){right, body.y + SCREW_INSET}, colors);
    draw_screw(canvas, (Point){body.x + SCREW_INSET, bottom}, colors);
    draw_screw(canvas, (Point){right, bottom}, colors);
    Rect label = parts->label;
    fill_round(canvas, label, LABEL_RADIUS, colors->label);
    canvas_fill_rect(canvas,
                     (Rect){label.x + STRIPE_SIDE, label.y + CASSETTE_STRIPE_TOP,
                            label.width - 2 * STRIPE_SIDE, STRIPE_HEIGHT},
                     colors->stripe);
    fill_round(canvas, parts->window, WINDOW_RADIUS, colors->edge);
    fill_round(canvas, inset_rect(parts->window, 1), WINDOW_RADIUS - 1, colors->window);
    draw_base(canvas, parts, colors);
}

static void draw_reel(Canvas *canvas, Point center, int pack, int turn,
                      const CassetteColors *colors)
{
    fill_disc(canvas, center, pack, colors->tape);
    fill_disc(canvas, center, HUB_RADIUS, colors->reel);
    fill_disc(canvas, center, HUB_HOLE, colors->window);
    for (int tooth = 0; tooth < TOOTH_COUNT; tooth++) {
        Point offset =
            TEETH[(turn + tooth * CASSETTE_TURN_STEPS / TOOTH_COUNT) % CASSETTE_TURN_STEPS];
        fill_disc(canvas, (Point){center.x + offset.x, center.y + offset.y}, TOOTH_RADIUS,
                  colors->window);
    }
}

void cassette_draw_reels(Canvas *canvas, const CassetteParts *parts, const CassetteColors *colors,
                         uint32_t progress_permille, int turn)
{
    uint32_t done = progress_permille > CASSETTE_PERMILLE ? CASSETTE_PERMILLE : progress_permille;
    int left =
        PACK_MIN + (int)((uint32_t)PACK_SPAN * (CASSETTE_PERMILLE - done) / CASSETTE_PERMILLE);
    int right = PACK_MIN + (int)((uint32_t)PACK_SPAN * done / CASSETTE_PERMILLE);
    int bottom = parts->left_reel.y + PACK_MIN + PACK_SPAN - TAPE_LINE;
    canvas_fill_rect(
        canvas,
        (Rect){parts->left_reel.x, bottom, parts->right_reel.x - parts->left_reel.x, TAPE_LINE},
        colors->tape);
    int step = ((turn % CASSETTE_TURN_STEPS) + CASSETTE_TURN_STEPS) % CASSETTE_TURN_STEPS;
    draw_reel(canvas, parts->left_reel, left, step, colors);
    draw_reel(canvas, parts->right_reel, right,
              (step + CASSETTE_TURN_STEPS / 2) % CASSETTE_TURN_STEPS, colors);
}

static Rect board_cassette_area(const Theme *theme)
{
    const NowPlayingLayout *layout = &BOARD_LAYOUT.now_playing;
    return (Rect){(theme->metrics.screen_width - layout->cassette_width) / 2, layout->cassette_top,
                  layout->cassette_width, layout->cassette_height};
}

static void draw_centered(Canvas *canvas, TextStyle style, Rect area, const char *text)
{
    label_draw(canvas, style,
               (Label){.area = label_centered_area(style.font, area, text), .text = text});
}

void cassette_draw_notice(Canvas *canvas, const Theme *theme, const char *heading,
                          const char *message, bool dac)
{
    const NowPlayingLayout *layout = &BOARD_LAYOUT.now_playing;
    header_draw(canvas, theme, heading);
    CassetteParts parts = cassette_parts(board_cassette_area(theme));
    CassetteColors colors = cassette_colors(theme->style);
    cassette_draw_body(canvas, &parts, &colors,
                       (CassetteBackdrop){theme_content_area(theme), theme->colors.background});
    cassette_draw_reels(canvas, &parts, &colors, CASSETTE_PERMILLE / 2, 0);
    Rect label = parts.label;
    TextStyle title = {.font = theme->fonts.title, .color = colors.ink};
    TextStyle small = {.font = theme->fonts.small, .color = colors.ink_soft};
    draw_centered(
        canvas, title,
        (Rect){label.x, label.y + layout->title_top, label.width, layout->text_line_height},
        heading);
    draw_centered(
        canvas, small,
        (Rect){label.x, label.y + layout->footer_top, label.width, layout->small_line_height},
        message);
    TextStyle brand = {.font = theme->fonts.small, .color = colors.stripe};
    draw_centered(canvas, brand, cassette_window_middle(&parts), "GRAYSAND");
    Rect base = parts.base;
    int left = base.x + base.width / 2 - CHIP_WIDTH - CHIP_GAP / 2;
    Rect chip = {left, base.y + CHIP_TOP, CHIP_WIDTH, CHIP_HEIGHT};
    for (int index = 0; index < 2; index++) {
        bool lit = (index == 1) == dac;
        fill_round(canvas, chip, CHIP_RADIUS, lit ? colors.stripe : colors.tape);
        TextStyle style = {.font = theme->fonts.small, .color = lit ? colors.window : colors.reel};
        draw_centered(canvas, style, chip, index == 0 ? "DATA" : "DAC");
        chip.x += CHIP_WIDTH + CHIP_GAP;
    }
}
