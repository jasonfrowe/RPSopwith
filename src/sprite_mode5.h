#ifndef SPRITE_MODE5_H
#define SPRITE_MODE5_H

#include <stdint.h>
#include <stdbool.h>

// Remove this when LLVM-MOS-SDK is updated for MODE-5.
typedef struct {
  int x_pos_px;
  int y_pos_px;
  unsigned xram_sprite_ptr;
  unsigned palette_ptr;
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

static const uint16_t enemy_palette[16] = {
    0x0000,
    0xFDEE,
    0x19EA,
    0xF83F,
    0xFFFF,
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

// Palette extracted from Sprites/targets_strip.png
static const uint16_t targets_palette[16] = {
    0x0000,
    0x7D36,
    0xA67B,
    0x3AAC,
    0x0020,
    0x0020,
    0x0020,
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

static const uint16_t player_targets_palette[16] = {
    0x0000,
    0xFFFF,
    0x02B5,
    0xAD60,
    0x0020,
    0x0020,
    0x0020,
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

// Palette extracted from Sprites/projectiles_strip.png
static const uint16_t projectiles_palette[16] = {
    0x0000,
    0xF7BE,
    0xB5B6,
    0x52AA,
    0x7FFF,
    0x8C71,
    0x0020,
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

void sprite_mode5_players_init(void);
void sprite_mode5_set_position(int16_t x, int16_t y);
void sprite_mode5_init_targets(void);
void sprite_mode5_init_projectiles(void);
void sprite_mode5_set_projectile(uint8_t slot, int16_t x, int16_t y,
                                 uint8_t frame_index, bool visible);

#endif // SPRITE_MODE5_H