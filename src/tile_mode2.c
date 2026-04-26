#include <rp6502.h>
#include <stdbool.h>
#include <stdint.h>
#include "tile_mode2.h"
#include "constants.h"

unsigned TILE_GROUND_CONFIG;
unsigned TILE_HUD_CONFIG;

static uint8_t xram_read_u8(unsigned addr)
{
    RIA.addr0 = addr;
    RIA.step0 = 1;
    return RIA.rw0;
}

static uint8_t tile_pixel_index(uint8_t tile_id, uint8_t x, uint8_t y)
{
    unsigned addr = GROUND_TILES + ((unsigned)tile_id * 32u) + ((unsigned)y * 4u) + (x >> 1);
    uint8_t packed = xram_read_u8(addr);

    if ((x & 1u) == 0u) {
        return (uint8_t)((packed >> 4) & 0x0Fu);
    }

    return (uint8_t)(packed & 0x0Fu);
}

static int16_t wrap_world_px(int16_t x)
{
    int16_t world_width_px = (int16_t)(GROUND_WIDTH * 8);

    while (x < 0) {
        x = (int16_t)(x + world_width_px);
    }
    while (x >= world_width_px) {
        x = (int16_t)(x - world_width_px);
    }

    return x;
}

void tile_mode2_init(void) {
    TILE_GROUND_CONFIG = SPRITE_DATA_END; // Add after sprite config

    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, x_wrap, true);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, y_wrap, false);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, x_pos_px, 1500);
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

    tile_mode2_set_scroll_x((int16_t)PLAYER_START_WORLD_X_PX);
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
    uint8_t local_x = (uint8_t)(world_x_px & 0x07u);

    for (uint16_t row = 0; row < GROUND_HEIGHT; ++row) {
        unsigned tile_addr = GROUND_DATA + ((unsigned)row * GROUND_WIDTH) + tile_x;
        uint8_t tile_id = xram_read_u8(tile_addr);

        if (tile_id != 0u) {
            for (uint8_t y = 0; y < 8u; ++y) {
                if (tile_pixel_index(tile_id, local_x, y) == 2u) {
                    return (int16_t)((row * 8u) + y);
                }
            }
        }
    }

    return (int16_t)(SCREEN_HEIGHT - 1);
}

void tile_hud_init(void) {
    TILE_HUD_CONFIG = TILE_GROUND_CONFIG + sizeof(vga_mode2_config_t); // Add after ground config

    xram0_struct_set(TILE_HUD_CONFIG, vga_mode2_config_t, x_wrap, true);
    xram0_struct_set(TILE_HUD_CONFIG, vga_mode2_config_t, y_wrap, true);
    xram0_struct_set(TILE_HUD_CONFIG, vga_mode2_config_t, x_pos_px, 0);
    xram0_struct_set(TILE_HUD_CONFIG, vga_mode2_config_t, y_pos_px, 0);
    xram0_struct_set(TILE_HUD_CONFIG, vga_mode2_config_t, width_tiles,  HUD_MAP_WIDTH_TILES);
    xram0_struct_set(TILE_HUD_CONFIG, vga_mode2_config_t, height_tiles, HUD_MAP_HEIGHT_TILES);
    xram0_struct_set(TILE_HUD_CONFIG, vga_mode2_config_t, xram_data_ptr,    HUD_MAP_DATA); // tile ID grid
    xram0_struct_set(TILE_HUD_CONFIG, vga_mode2_config_t, xram_palette_ptr, HUD_PALETTE_ADDR);
    xram0_struct_set(TILE_HUD_CONFIG, vga_mode2_config_t, xram_tile_ptr,    HUD_TILES);        // tile bitmaps


    // Mode 2 args: MODE, OPTIONS, CONFIG, PLANE, BEGIN, END
    // OPTIONS: bit3=0 (8x8 tiles), bit[2:0]=2 (8-bit color index) => 0b0010 = 2
    // Plane 0 = background fill layer (behind sprite plane 1)
    if (xreg_vga_mode(2, 0x02, TILE_HUD_CONFIG, 1, 0, 0) < 0) {
        return;
    }

    RIA.addr0 = HUD_PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = hud_palette[i] & 0xFF;
        RIA.rw0 = hud_palette[i] >> 8;
    }

}