#include <rp6502.h>
#include "sprite_mode5.h"
#include "constants.h"

// Store the player config address for updates
unsigned PLAYER_CONFIG;
unsigned TARGETS_CONFIG;
unsigned PROJECTILES_CONFIG;
unsigned ENEMY_CONFIG;

void sprite_mode5_players_init(void){

    PLAYER_CONFIG = TILE_GROUND_CONFIG + (2u * sizeof(vga_mode2_config_t)); // Add after hud config

    // Start off-screen; flight_init will set the correct runway position on the first frame
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, x_pos_px, 160);
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, y_pos_px, 111);
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, xram_sprite_ptr, PLAYER_DATA);
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, palette_ptr, PLAYER_PALETTE_ADDR);

    ENEMY_CONFIG = PLAYER_CONFIG + sizeof(vga_mode5_sprite_t); // Just after player config
    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {

        unsigned ptr = ENEMY_CONFIG + (i * sizeof(vga_mode5_sprite_t));

        xram0_struct_set(ptr, vga_mode5_sprite_t, x_pos_px, -32); // Start off-screen
        xram0_struct_set(ptr, vga_mode5_sprite_t, y_pos_px, -32);
        xram0_struct_set(ptr, vga_mode5_sprite_t, xram_sprite_ptr, PLAYER_DATA);
        xram0_struct_set(ptr, vga_mode5_sprite_t, palette_ptr, ENEMY_PALETTE_ADDR);
    }

    // Mode 5 args: MODE, OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    if (xreg_vga_mode(5, 0x0A, PLAYER_CONFIG, 1 + MAX_ENEMIES, 2, 0, 0) < 0) {
        return;
    }


    RIA.addr0 = PLAYER_PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = player_palette[i] & 0xFF;
        RIA.rw0 = player_palette[i] >> 8;
    }

    RIA.addr0 = ENEMY_PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = enemy_palette[i] & 0xFF;
        RIA.rw0 = enemy_palette[i] >> 8;
    }

}

void sprite_mode5_init_targets(void) {
    TARGETS_CONFIG = ENEMY_CONFIG + (MAX_ENEMIES * sizeof(vga_mode5_sprite_t)); // Just after enemy config

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

    RIA.addr0 = PLAYER_TARGETS_PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = player_targets_palette[i] & 0xFF;
        RIA.rw0 = player_targets_palette[i] >> 8;
    }
}

void sprite_mode5_init_projectiles(void) {
    PROJECTILES_CONFIG = TARGETS_CONFIG + MAX_TARGETS * sizeof(vga_mode5_sprite_t); // Just after tile HUD config

    for (uint8_t i = 0; i < MAX_PROJECTILES; i++) {

        unsigned ptr = PROJECTILES_CONFIG + (i * sizeof(vga_mode5_sprite_t));

        xram0_struct_set(ptr, vga_mode5_sprite_t, x_pos_px, -32); // Start off-screen
        xram0_struct_set(ptr, vga_mode5_sprite_t, y_pos_px, -32);
        xram0_struct_set(ptr, vga_mode5_sprite_t, xram_sprite_ptr, PROJECTILE_DATA);
        xram0_struct_set(ptr, vga_mode5_sprite_t, palette_ptr, PROJECTILE_PALETTE_ADDR);
    }

    // Mode 5 args: MODE, OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    if (xreg_vga_mode(5, 0x0A, PROJECTILES_CONFIG, MAX_PROJECTILES, 1, 0, 0) < 0) {
        return;
    }

    RIA.addr0 = PROJECTILE_PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = projectiles_palette[i] & 0xFF;
        RIA.rw0 = projectiles_palette[i] >> 8;
    }
}

void sprite_mode5_set_position(int16_t x, int16_t y)
{
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, x_pos_px, x);
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, y_pos_px, y);
}

void sprite_mode5_set_target(uint8_t slot, int16_t x, int16_t y, uint8_t frame_index,
                             uint16_t palette_ptr, bool visible)
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
    xram0_struct_set(ptr, vga_mode5_sprite_t, palette_ptr, palette_ptr);
}

void sprite_mode5_set_projectile(uint8_t slot, int16_t x, int16_t y,
                                 uint8_t frame_index, bool visible)
{
    unsigned ptr;
    uint16_t sprite_ptr;

    if (slot >= MAX_PROJECTILES) {
        return;
    }

    ptr = PROJECTILES_CONFIG + ((unsigned)slot * sizeof(vga_mode5_sprite_t));
    if (!visible) {
        xram0_struct_set(ptr, vga_mode5_sprite_t, x_pos_px, -32);
        xram0_struct_set(ptr, vga_mode5_sprite_t, y_pos_px, -32);
        return;
    }

    sprite_ptr = (uint16_t)(PROJECTILE_DATA + ((unsigned)frame_index * PROJECTILE_FRAME_SIZE));
    xram0_struct_set(ptr, vga_mode5_sprite_t, x_pos_px, x);
    xram0_struct_set(ptr, vga_mode5_sprite_t, y_pos_px, y);
    xram0_struct_set(ptr, vga_mode5_sprite_t, xram_sprite_ptr, sprite_ptr);
}

void sprite_mode5_set_enemy(uint8_t slot, int16_t x, int16_t y,
                            uint8_t frame_index, bool visible)
{
    unsigned ptr;
    uint16_t sprite_ptr;

    if (slot >= MAX_ENEMIES) {
        return;
    }

    ptr = ENEMY_CONFIG + ((unsigned)slot * sizeof(vga_mode5_sprite_t));
    if (!visible) {
        xram0_struct_set(ptr, vga_mode5_sprite_t, x_pos_px, -32);
        xram0_struct_set(ptr, vga_mode5_sprite_t, y_pos_px, -32);
        return;
    }

    sprite_ptr = (uint16_t)(PLAYER_DATA + ((unsigned)frame_index * PLAYER_FRAME_SIZE));
    xram0_struct_set(ptr, vga_mode5_sprite_t, x_pos_px, x);
    xram0_struct_set(ptr, vga_mode5_sprite_t, y_pos_px, y);
    xram0_struct_set(ptr, vga_mode5_sprite_t, xram_sprite_ptr, sprite_ptr);
}
