#include <stdbool.h>
#include <stdint.h>

#include "constants.h"
#include "flight.h"
#include "ground_targets.h"
#include "projectiles.h"
#include "sprite_mode5.h"
#include "tile_mode2.h"

typedef struct ground_target_s {
    uint16_t world_x;
    uint8_t orient;
    uint8_t type;
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
    SCORE_DELTA_OX = -200,
    TARGETS_FPS_DIV = 6u,
    TARGET_MAX_VERTICAL_FIRE_PX = 100,
    TARGET_LEVEL2_FIRE_PERIOD_SCALE = 5u,
    TARGET_LEVEL3_FIRE_PERIOD_SCALE = 3u,
    TARGET_LEVEL4_PLUS_FIRE_PERIOD_SCALE = 2u,
    TARGET_TYPE_BUILDING = 0u,
    TARGET_TYPE_EXPLOSIVE_BUILDING = 1u,
    TARGET_TYPE_TRUCK = 2u,
    TARGET_TYPE_TANKER_TRUCK = 3u,
    TARGET_TYPE_OX = 4u,
    TARGET_TYPE_FRIENDLY_BUILDING = 5u,
    TARGET_OX_HITBOX_TOP_OFFSET_PX = 8,
    TARGET_OX_HITBOX_WIDTH_PX = 12,
    TARGET_OX_HITBOX_HEIGHT_PX = 6
};

