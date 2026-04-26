#include <stdbool.h>
#include <stdint.h>

#include "ambient_birds.h"
#include "ambient_flocks.h"
#include "constants.h"
#include "enemy_planes.h"
#include "flight.h"
#include "ground_targets.h"
#include "projectiles.h"
#include "sprite_mode5.h"

typedef struct enemy_plane_s {
    uint16_t home_x;
    uint16_t territory_l;
    uint16_t territory_r;
    uint16_t prev_world_x;
    uint16_t world_x;
    int16_t home_y;
    int16_t prev_plane_y;
    int16_t plane_y;
    int8_t vx;
    int8_t vy;
    int8_t flaps;
    uint8_t angle;
    uint8_t speed;
    uint8_t accel;
    uint8_t launch_delay_10hz;
    uint8_t respawn_delay_10hz;
    uint8_t fire_cooldown_10hz;
    int8_t fall_dx;
    int8_t fall_dy;
    int8_t fall_countdown;
    uint8_t smoke_cooldown_10hz;
    uint8_t crash_linger_10hz;
    bool airborne;
    bool orient;
    bool destroyed;
    bool falling;
    bool crashed;
} enemy_plane_t;

enum {
    WORLD_WIDTH_PX = (GROUND_WIDTH * 8),
    ENEMY_FPS_DIV = 6,
    MIN_SPEED = 5,
    MAX_SPEED = 10,
    MAX_THROTTLE = 4,
    CLOSE_RANGE_PX = 32,
    HOME_RANGE_PX = 16,
    ENEMY_MIN_CLEARANCE_PX = 28,
    ENEMY_TOP_LIMIT_PX = 28,
    ENEMY_LOITER_ALTITUDE_PX = 42,
    ENEMY_APPROACH_ALTITUDE_PX = 34,
    ENEMY_MIN_ALTITUDE_PX = 18,
    ENEMY_DIVE_COMMIT_RANGE_PX = 96,
    ENEMY_FIRE_SPEED_BONUS = 10,
    ENEMY_SHOT_COOLDOWN_TICKS = 3,
    ENEMY_RESPAWN_TICKS = 24,
    ENEMY_CRASH_LINGER_TICKS = 10,
    ENEMY_SCORE_DELTA = 100,
    TERRITORY_EDGE_MARGIN_PX = 32,
    FALL_COUNT_RESET = 10,
    FALL_MAX_DY = 10
};

static enemy_plane_t s_enemies[MAX_ENEMIES];
static uint8_t s_tick_div;
static uint16_t s_tick_count_10hz;

static const int16_t s_sintab[16] = {
    0, 98, 181, 237, 256, 237, 181, 98,
    0, -98, -181, -237, -256, -237, -181, -98
};

static const int8_t s_gravity_bias[16] = {
    0, -1, -2, -3, -4, -3, -2, -1,
    0, 1, 2, 3, 4, 3, 2, 1
};

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

static uint8_t angle_from_velocity(int8_t vx, int8_t vy)
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

