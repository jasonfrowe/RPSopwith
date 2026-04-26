#include <stdbool.h>
#include <stdint.h>

#include "constants.h"
#include "ground_targets.h"
#include "sprite_mode5.h"
#include "tile_mode2.h"

typedef struct ground_target_s {
    uint16_t world_x;
    uint8_t orient;
    uint8_t frame_standing;
    uint8_t frame_destroyed;
    int8_t y_offset_px;
    uint16_t palette_ptr;
    int16_t score_delta;
} ground_target_t;

#define FRAME_STANDING(orient) ((uint8_t)((orient) * 2u))
#define FRAME_DESTROYED(orient) ((uint8_t)(((orient) * 2u) + 1u))

#define OX_FRAME_STANDING ((uint8_t)16u)
#define OX_FRAME_DESTROYED ((uint8_t)17u)

enum {
    TARGET_VERTICAL_BIAS_PX = -1,
    SCORE_DELTA_BUILDING = 100,
    SCORE_DELTA_EXPLOSIVE_BUILDING = 200,
    SCORE_DELTA_OX = -200
};

static const ground_target_t s_targets[] = {
    {191,  1, FRAME_STANDING(1), FRAME_DESTROYED(1), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {284,  3, FRAME_STANDING(3), FRAME_DESTROYED(3), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {409,  1, FRAME_STANDING(1), FRAME_DESTROYED(1), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {539,  1, FRAME_STANDING(1), FRAME_DESTROYED(1), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {685,  3, FRAME_STANDING(3), FRAME_DESTROYED(3), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {807,  0, FRAME_STANDING(0), FRAME_DESTROYED(0), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {934,  1, FRAME_STANDING(1), FRAME_DESTROYED(1), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {1210, 2, FRAME_STANDING(2), FRAME_DESTROYED(2), 0, PLAYER_TARGETS_PALETTE_ADDR, -SCORE_DELTA_EXPLOSIVE_BUILDING},
    {1240, 0, FRAME_STANDING(0), FRAME_DESTROYED(0), 0, PLAYER_TARGETS_PALETTE_ADDR, -SCORE_DELTA_BUILDING},
    {1376, 8, OX_FRAME_STANDING, OX_FRAME_DESTROYED, 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_OX},
    {1440, 3, FRAME_STANDING(3), FRAME_DESTROYED(3), 0, PLAYER_TARGETS_PALETTE_ADDR, -SCORE_DELTA_BUILDING},
    {1550, 3, FRAME_STANDING(3), FRAME_DESTROYED(3), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {1608, 8, OX_FRAME_STANDING, OX_FRAME_DESTROYED, 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_OX},
    {1750, 0, FRAME_STANDING(0), FRAME_DESTROYED(0), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {1780, 2, FRAME_STANDING(2), FRAME_DESTROYED(2), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_EXPLOSIVE_BUILDING},
    {2024, 1, FRAME_STANDING(1), FRAME_DESTROYED(1), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {2159, 1, FRAME_STANDING(1), FRAME_DESTROYED(1), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {2279, 3, FRAME_STANDING(3), FRAME_DESTROYED(3), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {2390, 3, FRAME_STANDING(3), FRAME_DESTROYED(3), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {2549, 0, FRAME_STANDING(0), FRAME_DESTROYED(0), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {2678, 0, FRAME_STANDING(0), FRAME_DESTROYED(0), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {2763, 1, FRAME_STANDING(1), FRAME_DESTROYED(1), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
};

static uint8_t s_target_count;
static int16_t s_target_ground_y[MAX_TARGETS];
static bool s_target_destroyed[MAX_TARGETS];

static uint16_t wrap_world_x(int16_t x)
{
    int16_t world_width = (int16_t)(GROUND_WIDTH * 8);

    while (x < 0) {
        x = (int16_t)(x + world_width);
    }
    while (x >= world_width) {
        x = (int16_t)(x - world_width);
    }

    return (uint16_t)x;
}

static int16_t world_delta_to_screen_x(uint16_t obj_world_x, uint16_t camera_world_x)
{
    int16_t dx = (int16_t)obj_world_x - (int16_t)camera_world_x;
    int16_t world_width = (int16_t)(GROUND_WIDTH * 8);
    int16_t half_world = (int16_t)(world_width / 2);

    if (dx > half_world) {
        dx = (int16_t)(dx - world_width);
    } else if (dx < -half_world) {
        dx = (int16_t)(dx + world_width);
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

    for (i = 0; i < s_target_count; ++i) {
        s_target_ground_y[i] = tile_mode2_ground_y_at_world_x(s_targets[i].world_x);
        s_target_destroyed[i] = false;
    }

    for (i = 0; i < MAX_TARGETS; ++i) {
        sprite_mode5_set_target(i, -32, -32, 0, TARGETS_PALETTE_ADDR, false);
    }
}

void ground_targets_update(uint16_t camera_world_x)
{
    uint8_t i;

    for (i = 0; i < s_target_count; ++i) {
        const ground_target_t *t = &s_targets[i];
        int16_t dx = world_delta_to_screen_x(t->world_x, camera_world_x);
        int16_t screen_x = (int16_t)((SCREEN_WIDTH / 2) + dx);
        int16_t world_y = s_target_ground_y[i];
        int16_t screen_y = (int16_t)(world_y - TARGETS_SPRITE_SIZE_PX + 1 + t->y_offset_px + TARGET_VERTICAL_BIAS_PX);
        bool visible = (screen_x > -TARGETS_SPRITE_SIZE_PX) && (screen_x < SCREEN_WIDTH);
        uint8_t frame = s_target_destroyed[i] ? t->frame_destroyed : t->frame_standing;

        sprite_mode5_set_target(i, screen_x, screen_y, frame, t->palette_ptr, visible);
    }

    for (; i < MAX_TARGETS; ++i) {
        sprite_mode5_set_target(i, -32, -32, 0, TARGETS_PALETTE_ADDR, false);
    }
}

bool ground_targets_all_enemy_targets_destroyed(void)
{
    uint8_t enemy_targets = 0u;

    for (uint8_t i = 0; i < s_target_count; ++i) {
        if (s_targets[i].score_delta > 0) {
            ++enemy_targets;
            if (!s_target_destroyed[i]) {
                return false;
            }
        }
    }

    return enemy_targets > 0u;
}

ground_target_hit_type_t ground_targets_check_hit(uint16_t proj_world_x, int16_t proj_center_y,
                                                  uint16_t *hit_world_x, int16_t *hit_center_y,
                                                  int16_t *score_delta)
{
    int16_t world_width = (int16_t)(GROUND_WIDTH * 8);
    int16_t half_world = (int16_t)(world_width / 2);

    for (uint8_t i = 0; i < s_target_count; ++i) {
        int16_t dx;
        int16_t top_y;
        int16_t bot_y;

        if (s_target_destroyed[i]) {
            continue;
        }

        dx = (int16_t)proj_world_x - (int16_t)s_targets[i].world_x;
        if (dx > half_world) {
            dx = (int16_t)(dx - world_width);
        } else if (dx < -half_world) {
            dx = (int16_t)(dx + world_width);
        }

        top_y = (int16_t)(s_target_ground_y[i] - TARGETS_SPRITE_SIZE_PX + 1 +
                          s_targets[i].y_offset_px + TARGET_VERTICAL_BIAS_PX);
        bot_y = (int16_t)(top_y + TARGETS_SPRITE_SIZE_PX - 1);

        if (dx >= (int16_t)(1 - PROJECTILE_SPRITE_SIZE_PX / 2) &&
            dx <= (int16_t)(TARGETS_SPRITE_SIZE_PX - 1 + PROJECTILE_SPRITE_SIZE_PX / 2) &&
            proj_center_y >= (int16_t)(top_y - PROJECTILE_SPRITE_SIZE_PX / 2 + 1) &&
            proj_center_y <= (int16_t)(bot_y + PROJECTILE_SPRITE_SIZE_PX / 2)) {
            s_target_destroyed[i] = true;

            if (hit_world_x != 0) {
                *hit_world_x = wrap_world_x((int16_t)s_targets[i].world_x + (TARGETS_SPRITE_SIZE_PX / 2));
            }
            if (hit_center_y != 0) {
                *hit_center_y = (int16_t)(top_y + (TARGETS_SPRITE_SIZE_PX / 2));
            }
            if (score_delta != 0) {
                *score_delta = s_targets[i].score_delta;
            }

            if (s_targets[i].orient == 8u) {
                return GROUND_TARGET_HIT_NO_EXPLOSION;
            }
            if (s_targets[i].orient == 2u || s_targets[i].orient == 5u) {
                return GROUND_TARGET_HIT_EXPLOSIVE;
            }
            return GROUND_TARGET_HIT_NORMAL;
        }
    }

    return GROUND_TARGET_HIT_NONE;
}

ground_target_hit_type_t ground_targets_check_shot_hit(uint16_t shot_world_x, int16_t shot_center_y,
                                                       uint16_t *hit_world_x, int16_t *hit_center_y,
                                                       int16_t *score_delta)
{
    int16_t world_width = (int16_t)(GROUND_WIDTH * 8);
    int16_t half_world = (int16_t)(world_width / 2);

    for (uint8_t i = 0; i < s_target_count; ++i) {
        int16_t dx;
        int16_t top_y;
        int16_t bot_y;

        if (s_target_destroyed[i]) {
            continue;
        }

        dx = (int16_t)shot_world_x - (int16_t)s_targets[i].world_x;
        if (dx > half_world) {
            dx = (int16_t)(dx - world_width);
        } else if (dx < -half_world) {
            dx = (int16_t)(dx + world_width);
        }

        top_y = (int16_t)(s_target_ground_y[i] - TARGETS_SPRITE_SIZE_PX + 1 +
                          s_targets[i].y_offset_px + TARGET_VERTICAL_BIAS_PX);
        bot_y = (int16_t)(top_y + TARGETS_SPRITE_SIZE_PX - 1);

        if (dx >= 0 && dx <= (TARGETS_SPRITE_SIZE_PX - 1) &&
            shot_center_y >= top_y && shot_center_y <= bot_y) {
            s_target_destroyed[i] = true;

            if (hit_world_x != 0) {
                *hit_world_x = wrap_world_x((int16_t)s_targets[i].world_x + (TARGETS_SPRITE_SIZE_PX / 2));
            }
            if (hit_center_y != 0) {
                *hit_center_y = (int16_t)(top_y + (TARGETS_SPRITE_SIZE_PX / 2));
            }
            if (score_delta != 0) {
                *score_delta = s_targets[i].score_delta;
            }

            if (s_targets[i].orient == 8u) {
                return GROUND_TARGET_HIT_NO_EXPLOSION;
            }
            if (s_targets[i].orient == 2u || s_targets[i].orient == 5u) {
                return GROUND_TARGET_HIT_EXPLOSIVE;
            }
            return GROUND_TARGET_HIT_NORMAL;
        }
    }

    return GROUND_TARGET_HIT_NONE;
}

ground_target_hit_type_t ground_targets_check_plane_collision(uint16_t plane_world_x, int16_t plane_top_y,
                                                              uint16_t *hit_world_x, int16_t *hit_center_y,
                                                              int16_t *score_delta)
{
    int16_t world_width = (int16_t)(GROUND_WIDTH * 8);
    int16_t half_world = (int16_t)(world_width / 2);
    int16_t plane_bot_y = (int16_t)(plane_top_y + PLAYER_SPRITE_SIZE_PX - 1);

    for (uint8_t i = 0; i < s_target_count; ++i) {
        int16_t dx;
        int16_t top_y;
        int16_t bot_y;

        if (s_target_destroyed[i]) {
            continue;
        }

        dx = (int16_t)plane_world_x - (int16_t)s_targets[i].world_x;
        if (dx > half_world) {
            dx = (int16_t)(dx - world_width);
        } else if (dx < -half_world) {
            dx = (int16_t)(dx + world_width);
        }

        top_y = (int16_t)(s_target_ground_y[i] - TARGETS_SPRITE_SIZE_PX + 1 +
                          s_targets[i].y_offset_px + TARGET_VERTICAL_BIAS_PX);
        bot_y = (int16_t)(top_y + TARGETS_SPRITE_SIZE_PX - 1);

        if (dx >= -(PLAYER_SPRITE_SIZE_PX / 2) &&
            dx <= (TARGETS_SPRITE_SIZE_PX - 1 + PLAYER_SPRITE_SIZE_PX / 2) &&
            plane_top_y <= bot_y && plane_bot_y >= top_y) {
            s_target_destroyed[i] = true;

            if (hit_world_x != 0) {
                *hit_world_x = wrap_world_x((int16_t)s_targets[i].world_x + (TARGETS_SPRITE_SIZE_PX / 2));
            }
            if (hit_center_y != 0) {
                *hit_center_y = (int16_t)(top_y + (TARGETS_SPRITE_SIZE_PX / 2));
            }
            if (score_delta != 0) {
                *score_delta = s_targets[i].score_delta;
            }
            if (s_targets[i].orient == 8u) {
                return GROUND_TARGET_HIT_NO_EXPLOSION;
            }
            if (s_targets[i].orient == 2u || s_targets[i].orient == 5u) {
                return GROUND_TARGET_HIT_EXPLOSIVE;
            }
            return GROUND_TARGET_HIT_NORMAL;
        }
    }

    return GROUND_TARGET_HIT_NONE;
}