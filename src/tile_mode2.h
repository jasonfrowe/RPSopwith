#ifndef TILE_MODE2_H
#define TILE_MODE2_H

#include <stdbool.h>
#include <stdint.h>

// Palette extracted from Sprites/terrain_tileset.png
static const uint16_t tile_bg_palette[16] = {
    0x0000,
    0xFDEF,
    0x552A,
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
    0x0020,
};

// Palette extracted from Sprites/HUD_tiles.png
static const uint16_t hud_palette[16] = {
    0x0000,
    0xA820,
    0x0560,
    0xAD60,
    0x0035,
    0xA835,
    0x02B5,
    0xAD75,
    0x52AA,
    0xFAAA,
    0x57EA,
    0xFFEA,
    0x52BF,
    0xFABF,
    0x57FF,
    0xFFFF,
};

void tile_mode2_init(void);
void tile_mode2_set_scroll_x(int16_t world_scroll_px);
void tile_mode2_reset_ground_map(void);
int16_t tile_mode2_ground_y_at_world_x(uint16_t world_x_px);
void tile_mode2_update_ground_column(uint16_t world_x_px, const uint8_t *column_samples);
void tile_hud_init(void);

#endif // TILE_MODE2_H