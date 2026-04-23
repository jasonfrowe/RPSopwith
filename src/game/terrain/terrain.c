#include "game/terrain/terrain.h"

#include <stdbool.h>

#include "constants.h"
#include "game/terrain/original_ground_3000.h"

static uint8_t s_height[TERRAIN_WORLD_WIDTH];

void terrain_init(void)
{
    for (uint16_t x = 0; x < TERRAIN_WORLD_WIDTH; ++x) {
        uint16_t y = (uint16_t)((RPS_SOURCE_SCREEN_HEIGHT_PX - 1u) - s_original_ground[x]) +
                     RPS_TERRAIN_SCREEN_Y_OFFSET_PX;
        s_height[x] = (y > 239u) ? 239u : (uint8_t)y;
    }
}

uint8_t terrain_height_at_world_x(uint16_t world_x)
{
    return s_height[world_x % TERRAIN_WORLD_WIDTH];
}

void terrain_crater(uint16_t world_x, uint8_t radius, uint8_t depth)
{
    uint16_t center = (uint16_t)(world_x % TERRAIN_WORLD_WIDTH);

    for (int16_t dx = -(int16_t)radius; dx <= (int16_t)radius; ++dx) {
        uint16_t x = (uint16_t)((center + TERRAIN_WORLD_WIDTH + dx) % TERRAIN_WORLD_WIDTH);
        uint8_t attenuation = (uint8_t)((uint16_t)(dx < 0 ? -dx : dx) * depth / (radius == 0 ? 1 : radius));
        uint8_t carve = (attenuation >= depth) ? 0 : (uint8_t)(depth - attenuation);
        uint8_t base = s_height[x];

        if (carve > 0 && base < 238u) {
            uint16_t next = (uint16_t)base + carve;
            s_height[x] = (next > 239u) ? 239u : (uint8_t)next;
        }
    }
}