static const ground_target_t s_targets[] = {
    {191,  1, TARGET_TYPE_BUILDING, FRAME_STANDING(1), FRAME_DESTROYED(1), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {284,  3, TARGET_TYPE_BUILDING, FRAME_STANDING(3), FRAME_DESTROYED(3), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {409,  1, TARGET_TYPE_BUILDING, FRAME_STANDING(1), FRAME_DESTROYED(1), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {539,  1, TARGET_TYPE_TRUCK, FRAME_STANDING(1), FRAME_DESTROYED(1), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {685,  3, TARGET_TYPE_BUILDING, FRAME_STANDING(3), FRAME_DESTROYED(3), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {807,  0, TARGET_TYPE_BUILDING, FRAME_STANDING(0), FRAME_DESTROYED(0), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {934,  1, TARGET_TYPE_BUILDING, FRAME_STANDING(1), FRAME_DESTROYED(1), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {1210, 2, TARGET_TYPE_FRIENDLY_BUILDING, FRAME_STANDING(2), FRAME_DESTROYED(2), 0, PLAYER_TARGETS_PALETTE_ADDR, -SCORE_DELTA_EXPLOSIVE_BUILDING},
    {1240, 0, TARGET_TYPE_FRIENDLY_BUILDING, FRAME_STANDING(0), FRAME_DESTROYED(0), 0, PLAYER_TARGETS_PALETTE_ADDR, -SCORE_DELTA_BUILDING},
    {1376, 8, TARGET_TYPE_OX, OX_FRAME_STANDING, OX_FRAME_DESTROYED, 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_OX},
    {1440, 3, TARGET_TYPE_FRIENDLY_BUILDING, FRAME_STANDING(3), FRAME_DESTROYED(3), 0, PLAYER_TARGETS_PALETTE_ADDR, -SCORE_DELTA_BUILDING},
    {1550, 3, TARGET_TYPE_BUILDING, FRAME_STANDING(3), FRAME_DESTROYED(3), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {1608, 8, TARGET_TYPE_OX, OX_FRAME_STANDING, OX_FRAME_DESTROYED, 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_OX},
    {1750, 0, TARGET_TYPE_TRUCK, FRAME_STANDING(0), FRAME_DESTROYED(0), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {1780, 2, TARGET_TYPE_EXPLOSIVE_BUILDING, FRAME_STANDING(2), FRAME_DESTROYED(2), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_EXPLOSIVE_BUILDING},
    {2024, 1, TARGET_TYPE_TANKER_TRUCK, FRAME_STANDING(1), FRAME_DESTROYED(1), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {2159, 1, TARGET_TYPE_BUILDING, FRAME_STANDING(1), FRAME_DESTROYED(1), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {2279, 3, TARGET_TYPE_BUILDING, FRAME_STANDING(3), FRAME_DESTROYED(3), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {2390, 3, TARGET_TYPE_BUILDING, FRAME_STANDING(3), FRAME_DESTROYED(3), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {2549, 0, TARGET_TYPE_BUILDING, FRAME_STANDING(0), FRAME_DESTROYED(0), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {2678, 0, TARGET_TYPE_TRUCK, FRAME_STANDING(0), FRAME_DESTROYED(0), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
    {2763, 1, TARGET_TYPE_BUILDING, FRAME_STANDING(1), FRAME_DESTROYED(1), 0, TARGETS_PALETTE_ADDR, SCORE_DELTA_BUILDING},
};

static uint8_t s_target_count;
static int16_t s_target_ground_y[MAX_TARGETS];
static bool s_target_destroyed[MAX_TARGETS];
static uint8_t s_target_fire_cooldown[MAX_TARGETS];
static uint8_t s_level = 1u;
static uint8_t s_tick_div;
static uint16_t s_tick_count_10hz;

static const int16_t s_sintab[16] = {
    0, 98, 181, 237, 256, 237, 181, 98,
    0, -98, -181, -237, -256, -237, -181, -98
};

static int16_t abs_i16(int16_t v)
{
    return (v < 0) ? (int16_t)(-v) : v;
}

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

static bool target_is_hostile(const ground_target_t *target)
{
    return target->score_delta > 0 && target->type != TARGET_TYPE_OX;
}

static uint8_t target_aggression(const ground_target_t *target)
{
    if (target->type == TARGET_TYPE_TRUCK || target->type == TARGET_TYPE_TANKER_TRUCK) {
        return 5u;
    }

    if (target->type == TARGET_TYPE_BUILDING || target->type == TARGET_TYPE_EXPLOSIVE_BUILDING) {
        return 2u;
    }

    return 0u;
}

static bool target_enabled_for_level(uint8_t target_index, const ground_target_t *target)
{
    if (!target_is_hostile(target)) {
        return false;
    }

    if (s_level <= 2u) {
        return (target_index & 1u) == 0u;
    }

    if (s_level == 3u) {
        return (target_index & 3u) != 3u;
    }

    return true;
}

static uint8_t angle_from_delta(int16_t dx, int16_t dy)
{
    int16_t best_score = -32767;
    uint8_t best_angle = 0u;

    for (uint8_t a = 0; a < 16u; ++a) {
        int16_t dir_x = s_sintab[(uint8_t)((a + 4u) & 0x0Fu)];
        int16_t dir_y = (int16_t)(-s_sintab[a]);
        int16_t score = (int16_t)(dx * dir_x + dy * dir_y);

        if (score > best_score) {
            best_score = score;
            best_angle = a;
        }
    }

    return best_angle;
}

static int16_t target_fire_range_px(void)
{
    uint8_t game_num = (s_level > 0u) ? (uint8_t)(s_level - 1u) : 0u;
    int16_t target_range = 150;

    if (game_num < 6u) {
        target_range = (int16_t)(target_range - (15 * (6 - game_num)));
    }

    return target_range;
}

static int16_t target_fire_distance_metric(int16_t dx, int16_t dy)
{
    dx = abs_i16(dx);
    dy = abs_i16(dy);

    return (int16_t)(dx + dy + (dy >> 1));
}

static uint8_t target_fire_period_ticks(uint8_t aggression, uint8_t target_type)
{
    uint8_t scale;
    uint8_t period;

    if (s_level <= 2u) {
        scale = TARGET_LEVEL2_FIRE_PERIOD_SCALE;
    } else if (s_level == 3u) {
        scale = TARGET_LEVEL3_FIRE_PERIOD_SCALE;
    } else {
        scale = TARGET_LEVEL4_PLUS_FIRE_PERIOD_SCALE;
    }

    period = (uint8_t)(aggression * scale);

    if ((target_type == TARGET_TYPE_TRUCK || target_type == TARGET_TYPE_TANKER_TRUCK) && period > 1u) {
        --period;
    }

    return period;
}

static void update_target_fire(void)
{
    uint16_t player_world_x;
    int16_t player_center_y;
    int16_t fire_range_px;

    if (s_level < 2u || !flight_is_airborne() || flight_is_crashed() || flight_is_falling()) {
        return;
    }

    player_world_x = flight_world_x_physics();
    player_center_y = (int16_t)(flight_plane_y_physics() + PLANE_HITBOX_CENTER_Y_OFFSET_PX);
    fire_range_px = target_fire_range_px();

    for (uint8_t i = 0; i < s_target_count; ++i) {
        const ground_target_t *target = &s_targets[i];
        int16_t dx;
        int16_t top_y;
        int16_t target_center_y;
        int16_t dy;
        uint16_t shot_world_x;
        uint8_t angle;
        uint8_t aggression;

        if (s_target_destroyed[i] || !target_enabled_for_level(i, target)) {
            continue;
        }

        aggression = target_aggression(target);
        if (aggression == 0u) {
            continue;
        }

        if (s_target_fire_cooldown[i] > 0u) {
            --s_target_fire_cooldown[i];
            continue;
        }

        top_y = (int16_t)(s_target_ground_y[i] - TARGETS_SPRITE_SIZE_PX + 1 +
                          target->y_offset_px + TARGET_VERTICAL_BIAS_PX);
        target_center_y = (int16_t)(top_y + (TARGETS_SPRITE_SIZE_PX / 2));
        dx = world_delta_to_screen_x(player_world_x, target->world_x);
        dy = (int16_t)(player_center_y - target_center_y);

        if (abs_i16(dy) > TARGET_MAX_VERTICAL_FIRE_PX ||
            target_fire_distance_metric(dx, dy) > fire_range_px) {
            continue;
        }

        shot_world_x = wrap_world_x((int16_t)target->world_x + (TARGETS_SPRITE_SIZE_PX / 2));
        angle = angle_from_delta(dx, dy);
        if (projectiles_spawn_enemy_shot(shot_world_x, target_center_y, angle, 0u)) {
            s_target_fire_cooldown[i] = target_fire_period_ticks(aggression, target->type);
        }
    }
}

void ground_targets_set_level(uint8_t level)
{
    if (level == 0u) {
        level = 1u;
    }

    s_level = level;
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
        s_target_fire_cooldown[i] = 0u;
    }

    s_tick_div = 0u;
    s_tick_count_10hz = 0u;

    for (; i < MAX_TARGETS; ++i) {
        s_target_fire_cooldown[i] = 0u;
    }

    for (i = 0; i < MAX_TARGETS; ++i) {
        sprite_mode5_set_target(i, -32, -32, 0, TARGETS_PALETTE_ADDR, false);
    }
}

void ground_targets_update(uint16_t camera_world_x)
{
    uint8_t i;

    if (++s_tick_div >= TARGETS_FPS_DIV) {
        s_tick_div = 0u;
        ++s_tick_count_10hz;
        update_target_fire();
    }

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

uint8_t ground_targets_get_minimap_buildings(uint16_t *world_x, int16_t *ground_y,
                                             uint8_t max_points)
{
    uint8_t count = 0u;

    for (uint8_t i = 0; i < s_target_count && count < max_points; ++i) {
        uint8_t type = s_targets[i].type;

        if (s_target_destroyed[i]) {
            continue;
        }

        if (type != TARGET_TYPE_BUILDING &&
            type != TARGET_TYPE_EXPLOSIVE_BUILDING &&
            type != TARGET_TYPE_FRIENDLY_BUILDING) {
            continue;
        }

        world_x[count] = s_targets[i].world_x;
        ground_y[count] = s_target_ground_y[i];
        ++count;
    }

    return count;
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

            if (s_targets[i].type == TARGET_TYPE_OX) {
                return GROUND_TARGET_HIT_NO_EXPLOSION;
            }
            if (s_targets[i].type == TARGET_TYPE_EXPLOSIVE_BUILDING) {
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

            if (s_targets[i].type == TARGET_TYPE_OX) {
                return GROUND_TARGET_HIT_NO_EXPLOSION;
            }
            if (s_targets[i].type == TARGET_TYPE_EXPLOSIVE_BUILDING) {
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
    int16_t plane_hit_top_y = (int16_t)(plane_top_y + PLANE_HITBOX_TOP_OFFSET_PX);
    int16_t plane_bot_y = (int16_t)(plane_hit_top_y + PLANE_HITBOX_HEIGHT_PX - 1);

    for (uint8_t i = 0; i < s_target_count; ++i) {
        int16_t dx;
        int16_t top_y;
        int16_t bot_y;
        int16_t center_y;
        int16_t hit_width = TARGETS_SPRITE_SIZE_PX;
        int16_t hit_height = TARGETS_SPRITE_SIZE_PX;

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
        if (s_targets[i].type == TARGET_TYPE_OX) {
            top_y = (int16_t)(top_y + TARGET_OX_HITBOX_TOP_OFFSET_PX);
            hit_width = TARGET_OX_HITBOX_WIDTH_PX;
            hit_height = TARGET_OX_HITBOX_HEIGHT_PX;
        }
        bot_y = (int16_t)(top_y + hit_height - 1);
        center_y = (int16_t)(top_y + (hit_height / 2));

        if (dx >= -(int16_t)PLANE_HITBOX_HALF_WIDTH_LEFT_PX &&
            dx <= (int16_t)(hit_width - 1 + PLANE_HITBOX_HALF_WIDTH_RIGHT_PX) &&
            plane_hit_top_y <= bot_y && plane_bot_y >= top_y) {
            s_target_destroyed[i] = true;

            if (hit_world_x != 0) {
                *hit_world_x = wrap_world_x((int16_t)s_targets[i].world_x + (TARGETS_SPRITE_SIZE_PX / 2));
            }
            if (hit_center_y != 0) {
                *hit_center_y = center_y;
            }
            if (score_delta != 0) {
                *score_delta = s_targets[i].score_delta;
            }
            if (s_targets[i].type == TARGET_TYPE_OX) {
                return GROUND_TARGET_HIT_NO_EXPLOSION;
            }
            if (s_targets[i].type == TARGET_TYPE_EXPLOSIVE_BUILDING) {
                return GROUND_TARGET_HIT_EXPLOSIVE;
            }
            return GROUND_TARGET_HIT_NORMAL;
        }
    }

    return GROUND_TARGET_HIT_NONE;
}