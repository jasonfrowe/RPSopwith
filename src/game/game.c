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

static int8_t clamp_i8(int8_t value, int8_t min_value, int8_t max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static uint8_t wrap_angle16(int16_t angle)
{
    while (angle < 0) {
        angle += 16;
    }
    while (angle >= 16) {
        angle -= 16;
    }
    return (uint8_t)angle;
}

static uint8_t angle_step_toward(uint8_t angle, uint8_t target)
{
    int8_t delta = (int8_t)((target - angle) & 0x0F);
    if (delta == 0) {
        return angle;
    }
    if (delta < 8) {
        return (uint8_t)((angle + 1u) & 0x0Fu);
    }
    return (uint8_t)((angle - 1u) & 0x0Fu);
}

static void enter_stall_state(game_state_t *state)
{
    // Match Sopwith stallpln() setup as closely as possible.
    state->plane_orient = false;
    state->plane_pitch = 14; // 7 * ANGLES / 8 when ANGLES=16
    state->speed = 0;
    state->plane_vy = 0;
    state->stalled_high = true;
    state->stall_tick = 6; // STALLCOUNT in original
}

static int16_t wrap_world_x_i16(int32_t x)
{
    while (x < 0) {
        x += (int32_t)TERRAIN_WORLD_WIDTH;
    }
    while (x >= (int32_t)TERRAIN_WORLD_WIDTH) {
        x -= (int32_t)TERRAIN_WORLD_WIDTH;
    }
    return (int16_t)x;
}

// Sopwith table: sine(pi/8 increments) * 256.
static const int16_t s_sintab[16] = {
    0, 98, 181, 237, 256, 237, 181, 98,
    0, -98, -181, -237, -256, -237, -181, -98
};

// Sopwith table used to bias speed against climb/dive.
static const int8_t s_gravity_bias[16] = {
    0, -1, -2, -3, -4, -3, -2, -1,
    0, 1, 2, 3, 4, 3, 2, 1
};

void game_init(game_state_t *state)
{
    state->tick_count_10hz = 0;
    state->prev_world_x = RPS_SCREEN_WIDTH_PX / 2u;
    state->world_x = RPS_SCREEN_WIDTH_PX / 2u;
    state->plane_x = (int16_t)(RPS_SCREEN_WIDTH_PX / 2u);
    state->plane_y = (int16_t)(RPS_SCREEN_HEIGHT_PX - 40u);
    state->plane_bank = 0;
    state->plane_pitch = 0;
    state->plane_vy = 0;
    state->throttle = 0;
    state->speed = 0;
    state->airborne = false;
    state->plane_orient = false;
    state->stalled_high = false;
    state->landing = false;
    state->prev_flip_held = false;
    state->prev_land_held = false;
    state->stall_tick = 0;
    state->terrain_edit_cooldown = 0;
    state->crashed = false;

#if SOPWITH_FEATURE_TERRAIN
    terrain_init();
    state->plane_y = (int16_t)terrain_height_at_world_x(state->world_x) - (int16_t)RPS_MODE5_SPRITE_SIZE_PX;
#endif
}

void game_tick_10hz(game_state_t *state, const input_actions_t *actions)
{
    int16_t terrain_y;
    int8_t bank_target;
    int8_t flaps = 0;
    int16_t nspeed;
    uint8_t nangle;
    int16_t speed_limit;
    int16_t dx;
    int16_t dy;
    int8_t update = 0;
    bool stalled;
    const uint8_t stall_count = 6u;
    const uint8_t stall_recover_angle = 12u; // 3 * ANGLES / 4
    const int16_t stall_enter_y = (int16_t)(-(int16_t)(RPS_MODE5_SPRITE_SIZE_PX / 2u));

    state->tick_count_10hz++;
    state->prev_world_x = state->world_x;

#if SOPWITH_FEATURE_TERRAIN
    terrain_y = (int16_t)terrain_height_at_world_x(state->world_x);
#else
    terrain_y = (int16_t)(RPS_SCREEN_HEIGHT_PX - 8u);
#endif
    bank_target = 0;

#if SOPWITH_FEATURE_FLIGHT
    if (!state->crashed) {
        if (state->airborne && !state->stalled_high && state->plane_y <= stall_enter_y) {
            enter_stall_state(state);
        }

        stalled = state->stalled_high;
        if (stalled && ((uint8_t)state->plane_pitch == stall_recover_angle) &&
            (state->speed >= RPS_MIN_SPEED)) {
            state->stalled_high = false;
            stalled = false;
        }

        // Mapping in this port follows the user's Sopwith profile:
        // left=accelerate, right=decelerate, down=pull-up, up=pull-down.
        if (actions->left && state->throttle < RPS_MAX_THROTTLE) {
            state->throttle++;
            state->landing = false;
        }
        if (actions->right && state->throttle > 0) {
            state->throttle--;
            state->landing = false;
        }

        if (actions->down) {
            flaps = 1;
            state->landing = false;
        }
        if (actions->up) {
            flaps = -1;
            state->landing = false;
        }

        // Edge-detect flip at 10 Hz boundary (input polls at 60 Hz;
        // doing this here ensures we never miss or double-fire a press).
        if (actions->flip && !state->prev_flip_held && state->airborne) {
            state->plane_orient = !state->plane_orient;
            state->landing = false;
        }
        state->prev_flip_held = actions->flip;

        // Sopwith HOME: rising-edge press sets landing flag; any other
        // control clears it (mirroring ob_home semantics).
        if (actions->land && !state->prev_land_held && state->airborne) {
            state->landing = true;
        }
        state->prev_land_held = actions->land;

        if (state->plane_orient) {
            nangle = wrap_angle16((int16_t)state->plane_pitch - flaps);
        } else {
            nangle = wrap_angle16((int16_t)state->plane_pitch + flaps);
        }
        if (flaps != 0) {
            update = 1;
        }

        // Sopwith HOME-style guided descent: reduce throttle at cadence
        // and steer toward a shallow nose-down angle. No explicit flare —
        // the gentle-contact condition (plane_vy<=2, speed<=MIN+2) handles
        // touchdown exactly as Sopwith's aim() does.
        if (state->landing && state->airborne) {
            if ((state->tick_count_10hz & 0x3u) == 0u && state->throttle > 0) {
                state->throttle--;
            }
            // Nose slightly down: angle 15 right-flying, 9 left-flying.
            if (!state->plane_orient) {
                nangle = angle_step_toward(nangle, 15u);
            } else {
                nangle = angle_step_toward(nangle, 9u);
            }
            update = 1;
        }
        nspeed = state->speed;

        // Match Sopwith cadence: converge speed every 4 ticks.
        if ((state->tick_count_10hz & 0x3u) == 0u) {
            if (!state->airborne) {
                if (state->throttle == 0 && flaps == 0) {
                    nspeed = 0;
                } else {
                    nspeed = RPS_MIN_SPEED;
                }
                update = 1;
            } else {
                if (!stalled && nspeed < RPS_MIN_SPEED) {
                    nspeed--;
                    update = 1;
                } else {
                    speed_limit = RPS_MIN_SPEED + state->throttle + s_gravity_bias[nangle];
                    if (nspeed < speed_limit) {
                        nspeed++;
                        update = 1;
                    } else if (nspeed > speed_limit) {
                        nspeed--;
                        update = 1;
                    }
                }
            }
        }

        if (update) {
            if (state->airborne && !stalled && nspeed <= 0) {
                enter_stall_state(state);
                stalled = true;
                nspeed = state->speed;
                nangle = (uint8_t)state->plane_pitch;
            } else {
                state->plane_pitch = (int8_t)nangle;
                state->speed = (uint8_t)clamp_i16(nspeed, 0, (RPS_MAX_SPEED + RPS_MAX_THROTTLE));
            }
        }

        dx = (int16_t)((state->speed * s_sintab[((uint8_t)state->plane_pitch + 4u) & 15u]) / 256);
        dy = (int16_t)((state->speed * s_sintab[(uint8_t)state->plane_pitch & 15u]) / 256);

        if (!stalled) {
            state->world_x = (uint16_t)wrap_world_x_i16((int32_t)state->world_x + dx);
        }

        if (!state->airborne) {
            state->plane_y = (int16_t)(terrain_y - (int16_t)RPS_MODE5_SPRITE_SIZE_PX);
            state->plane_vy = 0;

            // Sopwith-like: once rolling at minimum speed, pulling up can lift off.
            if (flaps > 0 && state->speed >= RPS_MIN_SPEED) {
                state->airborne = true;
            }
        } else if (stalled) {
            // Sopwith stalled vertical drop: no forward motion, dy = -speed.
            state->plane_vy = (int8_t)state->speed;
            state->plane_y += state->plane_vy;

            if (state->stall_tick > 0) {
                state->stall_tick--;
            }
            if (state->stall_tick == 0) {
                state->plane_orient = !state->plane_orient;
                state->plane_pitch = (int8_t)((24 - (uint8_t)state->plane_pitch) & 0x0Fu);
                state->stall_tick = stall_count;
            }

            if ((int16_t)(state->plane_y + (int16_t)RPS_MODE5_SPRITE_SIZE_PX) > terrain_y) {
                if (state->plane_vy > 2 || state->speed > (RPS_MIN_SPEED + 2)) {
                    state->crashed = true;
                } else {
                    state->airborne = false;
                    state->stalled_high = false;
                    state->landing = false;
                    state->speed = 0;
                    state->throttle = 0;
                    state->plane_pitch = 0;
                    state->plane_y = (int16_t)(terrain_y - (int16_t)RPS_MODE5_SPRITE_SIZE_PX);
                    state->plane_vy = 0;
                }
            }
        } else {
            // Convert Sopwith dy (up-positive) to screen coordinates (down-positive).
            state->plane_vy = (int8_t)(-dy);
            state->plane_y += state->plane_vy;

            if ((int16_t)(state->plane_y + (int16_t)RPS_MODE5_SPRITE_SIZE_PX) > terrain_y) {
                // Hard impact still crashes; gentle contact lands.
                if (state->plane_vy > 2 || state->speed > (RPS_MIN_SPEED + 2)) {
                    state->crashed = true;
                    state->landing = false;
                } else {
                    state->airborne = false;
                    state->landing = false;
                    state->speed = 0;
                    state->throttle = 0;
                    state->plane_pitch = 0;
                    state->plane_y = (int16_t)(terrain_y - (int16_t)RPS_MODE5_SPRITE_SIZE_PX);
                    state->plane_vy = 0;
                }
            }
        }

        // Keep this as a smoothed secondary value; actual sprite heading now
        // follows plane_pitch directly in the renderer.
        if (nangle <= 8u) {
            bank_target = (int8_t)nangle;
        } else {
            bank_target = (int8_t)(16u - nangle);
        }
        bank_target = clamp_i8(bank_target, 0, RPS_PLAYER_BANK_MAX);
    }
#else
    (void)actions;
#endif

    state->plane_x = clamp_i16(state->plane_x, (int16_t)(RPS_SCREEN_WIDTH_PX / 2u - 8), (int16_t)(RPS_SCREEN_WIDTH_PX / 2u + 8));
    // Allow climbing well above view so the player can recover naturally.
    state->plane_y = clamp_i16(state->plane_y, (int16_t)(-(int16_t)RPS_SCREEN_HEIGHT_PX), (int16_t)(RPS_SCREEN_HEIGHT_PX - 1u));

    // Bank animation follows flight angle magnitude, right-facing only.
    bank_target = clamp_i8(bank_target, 0, RPS_PLAYER_BANK_MAX);
    if (state->plane_bank < bank_target) {
        state->plane_bank++;
    } else if (state->plane_bank > bank_target) {
        state->plane_bank--;
    }

    if (state->terrain_edit_cooldown > 0) {
        state->terrain_edit_cooldown--;
    }

#if SOPWITH_FEATURE_TERRAIN
    if (actions->bomb && state->terrain_edit_cooldown == 0) {
        terrain_crater(state->world_x, 8, 6);
        state->terrain_edit_cooldown = 2;
    }
#endif

    if (actions->start && state->crashed) {
        state->airborne = false;
        state->plane_pitch = 0;
        state->plane_vy = 0;
        state->throttle = 0;
        state->speed = 0;
        state->plane_orient = false;
        state->stalled_high = false;
        state->landing = false;
        state->stall_tick = 0;
        state->crashed = false;
    }
}

void game_render_interpolated(const game_state_t *state, uint8_t subframe)
{
    platform_video_render(state, subframe);
}