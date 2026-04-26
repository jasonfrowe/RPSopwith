#include <stdbool.h>
#include <stdint.h>

#include "ambient_birds.h"
#include "ambient_flocks.h"
#include "constants.h"
#include "enemy_planes.h"
#include "flight.h"
#include "ground_targets.h"
#include "projectiles.h"
#include "resources.h"
#include "sprite_mode5.h"
#include "text_mode1.h"

typedef struct projectile_s {
    uint16_t world_x;
    int16_t center_y;
    int8_t vx;
    int8_t vy;
    uint8_t frame_index;
    uint8_t owner;
    uint8_t life_ticks;
    uint8_t gravity_ticks;
    bool active;
    bool bomb;
    bool explosion;
    bool smoke;
} projectile_t;

enum {
    PROJ_OWNER_PLAYER = 0,
    PROJ_OWNER_ENEMY = 1,
    SHOT_FRAME = 0,
    SMOKE_FRAME = 1,
    BOMB_FRAME_BASE = 2,
    DEBRIS_FRAME_BASE = 21,
    DEBRIS_FRAME_COUNT = 8,
    EXPL_LIFE_TICKS = 3,
    EXPL_FALL_MIN_SPEED = 4,
    EXPL_NORMAL_SPEED = 4,
    EXPL_BIG_SPEED = 8,
    EXPL_STEP_BIG = 1,
    EXPL_STEP_NORMAL = 2,
    SHOT_SPEED = 10,
    SHOT_LIFE_TICKS = 10,
    SHOT_FIRE_COOLDOWN_TICKS = 1,
    BOMB_FIRE_COOLDOWN_TICKS = 10,
    BOMB_PLANE_HIT_HALF_SIZE = 6,
    SMOKE_SPAWN_COOLDOWN_TICKS = 2,
    SMOKE_LIFE_TICKS = 10,
    BOMB_GRAVITY_TICKS = 5,
    BOMB_MAX_FALL_SPEED = 10,
    PROJECTILE_TICK_DIV = 6
};

static projectile_t s_projectiles[MAX_COMBAT_PROJECTILES];
static uint8_t s_shot_cooldown;
static uint8_t s_bomb_spawn_cooldown;
static uint8_t s_tick_div;
static bool s_fire_latched;
static bool s_bomb_latched;
static uint8_t s_smoke_spawn_cooldown;
static uint16_t s_expl_seed = 0x79B1u;

static const int16_t s_sintab[16] = {
    0, 98, 181, 237, 256, 237, 181, 98,
    0, -98, -181, -237, -256, -237, -181, -98
};

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

static void hide_all_combat_slots(void)
{
    for (uint8_t i = 0; i < MAX_COMBAT_PROJECTILES; ++i) {
        sprite_mode5_set_projectile(i, -32, -32, 0, false);
    }
}

static int8_t projectile_dx_for_speed(uint8_t angle, uint8_t speed)
{
    return (int8_t)((speed * s_sintab[(angle + 4u) & 0x0Fu]) / 256);
}

static int8_t projectile_dy_for_speed(uint8_t angle, uint8_t speed)
{
    return (int8_t)(-((speed * s_sintab[angle & 0x0Fu]) / 256));
}

static uint8_t bomb_frame_for_velocity(int8_t vx, int8_t vy)
{
    uint8_t direction;

    if (vx == 0) {
        if (vy < 0) {
            direction = 6u;
        } else if (vy > 0) {
            direction = 2u;
        } else {
            direction = 0u;
        }
    } else if (vx > 0) {
        if (vy < 0) {
            direction = 7u;
        } else if (vy > 0) {
            direction = 1u;
        } else {
            direction = 0u;
        }
    } else {
        if (vy < 0) {
            direction = 5u;
        } else if (vy > 0) {
            direction = 3u;
        } else {
            direction = 4u;
        }
    }

    return (uint8_t)(BOMB_FRAME_BASE + direction);
}

static uint16_t next_rand16(void)
{
    s_expl_seed = (uint16_t)((s_expl_seed * 1103u) + 7491u);
    return s_expl_seed;
}

