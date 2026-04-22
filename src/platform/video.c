#include "platform/video.h"

#include <rp6502.h>
#include <stdint.h>
#include <string.h>

#include "constants.h"
#include "game/terrain/terrain.h"

typedef struct {
    int16_t x_pos_px;
    int16_t y_pos_px;
    uint16_t xram_sprite_ptr;
    uint16_t palette_ptr;
} vga_mode5_sprite_t;

enum {
    TILE_INDEX_SKY = 1,
    TILE_INDEX_GROUND = 2,

    PLAYER_COLOR_BODY = 3,
    PLAYER_COLOR_WING = 4,
    PLAYER_COLOR_TAIL = 5
};

static uint8_t s_surface_row_cache[RPS_TILEMAP_WIDTH_TILES];
static uint8_t s_surface_row_cache_valid;

static uint16_t wrap_world_x(int32_t x)
{
    int32_t wrapped = x % TERRAIN_WORLD_WIDTH;
    if (wrapped < 0) {
        wrapped += TERRAIN_WORLD_WIDTH;
    }
    return (uint16_t)wrapped;
}

static uint16_t interpolate_world_x(uint16_t prev_x, uint16_t curr_x, uint8_t subframe)
{
    int16_t diff = (int16_t)curr_x - (int16_t)prev_x;

    if (diff > (int16_t)(TERRAIN_WORLD_WIDTH / 2u)) {
        diff -= (int16_t)TERRAIN_WORLD_WIDTH;
    } else if (diff < -(int16_t)(TERRAIN_WORLD_WIDTH / 2u)) {
        diff += (int16_t)TERRAIN_WORLD_WIDTH;
    }

    return wrap_world_x((int32_t)prev_x + ((int32_t)diff * (int32_t)subframe) / 6);
}

static void write_palette_entry(uint8_t index, uint16_t rgb555)
{
    RIA.addr0 = RPS_XRAM_MODE2_PALETTE_ADDR + ((unsigned)index * 2u);
    RIA.step0 = 1;
    RIA.rw0 = (uint8_t)(rgb555 & 0xFFu);
    RIA.rw0 = (uint8_t)(rgb555 >> 8);
}

static void write_solid_4bpp_tile(uint8_t tile_index, uint8_t color_index)
{
    uint8_t packed = (uint8_t)((color_index << 4) | color_index);

    RIA.addr0 = RPS_XRAM_MODE2_TILESET_ADDR + ((unsigned)tile_index * RPS_MODE2_TILE_BYTES_4BPP);
    RIA.step0 = 1;
    for (uint8_t i = 0; i < RPS_MODE2_TILE_BYTES_4BPP; ++i) {
        RIA.rw0 = packed;
    }
}

static void write_mode5_palette_entry(uint8_t index, uint16_t rgb555)
{
    RIA.addr0 = RPS_XRAM_MODE5_PALETTE_ADDR + ((unsigned)index * 2u);
    RIA.step0 = 1;
    RIA.rw0 = (uint8_t)(rgb555 & 0xFFu);
    RIA.rw0 = (uint8_t)(rgb555 >> 8);
}

static void write_player_sprite_bitmap(void)
{
    RIA.addr0 = RPS_XRAM_MODE5_SPRITE_DATA_ADDR;
    RIA.step0 = 1;

    for (uint8_t y = 0; y < RPS_MODE5_SPRITE_SIZE_PX; ++y) {
        for (uint8_t x = 0; x < RPS_MODE5_SPRITE_SIZE_PX; x += 2) {
            uint8_t c0 = 0;
            uint8_t c1 = 0;

            uint8_t x0 = x;
            uint8_t x1 = (uint8_t)(x + 1);

            if ((y >= 4 && y <= 11) && (x0 >= 6 && x0 <= 9)) {
                c0 = PLAYER_COLOR_BODY;
            }
            if ((y >= 4 && y <= 11) && (x1 >= 6 && x1 <= 9)) {
                c1 = PLAYER_COLOR_BODY;
            }
            if (y == 7 && x0 >= 2 && x0 <= 13) {
                c0 = PLAYER_COLOR_WING;
            }
            if (y == 7 && x1 >= 2 && x1 <= 13) {
                c1 = PLAYER_COLOR_WING;
            }
            if ((y >= 10 && y <= 14) && (x0 == 5 || x0 == 10)) {
                c0 = PLAYER_COLOR_TAIL;
            }
            if ((y >= 10 && y <= 14) && (x1 == 5 || x1 == 10)) {
                c1 = PLAYER_COLOR_TAIL;
            }

            RIA.rw0 = (uint8_t)((c0 << 4) | c1);
        }
    }
}

