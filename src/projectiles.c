#include <stdbool.h>
#include <stdint.h>

#include "constants.h"
#include "flight.h"
#include "ground_targets.h"
#include "projectiles.h"
#include "sprite_mode5.h"

typedef struct projectile_s {
    uint16_t world_x;
    int16_t center_y;
    int8_t vx;
    int8_t vy;
    uint8_t frame_index;
    uint8_t life_ticks;
    uint8_t gravity_ticks;
    bool active;
    bool bomb;
    bool explosion;
} projectile_t;

enum {
    SHOT_FRAME = 0,
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
static uint16_t s_expl_seed = 0x79B1u;

static const int16_t s_sintab[16] = {
    0, 98, 181, 237, 256, 237, 181, 98,
    0, -98, -181, -237, -256, -237, -181, -98
};

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

static uint16_t wrap_world_x(int32_t x)
{
    int32_t world_width = (int32_t)(GROUND_WIDTH * 8);
    int32_t wrapped = x % world_width;

    if (wrapped < 0) {
        wrapped += world_width;
    }

    return (uint16_t)wrapped;
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

static void init_explosion_fragment(projectile_t *p, uint16_t world_x, int16_t center_y,
                                    int8_t base_vx, int8_t base_vy, uint8_t angle,
                                    uint8_t fragment_speed)
{
    p->vx = (int8_t)(projectile_dx_for_speed(angle, fragment_speed) + (base_vx >> 2));
    p->vy = (int8_t)(projectile_dy_for_speed(angle, fragment_speed) + (base_vy >> 2));
    // Sopwith initexpl places each fragment immediately offset by its initial velocity,
    // which creates the instant circular pop instead of a tight center cluster.
    p->world_x = wrap_world_x((int32_t)world_x + p->vx);
    p->center_y = (int16_t)(center_y + p->vy);
    p->frame_index = (uint8_t)(DEBRIS_FRAME_BASE + ((next_rand16() >> 5) & 0x07u));
    p->life_ticks = EXPL_LIFE_TICKS;
    p->gravity_ticks = 0;
    p->bomb = false;
    p->explosion = true;
    p->active = true;
}

static void spawn_explosion_from(projectile_t *source, uint16_t center_world_x, int16_t center_y)
{
    uint16_t world_x = center_world_x;
    int8_t base_vx = source->vx;
    int8_t base_vy = source->vy;
    uint8_t step = EXPL_STEP_NORMAL;
    uint8_t fragment_speed = EXPL_NORMAL_SPEED;
    uint8_t angle;
    bool first = true;

    if ((next_rand16() & 0x07u) == 7u) {
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

        init_explosion_fragment(p, world_x, center_y, base_vx, base_vy, angle, fragment_speed);
    }
}

static void spawn_explosive_target_explosion(projectile_t *source, uint16_t center_world_x, int16_t center_y)
{
    uint16_t world_x = center_world_x;
    int8_t base_vx = 0;
    int8_t base_vy = 0;
    uint8_t angle;
    bool first = true;

    for (angle = 1u; angle <= 15u; angle = (uint8_t)(angle + EXPL_STEP_BIG)) {
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

        init_explosion_fragment(p, world_x, center_y, base_vx, base_vy, angle, EXPL_BIG_SPEED);
    }
}

static void spawn_shot(void)
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

        p->world_x = plane_world_x;
        p->center_y = plane_center_y;
        p->vx = projectile_dx_for_speed(pitch, shot_speed);
        p->vy = projectile_dy_for_speed(pitch, shot_speed);

        p->frame_index = SHOT_FRAME;
        p->bomb = false;
        p->explosion = false;
        p->life_ticks = SHOT_LIFE_TICKS;
        p->gravity_ticks = 0;
        p->active = true;
        break;
    }
}

static void spawn_bomb(void)
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

        p->world_x = wrap_world_x((int32_t)plane_world_x + release_dx);
        p->center_y = (int16_t)(plane_center_y + release_dy);
        p->vx = projectile_dx_for_speed(pitch, plane_speed);
        p->vy = projectile_dy_for_speed(pitch, plane_speed);
        p->frame_index = bomb_frame_for_velocity(p->vx, p->vy);
        p->bomb = true;
        p->explosion = false;
        p->life_ticks = 0;
        p->gravity_ticks = BOMB_GRAVITY_TICKS;
        p->active = true;
        break;
    }
}