static bool bomb_hits_plane(uint16_t bomb_world_x, int16_t bomb_center_y)
{
    int16_t plane_dx;
    int16_t plane_dy;

    if (flight_is_crashed()) {
        return false;
    }

    plane_dx = world_delta_to_screen_x(bomb_world_x, flight_world_x_physics());
    plane_dy = (int16_t)(bomb_center_y -
                         (int16_t)(flight_plane_y_physics() + PLANE_HITBOX_CENTER_Y_OFFSET_PX));

    return (plane_dx >= -BOMB_PLANE_HIT_HALF_SIZE &&
            plane_dx <= BOMB_PLANE_HIT_HALF_SIZE &&
            plane_dy >= -BOMB_PLANE_HIT_HALF_SIZE &&
            plane_dy <= BOMB_PLANE_HIT_HALF_SIZE);
}

static void init_explosion_fragment(projectile_t *p, uint16_t world_x, int16_t center_y,
                                    int8_t base_vx, int8_t base_vy, uint8_t angle,
                                    uint8_t fragment_speed)
{
    p->vx = (int8_t)(projectile_dx_for_speed(angle, fragment_speed) + (base_vx >> 2));
    p->vy = (int8_t)(projectile_dy_for_speed(angle, fragment_speed) + (base_vy >> 2));
    p->world_x = wrap_world_x((int16_t)world_x + p->vx);
    p->center_y = (int16_t)(center_y + p->vy);
    p->frame_index = (uint8_t)(DEBRIS_FRAME_BASE + ((next_rand16() >> 5) & (DEBRIS_FRAME_COUNT - 1u)));
    p->owner = PROJ_OWNER_PLAYER;
    p->life_ticks = EXPL_LIFE_TICKS;
    p->gravity_ticks = 0u;
    p->bomb = false;
    p->explosion = true;
    p->smoke = false;
    p->active = true;
}

static void spawn_explosion_from(projectile_t *source, uint16_t center_world_x, int16_t center_y,
                                 bool big_explosion)
{
    int8_t base_vx = source->vx;
    int8_t base_vy = source->vy;
    uint8_t step = big_explosion ? EXPL_STEP_BIG : EXPL_STEP_NORMAL;
    uint8_t fragment_speed = big_explosion ? EXPL_BIG_SPEED : EXPL_NORMAL_SPEED;
    uint8_t angle;
    bool first = true;

    if (!big_explosion && (next_rand16() & 0x07u) == 7u) {
        fragment_speed = (uint8_t)(EXPL_NORMAL_SPEED * 2u);
    }

    for (angle = 1u; angle <= 15u; angle = (uint8_t)(angle + step)) {
        projectile_t *p = 0;

        if (first) {
            p = source;
            first = false;
        } else {
            for (uint8_t i = 0; i < MAX_COMBAT_PROJECTILES; ++i) {
                if (!s_projectiles[i].active) {
                    p = &s_projectiles[i];
                    break;
                }
            }
            if (p == 0) {
                break;
            }
        }

        init_explosion_fragment(p, center_world_x, center_y, base_vx, base_vy, angle, fragment_speed);
    }
}

static bool spawn_shot(void)
{
    uint8_t pitch = flight_plane_pitch();
    uint8_t shot_speed = (uint8_t)(flight_plane_speed() + SHOT_SPEED);
    uint16_t plane_world_x = flight_world_x();
    int16_t plane_center_y = (int16_t)(flight_plane_y() + (PLAYER_SPRITE_SIZE_PX / 2) - 1);

    for (uint8_t i = 0; i < MAX_COMBAT_PROJECTILES; ++i) {
        projectile_t *p = &s_projectiles[i];

        if (p->active) {
            continue;
        }

        if (!resources_try_consume_bullet()) {
            return false;
        }

        p->world_x = plane_world_x;
        p->center_y = plane_center_y;
        p->vx = projectile_dx_for_speed(pitch, shot_speed);
        p->vy = projectile_dy_for_speed(pitch, shot_speed);
        p->frame_index = SHOT_FRAME;
        p->owner = PROJ_OWNER_PLAYER;
        p->bomb = false;
        p->explosion = false;
        p->smoke = false;
        p->life_ticks = SHOT_LIFE_TICKS;
        p->gravity_ticks = 0u;
        p->active = true;
        return true;
    }

    return false;
}

