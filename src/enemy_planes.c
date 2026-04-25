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
    uint8_t launch_delay_10hz;
    bool airborne;
    bool orient;
} enemy_plane_t;

enum {
    WORLD_WIDTH_PX = (GROUND_WIDTH * 8),
    HOME_SPAN_PX = 20,
    GROUND_CONTACT_FROM_TOP_PX = PLAYER_GROUND_CONTACT_FROM_TOP_PX,
    ENEMY_FPS_DIV = 6,
    ENEMY_SPEED_PX = 4,
    ENEMY_CRUISE_CLEARANCE_PX = 42,
    ENEMY_TERRITORY_MARGIN_PX = 16,
};

static enemy_plane_t s_enemies[MAX_ENEMIES];
static uint8_t s_tick_div = 0;
static uint16_t s_tick_10hz = 0;

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

static void enemy_flight_tick_10hz(enemy_plane_t *e)
{
    int16_t terrain_y;
    int16_t cruise_top_y;

    if (!e->airborne) {
        if (e->launch_delay_10hz > 0u) {
            --e->launch_delay_10hz;
            return;
        }
        e->airborne = true;
        e->vx = e->orient ? -ENEMY_SPEED_PX : ENEMY_SPEED_PX;
        e->vy = -1;
    }

    if (e->world_x <= (uint16_t)(e->territory_l + ENEMY_TERRITORY_MARGIN_PX)) {
        e->vx = ENEMY_SPEED_PX;
        e->orient = false;
    } else if (e->world_x >= (uint16_t)(e->territory_r - ENEMY_TERRITORY_MARGIN_PX)) {
        e->vx = -ENEMY_SPEED_PX;
        e->orient = true;
    }

    e->world_x = (uint16_t)wrap_world_x((int32_t)e->world_x + e->vx);

    terrain_y = flight_terrain_y_at(e->world_x);
    cruise_top_y = (int16_t)(terrain_y - GROUND_CONTACT_FROM_TOP_PX - ENEMY_CRUISE_CLEARANCE_PX);

    // Keep a gentle bob so flight is visibly alive while staying terrain-safe.
    if ((s_tick_10hz & 0x07u) < 4u) {
        e->plane_y -= 1;
    } else {
        e->plane_y += 1;
    }

    if (e->plane_y > cruise_top_y) {
        e->plane_y = cruise_top_y;
    }
    if (e->plane_y < (e->home_y - 64)) {
        e->plane_y = (int16_t)(e->home_y - 64);
    }
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
