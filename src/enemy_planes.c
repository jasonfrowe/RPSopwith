#include <stdbool.h>
#include <stdint.h>

#include "constants.h"
#include "enemy_planes.h"
#include "flight.h"
#include "sprite_mode5.h"

typedef struct enemy_plane_s {
    uint16_t home_x;
    uint16_t territory_l;
    uint16_t territory_r;
    uint16_t world_x;
    int16_t home_y;
    int16_t plane_y;
    int8_t vx;
    int8_t vy;
    uint8_t angle;
    uint8_t speed;
    int8_t flaps;
    uint8_t launch_delay_10hz;
    bool airborne;
    bool orient;
} enemy_plane_t;

enum {
    WORLD_WIDTH_PX = (GROUND_WIDTH * 8),
    HOME_SPAN_PX = 20,
    GROUND_CONTACT_FROM_TOP_PX = PLAYER_GROUND_CONTACT_FROM_TOP_PX,
    ENEMY_FPS_DIV = 6,
    MIN_SPEED = 4,
    MAX_SPEED = 8,
    ENEMY_CRUISE_CLEARANCE_PX = 42,
    ENEMY_TERRITORY_MARGIN_PX = 16,
    HOME_RANGE_PX = 16,
};

static enemy_plane_t s_enemies[MAX_ENEMIES];
static uint8_t s_tick_div = 0;
static uint16_t s_tick_10hz = 0;

static const int16_t s_sintab[16] = {
    0, 98, 181, 237, 256, 237, 181, 98,
    0, -98, -181, -237, -256, -237, -181, -98
};