static int16_t world_delta_to_screen_x(uint16_t obj_world_x, uint16_t camera_world_x)
{
    return wrapped_world_delta(camera_world_x, obj_world_x);
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

static uint8_t clamp_u8(uint8_t v, uint8_t lo, uint8_t hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static int16_t range_metric(int16_t x, int16_t y, int16_t ax, int16_t ay)
{
    int16_t dx = abs_i16((int16_t)(x - ax));
    int16_t dy = abs_i16((int16_t)(y - ay));

    dy = (int16_t)(dy + (dy >> 1));
    if (dx < 125 && dy < 125) {
        return (int16_t)(dx * dx + dy * dy);
    }

    if (dx < dy) {
        int16_t t = dx;
        dx = dy;
        dy = t;
    }

    return (int16_t)(-((7 * dx + (dy << 2)) >> 3));
}

static void velocity_from_angle(uint8_t angle, uint8_t speed, int8_t *vx, int8_t *vy)
{
    int16_t dx = (int16_t)((speed * s_sintab[(uint8_t)((angle + 4u) & 0x0Fu)]) / 256);
    int16_t dy = (int16_t)((speed * s_sintab[angle & 0x0Fu]) / 256);

    *vx = (int8_t)dx;
    *vy = (int8_t)(-dy);
}

static int16_t home_plane_top_y(uint16_t home_x)
{
    int16_t ground = 0;

    for (uint8_t i = 0u; i <= 20u; ++i) {
        int16_t y = flight_terrain_y_at(wrap_world_x((int16_t)home_x + i));
        if (y > ground) {
            ground = y;
        }
    }

    return (int16_t)(ground - PLAYER_GROUND_CONTACT_FROM_TOP_PX);
}

static bool in_territory(const enemy_plane_t *e, uint16_t world_x)
{
    if (e->territory_l <= e->territory_r) {
        return world_x >= e->territory_l && world_x <= e->territory_r;
    }
    return world_x >= e->territory_l || world_x <= e->territory_r;
}

static bool within_home_range(const enemy_plane_t *e)
{
    return abs_i16(wrapped_world_delta(e->world_x, e->home_x)) < HOME_RANGE_PX &&
           abs_i16((int16_t)(e->plane_y - e->home_y)) < HOME_RANGE_PX;
}

static uint8_t player_pitch_for_enemy(void)
{
    if (flight_plane_orient()) {
        return (uint8_t)((16u - flight_plane_pitch()) & 0x0Fu);
    }
    return flight_plane_pitch();
}

static bool shoot_will_hit_player(const enemy_plane_t *e)
{
    uint16_t bullet_world_x = e->world_x;
    int16_t bullet_center_y = (int16_t)(e->plane_y + (PLAYER_SPRITE_SIZE_PX / 2));
    uint8_t shot_speed = (uint8_t)(e->speed + ENEMY_FIRE_SPEED_BONUS);
    int8_t bvx;
    int8_t bvy;
    uint16_t player_world_x = flight_world_x_physics();
    int16_t player_center_y = (int16_t)(flight_plane_y_physics() + (PLAYER_SPRITE_SIZE_PX / 2));
    uint8_t player_speed = flight_plane_speed();
    uint8_t player_pitch = player_pitch_for_enemy();
    int8_t pvx;
    int8_t pvy;

    if (flight_is_crashed() || flight_is_falling()) {
        return false;
    }

    velocity_from_angle(e->angle, shot_speed, &bvx, &bvy);
    velocity_from_angle(player_pitch, player_speed, &pvx, &pvy);

    for (uint8_t i = 0; i < 10u; ++i) {
        int16_t dx;
        int16_t dy;

        bullet_world_x = wrap_world_x((int16_t)bullet_world_x + bvx);
        bullet_center_y = (int16_t)(bullet_center_y + bvy);
        player_world_x = wrap_world_x((int16_t)player_world_x + pvx);
        player_center_y = (int16_t)(player_center_y + pvy);

        dx = wrapped_world_delta(player_world_x, bullet_world_x);
        dy = (int16_t)(bullet_center_y - player_center_y);
        if (dx >= -8 && dx <= 8 && dy >= -8 && dy <= 8) {
            return true;
        }
    }

    return false;
}

static void pick_target(const enemy_plane_t *e, bool has_target,
                        int16_t *ax, int16_t *ay)
{
    uint16_t player_x = flight_world_x_physics();

    if (has_target) {
        int8_t pvx;
        int8_t pvy;
        int16_t target_y;
        int16_t dx_to_player;

        velocity_from_angle(player_pitch_for_enemy(), flight_plane_speed(), &pvx, &pvy);
        *ax = (int16_t)(player_x - ((CLOSE_RANGE_PX * pvx) / 8));
        target_y = (int16_t)(flight_plane_y_physics() - ((CLOSE_RANGE_PX * pvy) / 8));

        dx_to_player = abs_i16(wrapped_world_delta(e->world_x, player_x));
        if (dx_to_player > ENEMY_DIVE_COMMIT_RANGE_PX) {
            int16_t terrain_y = flight_terrain_y_at(e->world_x);
            int16_t approach_top = (int16_t)(terrain_y -
                                             PLAYER_GROUND_CONTACT_FROM_TOP_PX -
                                             ENEMY_APPROACH_ALTITUDE_PX);
            if (target_y > approach_top) {
                target_y = approach_top;
            }
        }

        *ay = target_y;
        return;
    }

    {
        int16_t home_sweep = ((s_tick_count_10hz & 0x1Fu) < 16u) ? 24 : -24;

        *ax = (int16_t)wrap_world_x((int16_t)e->home_x + home_sweep);
        *ay = (int16_t)(e->home_y - ENEMY_LOITER_ALTITUDE_PX);
    }
}

static void enemy_aim(enemy_plane_t *e, bool has_target)
{
    static const int8_t cflaps[3] = {0, -1, 1};
    int16_t ax;
    int16_t ay;
    int16_t crange[3];
    int16_t calt[3];
    bool unsafe_dive[3];
    int16_t rmin;
    bool found_choice;
    uint8_t best_i = 0u;

    pick_target(e, has_target, &ax, &ay);

    for (uint8_t i = 0; i < 3u; ++i) {
        uint8_t nangle = (uint8_t)((e->angle + (e->orient ? -cflaps[i] : cflaps[i]) + 16) & 0x0Fu);
        uint8_t nspeed = clamp_u8((uint8_t)(e->speed + 1u), MIN_SPEED, MAX_SPEED);
        int8_t cvx;
        int8_t cvy;
        uint16_t nx;
        int16_t ny;
        int16_t candidate_dx_to_player;

        velocity_from_angle(nangle, nspeed, &cvx, &cvy);
        nx = wrap_world_x((int16_t)e->world_x + cvx);
        ny = (int16_t)(e->plane_y + cvy);

        crange[i] = range_metric((int16_t)nx, ny, ax, ay);
        calt[i] = (int16_t)(flight_terrain_y_at(nx) - (ny + PLAYER_GROUND_CONTACT_FROM_TOP_PX));

        candidate_dx_to_player = abs_i16(wrapped_world_delta(nx, flight_world_x_physics()));
        unsafe_dive[i] = (calt[i] < ENEMY_MIN_ALTITUDE_PX) &&
                         (!has_target || candidate_dx_to_player > ENEMY_DIVE_COMMIT_RANGE_PX);
    }

    rmin = 32767;
    found_choice = false;
    for (uint8_t i = 0; i < 3u; ++i) {
        if (!unsafe_dive[i] && crange[i] >= 0 && crange[i] < rmin) {
            rmin = crange[i];
            best_i = i;
            found_choice = true;
        }
    }

    if (!found_choice) {
        rmin = -32767;
        for (uint8_t i = 0; i < 3u; ++i) {
            if (!unsafe_dive[i] && crange[i] < 0 && crange[i] > rmin) {
                rmin = crange[i];
                best_i = i;
                found_choice = true;
            }
        }
    }

    if (!found_choice) {
        int16_t best_alt = calt[0];
        best_i = 0u;

        for (uint8_t i = 1; i < 3u; ++i) {
            if (calt[i] > best_alt) {
                best_alt = calt[i];
                best_i = i;
            }
        }
    }

    if (e->speed < MIN_SPEED) {
        e->accel = MAX_THROTTLE;
    }

    if (e->accel < MAX_THROTTLE) {
        ++e->accel;
    }

    e->flaps = cflaps[best_i];
}

static void enemy_try_fire(enemy_plane_t *e)
{
    int16_t dx = wrapped_world_delta(e->world_x, flight_world_x_physics());

    if (e->fire_cooldown_10hz > 0u) {
        --e->fire_cooldown_10hz;
    }

    if (abs_i16(dx) > 180 || e->fire_cooldown_10hz != 0u) {
        return;
    }

    if (shoot_will_hit_player(e)) {
        if (projectiles_spawn_enemy_shot(e->world_x,
                                         (int16_t)(e->plane_y + (PLAYER_SPRITE_SIZE_PX / 2)),
                                         e->angle,
                                         e->speed)) {
            e->fire_cooldown_10hz = ENEMY_SHOT_COOLDOWN_TICKS;
        }
    }
}

static uint8_t enemy_frame_index(const enemy_plane_t *e)
{
    int8_t vx = e->vx;
    int8_t vy = e->vy;
    uint8_t angle;
    bool facing_left;

    if (e->falling) {
        vx = e->fall_dx;
        vy = e->fall_dy;
    }

    if (vx == 0 && vy == 0) {
        angle = e->angle & 0x0Fu;
        facing_left = e->orient;
    } else {
        angle = angle_from_velocity(vx, vy);
        facing_left = vx < 0;
    }

    if (facing_left) {
        return (uint8_t)(16u + ((16u - angle) & 0x0Fu));
    }
    return angle;
}

static void enemy_reset_home(enemy_plane_t *e)
{
    e->world_x = e->home_x;
    e->prev_world_x = e->home_x;
    e->plane_y = e->home_y;
    e->prev_plane_y = e->home_y;
    e->speed = 0u;
    e->accel = MAX_THROTTLE;
    e->flaps = 0;
    e->vx = 0;
    e->vy = 0;
    e->fall_dx = 0;
    e->fall_dy = 0;
    e->fall_countdown = FALL_COUNT_RESET;
    e->smoke_cooldown_10hz = 0u;
    e->crash_linger_10hz = 0u;
    e->airborne = false;
    e->launch_delay_10hz = 0u;
    e->falling = false;
    e->crashed = false;
}

static void enemy_start_falling(enemy_plane_t *e)
{
    int8_t impact_vx;
    int8_t impact_vy;

    if (e->destroyed || e->falling || e->crashed) {
        return;
    }

    velocity_from_angle(e->angle, e->speed, &impact_vx, &impact_vy);

    e->falling = true;
    e->crashed = false;
    e->airborne = true;
    e->fall_dx = (impact_vx != 0) ? impact_vx : e->vx;
    e->fall_dy = (impact_vy != 0) ? impact_vy : e->vy;
    if (e->fall_dx == 0) {
        e->fall_dx = e->orient ? -1 : 1;
    }
    e->fall_countdown = FALL_COUNT_RESET;
    e->smoke_cooldown_10hz = 0u;
}

static void enemy_tick_falling(enemy_plane_t *e)
{
    uint16_t hit_world_x = 0u;
    int16_t hit_center_y = 0;
    int16_t score_delta = 0;
    int16_t terrain_y;
    ground_target_hit_type_t hit_type;

    e->prev_world_x = e->world_x;
    e->prev_plane_y = e->plane_y;

    e->world_x = wrap_world_x((int16_t)e->world_x + e->fall_dx);
    e->plane_y = (int16_t)(e->plane_y + e->fall_dy);

    e->fall_countdown -= 2;
    if (e->fall_countdown <= 0) {
        if (e->fall_dy > 0) {
            if (e->fall_dx < 0) {
                ++e->fall_dx;
            } else if (e->fall_dx > 0) {
                --e->fall_dx;
            }
        }
        if (e->fall_dy < FALL_MAX_DY) {
            ++e->fall_dy;
        }
        e->fall_countdown = FALL_COUNT_RESET;
    }

    if (e->smoke_cooldown_10hz == 0u) {
        (void)projectiles_spawn_smoke_trail(e->world_x,
                                            (int16_t)(e->plane_y + (PLAYER_SPRITE_SIZE_PX / 2)),
                                            e->fall_dx,
                                            e->fall_dy);
        e->smoke_cooldown_10hz = 1u;
    } else {
        --e->smoke_cooldown_10hz;
    }

    hit_type = ground_targets_check_plane_collision(e->world_x, e->plane_y,
                                                    &hit_world_x, &hit_center_y,
                                                    &score_delta);

    if (hit_type != GROUND_TARGET_HIT_NONE) {
        projectiles_spawn_crash_explosion(hit_world_x, hit_center_y, false);
    }

    terrain_y = flight_terrain_y_at(e->world_x);

    if (e->plane_y >= (terrain_y - PLAYER_GROUND_CONTACT_FROM_TOP_PX) ||
        e->plane_y >= (SCREEN_HEIGHT + PLAYER_SPRITE_SIZE_PX)) {
        projectiles_spawn_crash_explosion(e->world_x,
                                          (int16_t)(e->plane_y + (PLAYER_SPRITE_SIZE_PX / 2)),
                                          false);
        e->falling = false;
        e->crashed = true;
        e->crash_linger_10hz = ENEMY_CRASH_LINGER_TICKS;
        e->vx = 0;
        e->vy = 0;
        e->speed = 0u;
        e->prev_world_x = e->world_x;
        e->prev_plane_y = e->plane_y;
    }
}

static void enemy_enforce_territory(enemy_plane_t *e)
{
    bool left_side = in_territory(e, wrap_world_x((int16_t)e->world_x - TERRITORY_EDGE_MARGIN_PX));
    bool right_side = in_territory(e, wrap_world_x((int16_t)e->world_x + TERRITORY_EDGE_MARGIN_PX));

    if (!left_side && e->orient) {
        e->orient = false;
        e->angle = 0u;
        e->flaps = 0;
    }
    if (!right_side && !e->orient) {
        e->orient = true;
        e->angle = 8u;
        e->flaps = 0;
    }
}

static void enemy_tick_10hz(enemy_plane_t *e)
{
    int16_t speed_limit;
    int16_t max_plane_top;
    bool has_target;
    bool player_alive;

    if (e->falling) {
        enemy_tick_falling(e);
        return;
    }

    if (e->crashed) {
        e->prev_world_x = e->world_x;
        e->prev_plane_y = e->plane_y;
        if (e->crash_linger_10hz > 0u) {
            --e->crash_linger_10hz;
            return;
        }
        e->crashed = false;
        e->destroyed = true;
        e->respawn_delay_10hz = ENEMY_RESPAWN_TICKS;
        return;
    }

    if (e->destroyed) {
        if (e->respawn_delay_10hz > 0u) {
            --e->respawn_delay_10hz;
            return;
        }
        e->destroyed = false;
        enemy_reset_home(e);
        return;
    }

    player_alive = !flight_is_crashed() && !flight_is_falling();
    has_target = player_alive && in_territory(e, flight_world_x_physics());

    e->prev_world_x = e->world_x;
    e->prev_plane_y = e->plane_y;

    if (!e->airborne) {
        // Stay at home while waiting to launch
        e->world_x = e->home_x;
        e->plane_y = e->home_y;
        e->vx = 0;
        e->vy = 0;

        if (flight_is_at_home_base()) {
            e->launch_delay_10hz = 0u;
        }

        if (e->launch_delay_10hz > 0u) {
            --e->launch_delay_10hz;
            return;
        }

        // Launch when delay expires - no "patrol mode" gate
        e->airborne = true;
        e->speed = MIN_SPEED;
        e->accel = MAX_THROTTLE;
        e->angle = e->orient ? 8u : 0u;
    }

    enemy_aim(e, has_target);
    e->angle = (uint8_t)((e->angle + (e->orient ? -e->flaps : e->flaps) + 16) & 0x0Fu);

    if ((s_tick_count_10hz & 0x03u) == 0u) {
        speed_limit = (int16_t)(MIN_SPEED + e->accel + s_gravity_bias[e->angle]);
        speed_limit = clamp_i16(speed_limit, MIN_SPEED, MAX_SPEED);

        if (e->speed < (uint8_t)speed_limit) {
            ++e->speed;
        } else if (e->speed > (uint8_t)speed_limit) {
            --e->speed;
        }
    }

    velocity_from_angle(e->angle, e->speed, &e->vx, &e->vy);
    if (e->flaps == 0 && e->vx != 0) {
        e->orient = e->vx < 0;
    }
    e->world_x = wrap_world_x((int16_t)e->world_x + e->vx);
    e->plane_y = (int16_t)(e->plane_y + e->vy);

    enemy_enforce_territory(e);

    e->plane_y = clamp_i16(e->plane_y, (int16_t)(-SCREEN_HEIGHT), SCREEN_HEIGHT - 1);

    if (has_target) {
        uint16_t hit_world_x = 0u;
        int16_t hit_center_y = 0;
        int16_t score_delta = 0;
        ground_target_hit_type_t hit_type =
            ground_targets_check_plane_collision(e->world_x, e->plane_y,
                                                 &hit_world_x, &hit_center_y,
                                                 &score_delta);

        if (hit_type != GROUND_TARGET_HIT_NONE) {
            enemy_start_falling(e);
            return;
        }
    }

    {
        int16_t plane_center_y = (int16_t)(e->plane_y + (PLAYER_SPRITE_SIZE_PX / 2));
        bool flock_hit = ambient_flocks_scatter_at(e->world_x,
                                                   plane_center_y,
                                                   (uint8_t)(PLAYER_SPRITE_SIZE_PX / 2));
        bool bird_hit = ambient_birds_check_plane_hit(e->world_x,
                                                      plane_center_y,
                                                      (uint8_t)(PLAYER_SPRITE_SIZE_PX / 2));

        if (flock_hit || bird_hit) {
            enemy_start_falling(e);
            if (flock_hit) {
                ambient_birds_spawn_splat(e->world_x, plane_center_y);
            }
            return;
        }
    }

    if (has_target) {
        enemy_try_fire(e);
    }
}

static void enemy_render(enemy_plane_t *e, uint8_t slot, uint16_t camera_world_x)
{
    uint16_t render_world_x;
    int16_t render_y;
    int16_t dx;
    int16_t center_x;
    int16_t sprite_x;
    int16_t interp_world;
    int16_t interp_y;
    int16_t world_delta;
    int16_t plane_delta;
    int16_t half;
    bool visible;

    if (e->destroyed || (!e->airborne && !e->falling && !e->crashed && e->launch_delay_10hz > 0u)) {
        sprite_mode5_set_enemy(slot, -32, -32, 0, false);
        return;
    }

    world_delta = wrapped_world_delta(e->prev_world_x, e->world_x);
    plane_delta = (int16_t)(e->plane_y - e->prev_plane_y);
    half = (int16_t)(ENEMY_FPS_DIV / 2);

    if (world_delta >= 0) {
        interp_world = (int16_t)(((world_delta * (int16_t)s_tick_div) + half) / ENEMY_FPS_DIV);
    } else {
        interp_world = (int16_t)(((world_delta * (int16_t)s_tick_div) - half) / ENEMY_FPS_DIV);
    }

    if (plane_delta >= 0) {
        interp_y = (int16_t)(((plane_delta * (int16_t)s_tick_div) + half) / ENEMY_FPS_DIV);
    } else {
        interp_y = (int16_t)(((plane_delta * (int16_t)s_tick_div) - half) / ENEMY_FPS_DIV);
    }

    render_world_x = wrap_world_x((int16_t)e->prev_world_x + interp_world);
    render_y = (int16_t)(e->prev_plane_y + interp_y);

    dx = world_delta_to_screen_x(render_world_x, camera_world_x);
    center_x = (int16_t)((SCREEN_WIDTH / 2) + dx);
    sprite_x = (int16_t)(center_x - (PLAYER_SPRITE_SIZE_PX / 2));
    visible = (sprite_x > -PLAYER_SPRITE_SIZE_PX) &&
              (sprite_x < SCREEN_WIDTH) &&
              (render_y > -PLAYER_SPRITE_SIZE_PX) &&
              (render_y < SCREEN_HEIGHT);

    sprite_mode5_set_enemy(slot, sprite_x, render_y, enemy_frame_index(e), visible);
}

void enemy_planes_init(void)
{
    s_enemies[0].home_x = 588u;
    s_enemies[0].territory_l = 48u;
    s_enemies[0].territory_r = 1154u;
    s_enemies[0].orient = false;

    s_enemies[1].home_x = 1720u;
    s_enemies[1].territory_l = 1155u;
    s_enemies[1].territory_r = 2089u;
    s_enemies[1].orient = true;

    s_enemies[2].home_x = 2456u;
    s_enemies[2].territory_l = 2090u;
    s_enemies[2].territory_r = (uint16_t)(WORLD_WIDTH_PX - 48);
    s_enemies[2].orient = true;

    for (uint8_t i = 0; i < MAX_ENEMIES; ++i) {
        s_enemies[i].home_y = home_plane_top_y(s_enemies[i].home_x);
        s_enemies[i].destroyed = false;
        s_enemies[i].crashed = false;
        s_enemies[i].respawn_delay_10hz = 0u;
        s_enemies[i].fire_cooldown_10hz = 0u;
        enemy_reset_home(&s_enemies[i]);
        s_enemies[i].launch_delay_10hz = 20u;  // All enemies start with 20 tick delay
        sprite_mode5_set_enemy(i, -32, -32, 0, false);
    }

    // Nearest enemy to player start takes off immediately
    uint8_t nearest = 1;  // Enemy 1 (home_x=1720) is nearest to PLAYER_START_WORLD_X_PX (1270)
    s_enemies[nearest].launch_delay_10hz = 0u;

    s_tick_div = 0u;
    s_tick_count_10hz = 0u;
}

void enemy_planes_update(uint16_t camera_world_x)
{
    if (++s_tick_div >= ENEMY_FPS_DIV) {
        s_tick_div = 0u;
        ++s_tick_count_10hz;
        for (uint8_t i = 0; i < MAX_ENEMIES; ++i) {
            enemy_tick_10hz(&s_enemies[i]);
        }
    }

    for (uint8_t i = 0; i < MAX_ENEMIES; ++i) {
        enemy_render(&s_enemies[i], i, camera_world_x);
    }
}

bool enemy_planes_check_shot_hit(uint16_t shot_world_x, int16_t shot_center_y,
                                 uint16_t *hit_world_x, int16_t *hit_center_y,
                                 int16_t *score_delta, bool *big_explosion)
{
    for (uint8_t i = 0; i < MAX_ENEMIES; ++i) {
        enemy_plane_t *e = &s_enemies[i];
        int16_t dx;
        int16_t top_y;
        int16_t bot_y;

        if (e->destroyed || e->falling || e->crashed) {
            continue;
        }

        dx = wrapped_world_delta(e->world_x, shot_world_x);
        top_y = e->plane_y;
        bot_y = (int16_t)(e->plane_y + PLAYER_SPRITE_SIZE_PX - 1);

        if (dx >= -(PLAYER_SPRITE_SIZE_PX / 2) &&
            dx <= (PLAYER_SPRITE_SIZE_PX / 2) &&
            shot_center_y >= top_y && shot_center_y <= bot_y) {
            enemy_start_falling(e);
            if (hit_world_x != 0) {
                *hit_world_x = e->world_x;
            }
            if (hit_center_y != 0) {
                *hit_center_y = (int16_t)(e->plane_y + (PLAYER_SPRITE_SIZE_PX / 2));
            }
            if (score_delta != 0) {
                *score_delta = ENEMY_SCORE_DELTA;
            }
            if (big_explosion != 0) {
                *big_explosion = false;
            }
            return true;
        }
    }

    return false;
}

bool enemy_planes_check_fragment_hit(uint16_t world_x, int16_t center_y, uint8_t half_size,
                                     uint16_t *hit_world_x, int16_t *hit_center_y,
                                     bool *big_explosion)
{
    for (uint8_t i = 0; i < MAX_ENEMIES; ++i) {
        enemy_plane_t *e = &s_enemies[i];
        int16_t dx;
        int16_t enemy_center_y;
        int16_t dy;

        if (e->destroyed || e->falling || e->crashed) {
            continue;
        }

        dx = wrapped_world_delta(e->world_x, world_x);
        enemy_center_y = (int16_t)(e->plane_y + (PLAYER_SPRITE_SIZE_PX / 2));
        dy = (int16_t)(enemy_center_y - center_y);

        if (dx >= -(int16_t)half_size && dx <= (int16_t)half_size &&
            dy >= -(int16_t)half_size && dy <= (int16_t)half_size) {
            enemy_start_falling(e);
            if (hit_world_x != 0) {
                *hit_world_x = e->world_x;
            }
            if (hit_center_y != 0) {
                *hit_center_y = enemy_center_y;
            }
            if (big_explosion != 0) {
                *big_explosion = false;
            }
            return true;
        }
    }

    return false;
}

bool enemy_planes_check_player_collision(uint16_t player_world_x, int16_t player_plane_y,
                                         uint16_t *hit_world_x, int16_t *hit_center_y,
                                         bool *big_explosion)
{
    for (uint8_t i = 0; i < MAX_ENEMIES; ++i) {
        enemy_plane_t *e = &s_enemies[i];
        int16_t dx;
        int16_t enemy_center_y;
        int16_t player_center_y;
        int16_t dy;

        if (e->destroyed) {
            continue;
        }

        dx = wrapped_world_delta(e->world_x, player_world_x);
        enemy_center_y = (int16_t)(e->plane_y + (PLAYER_SPRITE_SIZE_PX / 2));
        player_center_y = (int16_t)(player_plane_y + (PLAYER_SPRITE_SIZE_PX / 2));
        dy = (int16_t)(enemy_center_y - player_center_y);

        if (dx >= -10 && dx <= 10 && dy >= -10 && dy <= 10) {
            enemy_start_falling(e);
            if (hit_world_x != 0) {
                *hit_world_x = e->world_x;
            }
            if (hit_center_y != 0) {
                *hit_center_y = enemy_center_y;
            }
            if (big_explosion != 0) {
                *big_explosion = false;
            }
            return true;
        }
    }

    return false;
}
