#include "lyrics.h"

#include "hal/storage.h"

enum {
    LYRICS_BYTES = 12288,
    LYRICS_PATH = 260,
    STAMPS_PER_LINE = 8,
    MINUTE_MS = 60000,
    SECOND_MS = 1000
};

static char stored[LYRICS_BYTES];
static bool loaded;

static bool digit(char character)
{
    return character >= '0' && character <= '9';
}

static bool lrc_path(const char *audio, char *path, size_t capacity)
{
    size_t length = 0;
    size_t dot = 0;
    bool has_dot = false;
    while (audio[length] != '\0') {
        if (length + 1 >= capacity) {
            return false;
        }
        char character = audio[length];
        path[length] = character;
        if (character == '/' || character == '\\') {
            has_dot = false;
        }
        if (character == '.') {
            dot = length;
            has_dot = true;
        }
        length++;
    }
    size_t at = has_dot ? dot : length;
    const char extension[] = ".lrc";
    for (int index = 0; extension[index] != '\0'; index++) {
        if (at + 1 >= capacity) {
            return false;
        }
        path[at++] = extension[index];
    }
    path[at] = '\0';
    return true;
}

static bool parse_stamp(const char **cursor, const char *end, uint32_t *milliseconds)
{
    const char *text = *cursor;
    if (text >= end || *text != '[') {
        return false;
    }
    text++;
    if (text >= end || !digit(*text)) {
        return false;
    }
    uint32_t minutes = 0;
    int digits = 0;
    while (text < end && digit(*text) && digits < 4) {
        minutes = minutes * 10 + (uint32_t)(*text - '0');
        text++;
        digits++;
    }
    if (digits == 0 || text >= end || *text != ':') {
        return false;
    }
    text++;
    if (text + 1 >= end || !digit(text[0]) || !digit(text[1])) {
        return false;
    }
    uint32_t seconds = (uint32_t)(text[0] - '0') * 10 + (uint32_t)(text[1] - '0');
    text += 2;
    if (seconds >= 60) {
        return false;
    }
    uint32_t fraction = 0;
    int fraction_digits = 0;
    if (text < end && *text == '.') {
        text++;
        while (text < end && digit(*text) && fraction_digits < 3) {
            fraction = fraction * 10 + (uint32_t)(*text - '0');
            text++;
            fraction_digits++;
        }
        while (text < end && digit(*text)) {
            text++;
        }
    }
    if (text >= end || *text != ']') {
        return false;
    }
    for (int index = fraction_digits; index < 3; index++) {
        fraction *= 10;
    }
    *milliseconds = minutes * MINUTE_MS + seconds * SECOND_MS + fraction;
    *cursor = text + 1;
    return true;
}

static void copy_span(const char *start, const char *end, char *buffer, size_t capacity)
{
    while (start < end && (*start == ' ' || *start == '\t')) {
        start++;
    }
    while (end > start && (end[-1] == ' ' || end[-1] == '\t')) {
        end--;
    }
    size_t index = 0;
    while (start < end && index + 1 < capacity) {
        buffer[index++] = *start++;
    }
    buffer[index] = '\0';
}

typedef struct LyricSpan {
    bool found;
    uint32_t from;
    uint32_t until;
} LyricSpan;

static LyricSpan find_line(const char *text, uint32_t position_ms, char *buffer, size_t capacity);

bool lyrics_line_at(const char *text, uint32_t position_ms, char *buffer, size_t capacity)
{
    if (capacity == 0) {
        return false;
    }
    buffer[0] = '\0';
    if (text == NULL) {
        return false;
    }
    find_line(text, position_ms, buffer, capacity);
    return buffer[0] != '\0';
}

static LyricSpan find_line(const char *text, uint32_t position_ms, char *buffer, size_t capacity)
{
    buffer[0] = '\0';
    if ((unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB &&
        (unsigned char)text[2] == 0xBF) {
        text += 3;
    }
    bool found = false;
    uint32_t best = 0;
    bool later = false;
    uint32_t next = 0;
    const char *cursor = text;
    while (*cursor != '\0') {
        const char *line = cursor;
        while (*cursor != '\0' && *cursor != '\n') {
            cursor++;
        }
        const char *end = cursor;
        if (*cursor == '\n') {
            cursor++;
        }
        if (end > line && end[-1] == '\r') {
            end--;
        }
        const char *mark = line;
        uint32_t stamps[STAMPS_PER_LINE];
        int count = 0;
        while (mark < end && *mark == '[') {
            uint32_t stamp = 0;
            const char *before = mark;
            if (!parse_stamp(&mark, end, &stamp) || mark == before) {
                break;
            }
            if (count < STAMPS_PER_LINE) {
                stamps[count++] = stamp;
            }
        }
        for (int index = 0; index < count; index++) {
            if (stamps[index] <= position_ms && (!found || stamps[index] >= best)) {
                found = true;
                best = stamps[index];
                copy_span(mark, end, buffer, capacity);
            }
            if (stamps[index] > position_ms && (!later || stamps[index] < next)) {
                later = true;
                next = stamps[index];
            }
        }
    }
    return (LyricSpan){.found = found, .from = best, .until = later ? next : UINT32_MAX};
}

bool lyrics_window(uint32_t position_ms, char lines[][LYRICS_LINE_CAPACITY], int count,
                   uint32_t *from, uint32_t *until)
{
    int middle = count / 2;
    for (int index = 0; index < count; index++) {
        lines[index][0] = '\0';
    }
    *from = 0;
    *until = UINT32_MAX;
    if (!loaded) {
        return false;
    }
    LyricSpan span = find_line(stored, position_ms, lines[middle], LYRICS_LINE_CAPACITY);
    *from = span.from;
    *until = span.until;
    LyricSpan before = span;
    for (int index = middle - 1; index >= 0 && before.found && before.from > 0; index--) {
        before = find_line(stored, before.from - 1, lines[index], LYRICS_LINE_CAPACITY);
    }
    LyricSpan after = span;
    for (int index = middle + 1; index < count && after.until != UINT32_MAX; index++) {
        after = find_line(stored, after.until, lines[index], LYRICS_LINE_CAPACITY);
    }
    return true;
}

void lyrics_clear(void)
{
    loaded = false;
    stored[0] = '\0';
}

void lyrics_load(const char *audio_path)
{
    lyrics_clear();
    char path[LYRICS_PATH];
    if (audio_path == NULL || !lrc_path(audio_path, path, sizeof path)) {
        return;
    }
    HalFile file;
    if (!hal_file_open(&file, path)) {
        return;
    }
    uint32_t length = file.size < LYRICS_BYTES ? file.size : LYRICS_BYTES - 1;
    bool read = length == 0 || hal_file_read(file, 0, stored, length);
    hal_file_close(&file);
    if (!read) {
        stored[0] = '\0';
        return;
    }
    stored[length] = '\0';
    loaded = true;
}

bool lyrics_line(uint32_t position_ms, char *buffer, size_t capacity)
{
    if (!loaded) {
        if (capacity > 0) {
            buffer[0] = '\0';
        }
        return false;
    }
    return lyrics_line_at(stored, position_ms, buffer, capacity);
}
