#pragma once

#include "ui/theme.h"

#include "settings/settings.h"

const Theme *default_theme_for(ThemeChoice choice);
extern const Theme ECHO_NOTICE_THEME;
#define BOARD_THEME (*default_theme_for(THEME_PODIUM))
#define NOTICE_THEME ECHO_NOTICE_THEME
