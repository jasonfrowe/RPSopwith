#include <rp6502.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "constants.h"
#include "flight.h"
#include "input.h"
#include "original_ground_3000.h"
#include "sprite_mode5.h"
#include "tile_mode2.h"

typedef struct flight_state_s {
    uint32_t tick_count_10hz;
    uint16_t prev_world_x;
    uint16_t world_x;
    int16_t plane_x;
    int16_t prev_plane_y;
    int16_t plane_y;
    int8_t plane_bank;
    int8_t plane_pitch;
    int8_t plane_vy;
    uint8_t throttle;
    uint8_t speed;
    bool airborne;
    bool plane_orient;
    bool stalled_high;
    bool landing;
    bool prev_flip_held;
    bool prev_land_held;
    uint8_t stall_tick;
    bool crashed;
} flight_state_t;

enum {
    WORLD_WIDTH_PX = (GROUND_WIDTH * 8),
    PLAYER_RUNWAY_SPAN_PX = 20,
    FLIGHT_FPS_DIV = 6,
    MIN_SPEED = 4,
    MAX_SPEED = 8,
    MAX_THROTTLE = 4
};

static uint8_t s_terrain_height[WORLD_WIDTH_PX];
static bool s_terrain_ready = false;
static flight_state_t s_flight;
static uint8_t s_tick_div = 0;
static uint16_t s_render_world_x;
static int16_t s_render_plane_y;

static const int16_t s_sintab[16] = {
    0, 98, 181, 237, 256, 237, 181, 98,
    0, -98, -181, -237, -256, -237, -181, -98
};

static const int8_t s_gravity_bias[16] = {
    0, -1, -2, -3, -4, -3, -2, -1,
    0, 1, 2, 3, 4, 3, 2, 1
};

static int16_t wrap_world_x(int32_t x)
{
    int32_t world_width_px = WORLD_WIDTH_PX;
    int32_t wrapped = x % world_width_px;
    if (wrapped < 0) {
        wrapped += world_width_px;
    }
    return (int16_t)wrapped;
}

