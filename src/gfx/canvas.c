#include "gfx/canvas.h"

static Color *pixel_address(const Canvas *canvas, int x, int y)
{
    int column = x - canvas->bounds.x;
    int row = y - canvas->bounds.y;
    return &canvas->pixels[row * canvas->bounds.width + column];
}

static void fill_row(Color *row, int width, Color color)
{
    int column = 0;
    if (width > 0 && ((uintptr_t)row & 2u) != 0) {
        row[0] = color;
        column = 1;
    }
    uint32_t pair = (uint32_t)color | (uint32_t)color << 16;
    uint32_t quad[4] = {pair, pair, pair, pair};
    for (; column + 7 < width; column += 8) {
        __builtin_memcpy(&row[column], quad, sizeof quad);
    }
    for (; column + 1 < width; column += 2) {
        __builtin_memcpy(&row[column], &pair, sizeof pair);
    }
    if (column < width) {
        row[column] = color;
    }
}

static bool contains(Rect area, Point point)
{
    return point.x >= area.x && point.y >= area.y && point.x - area.x < area.width &&
           point.y - area.y < area.height;
}

Canvas canvas_make(Color *pixels, Rect bounds)
{
    return (Canvas){.pixels = pixels, .bounds = bounds, .clip = bounds};
}

Canvas canvas_clipped(const Canvas *canvas, Rect area)
{
    Canvas clipped = *canvas;
    clipped.clip = rect_intersection(canvas->clip, area);
    return clipped;
}

Canvas canvas_shifted(const Canvas *canvas, int dx, Rect area)
{
    Canvas shifted = *canvas;
    shifted.bounds.x -= dx;
    shifted.clip.x -= dx;
    shifted.clip = rect_intersection(shifted.clip, area);
    return shifted;
}

void canvas_clear(Canvas *canvas, Color color)
{
    canvas_fill_rect(canvas, canvas->clip, color);
}

void canvas_fill_rect(Canvas *canvas, Rect area, Color color)
{
    Rect visible = rect_intersection(canvas->clip, area);
    if (rect_is_empty(visible)) {
        return;
    }
    for (int y = visible.y; y < visible.y + visible.height; y++) {
        fill_row(pixel_address(canvas, visible.x, y), visible.width, color);
    }
}

void canvas_fill_gradient(Canvas *canvas, Rect area, Color top, Color bottom)
{
    for (int row = 0; row < area.height; row++) {
        Color color = color_between(top, bottom, row, area.height - 1);
        canvas_fill_rect(canvas, (Rect){area.x, area.y + row, area.width, 1}, color);
    }
}

void canvas_blend_pixel(Canvas *canvas, Point point, Color color, uint8_t alpha)
{
    if (!contains(canvas->clip, point)) {
        return;
    }
    Color *pixel = pixel_address(canvas, point.x, point.y);
    *pixel = color_blend(*pixel, color, alpha);
}

Color canvas_pixel_at(const Canvas *canvas, int x, int y)
{
    return contains(canvas->bounds, (Point){x, y}) ? *pixel_address(canvas, x, y) : 0;
}
