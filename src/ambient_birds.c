#include <stdbool.h>
#include <stdint.h>

#include "ambient_birds.h"
#include "constants.h"
#include "flight.h"
#include "sprite_mode5.h"

enum {
    BIRD_FRAME_A = 18u,
    BIRD_FRAME_B = 19u,
    BIRD_SPLAT_FRAME = 20u,
    BIRD_MOVE_TICK_DIV = 3u,
    BIRD_TOP_LIMIT_Y = 8,
    BIRD_GROUND_CLEARANCE_Y = 6,
    AMBIENT_BIRD_BASE_COUNT = 4u,
    BIRD_SPLAT_LIFE_TICKS = 10u
};

typedef struct ambient_bird_s {
    uint16_t world_x;
    int16_t world_y;
    int8_t vx;
    int8_t vy;
    bool active;
} ambient_bird_t;

static ambient_bird_t s_birds[MAX_BIRD_SPRITES];
static uint8_t s_anim_tick;
static uint8_t s_move_tick;
static uint8_t s_scatter_alloc_cursor;
static uint16_t s_splat_world_x[MAX_SPLAT_SPRITES];
static int16_t s_splat_world_y[MAX_SPLAT_SPRITES];
static uint8_t s_splat_life[MAX_SPLAT_SPRITES];
static uint8_t s_splat_alloc_cursor;

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

static uint16_t world_width_px(void)
{
    return (uint16_t)(GROUND_WIDTH * 8u);
}

static int16_t wrapped_world_dx(uint16_t target_x, uint16_t source_x)
{
    int16_t dx = (int16_t)target_x - (int16_t)source_x;
    int16_t world_width = (int16_t)world_width_px();
    int16_t half_world = (int16_t)(world_width / 2);

    if (dx > half_world) {
        dx -= world_width;
    } else if (dx < -half_world) {
        dx += world_width;
    }

    return dx;
}

static void tick_bird(ambient_bird_t *bird)
{
    uint16_t max_x = (uint16_t)(world_width_px() - 1u);
    int16_t next_x = (int16_t)bird->world_x + bird->vx;
    int16_t next_y = (int16_t)bird->world_y + bird->vy;

    if (next_x <= 0) {
        next_x = 0;
        bird->vx = 1;
    } else if (next_x >= (int16_t)max_x) {
        next_x = max_x;
        bird->vx = -1;
    }

    {
        int16_t terrain_y = flight_terrain_y_at((uint16_t)next_x);
        int16_t y_limit = (int16_t)(terrain_y - BIRD_GROUND_CLEARANCE_Y - PROJECTILE_SPRITE_SIZE_PX);

        if (next_y <= BIRD_TOP_LIMIT_Y) {
            next_y = BIRD_TOP_LIMIT_Y;
            bird->vy = 1;
        } else if (next_y >= y_limit) {
            next_y = y_limit;
            bird->vy = -1;
        }
    }

    bird->world_x = (uint16_t)next_x;
    bird->world_y = next_y;
}

static uint8_t allocate_scatter_slot(void)
{
    uint8_t start = s_scatter_alloc_cursor;
    uint8_t i;

    for (i = 0; i < (uint8_t)(MAX_BIRD_SPRITES - AMBIENT_BIRD_BASE_COUNT); ++i) {
        uint8_t slot = (uint8_t)(AMBIENT_BIRD_BASE_COUNT +
                         ((start + i) % (MAX_BIRD_SPRITES - AMBIENT_BIRD_BASE_COUNT)));
        if (!s_birds[slot].active) {
            s_scatter_alloc_cursor = (uint8_t)((slot - AMBIENT_BIRD_BASE_COUNT + 1u) %
                                              (MAX_BIRD_SPRITES - AMBIENT_BIRD_BASE_COUNT));
            return slot;
        }
    }

    {
        uint8_t slot = (uint8_t)(AMBIENT_BIRD_BASE_COUNT + s_scatter_alloc_cursor);
        s_scatter_alloc_cursor = (uint8_t)((s_scatter_alloc_cursor + 1u) %
                                           (MAX_BIRD_SPRITES - AMBIENT_BIRD_BASE_COUNT));
        return slot;
    }
}

