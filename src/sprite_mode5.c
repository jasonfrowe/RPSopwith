#include <rp6502.h>
#include <stdint.h>
#include "constants.h"
#include "sprite_mode5.h"
#include "tile_mode2.h"

// Store the player config address for updates
unsigned PLAYER_CONFIG;
unsigned TARGETS_CONFIG;

void sprite_mode5_init(void) {
    int16_t center_x = (int16_t)((SCREEN_WIDTH - PLAYER_SPRITE_SIZE_PX) / 2);
    int16_t center_y = (int16_t)((SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX) * 2 / 3); // Start slightly lower than center for better composition

    PLAYER_CONFIG = SPRITE_DATA_END; // Just after the end of sprite data

    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, x_pos_px, center_x);
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, y_pos_px, center_y);
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, xram_sprite_ptr, PLAYER_DATA);
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, palette_ptr, PLAYER_PALETTE_ADDR);

    // Mode 5 args: MODE, OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    if (xreg_vga_mode(5, 0x0A, PLAYER_CONFIG, 1, 2, 0, 0) < 0) {
        return;
    }


    RIA.addr0 = PLAYER_PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = player_palette[i] & 0xFF;
        RIA.rw0 = player_palette[i] >> 8;
    }
}

void sprite_mode5_init_targets(void) {
    TARGETS_CONFIG = TILE_GROUND_CONFIG + sizeof(vga_mode2_config_t); // Just after tile HUD config

    for (uint8_t i = 0; i < MAX_TARGETS; i++) {

        unsigned ptr = TARGETS_CONFIG + (i * sizeof(vga_mode5_sprite_t));

        xram0_struct_set(ptr, vga_mode5_sprite_t, x_pos_px, -32); // Start off-screen
        xram0_struct_set(ptr, vga_mode5_sprite_t, y_pos_px, -32);
        xram0_struct_set(ptr, vga_mode5_sprite_t, xram_sprite_ptr, TARGETS_DATA);
        xram0_struct_set(ptr, vga_mode5_sprite_t, palette_ptr, TARGETS_PALETTE_ADDR);
    }

    // Mode 5 args: MODE, OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    if (xreg_vga_mode(5, 0x0A, TARGETS_CONFIG, MAX_TARGETS, 0, 0, 0) < 0) {
        return;
    }

    RIA.addr0 = TARGETS_PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = targets_palette[i] & 0xFF;
        RIA.rw0 = targets_palette[i] >> 8;
    }
}


void sprite_mode5_set_position(int16_t x, int16_t y)
{
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, x_pos_px, x);
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, y_pos_px, y);
}

void sprite_mode5_set_target(uint8_t slot, int16_t x, int16_t y, uint8_t frame_index, bool visible)
{
    unsigned ptr;
    uint16_t sprite_ptr;

    if (slot >= MAX_TARGETS) {
        return;
    }

    ptr = TARGETS_CONFIG + ((unsigned)slot * sizeof(vga_mode5_sprite_t));
    if (!visible) {
        xram0_struct_set(ptr, vga_mode5_sprite_t, x_pos_px, -32);
        xram0_struct_set(ptr, vga_mode5_sprite_t, y_pos_px, -32);
        return;
    }

    sprite_ptr = (uint16_t)(TARGETS_DATA + ((unsigned)frame_index * TARGETS_FRAME_SIZE));
    xram0_struct_set(ptr, vga_mode5_sprite_t, x_pos_px, x);
    xram0_struct_set(ptr, vga_mode5_sprite_t, y_pos_px, y);
    xram0_struct_set(ptr, vga_mode5_sprite_t, xram_sprite_ptr, sprite_ptr);
}