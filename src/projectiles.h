#ifndef PROJECTILES_H
#define PROJECTILES_H

#include <stdbool.h>
#include <stdint.h>

#include "input.h"

void projectiles_init(void);
void projectiles_update(uint16_t camera_world_x, const input_actions_t *actions);
void projectiles_spawn_crash_explosion(uint16_t world_x, int16_t center_y, bool big_explosion);
bool projectiles_spawn_enemy_shot(uint16_t world_x, int16_t center_y,
								  uint8_t angle, uint8_t speed);
bool projectiles_spawn_smoke_trail(uint16_t world_x, int16_t center_y,
                                   int8_t vx, int8_t vy);

#endif // PROJECTILES_H