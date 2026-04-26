#include <rp6502.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "constants.h"
#include "enemy_planes.h"
#include "flight.h"
#include "ground_targets.h"
#include "projectiles.h"
#include "sprite_mode5.h"
#include "text_mode1.h"
#include "tile_mode2.h"

enum {
    WORLD_WIDTH_PX = (GROUND_WIDTH * 8),
    PLAYER_RUNWAY_SPAN_PX = 20,
    PLAYER_HOME_APPROACH_OFFSET_PX = 16,
    PLAYER_HOME_SNAP_RANGE_PX = 16,
    PLAYER_HOME_LANDING_RANGE_X_PX = 96,
    PLAYER_HOME_LANDING_RANGE_Y_PX = 48,
    FLIGHT_FPS_DIV = 6,
    FLIP_REPEAT_INITIAL_DELAY_TICKS = 3,
    FLIP_REPEAT_INTERVAL_TICKS = 1,
    MIN_SPEED = 5,
    MAX_SPEED = 10,
    MAX_THROTTLE = 4
};

typedef struct flight_state_s {
    uint16_t tick_count_10hz;
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
    bool prev_land_held;
    uint8_t flip_repeat_timer;
    uint8_t stall_tick;
    bool falling;
    bool crashed;
    int8_t fall_dx;
    int8_t fall_dy;
    int8_t fall_countdown;
} flight_state_t;

static flight_state_t s_flight;
static uint8_t s_tick_div = 0;
static uint16_t s_render_world_x;
static int16_t s_render_plane_y;
static bool s_crash_explosion_pending = false;
static uint16_t s_crash_explosion_world_x;
static int16_t s_crash_explosion_center_y;
static bool s_crash_explosion_apply_crater;
static bool s_crash_explosion_big;

static const int16_t s_sintab[16] = {
    0, 98, 181, 237, 256, 237, 181, 98,
    0, -98, -181, -237, -256, -237, -181, -98
};

static const int8_t s_gravity_bias[16] = {
    0, -1, -2, -3, -4, -3, -2, -1,
    0, 1, 2, 3, 4, 3, 2, 1
};

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

static int16_t abs_i16(int16_t v)
{
    return (v < 0) ? (int16_t)(-v) : v;
}

static uint16_t wrap_world_x(int16_t x)
{
    int16_t world_width = (int16_t)WORLD_WIDTH_PX;

    while (x < 0) {
        x = (int16_t)(x + world_width);
    }
    while (x >= world_width) {
        x = (int16_t)(x - world_width);
    }

    return (uint16_t)x;
}

static uint16_t wrap_world_x_add(uint16_t world_x, int16_t delta)
{
    return wrap_world_x((int16_t)world_x + delta);
}

static int16_t wrapped_world_delta(uint16_t from_x, uint16_t to_x)
{
    int16_t delta = (int16_t)to_x - (int16_t)from_x;
    int16_t half_world = (int16_t)(WORLD_WIDTH_PX / 2);

    if (delta > half_world) {
        delta = (int16_t)(delta - (int16_t)WORLD_WIDTH_PX);
    } else if (delta < -half_world) {
        delta = (int16_t)(delta + (int16_t)WORLD_WIDTH_PX);
    }

    return delta;
}

