#ifndef GROUND_TARGETS_H
#define GROUND_TARGETS_H

#include <stdbool.h>
#include <stdint.h>

void ground_targets_init(void);
void ground_targets_update(uint16_t camera_world_x);
bool ground_targets_check_hit(uint16_t proj_world_x, int16_t proj_center_y);

#endif // GROUND_TARGETS_H