static bool spawn_bomb(void)
{
    uint8_t pitch = flight_plane_pitch();
    uint8_t plane_speed = flight_plane_speed();
    uint8_t release_angle = flight_plane_orient() ?
        (uint8_t)((pitch + 4u) & 0x0Fu) :
        (uint8_t)((pitch + 12u) & 0x0Fu);
    uint16_t plane_world_x = flight_world_x();
    int16_t plane_center_y = (int16_t)(flight_plane_y() + (PLAYER_SPRITE_SIZE_PX / 2));
    int16_t release_dx = (int16_t)((10 * s_sintab[(release_angle + 4u) & 0x0Fu]) / 256);
    int16_t release_dy = (int16_t)(-((10 * s_sintab[release_angle]) / 256));

    for (uint8_t i = 0; i < MAX_COMBAT_PROJECTILES; ++i) {
        projectile_t *p = &s_projectiles[i];

        if (p->active) {
            continue;
        }

        if (!resources_try_consume_bomb()) {
            return false;
        }

        p->world_x = wrap_world_x((int16_t)plane_world_x + release_dx);
        p->center_y = (int16_t)(plane_center_y + release_dy);
        p->vx = projectile_dx_for_speed(pitch, plane_speed);
        p->vy = projectile_dy_for_speed(pitch, plane_speed);
        p->frame_index = bomb_frame_for_velocity(p->vx, p->vy);
        p->owner = PROJ_OWNER_PLAYER;
        p->bomb = true;
        p->explosion = false;
        p->smoke = false;
        p->life_ticks = 0u;
        p->gravity_ticks = BOMB_GRAVITY_TICKS;
        p->active = true;
        return true;
    }

    return false;
}

static void spawn_smoke(void)
{
    uint8_t pitch = flight_plane_pitch();
    uint8_t speed = flight_plane_speed();
    uint16_t plane_world_x = flight_world_x_physics();
    int16_t plane_center_y = (int16_t)(flight_plane_y_physics() + (PLAYER_SPRITE_SIZE_PX / 2));
    int8_t plane_vx = projectile_dx_for_speed(pitch, speed);
    int8_t plane_vy = projectile_dy_for_speed(pitch, speed);

    for (uint8_t i = 0; i < MAX_COMBAT_PROJECTILES; ++i) {
        projectile_t *p = &s_projectiles[i];

        if (p->active) {
            continue;
        }

        p->world_x = wrap_world_x((int16_t)plane_world_x - (int16_t)(plane_vx * 2));
        p->center_y = (int16_t)(plane_center_y - (plane_vy * 2));
        p->vx = (int8_t)(plane_vx / 2);
        p->vy = (int8_t)(plane_vy / 2 - 1);
        p->frame_index = SMOKE_FRAME;
        p->owner = PROJ_OWNER_PLAYER;
        p->bomb = false;
        p->explosion = false;
        p->smoke = true;
        p->life_ticks = SMOKE_LIFE_TICKS;
        p->gravity_ticks = 0u;
        p->active = true;
        break;
    }
}

bool projectiles_spawn_smoke_trail(uint16_t world_x, int16_t center_y,
                                   int8_t vx, int8_t vy)
{
    for (uint8_t i = 0; i < MAX_COMBAT_PROJECTILES; ++i) {
        projectile_t *p = &s_projectiles[i];

        if (p->active) {
            continue;
        }

        p->world_x = wrap_world_x((int16_t)world_x - (int16_t)(vx * 2));
        p->center_y = (int16_t)(center_y - (vy * 2));
        p->vx = (int8_t)(vx / 2);
        p->vy = (int8_t)(vy / 2 - 1);
        p->frame_index = SMOKE_FRAME;
        p->owner = PROJ_OWNER_PLAYER;
        p->bomb = false;
        p->explosion = false;
        p->smoke = true;
        p->life_ticks = SMOKE_LIFE_TICKS;
        p->gravity_ticks = 0u;
        p->active = true;
        return true;
    }

    return false;
}

