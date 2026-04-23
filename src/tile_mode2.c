#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "tile_mode2.h"
#include "sprite_mode5.h"


unsigned TILE_GROUND_CONFIG;

static uint8_t xram_read_u8(unsigned addr)
{
    RIA.addr0 = addr;
    RIA.step0 = 1;
    return RIA.rw0;
}

static int16_t wrap_world_px(int32_t x)
{
    int32_t world_width_px = (int32_t)GROUND_WIDTH * 8;
    int32_t wrapped = x % world_width_px;
    if (wrapped < 0) {
        wrapped += world_width_px;
    }
    return (int16_t)wrapped;
}

void tile_mode2_init(void) {
    TILE_GROUND_CONFIG = PLAYER_CONFIG + sizeof(vga_mode5_sprite_t); // Add after sprite config

    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, x_wrap, true);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, y_wrap, true);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, x_pos_px, 0);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, y_pos_px, 0);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, width_tiles,  GROUND_WIDTH);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, height_tiles, GROUND_HEIGHT);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, xram_data_ptr,    GROUND_DATA); // tile ID grid
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, xram_palette_ptr, TILE_GROUND_PALETTE_ADDR);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, xram_tile_ptr,    GROUND_TILES);        // tile bitmaps


    // Mode 2 args: MODE, OPTIONS, CONFIG, PLANE, BEGIN, END
    // OPTIONS: bit3=0 (8x8 tiles), bit[2:0]=2 (8-bit color index) => 0b0010 = 2
    // Plane 0 = background fill layer (behind sprite plane 1)
    if (xreg_vga_mode(2, 0x02, TILE_GROUND_CONFIG, 0, 0, 0) < 0) {
        return;
    }

    RIA.addr0 = TILE_GROUND_PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = tile_bg_palette[i] & 0xFF;
        RIA.rw0 = tile_bg_palette[i] >> 8;
    }

}

void tile_mode2_set_scroll_x(int16_t world_scroll_px)
{
    int16_t wrapped_world_x = wrap_world_px(world_scroll_px);
    int16_t camera_left = (int16_t)(wrapped_world_x - (SCREEN_WIDTH / 2));
    int16_t wrapped_left = wrap_world_px(camera_left);

    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, x_pos_px, (int16_t)(-wrapped_left));
}

int16_t tile_mode2_ground_y_at_world_x(uint16_t world_x_px)
{
    uint16_t tile_x = (uint16_t)((world_x_px >> 3) % GROUND_WIDTH);

    for (uint16_t row = 0; row < GROUND_HEIGHT; ++row) {
        unsigned tile_addr = GROUND_DATA + ((unsigned)row * GROUND_WIDTH) + tile_x;
        if (xram_read_u8(tile_addr) != 0u) {
            return (int16_t)(row * 8);
        }
    }

    return (int16_t)(SCREEN_HEIGHT - 1);
}