static bool init_player_sprite_layer(void)
{
    int16_t x = (int16_t)((RPS_SCREEN_WIDTH_PX - RPS_MODE5_SPRITE_SIZE_PX) / 2u);
    int16_t y = (int16_t)((RPS_SCREEN_HEIGHT_PX - RPS_MODE5_SPRITE_SIZE_PX) / 2u);

    xram0_struct_set(RPS_XRAM_MODE5_CONFIG_ADDR, vga_mode5_sprite_t, x_pos_px, x);
    xram0_struct_set(RPS_XRAM_MODE5_CONFIG_ADDR, vga_mode5_sprite_t, y_pos_px, y);
    xram0_struct_set(RPS_XRAM_MODE5_CONFIG_ADDR, vga_mode5_sprite_t, xram_sprite_ptr, RPS_XRAM_MODE5_SPRITE_DATA_ADDR);
    xram0_struct_set(RPS_XRAM_MODE5_CONFIG_ADDR, vga_mode5_sprite_t, palette_ptr, RPS_XRAM_MODE5_PALETTE_ADDR);

    if (xreg_vga_mode(5, 0x0A, RPS_XRAM_MODE5_CONFIG_ADDR, 1, 1, 0, 0) < 0) {
        return false;
    }

    write_mode5_palette_entry(0, 0x0000);
    write_mode5_palette_entry(PLAYER_COLOR_BODY, 0xFFFF);
    write_mode5_palette_entry(PLAYER_COLOR_WING, 0x7C00);
    write_mode5_palette_entry(PLAYER_COLOR_TAIL, 0x03E0);
    write_player_sprite_bitmap();

    return true;
}

static void update_player_sprite_position(const game_state_t *state)
{
    int16_t sprite_x = (int16_t)(state->plane_x - (int16_t)(RPS_MODE5_SPRITE_SIZE_PX / 2u));
    int16_t sprite_y = (int16_t)(state->plane_y - (int16_t)(RPS_MODE5_SPRITE_SIZE_PX / 2u));

    if (sprite_x < 0) {
        sprite_x = 0;
    }
    if (sprite_y < 0) {
        sprite_y = 0;
    }
    if (sprite_x > (int16_t)(RPS_SCREEN_WIDTH_PX - RPS_MODE5_SPRITE_SIZE_PX)) {
        sprite_x = (int16_t)(RPS_SCREEN_WIDTH_PX - RPS_MODE5_SPRITE_SIZE_PX);
    }
    if (sprite_y > (int16_t)(RPS_SCREEN_HEIGHT_PX - RPS_MODE5_SPRITE_SIZE_PX)) {
        sprite_y = (int16_t)(RPS_SCREEN_HEIGHT_PX - RPS_MODE5_SPRITE_SIZE_PX);
    }

    xram0_struct_set(RPS_XRAM_MODE5_CONFIG_ADDR, vga_mode5_sprite_t, x_pos_px, sprite_x);
    xram0_struct_set(RPS_XRAM_MODE5_CONFIG_ADDR, vga_mode5_sprite_t, y_pos_px, sprite_y);
}

