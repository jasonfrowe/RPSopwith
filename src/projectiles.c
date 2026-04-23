#include <stdbool.h>
#include <stdint.h>

#include "constants.h"
#include "flight.h"
#include "projectiles.h"
#include "sprite_mode5.h"

typedef struct projectile_s {
    uint16_t world_x;
    int16_t screen_y;
    int8_t vx;
    int8_t vy;
    uint8_t frame_index;
    bool bomb;
    uint8_t life_ticks;
    bool active;
} projectile_t;

enum {
    SHOT_FRAME = 0,
    BOMB_FRAME = 2,
    SHOT_SPAWN_COOLDOWN_TICKS = 2,
    BOMB_SPAWN_COOLDOWN_TICKS = 5,
    SHOT_MAX_LIFE_TICKS = 24,
    BOMB_MAX_LIFE_TICKS = 36,
    BOMB_MAX_FALL_SPEED = 5,
    PROJECTILE_TICK_DIV = 6
};

static projectile_t s_projectiles[MAX_COMBAT_PROJECTILES];
static uint8_t s_spawn_cooldown;
static uint8_t s_bomb_spawn_cooldown;
static uint8_t s_tick_div;

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

static void spawn_shot(void)
{
    uint8_t pitch = flight_plane_pitch();
    int16_t nose_dx = (int16_t)(s_sintab[(pitch + 4u) & 0x0Fu] / 32);
    int16_t nose_dy = (int16_t)(-s_sintab[pitch] / 32);
    int8_t shot_vx;
    int8_t shot_vy;

    if (nose_dx >= 0) {
        shot_vx = (int8_t)(3 + (nose_dx / 4));
    } else {
        shot_vx = (int8_t)(-3 + (nose_dx / 4));
    }
    if (shot_vx == 0) {
        shot_vx = 3;
    }

    shot_vy = (int8_t)(nose_dy / 4);

    for (uint8_t i = 0; i < MAX_COMBAT_PROJECTILES; ++i) {
        projectile_t *p = &s_projectiles[i];

        if (p->active) {
            continue;
        }

        p->world_x = wrap_world_x((int32_t)flight_world_x() + nose_dx);
        p->screen_y = (int16_t)(flight_plane_y() + (PLAYER_SPRITE_SIZE_PX / 2) + nose_dy);
        p->vx = shot_vx;
        p->vy = shot_vy;
        p->frame_index = SHOT_FRAME;
        p->bomb = false;
        p->life_ticks = SHOT_MAX_LIFE_TICKS;
        p->active = true;
        break;
    }
}

static void spawn_bomb(void)
{
    uint8_t pitch = flight_plane_pitch();
    int16_t nose_dx = (int16_t)(s_sintab[(pitch + 4u) & 0x0Fu] / 24);
    int16_t nose_dy = (int16_t)(-s_sintab[pitch] / 24);

    for (uint8_t i = 0; i < MAX_COMBAT_PROJECTILES; ++i) {
        projectile_t *p = &s_projectiles[i];

        if (p->active) {
            continue;
        }

        p->world_x = wrap_world_x((int32_t)flight_world_x() + nose_dx);
        p->screen_y = (int16_t)(flight_plane_y() + (PLAYER_SPRITE_SIZE_PX / 2) + nose_dy);
        p->vx = (int8_t)(nose_dx / 8);
        p->vy = 1;
        p->frame_index = BOMB_FRAME;
        p->bomb = true;
        p->life_ticks = BOMB_MAX_LIFE_TICKS;
        p->active = true;
        break;
    }
}

void projectiles_init(void)
{
    for (uint8_t i = 0; i < MAX_COMBAT_PROJECTILES; ++i) {
        s_projectiles[i].active = false;
    }

    s_spawn_cooldown = 0;
    s_bomb_spawn_cooldown = 0;
    s_tick_div = 0;
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

    if (s_spawn_cooldown > 0) {
        --s_spawn_cooldown;
    }
    if (s_bomb_spawn_cooldown > 0) {
        --s_bomb_spawn_cooldown;
    }

    if (fire_held && s_spawn_cooldown == 0 && !flight_is_crashed()) {
        spawn_shot();
        s_spawn_cooldown = SHOT_SPAWN_COOLDOWN_TICKS;
    }
    if (bomb_held && s_bomb_spawn_cooldown == 0 && !flight_is_crashed()) {
        spawn_bomb();
        s_bomb_spawn_cooldown = BOMB_SPAWN_COOLDOWN_TICKS;
    }

    if (++s_tick_div >= PROJECTILE_TICK_DIV) {
        s_tick_div = 0;

        for (uint8_t i = 0; i < MAX_COMBAT_PROJECTILES; ++i) {
            projectile_t *p = &s_projectiles[i];

            if (!p->active) {
                continue;
            }

            p->world_x = wrap_world_x((int32_t)p->world_x + p->vx);
            p->screen_y = (int16_t)(p->screen_y + p->vy);
            if (p->bomb && p->vy < BOMB_MAX_FALL_SPEED) {
                p->vy++;
            }
            if (p->bomb) {
                p->frame_index = (uint8_t)(BOMB_FRAME + ((p->life_ticks >> 2) & 1u));
            }

            if (p->life_ticks > 0) {
                --p->life_ticks;
            }

            if (p->life_ticks == 0 || p->screen_y < -PROJECTILE_SPRITE_SIZE_PX || p->screen_y >= SCREEN_HEIGHT) {
                p->active = false;
            }
        }
    }

    for (uint8_t i = 0; i < MAX_COMBAT_PROJECTILES; ++i) {
        projectile_t *p = &s_projectiles[i];

        if (!p->active) {
            sprite_mode5_set_projectile(i, -32, -32, 0, false);
            continue;
        }

        int16_t dx = world_delta_to_screen_x(p->world_x, camera_world_x);
        int16_t screen_center_x = (int16_t)((SCREEN_WIDTH / 2) + dx);
        int16_t sprite_x = (int16_t)(screen_center_x - (PROJECTILE_SPRITE_SIZE_PX / 2));
        int16_t sprite_y = (int16_t)(p->screen_y - (PROJECTILE_SPRITE_SIZE_PX / 2));
        bool visible = (sprite_x > -PROJECTILE_SPRITE_SIZE_PX) && (sprite_x < SCREEN_WIDTH) &&
                       (sprite_y > -PROJECTILE_SPRITE_SIZE_PX) && (sprite_y < SCREEN_HEIGHT);

        sprite_mode5_set_projectile(i, sprite_x, sprite_y, p->frame_index, visible);
    }

}