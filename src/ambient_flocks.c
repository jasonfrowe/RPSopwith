#include <stdbool.h>
#include <stdint.h>

#include "ambient_birds.h"
#include "constants.h"
#include "ambient_flocks.h"
#include "sprite_mode5.h"

// Frame indices in projectiles_strip.png:
// 0 shot, 1 smoke, 2-9 bomb, 10-13 missile, 14-15 burst, 16-17 flock.
#define FLOCK_FRAME_A ((uint8_t)16u)
#define FLOCK_FRAME_B ((uint8_t)17u)

enum {
    FLOCK_MOVE_TICK_DIV = 6u,
        FLOCK_MOVE_STEP_PX = 2,
        FLOCK_TERRITORY_L = 370u,
        FLOCK_TERRITORY_R = 2630u
};

// Faithful flock spawn X positions from SDL Sopwith original level (swgames.c).
static const uint16_t s_flock_spawn_world_x[] = {370u, 1000u, 1630u, 2630u};
static const int16_t s_flock_screen_y[] = {8, 14, 10, 6};

static uint8_t s_anim_tick;
static uint8_t s_move_tick;
static uint16_t s_flock_world_x[MAX_FLOCK_SPRITES];
static uint16_t s_flock_min_x[MAX_FLOCK_SPRITES];
static uint16_t s_flock_max_x[MAX_FLOCK_SPRITES];
static int8_t s_flock_vx[MAX_FLOCK_SPRITES];
static bool s_flock_active[MAX_FLOCK_SPRITES];

static uint16_t clamp_world_x(uint16_t x)
{
    uint16_t world_max = (uint16_t)(GROUND_WIDTH * 8u - 1u);
    if (x > world_max) {
        return world_max;
    }
    return x;
}

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
    uint8_t flock_base = (uint8_t)MAX_COMBAT_PROJECTILES;

    s_anim_tick = 0;
    s_move_tick = 0;
    for (i = 0; i < MAX_FLOCK_SPRITES; ++i) {
        uint16_t spawn_x = s_flock_spawn_world_x[i];
        uint16_t min_x = FLOCK_TERRITORY_L;
        uint16_t max_x = clamp_world_x(FLOCK_TERRITORY_R);

        s_flock_world_x[i] = spawn_x;
        s_flock_min_x[i] = min_x;
        s_flock_max_x[i] = max_x;
        s_flock_vx[i] = (spawn_x < ((FLOCK_TERRITORY_L + FLOCK_TERRITORY_R) / 2u))
                        ? FLOCK_MOVE_STEP_PX
                        : -FLOCK_MOVE_STEP_PX;
        s_flock_active[i] = true;

        sprite_mode5_set_projectile((uint8_t)(flock_base + i), -32, -32, 0, false);
    }
}

bool ambient_flocks_scatter_at(uint16_t world_x, int16_t center_y, uint8_t half_size_px)
{
    uint8_t i;

    for (i = 0; i < MAX_FLOCK_SPRITES; ++i) {
        int16_t world_width = (int16_t)(GROUND_WIDTH * 8);
        int16_t half_world = (int16_t)(world_width / 2);
        int16_t flock_left = (int16_t)s_flock_world_x[i];
        int16_t flock_top = s_flock_screen_y[i];
        int16_t dx;

        if (!s_flock_active[i]) {
            continue;
        }

        dx = (int16_t)world_x - flock_left;
        if (dx > half_world) {
            dx -= world_width;
        } else if (dx < -half_world) {
            dx += world_width;
        }

        if (dx >= -(int16_t)half_size_px &&
            dx <= (int16_t)(PROJECTILE_SPRITE_SIZE_PX - 1 + half_size_px) &&
            center_y >= (int16_t)(flock_top - half_size_px) &&
            center_y <= (int16_t)(flock_top + PROJECTILE_SPRITE_SIZE_PX - 1 + half_size_px)) {
            s_flock_active[i] = false;
            ambient_birds_spawn_scatter((uint16_t)(flock_left + (PROJECTILE_SPRITE_SIZE_PX / 2)),
                                        (int16_t)(flock_top + (PROJECTILE_SPRITE_SIZE_PX / 2)));
            return true;
        }
    }

    return false;
}

void ambient_flocks_update(uint16_t camera_world_x)
{
    uint8_t i;
    uint8_t frame;
    uint8_t flock_base = (uint8_t)MAX_COMBAT_PROJECTILES;

    s_anim_tick++;
    frame = ((s_anim_tick >> 4) & 1u) ? FLOCK_FRAME_B : FLOCK_FRAME_A;

    if (++s_move_tick >= FLOCK_MOVE_TICK_DIV) {
        s_move_tick = 0;
        for (i = 0; i < MAX_FLOCK_SPRITES; ++i) {
            if (!s_flock_active[i]) {
                continue;
            }

            int16_t next_x = (int16_t)s_flock_world_x[i] + s_flock_vx[i];

            if (next_x <= (int16_t)s_flock_min_x[i]) {
                next_x = s_flock_min_x[i];
                s_flock_vx[i] = FLOCK_MOVE_STEP_PX;
            } else if (next_x >= (int16_t)s_flock_max_x[i]) {
                next_x = s_flock_max_x[i];
                s_flock_vx[i] = -FLOCK_MOVE_STEP_PX;
            }

            s_flock_world_x[i] = (uint16_t)next_x;
        }
    }

    for (i = 0; i < MAX_FLOCK_SPRITES; ++i) {
        if (!s_flock_active[i]) {
            sprite_mode5_set_projectile((uint8_t)(flock_base + i), -32, -32, 0, false);
            continue;
        }

        int16_t dx = world_delta_to_screen_x(s_flock_world_x[i], camera_world_x);
        int16_t screen_x = (int16_t)((SCREEN_WIDTH / 2) + dx);
        int16_t screen_y = s_flock_screen_y[i];
        bool visible = (screen_x > -PROJECTILE_SPRITE_SIZE_PX) && (screen_x < SCREEN_WIDTH);

        sprite_mode5_set_projectile((uint8_t)(flock_base + i), screen_x, screen_y, frame, visible);
    }
}