static uint8_t allocate_splat_slot(void)
{
    uint8_t i;

    for (i = 0; i < MAX_SPLAT_SPRITES; ++i) {
        if (s_splat_life[i] == 0u) {
            return i;
        }
    }

    {
        uint8_t slot = s_splat_alloc_cursor;
        s_splat_alloc_cursor = (uint8_t)((s_splat_alloc_cursor + 1u) % MAX_SPLAT_SPRITES);
        return slot;
    }
}

void ambient_birds_init(void)
{
    static const uint16_t spawn_x[MAX_BIRD_SPRITES] = {250u, 880u, 1490u, 2280u};
    static const int16_t spawn_y[MAX_BIRD_SPRITES] = {36, 48, 30, 42};
    static const int8_t spawn_vx[MAX_BIRD_SPRITES] = {1, -1, 1, -1};
    static const int8_t spawn_vy[MAX_BIRD_SPRITES] = {1, 1, -1, -1};
    uint8_t i;
    uint8_t bird_base = (uint8_t)(MAX_COMBAT_PROJECTILES + MAX_FLOCK_SPRITES);

    s_anim_tick = 0;
    s_move_tick = 0;
    s_scatter_alloc_cursor = 0;
    s_splat_alloc_cursor = 0;

    for (i = 0; i < MAX_SPLAT_SPRITES; ++i) {
        s_splat_world_x[i] = 0;
        s_splat_world_y[i] = 0;
        s_splat_life[i] = 0;
    }

    for (i = 0; i < MAX_BIRD_SPRITES; ++i) {
        if (i < AMBIENT_BIRD_BASE_COUNT) {
            s_birds[i].world_x = spawn_x[i];
            s_birds[i].world_y = spawn_y[i];
            s_birds[i].vx = spawn_vx[i];
            s_birds[i].vy = spawn_vy[i];
            s_birds[i].active = true;
        } else {
            s_birds[i].world_x = 0;
            s_birds[i].world_y = 0;
            s_birds[i].vx = 0;
            s_birds[i].vy = 0;
            s_birds[i].active = false;
        }

        sprite_mode5_set_projectile((uint8_t)(bird_base + i), -32, -32, 0, false);
    }

    {
        uint8_t splat_base = (uint8_t)(MAX_COMBAT_PROJECTILES + MAX_FLOCK_SPRITES + MAX_BIRD_SPRITES);
        for (i = 0; i < MAX_SPLAT_SPRITES; ++i) {
            sprite_mode5_set_projectile((uint8_t)(splat_base + i), -32, -32, 0, false);
        }
    }
}

void ambient_birds_spawn_splat(uint16_t world_x, int16_t center_y)
{
    uint8_t slot = allocate_splat_slot();
    s_splat_world_x[slot] = world_x;
    s_splat_world_y[slot] = (int16_t)(center_y - (PROJECTILE_SPRITE_SIZE_PX / 2));
    s_splat_life[slot] = BIRD_SPLAT_LIFE_TICKS;
}

bool ambient_birds_check_plane_hit(uint16_t world_x, int16_t center_y, uint8_t half_size_px)
{
    uint8_t i;

    for (i = 0; i < MAX_BIRD_SPRITES; ++i) {
        int16_t dx;

        if (!s_birds[i].active) {
            continue;
        }

        dx = wrapped_world_dx(s_birds[i].world_x, world_x);
        if (dx >= -(int16_t)half_size_px &&
            dx <= (int16_t)(PROJECTILE_SPRITE_SIZE_PX - 1 + half_size_px) &&
            center_y >= (int16_t)(s_birds[i].world_y - half_size_px) &&
            center_y <= (int16_t)(s_birds[i].world_y + PROJECTILE_SPRITE_SIZE_PX - 1 + half_size_px)) {
            s_birds[i].active = false;
            ambient_birds_spawn_splat(s_birds[i].world_x,
                                      (int16_t)(s_birds[i].world_y + (PROJECTILE_SPRITE_SIZE_PX / 2)));
            return true;
        }
    }

    return false;
}

