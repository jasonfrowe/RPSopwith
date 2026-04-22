#ifndef GAME_TERRAIN_TERRAIN_H
#define GAME_TERRAIN_TERRAIN_H

#include <stdint.h>

#include "constants.h"

void terrain_init(void);
uint8_t terrain_height_at_world_x(uint16_t world_x);
void terrain_crater(uint16_t world_x, uint8_t radius, uint8_t depth);

#endif