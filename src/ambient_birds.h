#ifndef AMBIENT_BIRDS_H
#define AMBIENT_BIRDS_H

#include <stdbool.h>
#include <stdint.h>

void ambient_birds_init(void);
void ambient_birds_update(uint16_t camera_world_x);
bool ambient_birds_check_plane_hit(uint16_t world_x, int16_t center_y,
								   uint8_t half_width_px, uint8_t half_height_px);
bool ambient_birds_check_projectile_hit(uint16_t world_x, int16_t center_y);
void ambient_birds_spawn_splat(uint16_t world_x, int16_t center_y);
void ambient_birds_spawn_scatter(uint16_t world_x, int16_t center_y);

#endif // AMBIENT_BIRDS_H