static bool projectile_update_explosion(projectile_t *p)
{
    int16_t terrain_y;
    int16_t plane_dx;
    int16_t plane_dy;

    p->world_x = wrap_world_x((int16_t)p->world_x + p->vx);
    p->center_y = (int16_t)(p->center_y + p->vy);

    if (!flight_is_crashed()) {
        plane_dx = world_delta_to_screen_x(p->world_x, flight_world_x_physics());
        plane_dy = (int16_t)(p->center_y -
                             (int16_t)(flight_plane_y_physics() + PLANE_HITBOX_CENTER_Y_OFFSET_PX));
        if (plane_dx >= -(int16_t)PLANE_HITBOX_HALF_WIDTH_LEFT_PX &&
            plane_dx <= (int16_t)PLANE_HITBOX_HALF_WIDTH_RIGHT_PX &&
            plane_dy >= -(int16_t)PLANE_HITBOX_HALF_HEIGHT_UP_PX &&
            plane_dy <= (int16_t)PLANE_HITBOX_HALF_HEIGHT_DOWN_PX) {
            flight_apply_debris_hit();
            p->active = false;
            return true;
        }
    }

    if (enemy_planes_check_fragment_hit(p->world_x, p->center_y, 8u, 0, 0, 0)) {
        p->active = false;
        return true;
    }

    if (p->life_ticks > 0u) {
        --p->life_ticks;
    }
    if (p->life_ticks == 0u) {
        if (p->vy < 0) {
            if (p->vx < 0) {
                ++p->vx;
            } else if (p->vx > 0) {
                --p->vx;
            }
        }
        if (p->vy < EXPL_FALL_MIN_SPEED) {
            ++p->vy;
        }
        p->life_ticks = EXPL_LIFE_TICKS;
    }

    terrain_y = flight_terrain_y_at(wrap_world_x((int16_t)p->world_x + 4));
    if (p->center_y >= (terrain_y + (PROJECTILE_SPRITE_SIZE_PX / 2)) ||
        p->center_y < -PROJECTILE_SPRITE_SIZE_PX ||
        p->center_y >= (SCREEN_HEIGHT + PROJECTILE_SPRITE_SIZE_PX)) {
        p->active = false;
    }

    return true;
}

static bool projectile_update_smoke(projectile_t *p)
{
    p->world_x = wrap_world_x((int16_t)p->world_x + p->vx);
    p->center_y = (int16_t)(p->center_y + p->vy);

    if (p->life_ticks > 0u) {
        --p->life_ticks;
    }
    if (p->life_ticks == 0u) {
        p->active = false;
    }

    return true;
}

static void projectile_update_bomb(projectile_t *p, uint16_t prev_world_x, int16_t prev_center_y)
{
    ground_target_hit_type_t hit_target;
    bool hit_enemy;
    bool enemy_big_explosion = false;
    bool hit_ground;
    uint16_t impact_world_x;
    uint16_t enemy_hit_world_x = 0u;
    int16_t enemy_hit_center_y = 0;
    int16_t enemy_score_delta = 0;
    uint16_t hit_world_x = 0u;
    int16_t hit_center_y = 0;
    int16_t score_delta = 0;
    int16_t terrain_y;
    int16_t mid_center_y;
    int16_t half_dx;

    p->center_y = (int16_t)(p->center_y + p->vy);
    if (p->gravity_ticks > 0u) {
        --p->gravity_ticks;
    }
    if (p->gravity_ticks == 0u) {
        if (p->vy < 0) {
            if (p->vx < 0) {
                ++p->vx;
            } else if (p->vx > 0) {
                --p->vx;
            }
        }
        if (p->vy < BOMB_MAX_FALL_SPEED) {
            ++p->vy;
        }
        p->gravity_ticks = BOMB_GRAVITY_TICKS;
    }
    p->frame_index = bomb_frame_for_velocity(p->vx, p->vy);

    half_dx = (int16_t)(world_delta_to_screen_x(p->world_x, prev_world_x) / 2);
    mid_center_y = (int16_t)(prev_center_y + ((p->center_y - prev_center_y) / 2));

    if (bomb_hits_plane(p->world_x, p->center_y) ||
        bomb_hits_plane(prev_world_x, prev_center_y) ||
        bomb_hits_plane(wrap_world_x((int16_t)prev_world_x + half_dx), mid_center_y)) {
        flight_apply_bomb_hit(p->world_x, p->center_y);
        spawn_explosion_from(p, p->world_x, p->center_y, false);
        return;
    }

    (void)ambient_flocks_scatter_at(p->world_x, p->center_y,
                                    (uint8_t)(PROJECTILE_SPRITE_SIZE_PX / 2));

    hit_enemy = enemy_planes_check_shot_hit(p->world_x, p->center_y,
                                            &enemy_hit_world_x, &enemy_hit_center_y,
                                            &enemy_score_delta, &enemy_big_explosion);
    if (!hit_enemy) {
        hit_enemy = enemy_planes_check_shot_hit(prev_world_x, prev_center_y,
                                                &enemy_hit_world_x, &enemy_hit_center_y,
                                                &enemy_score_delta, &enemy_big_explosion);
    }
    if (!hit_enemy) {
        hit_enemy = enemy_planes_check_shot_hit(wrap_world_x((int16_t)prev_world_x + half_dx),
                                                mid_center_y,
                                                &enemy_hit_world_x, &enemy_hit_center_y,
                                                &enemy_score_delta, &enemy_big_explosion);
    }
    if (hit_enemy) {
        spawn_explosion_from(p, enemy_hit_world_x, enemy_hit_center_y, enemy_big_explosion);
        text_mode1_add_score(enemy_score_delta);
        return;
    }

    impact_world_x = wrap_world_x((int16_t)p->world_x + 4);
    terrain_y = flight_terrain_y_at(impact_world_x);
    hit_target = ground_targets_check_hit(p->world_x, p->center_y,
                                          &hit_world_x, &hit_center_y,
                                          &score_delta);
    hit_ground = p->center_y >= (terrain_y - 8);

    if (hit_target == GROUND_TARGET_HIT_EXPLOSIVE) {
        spawn_explosion_from(p, hit_world_x, hit_center_y, true);
        text_mode1_add_score(score_delta);
    } else if (hit_target == GROUND_TARGET_HIT_NO_EXPLOSION) {
        p->active = false;
        text_mode1_add_score(score_delta);
    } else if (hit_target == GROUND_TARGET_HIT_NORMAL || hit_ground) {
        if (hit_target == GROUND_TARGET_HIT_NORMAL) {
            p->vx = 0;
            p->vy = 0;
            spawn_explosion_from(p, hit_world_x, hit_center_y, false);
            text_mode1_add_score(score_delta);
        } else {
            flight_apply_bomb_crater(impact_world_x);
            spawn_explosion_from(p,
                                 p->world_x,
                                 (int16_t)(terrain_y - (PROJECTILE_SPRITE_SIZE_PX / 2)),
                                 false);
        }
    } else if (p->center_y >= (SCREEN_HEIGHT + PROJECTILE_SPRITE_SIZE_PX)) {
        p->active = false;
    }
}

