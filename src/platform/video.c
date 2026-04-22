#include "platform/video.h"

#include <rp6502.h>
#include <stdint.h>

#include "constants.h"
#include "game/terrain/terrain.h"

enum {
    TILE_INDEX_SKY = 1,
    TILE_INDEX_GROUND = 2
};

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

static void render_terrain_tilemap(uint16_t camera_world_x)
{
    int32_t camera_left = (int32_t)camera_world_x - ((int32_t)RPS_SCREEN_WIDTH_PX / 2);

    for (uint8_t tx = 0; tx < RPS_TILEMAP_WIDTH_TILES; ++tx) {
        uint16_t sample_world_x = wrap_world_x(camera_left + ((int32_t)tx * RPS_TILE_SIZE_PX) + (RPS_TILE_SIZE_PX / 2));
        uint8_t surface_px = terrain_height_at_world_x(sample_world_x);
        uint8_t surface_row = (uint8_t)(surface_px / RPS_TILE_SIZE_PX);

        RIA.addr0 = RPS_XRAM_MODE2_TILEMAP_ADDR + tx;
        RIA.step0 = RPS_TILEMAP_WIDTH_TILES;
        for (uint8_t ty = 0; ty < RPS_TILEMAP_HEIGHT_TILES; ++ty) {
            RIA.rw0 = (ty >= surface_row) ? TILE_INDEX_GROUND : TILE_INDEX_SKY;
        }
    }
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

    return true;
}

void platform_video_render(const game_state_t *state, uint8_t subframe)
{
    uint16_t camera_world_x = interpolate_world_x(state->prev_world_x, state->world_x, subframe);

    render_terrain_tilemap(camera_world_x);
}