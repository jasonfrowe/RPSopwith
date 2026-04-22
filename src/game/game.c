#include "game/game.h"

#include "config/feature_flags.h"

static int16_t clamp_i16(int16_t value, int16_t min_value, int16_t max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

void game_init(game_state_t *state)
{
    state->tick_count_10hz = 0;
    state->plane_x = 160;
    state->plane_y = 120;
    state->crashed = false;
}

void game_tick_10hz(game_state_t *state, const input_actions_t *actions)
{
    state->tick_count_10hz++;

#if SOPWITH_FEATURE_FLIGHT
    if (!state->crashed) {
        if (actions->left) {
            state->plane_x -= 2;
        }
        if (actions->right) {
            state->plane_x += 2;
        }
        if (actions->up) {
            state->plane_y -= 2;
        }
        if (actions->down) {
            state->plane_y += 2;
        }
    }
#else
    (void)actions;
#endif

    state->plane_x = clamp_i16(state->plane_x, 0, 319);
    state->plane_y = clamp_i16(state->plane_y, 0, 239);

#if SOPWITH_FEATURE_TERRAIN
    if (state->plane_y >= 232) {
        state->crashed = true;
    }
#endif

    if (actions->start) {
        state->crashed = false;
    }
}

void game_render_interpolated(const game_state_t *state, uint8_t subframe)
{
    (void)state;
    (void)subframe;
}