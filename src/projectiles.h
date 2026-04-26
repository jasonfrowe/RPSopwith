#ifndef PROJECTILES_H
#define PROJECTILES_H

#include <stdbool.h>
#include <stdint.h>

#include "input.h"

void projectiles_init(void);
void projectiles_update(uint16_t camera_world_x, const input_actions_t *actions);
void projectiles_spawn_crash_explosion(uint16_t world_x, int16_t center_y, bool big_explosion);

#endif // PROJECTILES_H