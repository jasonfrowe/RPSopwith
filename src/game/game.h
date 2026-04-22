#ifndef GAME_GAME_H
#define GAME_GAME_H

#include <stdbool.h>
#include <stdint.h>

#include "platform/input.h"

typedef struct game_state_s {
    uint32_t tick_count_10hz;
    uint16_t prev_world_x;
    uint16_t world_x;
    int16_t plane_x;
    int16_t plane_y;
    int8_t plane_bank;
    int8_t plane_pitch;
    int8_t plane_vy;
    uint8_t throttle;
    uint8_t speed;
    bool airborne;
    bool plane_orient;
    bool flip_latch;
    bool stalled_high;
    uint8_t stall_tick;
    uint8_t terrain_edit_cooldown;
    bool crashed;
} game_state_t;

void game_init(game_state_t *state);
void game_tick_10hz(game_state_t *state, const input_actions_t *actions);
void game_render_interpolated(const game_state_t *state, uint8_t subframe);

#endif