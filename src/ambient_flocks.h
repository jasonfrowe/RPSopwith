#ifndef AMBIENT_FLOCKS_H
#define AMBIENT_FLOCKS_H

#include <stdbool.h>
#include <stdint.h>

void ambient_flocks_init(void);
void ambient_flocks_update(uint16_t camera_world_x);
uint8_t ambient_flocks_get_minimap_flocks(uint16_t *world_x, int16_t *center_y,
										  uint8_t max_points);
bool ambient_flocks_scatter_at(uint16_t world_x, int16_t center_y, uint8_t half_size_px);

#endif // AMBIENT_FLOCKS_H
