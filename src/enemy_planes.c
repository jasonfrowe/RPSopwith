#include <stdbool.h>
#include <stdint.h>

#include "constants.h"
#include "enemy_planes.h"
#include "flight.h"
#include "sprite_mode5.h"

typedef struct enemy_plane_s {
    uint16_t world_x;
    int16_t plane_y;
    bool orient;
} enemy_plane_t;

enum {
    WORLD_WIDTH_PX = (GROUND_WIDTH * 8),
    HOME_SPAN_PX = 20,
    GROUND_CONTACT_FROM_TOP_PX = PLAYER_GROUND_CONTACT_FROM_TOP_PX,
};

static enemy_plane_t s_enemies[MAX_ENEMIES];

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

void enemy_planes_init(void)
{
    // Original authored side defenders (swgames.c):
    // left defender at x=588 orient=0, right defender at x=2456 orient=1.
    s_enemies[0].world_x = 588u;
    s_enemies[0].plane_y = home_plane_top_y(s_enemies[0].world_x);
    s_enemies[0].orient = false;

    s_enemies[1].world_x = 2456u;
    s_enemies[1].plane_y = home_plane_top_y(s_enemies[1].world_x);
    s_enemies[1].orient = true;

    for (uint8_t i = 0; i < MAX_ENEMIES; ++i) {
        sprite_mode5_set_enemy(i, -32, -32, 0, false);
    }
}

void enemy_planes_update(uint16_t camera_world_x)
{
    for (uint8_t i = 0; i < MAX_ENEMIES; ++i) {
        int16_t dx = world_delta_to_screen_x(s_enemies[i].world_x, camera_world_x);
        int16_t screen_center_x = (int16_t)((SCREEN_WIDTH / 2) + dx);
        int16_t sprite_x = (int16_t)(screen_center_x - (PLAYER_SPRITE_SIZE_PX / 2));
        int16_t sprite_y = s_enemies[i].plane_y;
        bool visible = (sprite_x > -PLAYER_SPRITE_SIZE_PX) && (sprite_x < SCREEN_WIDTH) &&
                       (sprite_y > -PLAYER_SPRITE_SIZE_PX) && (sprite_y < SCREEN_HEIGHT);
        uint8_t frame_index = s_enemies[i].orient ? 16u : 0u;

        sprite_mode5_set_enemy(i, sprite_x, sprite_y, frame_index, visible);
    }
}
