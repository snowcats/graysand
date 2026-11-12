#include "ui/screen.h"

#include <stddef.h>

#define PICTURE_PRIME 16777619u

ScreenAction screen_action_none(void)
{
    return (ScreenAction){.type = SCREEN_ACTION_NONE, .target = NULL};
}

ScreenAction screen_action_push(Screen *target)
{
    return (ScreenAction){.type = SCREEN_ACTION_PUSH, .target = target};
}

ScreenAction screen_action_pop(void)
{
    return (ScreenAction){.type = SCREEN_ACTION_POP, .target = NULL};
}

ScreenAction screen_action_pop_to_root(void)
{
    return (ScreenAction){.type = SCREEN_ACTION_POP_TO_ROOT, .target = NULL};
}

uint32_t screen_picture_mix(uint32_t hash, uint32_t value)
{
    for (int byte = 0; byte < 4; byte++) {
        hash = (hash ^ ((value >> (8 * byte)) & 0xFFu)) * PICTURE_PRIME;
    }
    return hash;
}

uint32_t screen_picture_text(uint32_t hash, const char *text)
{
    if (text == NULL) {
        return screen_picture_mix(hash, 0);
    }
    for (; *text != '\0'; text++) {
        hash = (hash ^ (unsigned char)*text) * PICTURE_PRIME;
    }
    return screen_picture_mix(hash, 1);
}
