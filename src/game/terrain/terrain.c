#include "game/terrain/terrain.h"

#include <stdbool.h>

static uint8_t s_height[TERRAIN_WORLD_WIDTH];

void terrain_init(void)
{
    uint16_t seed = 0x6502u;
    uint8_t y = 188u;

    for (uint16_t x = 0; x < TERRAIN_WORLD_WIDTH; ++x) {
        int8_t delta;

        seed = (uint16_t)(seed * 109u + 89u);
        delta = (int8_t)((seed >> 8) & 0x03u) - 1;

        if ((seed & 0x001Fu) == 0) {
            delta = (int8_t)((seed >> 5) & 0x07u) - 3;
        }

        if (delta > 0 && y < 220u) {
            y = (uint8_t)(y + (uint8_t)delta);
        } else if (delta < 0 && y > 120u) {
            y = (uint8_t)(y - (uint8_t)(-delta));
        }

        s_height[x] = y;
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