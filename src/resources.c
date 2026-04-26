#include <rp6502.h>
#include <stdbool.h>
#include <stdint.h>

#include "constants.h"
#include "resources.h"

enum {
    MAXROUNDS = 200,
    MAXBOMBS = 5,
    MAXMISSILES = 0,
    MAXFUEL = 9000,
    MAXCRASH = 5,
    GAUGE_TOP_Y = 25,
    GAUGE_BOT_Y = 26,
    GAUGE_LIVES_X = 12,
    GAUGE_FUEL_X = 13,
    GAUGE_BOMBS_X = 14,
    GAUGE_BULLETS_X = 15,
    GAUGE_MISSILES_X = 16,
    CYAN_GAUGE_BASE = 1,
    MAGENTA_GAUGE_BASE = 9,
    WHITE_GAUGE_BASE = 17
};

typedef struct resources_state_s {
    uint16_t fuel;
    uint16_t bullets;
    uint8_t bombs;
    uint8_t missiles;
    uint8_t lives;
    uint16_t tick_10hz;
} resources_state_t;

static resources_state_t s_resources;

static void hud_write_tile(uint8_t x, uint8_t y, uint8_t tile_id)
{
    unsigned addr = HUD_MAP_DATA + ((unsigned)y * HUD_MAP_WIDTH_TILES) + x;

    RIA.addr0 = addr;
    RIA.step0 = 1;
    RIA.rw0 = tile_id;
}

static uint16_t clamp_u16(uint16_t v, uint16_t max_v)
{
    return (v > max_v) ? max_v : v;
}

static uint8_t gauge_tile_for_units(uint8_t base_tile, uint8_t units)
{
    if (units == 0u) {
        return 0u;
    }
    if (units > 8u) {
        units = 8u;
    }
    return (uint8_t)(base_tile + units - 1u);
}

static void draw_gauge_2tiles(uint8_t x, uint16_t value, uint16_t max_value, uint8_t base_tile)
{
    uint16_t fill16;
    uint8_t top_units;
    uint8_t bot_units;

    if (max_value == 0u) {
        hud_write_tile(x, GAUGE_TOP_Y, 0u);
        hud_write_tile(x, GAUGE_BOT_Y, 0u);
        return;
    }

    fill16 = (uint16_t)(((uint32_t)value * 16u + (max_value / 2u)) / max_value);
    if (fill16 > 16u) {
        fill16 = 16u;
    }

    bot_units = (fill16 > 8u) ? 8u : (uint8_t)fill16;
    top_units = (fill16 > 8u) ? (uint8_t)(fill16 - 8u) : 0u;

    hud_write_tile(x, GAUGE_TOP_Y, gauge_tile_for_units(base_tile, top_units));
    hud_write_tile(x, GAUGE_BOT_Y, gauge_tile_for_units(base_tile, bot_units));
}

static void render_hud_gauges(void)
{
    draw_gauge_2tiles(GAUGE_LIVES_X, s_resources.lives, MAXCRASH, CYAN_GAUGE_BASE);
    draw_gauge_2tiles(GAUGE_FUEL_X, s_resources.fuel, MAXFUEL, CYAN_GAUGE_BASE);
    draw_gauge_2tiles(GAUGE_BOMBS_X, s_resources.bombs, MAXBOMBS, MAGENTA_GAUGE_BASE);
    draw_gauge_2tiles(GAUGE_BULLETS_X, s_resources.bullets, MAXROUNDS, WHITE_GAUGE_BASE);
    draw_gauge_2tiles(GAUGE_MISSILES_X, s_resources.missiles, MAXMISSILES, CYAN_GAUGE_BASE);
}

static void topup_counter_u16(uint16_t *counter, uint16_t max_value)
{
    if (*counter >= max_value) {
        *counter = max_value;
        return;
    }

    if (max_value < 20u) {
        if ((s_resources.tick_10hz % 20u) == 0u) {
            ++(*counter);
        }
    } else {
        *counter = (uint16_t)(*counter + (max_value / 100u));
    }

    *counter = clamp_u16(*counter, max_value);
}

void resources_init(void)
{
    s_resources.fuel = MAXFUEL;
    s_resources.bullets = MAXROUNDS;
    s_resources.bombs = MAXBOMBS;
    s_resources.missiles = MAXMISSILES;
    s_resources.lives = MAXCRASH;
    s_resources.tick_10hz = 0u;

    render_hud_gauges();
}

bool resources_tick_10hz(bool at_home, bool airborne, bool crashed, bool falling, uint8_t speed)
{
    bool out_of_fuel = false;
    uint16_t bombs_u16;
    uint16_t missiles_u16;

    ++s_resources.tick_10hz;

    if (at_home && !crashed) {
        topup_counter_u16(&s_resources.fuel, MAXFUEL);
        topup_counter_u16(&s_resources.bullets, MAXROUNDS);

        bombs_u16 = s_resources.bombs;
        missiles_u16 = s_resources.missiles;
        topup_counter_u16(&bombs_u16, MAXBOMBS);
        topup_counter_u16(&missiles_u16, MAXMISSILES);
        s_resources.bombs = (uint8_t)bombs_u16;
        s_resources.missiles = (uint8_t)missiles_u16;
    }

    if (airborne && !crashed && !falling) {
        if (speed >= s_resources.fuel) {
            s_resources.fuel = 0u;
            out_of_fuel = true;
        } else {
            s_resources.fuel = (uint16_t)(s_resources.fuel - speed);
        }
    }

    render_hud_gauges();
    return out_of_fuel;
}

bool resources_try_consume_bullet(void)
{
    if (s_resources.bullets == 0u) {
        return false;
    }

    --s_resources.bullets;
    render_hud_gauges();
    return true;
}

bool resources_try_consume_bomb(void)
{
    if (s_resources.bombs == 0u) {
        return false;
    }

    --s_resources.bombs;
    render_hud_gauges();
    return true;
}

bool resources_try_consume_missile(void)
{
    if (s_resources.missiles == 0u) {
        return false;
    }

    --s_resources.missiles;
    render_hud_gauges();
    return true;
}

void resources_on_plane_lost(void)
{
    if (s_resources.lives > 0u) {
        --s_resources.lives;
    }
    render_hud_gauges();
}

bool resources_can_respawn(void)
{
    return s_resources.lives > 0u;
}

void resources_on_respawn(void)
{
    s_resources.fuel = MAXFUEL;
    s_resources.bullets = MAXROUNDS;
    s_resources.bombs = MAXBOMBS;
    s_resources.missiles = MAXMISSILES;
    render_hud_gauges();
}

uint8_t resources_lives(void)
{
    return s_resources.lives;
}

uint16_t resources_fuel(void)
{
    return s_resources.fuel;
}

uint16_t resources_bullets(void)
{
    return s_resources.bullets;
}

uint8_t resources_bombs(void)
{
    return s_resources.bombs;
}

uint8_t resources_missiles(void)
{
    return s_resources.missiles;
}