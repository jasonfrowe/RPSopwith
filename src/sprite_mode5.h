#ifndef SPRITE_MODE5_H
#define SPRITE_MODE5_H

#include <stdint.h>
#include <stdbool.h>

// Remove this when LLVM-MOS-SDK is updated for MODE-5.
typedef struct {
    int16_t x_pos_px;
    int16_t y_pos_px;
    uint16_t xram_sprite_ptr;
    uint16_t palette_ptr;
} vga_mode5_sprite_t;


// Palette extracted from Sprites/player_bank_strip.png
static const uint16_t player_palette[16] = {
    0x0000,
    0xFDEE,
    0x19EA,
    0xFFFF,
    0x0030,
    0x0520,
    0x57FF,
    0x0020,
    0x0020,
    0x0020,
    0x0020,
    0x0020,
    0x0020,
    0x0020,
    0x0020,
    0x0020,
};

void sprite_mode5_init(void);
void sprite_mode5_set_position(int16_t x, int16_t y);
void sprite_mode5_init_targets(void);
void sprite_mode5_set_target(uint8_t slot, int16_t x, int16_t y, uint8_t frame_index, bool visible);

#endif // SPRITE_MODE5_H