static int16_t wrap_world_x(int32_t x)
{
    int32_t wrapped = x % WORLD_WIDTH_PX;
    if (wrapped < 0) {
        wrapped += WORLD_WIDTH_PX;
    }
    return (int16_t)wrapped;
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

static uint16_t abs_i16(int16_t v)
{
    return (v < 0) ? (uint16_t)(-v) : (uint16_t)v;
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

static int16_t world_delta_to_screen_x(uint16_t obj_world_x, uint16_t camera_world_x)
{
    int16_t dx = (int16_t)obj_world_x - (int16_t)camera_world_x;
    int16_t half_world = (int16_t)(WORLD_WIDTH_PX / 2);

    if (dx > half_world) {
        dx -= (int16_t)WORLD_WIDTH_PX;
    } else if (dx < -half_world) {
        dx += (int16_t)WORLD_WIDTH_PX;
    }

    return dx;
}

static int16_t home_plane_top_y(uint16_t spawn_world_x)
{
    int16_t ground = 0;

    for (uint16_t x = spawn_world_x; x <= (uint16_t)(spawn_world_x + HOME_SPAN_PX); ++x) {
        int16_t y = flight_terrain_y_at((uint16_t)(x % WORLD_WIDTH_PX));
        if (y > ground) {
            ground = y;
        }
    }

    return (int16_t)(ground - GROUND_CONTACT_FROM_TOP_PX);
}

static uint8_t nearest_enemy_to_player_start(void)
{
    uint8_t best = 0u;
    uint16_t best_dist = 0xFFFFu;

    for (uint8_t i = 0; i < MAX_ENEMIES; ++i) {
        uint16_t d = abs_i16(wrapped_world_delta(PLAYER_START_WORLD_X_PX, s_enemies[i].home_x));
        if (d < best_dist) {
            best_dist = d;
            best = i;
        }
    }

    return best;
}

static void velocity_from_angle(uint8_t angle, uint8_t speed, int8_t *vx, int8_t *vy)
{
    int16_t dx = (int16_t)(((int16_t)speed * s_sintab[(angle + 4u) & 0x0Fu]) / 256);
    int16_t dy = (int16_t)(((int16_t)speed * s_sintab[angle & 0x0Fu]) / 256);

    *vx = (int8_t)dx;
    *vy = (int8_t)(-dy);
}

static int16_t range_metric(int16_t x, int16_t y, int16_t ax, int16_t ay)
{
    int16_t dx = abs_i16((int16_t)(x - ax));
    int16_t dy = abs_i16((int16_t)(y - ay));

    // Lightweight weighted Manhattan metric to avoid expensive 32-bit math.
    return (int16_t)(dx + dy + (dy >> 1));
}

static bool within_home_range(const enemy_plane_t *e)
{
    return abs_i16(wrapped_world_delta(e->world_x, e->home_x)) < HOME_RANGE_PX &&
           abs_i16((int16_t)(e->plane_y - e->home_y)) < HOME_RANGE_PX;
}

static bool player_in_enemy_territory(const enemy_plane_t *e, uint16_t player_x)
{
    return (player_x >= e->territory_l) && (player_x <= e->territory_r);
}

static bool candidate_crash_risk(const enemy_plane_t *e, uint16_t nx, int16_t ny, int8_t vy)
{
    int16_t terrain_y = flight_terrain_y_at(nx);
    int16_t alt = (int16_t)(terrain_y - (ny + (PLAYER_SPRITE_SIZE_PX / 2)));
    int16_t lookahead = (within_home_range(e) ? 1 : 3);

    if (alt > 50) {
        return false;
    }

    return (int16_t)(alt - (vy * lookahead)) < 8;
}

static void enemy_aim_cruise(enemy_plane_t *e)
{
    static const int8_t cflaps[3] = {0, -1, 1};
    uint16_t player_x = flight_world_x_physics();
    int16_t player_y = flight_plane_y_physics();
    int16_t ax;
    int16_t ay;
    int16_t best_r = 32767;
    uint8_t best_i = 0;
    bool found_safe = false;

    if (player_in_enemy_territory(e, player_x)) {
        ax = (int16_t)player_x;
        ay = player_y;
    } else {
        int16_t home_sweep = ((s_tick_10hz & 0x1Fu) < 16u) ? 24 : -24;
        ax = (int16_t)wrap_world_x((int32_t)e->home_x + home_sweep);
        ay = (int16_t)(e->home_y - 28);
    }

    for (uint8_t i = 0; i < 3u; ++i) {
        int8_t flap = cflaps[i];
        uint8_t nangle = (uint8_t)((e->angle + (e->orient ? -flap : flap) + 16) & 0x0F);
        uint8_t nspeed = clamp_u8((uint8_t)(e->speed + 1u), MIN_SPEED, MAX_SPEED);
        int8_t cvx;
        int8_t cvy;
        uint16_t nx;
        int16_t ny;
        int16_t r;

        velocity_from_angle(nangle, nspeed, &cvx, &cvy);
        nx = (uint16_t)wrap_world_x((int32_t)e->world_x + cvx);
        ny = (int16_t)(e->plane_y + cvy);
        r = range_metric((int16_t)nx, ny, ax, ay);

        if (!candidate_crash_risk(e, nx, ny, cvy)) {
            if (!found_safe || r < best_r) {
                found_safe = true;
                best_r = r;
                best_i = i;
            }
        }
    }

    if (!found_safe) {
        int16_t best_alt = -32767;
        best_i = 0;
        for (uint8_t i = 0; i < 3u; ++i) {
            int8_t flap = cflaps[i];
            uint8_t nangle = (uint8_t)((e->angle + (e->orient ? -flap : flap) + 16) & 0x0F);
            uint8_t nspeed = clamp_u8((uint8_t)(e->speed + 1u), MIN_SPEED, MAX_SPEED);
            int8_t cvx;
            int8_t cvy;
            uint16_t nx;
            int16_t ny;
            int16_t alt;

            velocity_from_angle(nangle, nspeed, &cvx, &cvy);
            nx = (uint16_t)wrap_world_x((int32_t)e->world_x + cvx);
            ny = (int16_t)(e->plane_y + cvy);
            alt = (int16_t)(flight_terrain_y_at(nx) - (ny + (PLAYER_SPRITE_SIZE_PX / 2)));

            if (alt > best_alt) {
                best_alt = alt;
                best_i = i;
            }
        }
    }

    e->flaps = cflaps[best_i];
}

static void enemy_move_tick_10hz(enemy_plane_t *e)
{
    uint8_t nangle = (uint8_t)((e->angle + (e->orient ? -e->flaps : e->flaps) + 16) & 0x0F);
    uint8_t target_speed = (uint8_t)(MIN_SPEED + 2u);

    if (!e->airborne) {
        e->airborne = true;
        e->speed = target_speed;
    } else if (e->speed < target_speed) {
        ++e->speed;
    } else if (e->speed > target_speed) {
        --e->speed;
    }

    e->angle = nangle;
    velocity_from_angle(e->angle, e->speed, &e->vx, &e->vy);

    e->world_x = (uint16_t)wrap_world_x((int32_t)e->world_x + e->vx);
    e->plane_y = (int16_t)(e->plane_y + e->vy);

    // Turn cleanly at territory edges so enemies don't stall there.
    if (e->world_x <= (uint16_t)(e->territory_l + ENEMY_TERRITORY_MARGIN_PX)) {
        e->world_x = e->territory_l;
        e->orient = false;
        e->angle = 0u;
        e->flaps = 0;
        velocity_from_angle(e->angle, e->speed, &e->vx, &e->vy);
    } else if (e->world_x >= (uint16_t)(e->territory_r - ENEMY_TERRITORY_MARGIN_PX)) {
        e->world_x = e->territory_r;
        e->orient = true;
        e->angle = 8u;
        e->flaps = 0;
        velocity_from_angle(e->angle, e->speed, &e->vx, &e->vy);
    }

    {
        int16_t terrain_y = flight_terrain_y_at(e->world_x);
        int16_t min_top = (int16_t)(terrain_y - GROUND_CONTACT_FROM_TOP_PX - ENEMY_CRUISE_CLEARANCE_PX);
        if (e->plane_y > min_top) {
            e->plane_y = min_top;
        }
    }

    e->plane_y = clamp_i16(e->plane_y, -SCREEN_HEIGHT, SCREEN_HEIGHT - 1);
}

static void enemy_flight_tick_10hz(enemy_plane_t *e)
{
    if (!e->airborne) {
        if (e->launch_delay_10hz > 0u) {
            --e->launch_delay_10hz;
            return;
        }
        e->speed = MIN_SPEED;
        e->flaps = 0;
        e->angle = e->orient ? 8u : 0u;
    }

    enemy_aim_cruise(e);
    enemy_move_tick_10hz(e);
}

void enemy_planes_init(void)
{
    // Original non-player plane homes from swgames.c in single-player/computer mode:
    // x=588 (orient=0, territory 0..1154),
    // x=1720 (orient=1, territory 1155..2089),
    // x=2456 (orient=1, territory 2089..2999).
    s_enemies[0].home_x = 588u;
    s_enemies[0].territory_l = 0u;
    s_enemies[0].territory_r = 1154u;
    s_enemies[0].world_x = s_enemies[0].home_x;
    s_enemies[0].home_y = home_plane_top_y(s_enemies[0].home_x);
    s_enemies[0].plane_y = s_enemies[0].home_y;
    s_enemies[0].vx = 0;
    s_enemies[0].vy = 0;
    s_enemies[0].angle = 0u;
    s_enemies[0].speed = 0u;
    s_enemies[0].flaps = 0;
    s_enemies[0].launch_delay_10hz = 20u;
    s_enemies[0].airborne = false;
    s_enemies[0].orient = false;

    s_enemies[1].home_x = 1720u;
    s_enemies[1].territory_l = 1155u;
    s_enemies[1].territory_r = 2089u;
    s_enemies[1].world_x = s_enemies[1].home_x;
    s_enemies[1].home_y = home_plane_top_y(s_enemies[1].home_x);
    s_enemies[1].plane_y = s_enemies[1].home_y;
    s_enemies[1].vx = 0;
    s_enemies[1].vy = 0;
    s_enemies[1].angle = 8u;
    s_enemies[1].speed = 0u;
    s_enemies[1].flaps = 0;
    s_enemies[1].launch_delay_10hz = 20u;
    s_enemies[1].airborne = false;
    s_enemies[1].orient = true;

    s_enemies[2].home_x = 2456u;
    s_enemies[2].territory_l = 2089u;
    s_enemies[2].territory_r = (uint16_t)(WORLD_WIDTH_PX - 1);
    s_enemies[2].world_x = s_enemies[2].home_x;
    s_enemies[2].home_y = home_plane_top_y(s_enemies[2].home_x);
    s_enemies[2].plane_y = s_enemies[2].home_y;
    s_enemies[2].vx = 0;
    s_enemies[2].vy = 0;
    s_enemies[2].angle = 8u;
    s_enemies[2].speed = 0u;
    s_enemies[2].flaps = 0;
    s_enemies[2].launch_delay_10hz = 20u;
    s_enemies[2].airborne = false;
    s_enemies[2].orient = true;

    s_enemies[nearest_enemy_to_player_start()].launch_delay_10hz = 0u;
    s_tick_div = 0;
    s_tick_10hz = 0;

    for (uint8_t i = 0; i < MAX_ENEMIES; ++i) {
        sprite_mode5_set_enemy(i, -32, -32, 0, false);
    }
}

void enemy_planes_update(uint16_t camera_world_x)
{
    if (++s_tick_div >= ENEMY_FPS_DIV) {
        s_tick_div = 0;
        ++s_tick_10hz;
        for (uint8_t i = 0; i < MAX_ENEMIES; ++i) {
            enemy_flight_tick_10hz(&s_enemies[i]);
        }
    }

    for (uint8_t i = 0; i < MAX_ENEMIES; ++i) {
        int16_t dx = world_delta_to_screen_x(s_enemies[i].world_x, camera_world_x);
        int16_t screen_center_x = (int16_t)((SCREEN_WIDTH / 2) + dx);
        int16_t sprite_x = (int16_t)(screen_center_x - (PLAYER_SPRITE_SIZE_PX / 2));
        int16_t sprite_y = s_enemies[i].plane_y;
        bool visible = (sprite_x > -PLAYER_SPRITE_SIZE_PX) && (sprite_x < SCREEN_WIDTH) &&
                       (sprite_y > -PLAYER_SPRITE_SIZE_PX) && (sprite_y < SCREEN_HEIGHT);
        uint8_t frame_index = s_enemies[i].orient ? 24u : 0u;

        sprite_mode5_set_enemy(i, sprite_x, sprite_y, frame_index, visible);
    }
}
