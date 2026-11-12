#include "cover_cache.h"

const char COVER_CACHE_PATH[] = "/.echo/covers.bin";

static bool read_file(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    const CoverCache *cache = context;
    return hal_file_read(cache->file, offset, buffer, length);
}

void cover_cache_open(CoverCache *cache, const Library *library)
{
    cache->library = library;
    cache->available = false;
    cache->full_state = COVER_EMPTY;
    if (!hal_file_open(&cache->file, COVER_CACHE_PATH)) {
        return;
    }
    ByteSource source = {.read = read_file, .context = cache};
    cache->available = covers_open(&cache->covers, source, cache->file.size);
    if (!cache->available) {
        hal_file_close(&cache->file);
    }
}

void cover_cache_close(CoverCache *cache)
{
    if (cache->available) {
        hal_file_close(&cache->file);
        cache->available = false;
    }
}

static CoverState state_of(CoverState state, Image *image, Image loaded)
{
    if (state == COVER_LOADED) {
        *image = loaded;
    }
    return state;
}

CoverState cover_cache_full(const CoverCache *cache, uint16_t album, Image *image)
{
    if (cache->full_state == COVER_EMPTY || cache->full_album != album) {
        return COVER_EMPTY;
    }
    Image loaded = {.pixels = cache->full, .width = COVER_FULL_SIZE, .height = COVER_FULL_SIZE};
    return state_of(cache->full_state, image, loaded);
}

static CoverState read_cover(const CoverCache *cache, uint16_t album, CoverSize size, Color *pixels)
{
    uint32_t record = 0;
    uint32_t key = library_album_key(cache->library, album);
    bool found = cache->available && covers_find(&cache->covers, key, &record) &&
                 covers_read(&cache->covers, record, size, pixels);
    return found ? COVER_LOADED : COVER_MISSING;
}

bool cover_cache_load_full(CoverCache *cache, uint16_t album)
{
    Image ignored;
    if (cover_cache_full(cache, album, &ignored) != COVER_EMPTY) {
        return false;
    }
    cache->full_album = album;
    cache->full_state = read_cover(cache, album, COVER_FULL, cache->full);
    return true;
}

bool cover_cache_find(const CoverCache *cache, uint16_t album, uint32_t *record)
{
    return cache->available &&
           covers_find(&cache->covers, library_album_key(cache->library, album), record);
}

bool cover_cache_read_half_rows(const CoverCache *cache, uint32_t record, int first_row, int rows,
                                Color *pixels)
{
    return covers_read_rows(&cache->covers, record, COVER_HALF, first_row, rows, pixels);
}

enum {
    SCALE_CHUNK_ROWS = 8
};

typedef struct ScaleSource {
    const CoverCache *cache;
    uint32_t record;
    CoverSize size;
    int side;
    int first;
    int count;
    Color rows[COVER_FULL_SIZE * SCALE_CHUNK_ROWS];
} ScaleSource;

static bool hold_rows(ScaleSource *source, int first, int last)
{
    if (first >= source->first && last <= source->first + source->count) {
        return true;
    }
    int count = source->side - first < SCALE_CHUNK_ROWS ? source->side - first : SCALE_CHUNK_ROWS;
    source->first = first;
    source->count = 0;
    if (!covers_read_rows(&source->cache->covers, source->record, source->size, first, count,
                          source->rows)) {
        return false;
    }
    source->count = count;
    return true;
}

static int span_end(int index, int side, int size)
{
    int end = ((index + 1) * side + size - 1) / size;
    int start = index * side / size;
    return end > start ? end : start + 1;
}

static Color average(const ScaleSource *source, int x0, int x1, int y0, int y1)
{
    unsigned red = 0;
    unsigned green = 0;
    unsigned blue = 0;
    unsigned count = 0;
    for (int y = y0; y < y1; y++) {
        const Color *row = &source->rows[(y - source->first) * source->side];
        for (int x = x0; x < x1; x++) {
            red += row[x] >> 11;
            green += row[x] >> 5 & 0x3Fu;
            blue += row[x] & 0x1Fu;
            count++;
        }
    }
    return (Color)((red / count) << 11 | (green / count) << 5 | blue / count);
}

void cover_cache_draw(const CoverCache *cache, uint16_t album, Canvas *canvas, Rect cell,
                      Color empty)
{
    static ScaleSource source;
    Rect visible = rect_intersection(canvas->clip, cell);
    if (rect_is_empty(visible)) {
        return;
    }
    source.cache = cache;
    source.size = cell.width <= COVER_HALF_SIZE ? COVER_HALF : COVER_FULL;
    source.side = source.size == COVER_HALF ? COVER_HALF_SIZE : COVER_FULL_SIZE;
    source.count = 0;
    if (!cover_cache_find(cache, album, &source.record)) {
        canvas_fill_rect(canvas, cell, empty);
        return;
    }
    for (int y = visible.y; y < visible.y + visible.height; y++) {
        int y0 = (y - cell.y) * source.side / cell.height;
        int y1 = span_end(y - cell.y, source.side, cell.height);
        if (!hold_rows(&source, y0, y1)) {
            canvas_fill_rect(canvas, cell, empty);
            return;
        }
        Color *target = &canvas->pixels[(y - canvas->bounds.y) * canvas->bounds.width];
        for (int x = visible.x; x < visible.x + visible.width; x++) {
            int x0 = (x - cell.x) * source.side / cell.width;
            int x1 = span_end(x - cell.x, source.side, cell.width);
            target[x - canvas->bounds.x] = average(&source, x0, x1, y0, y1);
        }
    }
}
