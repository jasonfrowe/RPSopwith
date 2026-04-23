#include <stdbool.h>
#include <stdint.h>

#include "constants.h"
#include "ambient_flocks.h"
#include "sprite_mode5.h"

// Frame indices in projectiles_strip.png:
// 0 shot, 1 smoke, 2-3 bomb, 4-7 missile, 8-9 burst, 10-11 flock.
#define FLOCK_FRAME_A ((uint8_t)10u)
#define FLOCK_FRAME_B ((uint8_t)11u)

// Faithful flock spawn X positions from SDL Sopwith original level (swgames.c).
static const uint16_t s_flock_world_x[] = {370u, 1000u, 1630u, 2630u};

static uint8_t s_anim_tick;

static int16_t world_delta_to_screen_x(uint16_t obj_world_x, uint16_t camera_world_x)
{
    int16_t dx = (int16_t)obj_world_x - (int16_t)camera_world_x;
    int16_t world_width = (int16_t)(GROUND_WIDTH * 8);
    int16_t half_world = (int16_t)(world_width / 2);

    if (dx > half_world) {
        dx -= world_width;
    } else if (dx < -half_world) {
        dx += world_width;
    }

    return dx;
}

void ambient_flocks_init(void)
{
    uint8_t i;

    s_anim_tick = 0;
    for (i = 0; i < MAX_PROJECTILES; ++i) {
        sprite_mode5_set_projectile(i, -32, -32, 0, false);
    }
}

void ambient_flocks_update(uint16_t camera_world_x)
{
    uint8_t i;
    uint8_t frame;

    s_anim_tick++;
    frame = ((s_anim_tick >> 4) & 1u) ? FLOCK_FRAME_B : FLOCK_FRAME_A;

    for (i = 0; i < (uint8_t)(sizeof(s_flock_world_x) / sizeof(s_flock_world_x[0])); ++i) {
        int16_t dx = world_delta_to_screen_x(s_flock_world_x[i], camera_world_x);
        int16_t screen_x = (int16_t)((SCREEN_WIDTH / 2) + dx);
        int16_t screen_y = 0;
        bool visible = (screen_x > -PROJECTILE_SPRITE_SIZE_PX) && (screen_x < SCREEN_WIDTH);

        sprite_mode5_set_projectile(i, screen_x, screen_y, frame, visible);
    }

    for (; i < MAX_PROJECTILES; ++i) {
        sprite_mode5_set_projectile(i, -32, -32, 0, false);
    }
}
