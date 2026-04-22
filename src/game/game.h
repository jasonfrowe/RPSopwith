#ifndef GAME_GAME_H
#define GAME_GAME_H

#include <stdbool.h>
#include <stdint.h>

#include "platform/input.h"

typedef struct game_state_s {
    uint32_t tick_count_10hz;
    int16_t plane_x;
    int16_t plane_y;
    bool crashed;
} game_state_t;

void game_init(game_state_t *state);
void game_tick_10hz(game_state_t *state, const input_actions_t *actions);
void game_render_interpolated(const game_state_t *state, uint8_t subframe);

#endif