static void projectile_update_shot(projectile_t *p)
{
    ground_target_hit_type_t hit_target;
    bool hit_enemy;
    bool big_explosion = false;
    uint16_t hit_world_x = 0u;
    int16_t hit_center_y = 0;
    int16_t score_delta = 0;
    int16_t plane_dx;
    int16_t plane_dy;
    int16_t terrain_y;

    if (p->life_ticks > 0u) {
        --p->life_ticks;
    }
    if (p->life_ticks == 0u) {
        p->active = false;
        return;
    }

    p->center_y = (int16_t)(p->center_y + p->vy);

    if (ambient_birds_check_projectile_hit(p->world_x, p->center_y)) {
        p->active = false;
        return;
    }

    if (p->owner == PROJ_OWNER_ENEMY && !flight_is_crashed()) {
        plane_dx = world_delta_to_screen_x(p->world_x, flight_world_x_physics());
        plane_dy = (int16_t)(p->center_y -
                             (int16_t)(flight_plane_y_physics() + PLANE_HITBOX_CENTER_Y_OFFSET_PX));
        if (plane_dx >= -(int16_t)PLANE_HITBOX_HALF_WIDTH_LEFT_PX &&
            plane_dx <= (int16_t)PLANE_HITBOX_HALF_WIDTH_RIGHT_PX &&
            plane_dy >= -(int16_t)PLANE_HITBOX_HALF_HEIGHT_UP_PX &&
            plane_dy <= (int16_t)PLANE_HITBOX_HALF_HEIGHT_DOWN_PX) {
            flight_apply_debris_hit();
            p->active = false;
            return;
        }
    }

    terrain_y = flight_terrain_y_at(wrap_world_x((int16_t)p->world_x + 4));

    if (p->owner == PROJ_OWNER_PLAYER) {
        hit_enemy = enemy_planes_check_shot_hit(p->world_x, p->center_y,
                                                &hit_world_x, &hit_center_y,
                                                &score_delta, &big_explosion);
        if (hit_enemy) {
            p->active = false;
            text_mode1_add_score(score_delta);
            return;
        }

        hit_target = ground_targets_check_shot_hit(p->world_x, p->center_y,
                                                   &hit_world_x, &hit_center_y,
                                                   &score_delta);
    } else {
        hit_target = GROUND_TARGET_HIT_NONE;
    }

    if (hit_target == GROUND_TARGET_HIT_EXPLOSIVE) {
        spawn_explosion_from(p, hit_world_x, hit_center_y, true);
        text_mode1_add_score(score_delta);
    } else if (hit_target == GROUND_TARGET_HIT_NO_EXPLOSION) {
        p->active = false;
        text_mode1_add_score(score_delta);
    } else if (hit_target == GROUND_TARGET_HIT_NORMAL) {
        p->vx = 0;
        p->vy = 0;
        spawn_explosion_from(p, hit_world_x, hit_center_y, false);
        text_mode1_add_score(score_delta);
    } else if (p->center_y >= terrain_y ||
               p->center_y < -PROJECTILE_SPRITE_SIZE_PX ||
               p->center_y >= (SCREEN_HEIGHT + PROJECTILE_SPRITE_SIZE_PX)) {
        p->active = false;
    }
}

