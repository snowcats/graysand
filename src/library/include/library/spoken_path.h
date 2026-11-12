#pragma once

#include <stddef.h>

typedef enum SpokenKind {
    SPOKEN_NONE,
    SPOKEN_AUDIOBOOK,
    SPOKEN_PODCAST,
    SPOKEN_QURAN
} SpokenKind;

SpokenKind spoken_kind_of(const char *path);
SpokenKind spoken_kind_of_name(const char *name, size_t length);
