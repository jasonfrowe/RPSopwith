#include <stdbool.h>
#include <stdint.h>

#include "constants.h"
#include "ground_targets.h"
#include "sprite_mode5.h"
#include "tile_mode2.h"

// Faithful to SDL Sopwith original level object placements (swgames.c).
// We keep only ground objects currently representable by targets strip:
// TARGET types (orient 0..7) and OX.
typedef struct ground_target_s {
    uint16_t world_x;
    uint8_t frame_standing;
    uint8_t frame_destroyed;
    int8_t y_offset_px;
} ground_target_t;

// Strip frame layout from tools/assets/generate_targets.py:
// [target0_stand, target0_hit, target1_stand, target1_hit, ...]
// Orient mapping from SDL Sopwith:
// 0=hangar, 1=factory, 2=oil_tank, 3=tank, 4=truck, 5=tanker_truck, 6=flag, 7=tent
#define FRAME_STANDING(orient) ((uint8_t)((orient) * 2u))
#define FRAME_DESTROYED(orient) ((uint8_t)(((orient) * 2u) + 1u))

#define OX_FRAME_STANDING FRAME_STANDING(3u)
#define OX_FRAME_DESTROYED FRAME_DESTROYED(3u)

static const ground_target_t s_targets[] = {
    // Left side of map
    {191,  FRAME_STANDING(1), FRAME_DESTROYED(1), 0},
    {284,  FRAME_STANDING(3), FRAME_DESTROYED(3), 0},
    {409,  FRAME_STANDING(1), FRAME_DESTROYED(1), 0},
    {539,  FRAME_STANDING(1), FRAME_DESTROYED(1), 0},
    {685,  FRAME_STANDING(3), FRAME_DESTROYED(3), 0},
    {807,  FRAME_STANDING(0), FRAME_DESTROYED(0), 0},
    {934,  FRAME_STANDING(1), FRAME_DESTROYED(1), 0},

    // Player 1 base
    {1210, FRAME_STANDING(2), FRAME_DESTROYED(2), 0},
    {1240, FRAME_STANDING(0), FRAME_DESTROYED(0), 0},
    {1376, OX_FRAME_STANDING, OX_FRAME_DESTROYED, 0},
    {1440, FRAME_STANDING(3), FRAME_DESTROYED(3), 0},

    // Player 2 base
    {1550, FRAME_STANDING(3), FRAME_DESTROYED(3), 0},
    {1608, OX_FRAME_STANDING, OX_FRAME_DESTROYED, 0},
    {1750, FRAME_STANDING(0), FRAME_DESTROYED(0), 0},
    {1780, FRAME_STANDING(2), FRAME_DESTROYED(2), 0},
    {2024, FRAME_STANDING(1), FRAME_DESTROYED(1), 0},

    // Right side of map
    {2159, FRAME_STANDING(1), FRAME_DESTROYED(1), 0},
    {2279, FRAME_STANDING(3), FRAME_DESTROYED(3), 0},
    {2390, FRAME_STANDING(3), FRAME_DESTROYED(3), 0},
    {2549, FRAME_STANDING(0), FRAME_DESTROYED(0), 0},
    {2678, FRAME_STANDING(0), FRAME_DESTROYED(0), 0},
    {2763, FRAME_STANDING(1), FRAME_DESTROYED(1), 0},
};

static uint8_t s_target_count;

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

void ground_targets_init(void)
{
    uint8_t i;

    s_target_count = (uint8_t)(sizeof(s_targets) / sizeof(s_targets[0]));
    if (s_target_count > MAX_TARGETS) {
        s_target_count = MAX_TARGETS;
    }

    for (i = 0; i < MAX_TARGETS; ++i) {
        sprite_mode5_set_target(i, -32, -32, 0, false);
    }
}

void ground_targets_update(uint16_t camera_world_x)
{
    uint8_t i;

    for (i = 0; i < s_target_count; ++i) {
        const ground_target_t *t = &s_targets[i];
        int16_t dx = world_delta_to_screen_x(t->world_x, camera_world_x);
        int16_t screen_x = (int16_t)((SCREEN_WIDTH / 2) + dx);
        int16_t world_y = tile_mode2_ground_y_at_world_x(t->world_x);
        int16_t screen_y = (int16_t)(world_y - (TARGETS_SPRITE_SIZE_PX / 2) + t->y_offset_px);
        bool visible = (screen_x > -TARGETS_SPRITE_SIZE_PX) && (screen_x < SCREEN_WIDTH);

        sprite_mode5_set_target(i, screen_x, screen_y, t->frame_standing, visible);
    }

    for (; i < MAX_TARGETS; ++i) {
        sprite_mode5_set_target(i, -32, -32, 0, false);
    }
}