void ambient_birds_spawn_scatter(uint16_t world_x, int16_t center_y)
{
    static const int8_t scatter_vx[8] = {2, 1, 0, -1, -2, -1, 0, 1};
    static const int8_t scatter_vy[8] = {0, 1, 2, 1, 0, -1, -2, -1};
    uint8_t i;

    for (i = 0; i < 8u; ++i) {
        uint8_t slot = allocate_scatter_slot();
        s_birds[slot].world_x = world_x;
        s_birds[slot].world_y = (int16_t)(center_y - (PROJECTILE_SPRITE_SIZE_PX / 2));
        s_birds[slot].vx = scatter_vx[i];
        s_birds[slot].vy = scatter_vy[i];
        s_birds[slot].active = true;
    }
}

void ambient_birds_update(uint16_t camera_world_x)
{
    uint8_t i;
    uint8_t bird_base = (uint8_t)(MAX_COMBAT_PROJECTILES + MAX_FLOCK_SPRITES);
    uint8_t splat_base = (uint8_t)(bird_base + MAX_BIRD_SPRITES);
    uint8_t frame;

    s_anim_tick++;
    frame = ((s_anim_tick >> 3) & 1u) ? BIRD_FRAME_B : BIRD_FRAME_A;

    if (++s_move_tick >= BIRD_MOVE_TICK_DIV) {
        s_move_tick = 0;
        for (i = 0; i < MAX_BIRD_SPRITES; ++i) {
            if (s_birds[i].active) {
                tick_bird(&s_birds[i]);
            }
        }
    }

    for (i = 0; i < MAX_BIRD_SPRITES; ++i) {
        if (!s_birds[i].active) {
            sprite_mode5_set_projectile((uint8_t)(bird_base + i), -32, -32, 0, false);
            continue;
        }

        int16_t dx = world_delta_to_screen_x(s_birds[i].world_x, camera_world_x);
        int16_t screen_x = (int16_t)((SCREEN_WIDTH / 2) + dx);
        int16_t screen_y = s_birds[i].world_y;
        bool visible = (screen_x > -PROJECTILE_SPRITE_SIZE_PX) &&
                       (screen_x < SCREEN_WIDTH) &&
                       (screen_y > -PROJECTILE_SPRITE_SIZE_PX) &&
                       (screen_y < SCREEN_HEIGHT);

        sprite_mode5_set_projectile((uint8_t)(bird_base + i), screen_x, screen_y, frame, visible);
    }

    for (i = 0; i < MAX_SPLAT_SPRITES; ++i) {
        if (s_splat_life[i] == 0u) {
            sprite_mode5_set_projectile((uint8_t)(splat_base + i), -32, -32, 0, false);
            continue;
        }

        {
            int16_t dx = world_delta_to_screen_x(s_splat_world_x[i], camera_world_x);
            int16_t screen_x = (int16_t)((SCREEN_WIDTH / 2) + dx);
            int16_t screen_y = s_splat_world_y[i];
            bool visible = (screen_x > -PROJECTILE_SPRITE_SIZE_PX) &&
                           (screen_x < SCREEN_WIDTH) &&
                           (screen_y > -PROJECTILE_SPRITE_SIZE_PX) &&
                           (screen_y < SCREEN_HEIGHT);

            sprite_mode5_set_projectile((uint8_t)(splat_base + i), screen_x, screen_y,
                                        BIRD_SPLAT_FRAME, visible);
        }

        --s_splat_life[i];
    }
}