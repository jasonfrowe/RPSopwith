#include "game/game.h"

#include "constants.h"
#include "config/feature_flags.h"
#include "game/terrain/terrain.h"
#include "platform/video.h"

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
    state->prev_world_x = RPS_SCREEN_WIDTH_PX / 2u;
    state->world_x = RPS_SCREEN_WIDTH_PX / 2u;
    state->plane_x = (int16_t)(RPS_SCREEN_WIDTH_PX / 2u);
    state->plane_y = (int16_t)(RPS_SCREEN_HEIGHT_PX / 2u);
    state->terrain_edit_cooldown = 0;
    state->crashed = false;

#if SOPWITH_FEATURE_TERRAIN
    terrain_init();
#endif
}

void game_tick_10hz(game_state_t *state, const input_actions_t *actions)
{
    state->tick_count_10hz++;
    state->prev_world_x = state->world_x;

#if SOPWITH_FEATURE_FLIGHT
    if (!state->crashed) {
        if (actions->left) {
            state->plane_x -= 2;
            state->world_x = (uint16_t)((state->world_x + TERRAIN_WORLD_WIDTH - 2) % TERRAIN_WORLD_WIDTH);
        }
        if (actions->right) {
            state->plane_x += 2;
            state->world_x = (uint16_t)((state->world_x + 2) % TERRAIN_WORLD_WIDTH);
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

    state->plane_x = clamp_i16(state->plane_x, 0, (int16_t)(RPS_SCREEN_WIDTH_PX - 1u));
    state->plane_y = clamp_i16(state->plane_y, 0, (int16_t)(RPS_SCREEN_HEIGHT_PX - 1u));

    if (state->terrain_edit_cooldown > 0) {
        state->terrain_edit_cooldown--;
    }

#if SOPWITH_FEATURE_TERRAIN
    if (actions->bomb && state->terrain_edit_cooldown == 0) {
        terrain_crater(state->world_x, 8, 6);
        state->terrain_edit_cooldown = 2;
    }

    if (state->plane_y >= (int16_t)(terrain_height_at_world_x(state->world_x) - 2)) {
        state->crashed = true;
    }
#endif

    if (actions->start) {
        state->crashed = false;
    }
}

void game_render_interpolated(const game_state_t *state, uint8_t subframe)
{
    platform_video_render(state, subframe);
}