bool projectiles_spawn_enemy_shot(uint16_t world_x, int16_t center_y,
                                  uint8_t angle, uint8_t speed)
{
    uint8_t shot_speed = (uint8_t)(speed + SHOT_SPEED);

    for (uint8_t i = 0; i < MAX_COMBAT_PROJECTILES; ++i) {
        projectile_t *p = &s_projectiles[i];

        if (p->active) {
            continue;
        }

        p->world_x = world_x;
        p->center_y = center_y;
        p->vx = projectile_dx_for_speed(angle, shot_speed);
        p->vy = projectile_dy_for_speed(angle, shot_speed);
        p->frame_index = SHOT_FRAME;
        p->owner = PROJ_OWNER_ENEMY;
        p->bomb = false;
        p->explosion = false;
        p->smoke = false;
        p->life_ticks = SHOT_LIFE_TICKS;
        p->gravity_ticks = 0u;
        p->active = true;
        return true;
    }

    return false;
}

static void update_resource_tick(void)
{
    bool out_of_fuel;

    out_of_fuel = resources_tick_10hz(flight_is_at_home_base(),
                                      flight_is_airborne(),
                                      flight_is_crashed(),
                                      flight_is_falling(),
                                      flight_plane_speed());
    if (out_of_fuel && flight_is_airborne() && !flight_is_crashed() && !flight_is_falling()) {
        flight_apply_debris_hit();
    }
}

static void projectiles_tick_10hz(void)
{
    if (s_shot_cooldown > 0u) {
        --s_shot_cooldown;
    }
    if (s_bomb_spawn_cooldown > 0u) {
        --s_bomb_spawn_cooldown;
    }
    if (s_smoke_spawn_cooldown > 0u) {
        --s_smoke_spawn_cooldown;
    }

    update_resource_tick();

    if ((flight_is_wounded() || flight_is_falling()) && s_smoke_spawn_cooldown == 0u) {
        spawn_smoke();
        s_smoke_spawn_cooldown = SMOKE_SPAWN_COOLDOWN_TICKS;
    }

    for (uint8_t i = 0; i < MAX_COMBAT_PROJECTILES; ++i) {
        projectile_t *p = &s_projectiles[i];

        if (!p->active) {
            continue;
        }

        if (p->explosion) {
            if (projectile_update_explosion(p)) {
                continue;
            }
        }

        if (p->smoke) {
            if (projectile_update_smoke(p)) {
                continue;
            }
        }

        {
            uint16_t prev_world_x = p->world_x;
            int16_t prev_center_y = p->center_y;

            p->world_x = wrap_world_x((int16_t)p->world_x + p->vx);

            if (p->bomb) {
                projectile_update_bomb(p, prev_world_x, prev_center_y);
            } else {
                projectile_update_shot(p);
            }
        }
    }
}