static uint8_t wrap_angle16(int16_t angle)
{
    return (uint8_t)(angle & 0x0F);
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

static bool consume_flip_repeat(bool flip_held)
{
    if (!flip_held) {
        s_flight.flip_repeat_timer = 0u;
        return false;
    }

    if (s_flight.flip_repeat_timer == 0u) {
        s_flight.flip_repeat_timer = FLIP_REPEAT_INITIAL_DELAY_TICKS;
        return true;
    }

    --s_flight.flip_repeat_timer;
    if (s_flight.flip_repeat_timer == 0u) {
        s_flight.flip_repeat_timer = FLIP_REPEAT_INTERVAL_TICKS;
        return true;
    }

    return false;
}

static uint8_t pitch_from_velocity(int8_t vx, int8_t vy)
{
    int16_t best_score = -32767;
    uint8_t best_angle = 0u;

    for (uint8_t a = 0; a < 16u; ++a) {
        int16_t dir_x = s_sintab[(uint8_t)((a + 4u) & 0x0Fu)];
        int16_t dir_y = (int16_t)(-s_sintab[a]);
        int16_t score = (int16_t)((int16_t)vx * dir_x + (int16_t)vy * dir_y);

        if (score > best_score) {
            best_score = score;
            best_angle = a;
        }
    }

    return best_angle;
}

static uint8_t player_frame_index_for_state(void)
{
    uint8_t angle = (uint8_t)s_flight.plane_pitch & 0x0Fu;

    if (s_flight.plane_orient) {
        return (uint8_t)(16u + ((16u - angle) & 0x0Fu));
    }

    return angle;
}

static uint8_t sprite_pixel_4bpp(uint8_t frame_index, uint8_t x, uint8_t y)
{
    uint16_t frame_addr;
    uint16_t pixel_byte_addr;
    uint8_t packed;

    if (x >= PLAYER_SPRITE_SIZE_PX || y >= PLAYER_SPRITE_SIZE_PX) {
        return 0u;
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

static int16_t terrain_height_at_world_x(uint16_t world_x)
{
    return tile_mode2_ground_y_at_world_x(world_x);
}

static int16_t player_runway_ground_y(void)
{
    int16_t height = 0;

    for (uint16_t i = 0u; i <= PLAYER_RUNWAY_SPAN_PX; ++i) {
        uint16_t world_x = wrap_world_x_add(PLAYER_START_WORLD_X_PX, (int16_t)i);
        int16_t ground_y = terrain_height_at_world_x(world_x);

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

static int16_t player_home_plane_top_y(void)
{
    return plane_top_y_for_ground(player_runway_ground_y());
}

static int16_t player_home_approach_plane_top_y(void)
{
    return (int16_t)(player_home_plane_top_y() - PLAYER_HOME_APPROACH_OFFSET_PX);
}

static bool plane_is_in_home_snap_range(void)
{
    int16_t dx = wrapped_world_delta(s_flight.world_x, PLAYER_START_WORLD_X_PX);
    int16_t dy = (int16_t)(player_home_approach_plane_top_y() - s_flight.plane_y);

    return abs_i16(dx) < PLAYER_HOME_SNAP_RANGE_PX &&
           abs_i16(dy) < PLAYER_HOME_SNAP_RANGE_PX;
}

static bool plane_is_in_home_landing_range(void)
{
    int16_t dx = wrapped_world_delta(s_flight.world_x, PLAYER_START_WORLD_X_PX);
    int16_t dy = (int16_t)(player_home_approach_plane_top_y() - s_flight.plane_y);

    return abs_i16(dx) < PLAYER_HOME_LANDING_RANGE_X_PX &&
           abs_i16(dy) < PLAYER_HOME_LANDING_RANGE_Y_PX;
}

static bool plane_is_on_home_runway(void)
{
    int16_t dx = wrapped_world_delta(PLAYER_START_WORLD_X_PX, s_flight.world_x);
    int16_t home_ground_y = player_runway_ground_y();
    int16_t grounded_top_y = plane_top_y_for_ground(home_ground_y);

    return dx >= -8 &&
           dx <= (PLAYER_RUNWAY_SPAN_PX + 8) &&
           abs_i16((int16_t)(s_flight.plane_y - grounded_top_y)) <= 1;
}

static void update_autohome(uint8_t *nangle, int16_t *nspeed)
{
    int16_t dx = wrapped_world_delta(s_flight.world_x, PLAYER_START_WORLD_X_PX);
    int16_t dy = (int16_t)(player_home_approach_plane_top_y() - s_flight.plane_y);
    int8_t desired_vx;
    int8_t desired_vy;

    if (dx < -8) {
        desired_vx = -MAX_SPEED;
    } else if (dx > 8) {
        desired_vx = MAX_SPEED;
    } else {
        desired_vx = (int8_t)dx;
    }

    if (dy < -8) {
        desired_vy = -MAX_SPEED;
    } else if (dy > 8) {
        desired_vy = MAX_SPEED;
    } else {
        desired_vy = (int8_t)dy;
    }

    if (desired_vx != 0) {
        s_flight.plane_orient = desired_vx < 0;
    }

    *nangle = angle_step_toward(*nangle, pitch_from_velocity(desired_vx, desired_vy));
    if (*nspeed < (MIN_SPEED + 2)) {
        *nspeed = (MIN_SPEED + 2);
    }
    if (s_flight.throttle < MAX_THROTTLE) {
        s_flight.throttle = MAX_THROTTLE;
    }
}

static bool plane_collides_with_terrain(int16_t plane_top_y)
{
    uint8_t frame_index = player_frame_index_for_state();

    for (uint8_t sx = 0; sx < PLAYER_SPRITE_SIZE_PX; ++sx) {
        uint16_t world_x = wrap_world_x_add(s_flight.world_x, (int16_t)sx - 8);
        int16_t ground_y = terrain_height_at_world_x(world_x);
        int16_t sy = (int16_t)(ground_y - plane_top_y);

        if (sy >= PLAYER_SPRITE_SIZE_PX) {
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

static void enter_stall_state(void)
{
    s_flight.plane_orient = false;
    s_flight.plane_pitch = 14;
    s_flight.speed = 0u;
    s_flight.plane_vy = 0;
    s_flight.stalled_high = true;
    s_flight.stall_tick = 6u;
}

static void start_falling_from_damage(void)
{
    int16_t dx = (int16_t)((s_flight.speed * s_sintab[((uint8_t)s_flight.plane_pitch + 4u) & 15u]) / 256);
    int16_t dy = (int16_t)((s_flight.speed * s_sintab[(uint8_t)s_flight.plane_pitch & 15u]) / 256);

    s_flight.falling = true;
    s_flight.crashed = false;
    s_flight.airborne = true;
    s_flight.stalled_high = false;
    s_flight.landing = false;
    s_flight.plane_vy = (int8_t)(-dy);
    s_flight.fall_dx = (int8_t)dx;
    s_flight.fall_dy = (int8_t)(-dy);
    s_flight.fall_countdown = 10;
}

static void mark_plane_crash(uint16_t crash_world_x, int16_t crash_center_y,
                             bool apply_crater, bool big_explosion)
{
    s_flight.crashed = true;
    s_flight.falling = false;
    s_flight.airborne = false;
    s_flight.landing = false;
    s_flight.speed = 0u;
    s_flight.throttle = 0u;
    s_flight.plane_vy = 0;

    s_crash_explosion_pending = true;
    s_crash_explosion_world_x = crash_world_x;
    s_crash_explosion_center_y = crash_center_y;
    s_crash_explosion_apply_crater = apply_crater;
    s_crash_explosion_big = big_explosion;
}

static void queue_target_hit_explosion(uint16_t hit_world_x,
                                       int16_t hit_center_y,
                                       int16_t score_delta,
                                       ground_target_hit_type_t hit_type)
{
    text_mode1_add_score(score_delta);
    s_crash_explosion_pending = true;
    s_crash_explosion_world_x = hit_world_x;
    s_crash_explosion_center_y = hit_center_y;
    s_crash_explosion_apply_crater = false;
    s_crash_explosion_big = (hit_type == GROUND_TARGET_HIT_EXPLOSIVE);
}

static void reset_plane_to_home(void)
{
    memset(&s_flight, 0, sizeof(s_flight));
    s_flight.prev_world_x = PLAYER_START_WORLD_X_PX;
    s_flight.world_x = PLAYER_START_WORLD_X_PX;
    s_flight.plane_x = (int16_t)(SCREEN_WIDTH / 2u);
    s_flight.prev_plane_y = player_home_plane_top_y();
    s_flight.plane_y = player_home_plane_top_y();
    s_flight.fall_countdown = 10;
    s_flight.falling = false;
    s_flight.crashed = false;
}

static void apply_visuals(void)
{
    uint8_t frame_index = player_frame_index_for_state();
    int16_t sprite_x = (int16_t)(s_flight.plane_x - (int16_t)(PLAYER_SPRITE_SIZE_PX / 2u));

    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, xram_sprite_ptr,
                     (uint16_t)(PLAYER_DATA + ((unsigned)frame_index * PLAYER_FRAME_SIZE)));
    sprite_mode5_set_position(sprite_x, s_render_plane_y);
    tile_mode2_set_scroll_x((int16_t)s_render_world_x);
}

static void update_render_interpolation(void)
{
    int16_t world_delta = wrapped_world_delta(s_flight.prev_world_x, s_flight.world_x);
    int16_t plane_delta_y = (int16_t)(s_flight.plane_y - s_flight.prev_plane_y);
    int16_t interp_world;
    int16_t interp_plane;
    int16_t half = (int16_t)(FLIGHT_FPS_DIV / 2);

    if (world_delta >= 0) {
        interp_world = (int16_t)(((world_delta * (int16_t)s_tick_div) + half) / FLIGHT_FPS_DIV);
    } else {
        interp_world = (int16_t)(((world_delta * (int16_t)s_tick_div) - half) / FLIGHT_FPS_DIV);
    }

    if (plane_delta_y >= 0) {
        interp_plane = (int16_t)(((plane_delta_y * (int16_t)s_tick_div) + half) / FLIGHT_FPS_DIV);
    } else {
        interp_plane = (int16_t)(((plane_delta_y * (int16_t)s_tick_div) - half) / FLIGHT_FPS_DIV);
    }

    s_render_world_x = wrap_world_x((int16_t)(s_flight.prev_world_x + interp_world));
    s_render_plane_y = (int16_t)(s_flight.prev_plane_y + interp_plane);
}

static void flight_tick_10hz(const input_actions_t *actions)
{
    int16_t terrain_y;
    int16_t grounded_y;
    int8_t bank_target = 0;
    int8_t flaps = 0;
    int16_t nspeed;
    uint8_t nangle;
    int16_t speed_limit;
    int16_t dx;
    int16_t dy;
    bool update = false;

    s_flight.tick_count_10hz++;
    s_flight.prev_world_x = s_flight.world_x;
    s_flight.prev_plane_y = s_flight.plane_y;

    if (s_flight.crashed) {
        if (actions->start) {
            reset_plane_to_home();
        }
        goto finalize_tick;
    }

    if (s_flight.falling) {
        uint16_t hit_world_x = 0u;
        int16_t hit_center_y = 0;
        int16_t score_delta = 0;
        ground_target_hit_type_t hit_type;
        int8_t fall_flaps = 0;

        if (consume_flip_repeat(actions->flip != 0u)) {
            s_flight.plane_orient = !s_flight.plane_orient;
        }

        if (actions->down) {
            fall_flaps = 1;
        }
        if (actions->up) {
            fall_flaps = -1;
        }

        s_flight.world_x = wrap_world_x((int16_t)s_flight.world_x + s_flight.fall_dx);
        s_flight.plane_y = (int16_t)(s_flight.plane_y + s_flight.fall_dy);
        s_flight.plane_vy = s_flight.fall_dy;
        s_flight.plane_pitch = (int8_t)pitch_from_velocity(s_flight.fall_dx, s_flight.fall_dy);

        if ((s_flight.fall_dy > 0) && (s_flight.fall_dx != 0)) {
            if (s_flight.plane_orient ^ (s_flight.fall_dx < 0)) {
                s_flight.fall_countdown -= fall_flaps;
            } else {
                s_flight.fall_countdown += fall_flaps;
            }
        }

        s_flight.fall_countdown -= 2;
        if (s_flight.fall_countdown <= 0) {
            if (s_flight.fall_dy > 0) {
                if (s_flight.fall_dx < 0) {
                    ++s_flight.fall_dx;
                } else if (s_flight.fall_dx > 0) {
                    --s_flight.fall_dx;
                } else {
                    s_flight.plane_orient = !s_flight.plane_orient;
                }
            }
            if (s_flight.fall_dy < 10) {
                ++s_flight.fall_dy;
            }
            s_flight.fall_countdown = 10;
        }

        hit_type = ground_targets_check_plane_collision(s_flight.world_x, s_flight.plane_y,
                                                        &hit_world_x, &hit_center_y,
                                                        &score_delta);
        if (hit_type != GROUND_TARGET_HIT_NONE) {
            queue_target_hit_explosion(hit_world_x, hit_center_y, score_delta, hit_type);
        }

        if (plane_collides_with_terrain(s_flight.plane_y)) {
            mark_plane_crash(s_flight.world_x,
                             (int16_t)(s_flight.plane_y + (PLAYER_SPRITE_SIZE_PX / 2)),
                             true,
                             false);
        }

        goto finalize_tick;
    }

    if (s_flight.airborne && !s_flight.stalled_high && s_flight.plane_y <= 0) {
        enter_stall_state();
    }

    if (actions->left && s_flight.throttle < MAX_THROTTLE) {
        ++s_flight.throttle;
        s_flight.landing = false;
        update = true;
    }
    if (actions->right && s_flight.throttle > 0u) {
        --s_flight.throttle;
        s_flight.landing = false;
        update = true;
    }

    if (actions->down) {
        flaps = 1;
        s_flight.landing = false;
    }
    if (actions->up) {
        flaps = -1;
        s_flight.landing = false;
    }

    if (consume_flip_repeat(actions->flip != 0u)) {
        s_flight.plane_orient = !s_flight.plane_orient;
        s_flight.landing = false;
    }

    if (actions->land && !s_flight.prev_land_held && s_flight.airborne &&
        plane_is_in_home_landing_range()) {
        s_flight.landing = true;
    }
    s_flight.prev_land_held = actions->land != 0u;

    if (s_flight.plane_orient) {
        nangle = wrap_angle16((int16_t)s_flight.plane_pitch - flaps);
    } else {
        nangle = wrap_angle16((int16_t)s_flight.plane_pitch + flaps);
    }
    if (flaps != 0) {
        update = true;
    }

    nspeed = s_flight.speed;

    if (s_flight.landing && s_flight.airborne) {
        update_autohome(&nangle, &nspeed);
        update = true;
    }

    if ((s_flight.tick_count_10hz & 0x03u) == 0u) {
        if (s_flight.airborne) {
            if (!s_flight.stalled_high && nspeed < MIN_SPEED) {
                --nspeed;
                update = true;
            } else {
                speed_limit = (int16_t)(MIN_SPEED + s_flight.throttle + s_gravity_bias[nangle]);
                if (nspeed < speed_limit) {
                    ++nspeed;
                    update = true;
                } else if (nspeed > speed_limit) {
                    --nspeed;
                    update = true;
                }
            }
        }
    }

    if (update) {
        if (!s_flight.airborne) {
            nspeed = (s_flight.throttle == 0u && flaps == 0) ? 0 : MIN_SPEED;
        } else if (!s_flight.stalled_high && nspeed <= 0) {
            enter_stall_state();
            nspeed = 0;
        }

        s_flight.plane_pitch = (int8_t)nangle;
        s_flight.speed = (uint8_t)clamp_i16(nspeed, 0, (MAX_SPEED + MAX_THROTTLE));
    }

    dx = (int16_t)((s_flight.speed * s_sintab[((uint8_t)s_flight.plane_pitch + 4u) & 15u]) / 256);
    dy = (int16_t)((s_flight.speed * s_sintab[(uint8_t)s_flight.plane_pitch & 15u]) / 256);

    if (!s_flight.stalled_high) {
        s_flight.world_x = wrap_world_x_add(s_flight.world_x, dx);
    }

    terrain_y = terrain_height_at_world_x(s_flight.world_x);

    if (!s_flight.airborne) {
        grounded_y = plane_top_y_for_ground(terrain_y);
        s_flight.plane_y = grounded_y;
        s_flight.plane_vy = 0;

        if (s_flight.speed > 0u) {
            s_flight.airborne = true;
        }
    } else if (s_flight.stalled_high) {
        s_flight.plane_vy = (int8_t)s_flight.speed;
        s_flight.plane_y = (int16_t)(s_flight.plane_y + s_flight.plane_vy);

        if (s_flight.stall_tick > 0u) {
            --s_flight.stall_tick;
        }
        if (s_flight.stall_tick == 0u) {
            s_flight.plane_orient = !s_flight.plane_orient;
            s_flight.plane_pitch = (int8_t)((24 - (uint8_t)s_flight.plane_pitch) & 0x0Fu);
            s_flight.stall_tick = 6u;
        }

        if (plane_collides_with_terrain(s_flight.plane_y)) {
            mark_plane_crash(s_flight.world_x,
                             (int16_t)(s_flight.plane_y + (PLAYER_SPRITE_SIZE_PX / 2)),
                             true,
                             false);
        }
    } else {
        uint16_t hit_world_x = 0u;
        int16_t hit_center_y = 0;
        int16_t score_delta = 0;
        ground_target_hit_type_t hit_type;
        bool hit_enemy;

        s_flight.plane_vy = (int8_t)(-dy);
        s_flight.plane_y = (int16_t)(s_flight.plane_y + s_flight.plane_vy);

        if (s_flight.landing && plane_is_in_home_snap_range()) {
            reset_plane_to_home();
            goto finalize_tick;
        }

        if (plane_collides_with_terrain(s_flight.plane_y)) {
            mark_plane_crash(s_flight.world_x,
                             (int16_t)(s_flight.plane_y + (PLAYER_SPRITE_SIZE_PX / 2)),
                             true,
                             false);
        }

        if (!s_flight.crashed) {
            hit_type = ground_targets_check_plane_collision(s_flight.world_x, s_flight.plane_y,
                                                            &hit_world_x, &hit_center_y,
                                                            &score_delta);
            if (hit_type != GROUND_TARGET_HIT_NONE) {
                queue_target_hit_explosion(hit_world_x, hit_center_y, score_delta, hit_type);
                start_falling_from_damage();
            }

            if (!s_flight.falling && !s_flight.crashed) {
                hit_enemy = enemy_planes_check_player_collision(s_flight.world_x,
                                                                s_flight.plane_y,
                                                                0,
                                                                0,
                                                                0);
                if (hit_enemy) {
                    start_falling_from_damage();
                }
            }
        }
    }

    if ((uint8_t)nangle <= 8u) {
        bank_target = (int8_t)nangle;
    } else {
        bank_target = (int8_t)(16u - nangle);
    }

finalize_tick:
    s_flight.plane_x = clamp_i16(s_flight.plane_x,
                                 (int16_t)(SCREEN_WIDTH / 2u - 8),
                                 (int16_t)(SCREEN_WIDTH / 2u + 8));
    s_flight.plane_y = clamp_i16(s_flight.plane_y,
                                 (int16_t)(-SCREEN_HEIGHT),
                                 (int16_t)(SCREEN_HEIGHT - 1u));

    bank_target = clamp_i8(bank_target, 0, 4);
    if (s_flight.plane_bank < bank_target) {
        ++s_flight.plane_bank;
    } else if (s_flight.plane_bank > bank_target) {
        --s_flight.plane_bank;
    }
}

void flight_init(void)
{
    s_tick_div = 0u;
    reset_plane_to_home();
    s_render_world_x = s_flight.world_x;
    s_render_plane_y = s_flight.plane_y;
    apply_visuals();
}

void flight_update(const input_actions_t *actions)
{
    if (++s_tick_div >= FLIGHT_FPS_DIV) {
        s_tick_div = 0u;
        flight_tick_10hz(actions);
    }

    update_render_interpolation();
    apply_visuals();
}

uint16_t flight_world_x(void)
{
    return s_render_world_x;
}

uint16_t flight_world_x_physics(void)
{
    return s_flight.world_x;
}

int16_t flight_plane_y(void)
{
    return s_render_plane_y;
}

int16_t flight_plane_y_physics(void)
{
    return s_flight.plane_y;
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

bool flight_is_airborne(void)
{
    return s_flight.airborne;
}

bool flight_is_at_home_base(void)
{
    if (s_flight.crashed || s_flight.falling || s_flight.airborne) {
        return false;
    }

    return plane_is_on_home_runway();
}

bool flight_can_start_landing(void)
{
    return s_flight.airborne && !s_flight.crashed && !s_flight.falling &&
           plane_is_in_home_landing_range();
}

bool flight_is_crashed(void)
{
    return s_flight.crashed;
}

bool flight_is_wounded(void)
{
    return false;
}

bool flight_is_falling(void)
{
    return s_flight.falling;
}

void flight_apply_debris_hit(void)
{
    if (s_flight.crashed || s_flight.falling) {
        return;
    }

    start_falling_from_damage();
}

void flight_apply_bomb_hit(uint16_t impact_world_x, int16_t impact_center_y)
{
    (void)impact_world_x;
    (void)impact_center_y;

    if (s_flight.crashed || s_flight.falling || !s_flight.airborne) {
        return;
    }

    start_falling_from_damage();
}

bool flight_consume_plane_explosion(uint16_t *world_x, int16_t *center_y,
                                    bool *apply_crater,
                                    bool *big_explosion)
{
    if (!s_crash_explosion_pending) {
        return false;
    }

    s_crash_explosion_pending = false;
    if (world_x != 0) {
        *world_x = s_crash_explosion_world_x;
    }
    if (center_y != 0) {
        *center_y = s_crash_explosion_center_y;
    }
    if (apply_crater != 0) {
        *apply_crater = s_crash_explosion_apply_crater;
    }
    if (big_explosion != 0) {
        *big_explosion = s_crash_explosion_big;
    }
    return true;
}

int16_t flight_terrain_y_at(uint16_t world_x)
{
    return terrain_height_at_world_x(world_x);
}

void flight_apply_bomb_crater(uint16_t impact_world_x)
{
    (void)impact_world_x;
}