void projectiles_init(void)
{
    for (uint8_t i = 0; i < MAX_COMBAT_PROJECTILES; ++i) {
        s_projectiles[i].active = false;
    }

    s_shot_cooldown = 0;
    s_bomb_spawn_cooldown = 0;
    s_tick_div = 0;
    s_fire_latched = false;
    s_bomb_latched = false;
    hide_all_combat_slots();
}

void projectiles_update(uint16_t camera_world_x, const input_actions_t *actions)
{
    bool fire_held = false;
    bool bomb_held = false;

    if (actions != 0) {
        fire_held = actions->fire != 0;
        bomb_held = actions->bomb != 0;
    }

    if (fire_held) {
        s_fire_latched = true;
    }
    if (bomb_held) {
        s_bomb_latched = true;
    }

    if (++s_tick_div >= PROJECTILE_TICK_DIV) {
        s_tick_div = 0;

        if (s_shot_cooldown > 0) {
            --s_shot_cooldown;
        }
        if (s_bomb_spawn_cooldown > 0) {
            --s_bomb_spawn_cooldown;
        }

        for (uint8_t i = 0; i < MAX_COMBAT_PROJECTILES; ++i) {
            projectile_t *p = &s_projectiles[i];

            if (!p->active) {
                continue;
            }

            if (p->explosion) {
                int16_t terrain_y;

                p->world_x = wrap_world_x((int32_t)p->world_x + p->vx);
                p->center_y = (int16_t)(p->center_y + p->vy);

                if (p->life_ticks > 0) {
                    --p->life_ticks;
                }
                if (p->life_ticks == 0) {
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

                terrain_y = flight_terrain_y_at(wrap_world_x((int32_t)p->world_x + 4));
                if (p->center_y >= (terrain_y + (PROJECTILE_SPRITE_SIZE_PX / 2)) ||
                    p->center_y < -PROJECTILE_SPRITE_SIZE_PX ||
                    p->center_y >= (SCREEN_HEIGHT + PROJECTILE_SPRITE_SIZE_PX)) {
                    p->active = false;
                }
                continue;
            }

            p->world_x = wrap_world_x((int32_t)p->world_x + p->vx);
            if (p->bomb) {
                ground_target_hit_type_t hit_target;
                bool hit_ground;
                uint16_t impact_world_x;
                uint16_t hit_world_x = 0;
                int16_t hit_center_y = 0;

                p->center_y = (int16_t)(p->center_y + p->vy);
                if (p->gravity_ticks > 0) {
                    --p->gravity_ticks;
                }
                if (p->gravity_ticks == 0) {
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

                impact_world_x = wrap_world_x((int32_t)p->world_x + 4);
                int16_t terrain_y = flight_terrain_y_at(impact_world_x);
                hit_target = ground_targets_check_hit(p->world_x, p->center_y,
                                                      &hit_world_x, &hit_center_y);
                hit_ground = p->center_y >= (terrain_y - 8);

                if (hit_target == GROUND_TARGET_HIT_EXPLOSIVE) {
                    spawn_explosive_target_explosion(p, hit_world_x, hit_center_y);
                } else if (hit_target == GROUND_TARGET_HIT_NO_EXPLOSION) {
                    p->active = false;
                } else if (hit_target == GROUND_TARGET_HIT_NORMAL || hit_ground) {
                    if (hit_target == GROUND_TARGET_HIT_NORMAL) {
                        p->vx = 0;
                        p->vy = 0;
                        spawn_explosion_from(p, hit_world_x, hit_center_y);
                    } else {
                        flight_apply_bomb_crater(impact_world_x);
                        spawn_explosion_from(
                            p,
                            p->world_x,
                            (int16_t)(terrain_y - (PROJECTILE_SPRITE_SIZE_PX / 2)));
                    }
                } else if (p->center_y >= (SCREEN_HEIGHT + PROJECTILE_SPRITE_SIZE_PX)) {
                    p->active = false;
                }
            } else {
                ground_target_hit_type_t hit_target;
                bool hit_ground;
                uint16_t hit_world_x = 0;
                int16_t hit_center_y = 0;

                if (p->life_ticks > 0) {
                    --p->life_ticks;
                }
                if (p->life_ticks == 0) {
                    p->active = false;
                    continue;
                }

                p->center_y = (int16_t)(p->center_y + p->vy);

                int16_t terrain_y = flight_terrain_y_at(wrap_world_x((int32_t)p->world_x + 4));
                hit_target = ground_targets_check_shot_hit(p->world_x, p->center_y,
                                                           &hit_world_x, &hit_center_y);
                hit_ground = p->center_y >= terrain_y;

                if (hit_target == GROUND_TARGET_HIT_EXPLOSIVE) {
                    spawn_explosive_target_explosion(p, hit_world_x, hit_center_y);
                } else if (hit_target == GROUND_TARGET_HIT_NO_EXPLOSION) {
                    p->active = false;
                } else if (hit_target == GROUND_TARGET_HIT_NORMAL) {
                    p->vx = 0;
                    p->vy = 0;
                    spawn_explosion_from(p, hit_world_x, hit_center_y);
                } else if (hit_ground) {
                    p->active = false;
                } else {
                    if (p->center_y < -PROJECTILE_SPRITE_SIZE_PX ||
                        p->center_y >= (SCREEN_HEIGHT + PROJECTILE_SPRITE_SIZE_PX)) {
                        p->active = false;
                    }
                }
            }
        }

        if (s_fire_latched && s_shot_cooldown == 0 && !flight_is_crashed()) {
            spawn_shot();
            s_shot_cooldown = SHOT_FIRE_COOLDOWN_TICKS;
        }
        if (s_bomb_latched && s_bomb_spawn_cooldown == 0 && !flight_is_crashed()) {
            spawn_bomb();
            s_bomb_spawn_cooldown = BOMB_FIRE_COOLDOWN_TICKS;
        }

        s_fire_latched = false;
        s_bomb_latched = false;
    }

    for (uint8_t i = 0; i < MAX_COMBAT_PROJECTILES; ++i) {
        projectile_t *p = &s_projectiles[i];

        if (!p->active) {
            sprite_mode5_set_projectile(i, -32, -32, 0, false);
            continue;
        }

        uint16_t render_world_x = wrap_world_x(
            (int32_t)p->world_x + (int32_t)p->vx * s_tick_div / PROJECTILE_TICK_DIV);
        int16_t render_center_y = (int16_t)(p->center_y +
            (int16_t)p->vy * s_tick_div / PROJECTILE_TICK_DIV);

        int16_t dx = world_delta_to_screen_x(render_world_x, camera_world_x);
        int16_t screen_center_x = (int16_t)((SCREEN_WIDTH / 2) + dx);
        int16_t sprite_x = (int16_t)(screen_center_x - (PROJECTILE_SPRITE_SIZE_PX / 2));
        int16_t sprite_y = (int16_t)(render_center_y - (PROJECTILE_SPRITE_SIZE_PX / 2));
        bool visible = (sprite_x > -PROJECTILE_SPRITE_SIZE_PX) && (sprite_x < SCREEN_WIDTH) &&
                       (sprite_y > -PROJECTILE_SPRITE_SIZE_PX) && (sprite_y < SCREEN_HEIGHT);

        sprite_mode5_set_projectile(i, sprite_x, sprite_y, p->frame_index, visible);
    }

}

void projectiles_spawn_crash_explosion(uint16_t world_x, int16_t center_y)
{
    uint8_t angle;

    for (angle = 1u; angle <= 15u; angle = (uint8_t)(angle + EXPL_STEP_NORMAL)) {
        projectile_t *p = 0;
        uint8_t i;

        for (i = 0; i < MAX_COMBAT_PROJECTILES; ++i) {
            if (!s_projectiles[i].active) {
                p = &s_projectiles[i];
                break;
            }
        }
        if (p == 0) {
            break;
        }

        init_explosion_fragment(p, world_x, center_y, 0, 0, angle, EXPL_NORMAL_SPEED);
    }
}