static void projectiles_render(uint16_t camera_world_x)
{
    for (uint8_t i = 0; i < MAX_COMBAT_PROJECTILES; ++i) {
        projectile_t *p = &s_projectiles[i];
        uint16_t render_world_x;
        int16_t render_center_y;
        int16_t dx;
        int16_t screen_center_x;
        int16_t sprite_x;
        int16_t sprite_y;
        bool visible;

        if (!p->active) {
            sprite_mode5_set_projectile(i, -32, -32, 0, false);
            continue;
        }

        render_world_x = wrap_world_x((int16_t)p->world_x +
                                      (int16_t)(((int16_t)p->vx * s_tick_div) / PROJECTILE_TICK_DIV));
        render_center_y = (int16_t)(p->center_y +
                                    (int16_t)(((int16_t)p->vy * s_tick_div) / PROJECTILE_TICK_DIV));

        dx = world_delta_to_screen_x(render_world_x, camera_world_x);
        screen_center_x = (int16_t)((SCREEN_WIDTH / 2) + dx);
        sprite_x = (int16_t)(screen_center_x - (PROJECTILE_SPRITE_SIZE_PX / 2));
        sprite_y = (int16_t)(render_center_y - (PROJECTILE_SPRITE_SIZE_PX / 2));
        visible = (sprite_x > -PROJECTILE_SPRITE_SIZE_PX) &&
                  (sprite_x < SCREEN_WIDTH) &&
                  (sprite_y > -PROJECTILE_SPRITE_SIZE_PX) &&
                  (sprite_y < SCREEN_HEIGHT);

        sprite_mode5_set_projectile(i, sprite_x, sprite_y, p->frame_index, visible);
    }
}

void projectiles_init(void)
{
    for (uint8_t i = 0; i < MAX_COMBAT_PROJECTILES; ++i) {
        s_projectiles[i].active = false;
    }

    s_shot_cooldown = 0u;
    s_bomb_spawn_cooldown = 0u;
    s_tick_div = 0u;
    s_fire_latched = false;
    s_bomb_latched = false;
    s_smoke_spawn_cooldown = 0u;
    hide_all_combat_slots();
}

void projectiles_spawn_crash_explosion(uint16_t world_x, int16_t center_y, bool big_explosion)
{
    uint8_t angle;
    uint8_t step = big_explosion ? EXPL_STEP_BIG : EXPL_STEP_NORMAL;
    uint8_t speed = big_explosion ? EXPL_BIG_SPEED : EXPL_NORMAL_SPEED;

    for (angle = 1u; angle <= 15u; angle = (uint8_t)(angle + step)) {
        projectile_t *p = 0;

        for (uint8_t i = 0; i < MAX_COMBAT_PROJECTILES; ++i) {
            if (!s_projectiles[i].active) {
                p = &s_projectiles[i];
                break;
            }
        }
        if (p == 0) {
            break;
        }

        init_explosion_fragment(p, world_x, center_y, 0, 0, angle, speed);
    }
}

void projectiles_update(uint16_t camera_world_x, const input_actions_t *actions)
{
    bool fire_held = false;
    bool bomb_held = false;
    bool player_can_fire;
    uint16_t expl_world_x;
    int16_t expl_center_y;
    bool apply_crater;
    bool big_explosion;

    player_can_fire = !flight_is_crashed() && !flight_is_falling();

    if (actions != 0 && player_can_fire) {
        fire_held = actions->fire != 0;
        bomb_held = actions->bomb != 0;
    }

    if (fire_held) {
        s_fire_latched = true;
    }
    if (bomb_held) {
        s_bomb_latched = true;
    }

    if (!player_can_fire) {
        s_fire_latched = false;
        s_bomb_latched = false;
    }

    while (flight_consume_plane_explosion(&expl_world_x, &expl_center_y, &apply_crater, &big_explosion)) {
        projectiles_spawn_crash_explosion(expl_world_x, expl_center_y, big_explosion);
        if (apply_crater) {
            flight_apply_bomb_crater(expl_world_x);
        }
    }

    if (++s_tick_div >= PROJECTILE_TICK_DIV) {
        s_tick_div = 0u;
        projectiles_tick_10hz();

        if (s_fire_latched && s_shot_cooldown == 0u && player_can_fire) {
            if (spawn_shot()) {
                s_shot_cooldown = SHOT_FIRE_COOLDOWN_TICKS;
            }
        }
        if (s_bomb_latched && s_bomb_spawn_cooldown == 0u && player_can_fire) {
            if (spawn_bomb()) {
                s_bomb_spawn_cooldown = BOMB_FIRE_COOLDOWN_TICKS;
            }
        }

        s_fire_latched = false;
        s_bomb_latched = false;
    }

    projectiles_render(camera_world_x);
}
