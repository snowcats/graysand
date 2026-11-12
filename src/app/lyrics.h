#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    LYRICS_LINE_CAPACITY = 96
};

void lyrics_clear(void);
void lyrics_load(const char *audio_path);
bool lyrics_line_at(const char *text, uint32_t position_ms, char *buffer, size_t capacity);
bool lyrics_line(uint32_t position_ms, char *buffer, size_t capacity);
bool lyrics_window(uint32_t position_ms, char lines[][LYRICS_LINE_CAPACITY], int count,
                   uint32_t *from, uint32_t *until);