static void render_terrain_tilemap(uint16_t camera_world_x)
{
    int32_t camera_left = (int32_t)camera_world_x - ((int32_t)RPS_SCREEN_WIDTH_PX / 2);

    for (uint8_t tx = 0; tx < RPS_TILEMAP_WIDTH_TILES; ++tx) {
        uint16_t sample_world_x = wrap_world_x(camera_left + ((int32_t)tx * RPS_TILE_SIZE_PX) + (RPS_TILE_SIZE_PX / 2));
        uint8_t surface_px = terrain_height_at_world_x(sample_world_x);
        uint8_t surface_row = (uint8_t)(surface_px / RPS_TILE_SIZE_PX);
        uint8_t old_row = s_surface_row_cache[tx];

        if (!s_surface_row_cache_valid || old_row == 0xFFu) {
            RIA.addr0 = RPS_XRAM_MODE2_TILEMAP_ADDR + tx;
            RIA.step0 = RPS_TILEMAP_WIDTH_TILES;
            for (uint8_t ty = 0; ty < RPS_TILEMAP_HEIGHT_TILES; ++ty) {
                RIA.rw0 = (ty >= surface_row) ? TILE_INDEX_GROUND : TILE_INDEX_SKY;
            }
            s_surface_row_cache[tx] = surface_row;
            continue;
        }

        if (old_row == surface_row) {
            continue;
        }

        if (surface_row > old_row) {
            // Ground moved down: previous ground strip becomes sky.
            RIA.addr0 = RPS_XRAM_MODE2_TILEMAP_ADDR + tx + ((unsigned)old_row * RPS_TILEMAP_WIDTH_TILES);
            RIA.step0 = RPS_TILEMAP_WIDTH_TILES;
            for (uint8_t ty = old_row; ty < surface_row; ++ty) {
                RIA.rw0 = TILE_INDEX_SKY;
            }
        } else {
            // Ground moved up: previous sky strip becomes ground.
            RIA.addr0 = RPS_XRAM_MODE2_TILEMAP_ADDR + tx + ((unsigned)surface_row * RPS_TILEMAP_WIDTH_TILES);
            RIA.step0 = RPS_TILEMAP_WIDTH_TILES;
            for (uint8_t ty = surface_row; ty < old_row; ++ty) {
                RIA.rw0 = TILE_INDEX_GROUND;
            }
        }

        s_surface_row_cache[tx] = surface_row;
    }

    s_surface_row_cache_valid = 1u;
}

bool platform_video_init(void)
{
    // Mode-2 tile layer, 8x8 tiles, palette-indexed pixels.
    xram0_struct_set(RPS_XRAM_MODE2_CONFIG_ADDR, vga_mode2_config_t, x_wrap, false);
    xram0_struct_set(RPS_XRAM_MODE2_CONFIG_ADDR, vga_mode2_config_t, y_wrap, false);
    xram0_struct_set(RPS_XRAM_MODE2_CONFIG_ADDR, vga_mode2_config_t, x_pos_px, 0);
    xram0_struct_set(RPS_XRAM_MODE2_CONFIG_ADDR, vga_mode2_config_t, y_pos_px, 0);
    xram0_struct_set(RPS_XRAM_MODE2_CONFIG_ADDR, vga_mode2_config_t, width_tiles, RPS_TILEMAP_WIDTH_TILES);
    xram0_struct_set(RPS_XRAM_MODE2_CONFIG_ADDR, vga_mode2_config_t, height_tiles, RPS_TILEMAP_HEIGHT_TILES);
    xram0_struct_set(RPS_XRAM_MODE2_CONFIG_ADDR, vga_mode2_config_t, xram_data_ptr, RPS_XRAM_MODE2_TILEMAP_ADDR);
    xram0_struct_set(RPS_XRAM_MODE2_CONFIG_ADDR, vga_mode2_config_t, xram_palette_ptr, RPS_XRAM_MODE2_PALETTE_ADDR);
    xram0_struct_set(RPS_XRAM_MODE2_CONFIG_ADDR, vga_mode2_config_t, xram_tile_ptr, RPS_XRAM_MODE2_TILESET_ADDR);

    if (xreg_vga_mode(2, 0x02, RPS_XRAM_MODE2_CONFIG_ADDR, 0, 0, 0) < 0) {
        return false;
    }

    write_palette_entry(0, 0x0000);
    write_palette_entry(1, 0x7E1F);
    write_palette_entry(2, 0x0154);

    write_solid_4bpp_tile(TILE_INDEX_SKY, 1);
    write_solid_4bpp_tile(TILE_INDEX_GROUND, 2);

    memset(s_surface_row_cache, 0xFF, sizeof(s_surface_row_cache));
    s_surface_row_cache_valid = 0u;

    if (!init_player_sprite_layer()) {
        return false;
    }

    return true;
}

void platform_video_render(const game_state_t *state, uint8_t subframe)
{
    uint16_t camera_world_x = interpolate_world_x(state->prev_world_x, state->world_x, subframe);

    render_terrain_tilemap(camera_world_x);
    update_player_sprite_position(state);
}