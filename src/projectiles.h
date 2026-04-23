#ifndef PROJECTILES_H
#define PROJECTILES_H

#include <stdint.h>

#include "input.h"

void projectiles_init(void);
void projectiles_update(uint16_t camera_world_x, const input_actions_t *actions);

#endif // PROJECTILES_H