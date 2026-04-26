#include <rp6502.h>
#include <stdbool.h>
#include <stdint.h>

#include "ambient_flocks.h"
#include "constants.h"
#include "enemy_planes.h"
#include "flight.h"
#include "ground_targets.h"
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
    MINIMAP_PLAYER_COLOR = 7u,
    MINIMAP_BUILDING_COLOR = 11u,
    MINIMAP_ENEMY_COLOR = 9u,
    MINIMAP_FLOCK_COLOR = 10u,
    MINIMAP_UPDATE_DIV = 6u,
    MINIMAP_MAX_MARKERS = (MAX_TARGETS + MAX_ENEMIES + MAX_FLOCK_SPRITES + 1u)
};

typedef struct minimap_marker_restore_s {
    uint8_t x_px;
    uint8_t y_px;
    uint8_t color;
    uint8_t bg_color;
} minimap_marker_restore_t;

static minimap_marker_restore_t s_prev_markers[MINIMAP_MAX_MARKERS];
static minimap_marker_restore_t s_curr_markers[MINIMAP_MAX_MARKERS];
static uint8_t s_prev_count = 0u;
static uint8_t s_curr_count = 0u;
static uint8_t s_update_div = 0u;
static uint16_t s_marker_world_x[MAX_TARGETS];
static int16_t s_marker_y[MAX_TARGETS];

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
    s_prev_count = 0u;
    s_curr_count = 0u;
    s_update_div = (uint8_t)(MINIMAP_UPDATE_DIV - 1u);
}

static uint8_t minimap_scale_x(uint16_t world_x)
{
    uint16_t wrapped_world_x = (uint16_t)(world_x % WORLD_WIDTH_PX);
    uint8_t x_px = (uint8_t)(((wrapped_world_x / 75u) * 4u) +
                             (uint8_t)(((wrapped_world_x % 75u) * 4u) / 75u));

    if (x_px >= MINIMAP_WIDTH_PX) {
        x_px = (uint8_t)(MINIMAP_WIDTH_PX - 1u);
    }

    return x_px;
}

static uint8_t minimap_scale_y(int16_t y)
{
    uint8_t y_px;

    if (y < 0) {
        y = 0;
    } else if (y >= SCREEN_HEIGHT) {
        y = (SCREEN_HEIGHT - 1);
    }

    y_px = (uint8_t)(((uint16_t)(y / 15) * 2u) +
                     (uint8_t)((((uint16_t)y % 15u) * 2u) / 15u));
    y_px = (uint8_t)(y_px + MINIMAP_PLAYER_Y_OFFSET_PX);

    if (y_px >= MINIMAP_HEIGHT_PX) {
        y_px = (uint8_t)(MINIMAP_HEIGHT_PX - 1u);
    }

    return y_px;
}

static int16_t minimap_player_center_y(void)
{
    int16_t plane_y = flight_plane_y_physics();

    if (plane_y <= -(int16_t)PLANE_HITBOX_CENTER_Y_OFFSET_PX) {
        return 0;
    }
    if (plane_y >= (int16_t)(SCREEN_HEIGHT - 1 - PLANE_HITBOX_CENTER_Y_OFFSET_PX)) {
        return (int16_t)(SCREEN_HEIGHT - 1);
    }

    return (int16_t)(plane_y + PLANE_HITBOX_CENTER_Y_OFFSET_PX);
}

static int16_t marker_find_by_xy(const minimap_marker_restore_t *markers, uint8_t count,
                                 uint8_t x_px, uint8_t y_px)
{
    for (uint8_t i = 0u; i < count; ++i) {
        if (markers[i].x_px == x_px && markers[i].y_px == y_px) {
            return (int16_t)i;
        }
    }

    return -1;
}

static void minimap_add_marker(uint8_t x_px, uint8_t y_px, uint8_t color)
{
    int16_t existing = marker_find_by_xy(s_curr_markers, s_curr_count, x_px, y_px);

    if (existing >= 0) {
        s_curr_markers[(uint8_t)existing].color = color;
        return;
    }

    if (s_curr_count < MINIMAP_MAX_MARKERS) {
        s_curr_markers[s_curr_count].x_px = x_px;
        s_curr_markers[s_curr_count].y_px = y_px;
        s_curr_markers[s_curr_count].color = color;
        s_curr_markers[s_curr_count].bg_color = 0u;
        ++s_curr_count;
    }
}

static void minimap_apply_diff(void)
{
    for (uint8_t i = 0u; i < s_prev_count; ++i) {
        int16_t still_present = marker_find_by_xy(s_curr_markers, s_curr_count,
                                                  s_prev_markers[i].x_px,
                                                  s_prev_markers[i].y_px);

        if (still_present < 0) {
            minimap_write_pixel(s_prev_markers[i].x_px,
                                s_prev_markers[i].y_px,
                                s_prev_markers[i].bg_color);
        }
    }

    for (uint8_t i = 0u; i < s_curr_count; ++i) {
        int16_t prev_idx = marker_find_by_xy(s_prev_markers, s_prev_count,
                                             s_curr_markers[i].x_px,
                                             s_curr_markers[i].y_px);

        if (prev_idx >= 0) {
            s_curr_markers[i].bg_color = s_prev_markers[(uint8_t)prev_idx].bg_color;
            if (s_prev_markers[(uint8_t)prev_idx].color != s_curr_markers[i].color) {
                minimap_write_pixel(s_curr_markers[i].x_px,
                                    s_curr_markers[i].y_px,
                                    s_curr_markers[i].color);
            }
            continue;
        }

        s_curr_markers[i].bg_color = minimap_read_pixel(s_curr_markers[i].x_px,
                                                        s_curr_markers[i].y_px);
        minimap_write_pixel(s_curr_markers[i].x_px,
                            s_curr_markers[i].y_px,
                            s_curr_markers[i].color);
    }

    for (uint8_t i = 0u; i < s_curr_count; ++i) {
        s_prev_markers[i] = s_curr_markers[i];
    }
    s_prev_count = s_curr_count;
}

void minimap_update_player(uint16_t world_x)
{
    uint8_t count;

    if (++s_update_div < MINIMAP_UPDATE_DIV) {
        return;
    }
    s_update_div = 0u;
    s_curr_count = 0u;

    count = ground_targets_get_minimap_buildings(s_marker_world_x, s_marker_y, MAX_TARGETS);
    for (uint8_t i = 0u; i < count; ++i) {
        minimap_add_marker(minimap_scale_x(s_marker_world_x[i]),
                           minimap_scale_y(s_marker_y[i]),
                           MINIMAP_BUILDING_COLOR);
    }

    count = enemy_planes_get_minimap_fighters(s_marker_world_x, s_marker_y, MAX_ENEMIES);
    for (uint8_t i = 0u; i < count; ++i) {
        minimap_add_marker(minimap_scale_x(s_marker_world_x[i]),
                           minimap_scale_y(s_marker_y[i]),
                           MINIMAP_ENEMY_COLOR);
    }

    count = ambient_flocks_get_minimap_flocks(s_marker_world_x, s_marker_y, MAX_FLOCK_SPRITES);
    for (uint8_t i = 0u; i < count; ++i) {
        minimap_add_marker(minimap_scale_x(s_marker_world_x[i]),
                           minimap_scale_y(s_marker_y[i]),
                           MINIMAP_FLOCK_COLOR);
    }

    minimap_add_marker(minimap_scale_x(world_x),
                       minimap_scale_y(minimap_player_center_y()),
                       MINIMAP_PLAYER_COLOR);

    minimap_apply_diff();
}