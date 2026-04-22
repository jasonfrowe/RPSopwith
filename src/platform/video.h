#ifndef PLATFORM_VIDEO_H
#define PLATFORM_VIDEO_H

#include <stdbool.h>
#include <stdint.h>

#include "game/game.h"

bool platform_video_init(void);
void platform_video_render(const game_state_t *state, uint8_t subframe);

#endif