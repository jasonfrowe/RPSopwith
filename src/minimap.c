#include <rp6502.h>
#include <stdbool.h>
#include <stdint.h>

#include "constants.h"
#include "flight.h"
#include "minimap.h"

enum {
    MINIMAP_TILE_X = 19u,
    MINIMAP_TILE_Y = 24u,
    MINIMAP_WIDTH_TILES = 20u,
    MINIMAP_HEIGHT_TILES = 4u,
    MINIMAP_WIDTH_PX = (MINIMAP_WIDTH_TILES * 8u),
    MINIMAP_HEIGHT_PX = (MINIMAP_HEIGHT_TILES * 8u),
    WORLD_WIDTH_PX = (GROUND_WIDTH * 8),
    MINIMAP_PLAYER_Y_OFFSET_PX = 3u,
    MINIMAP_PLAYER_COLOR = 7u
};

static bool s_prev_valid = false;
static uint8_t s_prev_x_px;
static uint8_t s_prev_y_px;
static uint8_t s_prev_color;

static uint8_t xram_read_u8(unsigned addr)
{
    RIA.addr0 = addr;
    RIA.step0 = 1;
    return RIA.rw0;
}

static void xram_write_u8(unsigned addr, uint8_t value)
{
    RIA.addr0 = addr;
    RIA.step0 = 1;
    RIA.rw0 = value;
}

static uint8_t minimap_read_pixel(uint8_t x_px, uint8_t y_px)
{
    uint8_t tile_x = (uint8_t)(x_px >> 3);
    uint8_t tile_y = (uint8_t)(y_px >> 3);
    uint8_t local_x = (uint8_t)(x_px & 0x07u);
    uint8_t local_y = (uint8_t)(y_px & 0x07u);
    unsigned map_addr;
    uint8_t tile_id;
    unsigned byte_addr;
    uint8_t packed;

    map_addr = HUD_MAP_DATA +
               ((unsigned)(MINIMAP_TILE_Y + tile_y) * HUD_MAP_WIDTH_TILES) +
               (unsigned)(MINIMAP_TILE_X + tile_x);
    tile_id = xram_read_u8(map_addr);

    byte_addr = HUD_TILES + ((unsigned)tile_id * 32u) + ((unsigned)local_y * 4u) + (local_x >> 1);
    packed = xram_read_u8(byte_addr);

    if ((local_x & 1u) == 0u) {
        return (uint8_t)((packed >> 4) & 0x0Fu);
    }

    return (uint8_t)(packed & 0x0Fu);
}

static void minimap_write_pixel(uint8_t x_px, uint8_t y_px, uint8_t color)
{
    uint8_t tile_x = (uint8_t)(x_px >> 3);
    uint8_t tile_y = (uint8_t)(y_px >> 3);
    uint8_t local_x = (uint8_t)(x_px & 0x07u);
    uint8_t local_y = (uint8_t)(y_px & 0x07u);
    unsigned map_addr;
    uint8_t tile_id;
    unsigned byte_addr;
    uint8_t packed;

    map_addr = HUD_MAP_DATA +
               ((unsigned)(MINIMAP_TILE_Y + tile_y) * HUD_MAP_WIDTH_TILES) +
               (unsigned)(MINIMAP_TILE_X + tile_x);
    tile_id = xram_read_u8(map_addr);

    byte_addr = HUD_TILES + ((unsigned)tile_id * 32u) + ((unsigned)local_y * 4u) + (local_x >> 1);
    packed = xram_read_u8(byte_addr);

    color &= 0x0Fu;
    if ((local_x & 1u) == 0u) {
        packed = (uint8_t)((packed & 0x0Fu) | (uint8_t)(color << 4));
    } else {
        packed = (uint8_t)((packed & 0xF0u) | color);
    }

    xram_write_u8(byte_addr, packed);
}

void minimap_init(void)
{
    s_prev_valid = false;
}

void minimap_update_player(uint16_t world_x)
{
    uint16_t wrapped_world_x = (uint16_t)(world_x % WORLD_WIDTH_PX);
    uint32_t scaled_x = ((uint32_t)wrapped_world_x * (uint32_t)MINIMAP_WIDTH_PX) / (uint32_t)WORLD_WIDTH_PX;
    int16_t plane_center_y = (int16_t)(flight_plane_y_physics() + PLANE_HITBOX_CENTER_Y_OFFSET_PX);
    uint32_t scaled_y;
    uint8_t x_px = (uint8_t)scaled_x;
    uint8_t y_px;

    if (plane_center_y < 0) {
        plane_center_y = 0;
    } else if (plane_center_y >= SCREEN_HEIGHT) {
        plane_center_y = (SCREEN_HEIGHT - 1);
    }

    scaled_y = ((uint32_t)plane_center_y * (uint32_t)MINIMAP_HEIGHT_PX) / (uint32_t)SCREEN_HEIGHT;
    y_px = (uint8_t)scaled_y;
    y_px = (uint8_t)(y_px + MINIMAP_PLAYER_Y_OFFSET_PX);

    if (x_px >= MINIMAP_WIDTH_PX) {
        x_px = (uint8_t)(MINIMAP_WIDTH_PX - 1u);
    }
    if (y_px >= MINIMAP_HEIGHT_PX) {
        y_px = (uint8_t)(MINIMAP_HEIGHT_PX - 1u);
    }

    if (s_prev_valid && (s_prev_x_px != x_px || s_prev_y_px != y_px)) {
        minimap_write_pixel(s_prev_x_px, s_prev_y_px, s_prev_color);
        s_prev_valid = false;
    }

    if (!s_prev_valid) {
        s_prev_x_px = x_px;
        s_prev_y_px = y_px;
        s_prev_color = minimap_read_pixel(x_px, y_px);
        s_prev_valid = true;
    }

    minimap_write_pixel(x_px, y_px, MINIMAP_PLAYER_COLOR);
}