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

void tile_mode2_init(void);
void tile_mode2_set_scroll_x(int16_t world_scroll_px);
int16_t tile_mode2_ground_y_at_world_x(uint16_t world_x_px);

#endif // TILE_MODE2_H