static int16_t clamp_i16(int16_t v, int16_t lo, int16_t hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static int16_t wrapped_world_delta(uint16_t from_x, uint16_t to_x)
{
    int16_t delta = (int16_t)to_x - (int16_t)from_x;
    int16_t half_world = (int16_t)(WORLD_WIDTH_PX / 2);

    if (delta > half_world) {
        delta -= (int16_t)WORLD_WIDTH_PX;
    } else if (delta < -half_world) {
        delta += (int16_t)WORLD_WIDTH_PX;
    }

    return delta;
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

static uint8_t player_frame_index_for_state(const flight_state_t *state)
{
    uint8_t angle = (uint8_t)state->plane_pitch & 0x0Fu;

    if (state->plane_orient) {
        uint8_t a = (uint8_t)((16u - angle) & 0x0Fu);
        return (uint8_t)(16u + a);
    }

    return angle;
}

static uint8_t sprite_pixel_4bpp(uint8_t frame_index, uint8_t x, uint8_t y)
{
    uint16_t frame_addr;
    uint16_t pixel_byte_addr;
    uint8_t packed;

    if (x >= PLAYER_SPRITE_SIZE_PX || y >= PLAYER_SPRITE_SIZE_PX) {
        return 0;
    }

    frame_addr = (uint16_t)(PLAYER_DATA + ((unsigned)frame_index * PLAYER_FRAME_SIZE));
    pixel_byte_addr = (uint16_t)(frame_addr + ((unsigned)y * (PLAYER_SPRITE_SIZE_PX / 2u)) + (x >> 1));

    RIA.addr0 = pixel_byte_addr;
    RIA.step0 = 1;
    packed = RIA.rw0;

    if ((x & 1u) == 0u) {
        return (uint8_t)((packed >> 4) & 0x0Fu);
    }
    return (uint8_t)(packed & 0x0Fu);
}

static void terrain_init(void)
{
    for (uint16_t x = 0; x < WORLD_WIDTH_PX; ++x) {
        uint16_t y = (uint16_t)(199u - s_original_ground[x]);
        s_terrain_height[x] = (y > 239u) ? 239u : (uint8_t)y;
    }
    s_terrain_ready = true;
}

static uint8_t terrain_height_at_world_x(uint16_t world_x)
{
    if (!s_terrain_ready) {
        terrain_init();
    }
    return s_terrain_height[world_x % WORLD_WIDTH_PX];
}

static int16_t player_runway_ground_y(void)
{
    int16_t height = 0;

    for (uint16_t x = PLAYER_START_WORLD_X_PX;
         x <= (uint16_t)(PLAYER_START_WORLD_X_PX + PLAYER_RUNWAY_SPAN_PX);
         ++x) {
        int16_t ground_y = (int16_t)terrain_height_at_world_x((uint16_t)(x % WORLD_WIDTH_PX));
        if (ground_y > height) {
            height = ground_y;
        }
    }

    return height;
}

static int16_t plane_top_y_for_ground(int16_t ground_y)
{
    return (int16_t)(ground_y - (int16_t)PLAYER_GROUND_CONTACT_FROM_TOP_PX);
}

static bool can_touchdown_safely(const flight_state_t *state)
{
    uint8_t pitch = (uint8_t)state->plane_pitch & 0x0Fu;
    bool near_level = (pitch <= 1u) || (pitch >= 15u);

    // Sopwith effectively requires a gentle, upright touchdown.
    return (!state->plane_orient) && near_level;
}

static bool plane_collides_with_terrain(const flight_state_t *state, int16_t plane_top_y)
{
    int16_t center_world_x = (int16_t)state->world_x;
    uint8_t frame_index = player_frame_index_for_state(state);

    for (uint8_t sx = 0; sx < PLAYER_SPRITE_SIZE_PX; ++sx) {
        int16_t world_x = wrap_world_x((int32_t)center_world_x + (int16_t)sx - 8);
        int16_t ground_y = (int16_t)terrain_height_at_world_x((uint16_t)world_x);
        int16_t sy = (int16_t)(ground_y - plane_top_y);

        if (sy >= (int16_t)PLAYER_SPRITE_SIZE_PX) {
            continue;
        }

        if (sy < 0) {
            return true;
        }

        if (sprite_pixel_4bpp(frame_index, sx, (uint8_t)sy) != 0u) {
            return true;
        }
    }

    return false;
}

static void enter_stall_state(flight_state_t *state)
{
    state->plane_orient = false;
    state->plane_pitch = 14;
    state->speed = 0;
    state->plane_vy = 0;
    state->stalled_high = true;
    state->stall_tick = 6;
}

static void reset_plane_to_home(flight_state_t *state)
{
    state->prev_world_x = PLAYER_START_WORLD_X_PX;
    state->world_x = PLAYER_START_WORLD_X_PX;
    state->plane_x = (int16_t)(SCREEN_WIDTH / 2u);
    state->prev_plane_y = plane_top_y_for_ground(player_runway_ground_y());
    state->plane_y = plane_top_y_for_ground(player_runway_ground_y());
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
    state->crashed = false;
}

static void apply_visuals(const flight_state_t *state)
{
    uint8_t frame_index = player_frame_index_for_state(state);
    int16_t sprite_x = (int16_t)(state->plane_x - (int16_t)(PLAYER_SPRITE_SIZE_PX / 2u));

    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, xram_sprite_ptr,
                     (uint16_t)(PLAYER_DATA + ((unsigned)frame_index * PLAYER_FRAME_SIZE)));
    sprite_mode5_set_position(sprite_x, s_render_plane_y);
    tile_mode2_set_scroll_x((int16_t)s_render_world_x);
}

static void update_render_interpolation(const flight_state_t *state)
{
    int16_t world_delta = wrapped_world_delta(state->prev_world_x, state->world_x);
    int16_t plane_delta_y = (int16_t)(state->plane_y - state->prev_plane_y);
    int16_t world_pred = (int16_t)state->world_x + (int16_t)((world_delta * (int16_t)s_tick_div) / FLIGHT_FPS_DIV);
    int16_t plane_pred_y = state->plane_y + (int16_t)((plane_delta_y * (int16_t)s_tick_div) / FLIGHT_FPS_DIV);

    s_render_world_x = (uint16_t)wrap_world_x(world_pred);
    s_render_plane_y = plane_pred_y;
}

static void flight_tick_10hz(flight_state_t *state, const input_actions_t *actions)
{
    int16_t terrain_y = (int16_t)terrain_height_at_world_x(state->world_x);
    int16_t grounded_y;
    int8_t bank_target = 0;
    int8_t flaps = 0;
    int16_t nspeed;
    uint8_t nangle;
    int16_t speed_limit;
    int16_t dx;
    int16_t dy;
    int8_t update = 0;
    bool stalled;
    const uint8_t stall_count = 6u;
    const uint8_t stall_recover_angle = 12u;
    const int16_t stall_enter_y = 0;

    state->tick_count_10hz++;
    state->prev_world_x = state->world_x;
    state->prev_plane_y = state->plane_y;

    if (!state->crashed) {
        if (state->airborne && !state->stalled_high && state->plane_y <= stall_enter_y) {
            enter_stall_state(state);
            // Match Sopwith's immediate re-evaluation after entering stall.
            flight_tick_10hz(state, actions);
            return;
        }

        stalled = state->stalled_high;
        if (stalled && ((uint8_t)state->plane_pitch == stall_recover_angle) &&
            (state->speed >= MIN_SPEED)) {
            state->stalled_high = false;
            stalled = false;
        }

        if (actions->left && state->throttle < MAX_THROTTLE) {
            state->throttle++;
            state->landing = false;
            update = 1;
        }
        if (actions->right && state->throttle > 0) {
            state->throttle--;
            state->landing = false;
            update = 1;
        }

        if (actions->down) {
            flaps = 1;
            state->landing = false;
        }
        if (actions->up) {
            flaps = -1;
            state->landing = false;
        }

        if (actions->flip && !state->prev_flip_held && state->airborne) {
            state->plane_orient = !state->plane_orient;
            state->landing = false;
        }
        state->prev_flip_held = actions->flip;

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

        if (state->landing && state->airborne) {
            if ((state->tick_count_10hz & 0x3u) == 0u && state->throttle > 0) {
                state->throttle--;
            }
            if (!state->plane_orient) {
                nangle = angle_step_toward(nangle, 15u);
            } else {
                nangle = angle_step_toward(nangle, 9u);
            }
            update = 1;
        }
        nspeed = state->speed;

        if ((state->tick_count_10hz & 0x3u) == 0u) {
            if (state->airborne) {
                if (!stalled && nspeed < MIN_SPEED) {
                    nspeed--;
                    update = 1;
                } else {
                    speed_limit = MIN_SPEED + state->throttle + s_gravity_bias[nangle];
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
            if (!state->airborne) {
                if (state->throttle == 0 && flaps == 0) {
                    nspeed = 0;
                } else {
                    nspeed = MIN_SPEED;
                }
            } else if (!stalled && nspeed <= 0) {
                enter_stall_state(state);
                // Match Sopwith's immediate re-evaluation after entering stall.
                flight_tick_10hz(state, actions);
                return;
            }

            state->plane_pitch = (int8_t)nangle;
            state->speed = (uint8_t)clamp_i16(nspeed, 0, (MAX_SPEED + MAX_THROTTLE));
        }

        dx = (int16_t)((state->speed * s_sintab[((uint8_t)state->plane_pitch + 4u) & 15u]) / 256);
        dy = (int16_t)((state->speed * s_sintab[(uint8_t)state->plane_pitch & 15u]) / 256);

        if (!stalled) {
            state->world_x = (uint16_t)wrap_world_x((int32_t)state->world_x + dx);
        }

        if (!state->airborne) {
            grounded_y = plane_top_y_for_ground(terrain_y);

            if (state->speed > 0 && plane_collides_with_terrain(state, grounded_y)) {
                state->crashed = true;
            }

            state->plane_y = grounded_y;
            state->plane_vy = 0;

            if (state->speed > 0) {
                state->airborne = true;
            }
        } else if (stalled) {
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

            if (plane_collides_with_terrain(state, state->plane_y)) {
                // Match Sopwith behavior: stalled ground contact is a crash.
                state->crashed = true;
            }
        } else {
            state->plane_vy = (int8_t)(-dy);
            state->plane_y += state->plane_vy;

            if (plane_collides_with_terrain(state, state->plane_y)) {
                if (state->plane_vy > 2 || state->speed > (MIN_SPEED + 2) ||
                    !can_touchdown_safely(state)) {
                    state->crashed = true;
                    state->landing = false;
                } else {
                    state->airborne = false;
                    state->landing = false;
                    state->speed = 0;
                    state->throttle = 0;
                    state->plane_pitch = 0;
                    state->plane_y = plane_top_y_for_ground(terrain_y);
                    state->plane_vy = 0;
                }
            }
        }

        if (nangle <= 8u) {
            bank_target = (int8_t)nangle;
        } else {
            bank_target = (int8_t)(16u - nangle);
        }
        bank_target = clamp_i8(bank_target, 0, 4);
    }

    state->plane_x = clamp_i16(state->plane_x,
                               (int16_t)(SCREEN_WIDTH / 2u - 8),
                               (int16_t)(SCREEN_WIDTH / 2u + 8));
    state->plane_y = clamp_i16(state->plane_y,
                               (int16_t)(-(int16_t)SCREEN_HEIGHT),
                               (int16_t)(SCREEN_HEIGHT - 1u));

    bank_target = clamp_i8(bank_target, 0, 4);
    if (state->plane_bank < bank_target) {
        state->plane_bank++;
    } else if (state->plane_bank > bank_target) {
        state->plane_bank--;
    }

    if (actions->start && state->crashed) {
        reset_plane_to_home(state);
    }
}

void flight_init(void)
{
    terrain_init();
    s_tick_div = 0;
    memset(&s_flight, 0, sizeof(s_flight));
    reset_plane_to_home(&s_flight);
    s_render_world_x = s_flight.world_x;
    s_render_plane_y = s_flight.plane_y;
    apply_visuals(&s_flight);
}

void flight_update(const input_actions_t *actions)
{
    if (++s_tick_div >= FLIGHT_FPS_DIV) {
        s_tick_div = 0;
        flight_tick_10hz(&s_flight, actions);
    }

    update_render_interpolation(&s_flight);
    apply_visuals(&s_flight);
}

uint16_t flight_world_x(void)
{
    return s_render_world_x;
}

int16_t flight_plane_y(void)
{
    return s_render_plane_y;
}

uint8_t flight_plane_pitch(void)
{
    return (uint8_t)s_flight.plane_pitch & 0x0Fu;
}

uint8_t flight_plane_speed(void)
{
    return s_flight.speed;
}

bool flight_plane_orient(void)
{
    return s_flight.plane_orient;
}

bool flight_is_crashed(void)
{
    return s_flight.crashed;
}

int16_t flight_terrain_y_at(uint16_t world_x)
{
    return (int16_t)terrain_height_at_world_x(world_x);
}
