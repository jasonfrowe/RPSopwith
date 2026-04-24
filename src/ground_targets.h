#ifndef GROUND_TARGETS_H
#define GROUND_TARGETS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum ground_target_hit_type_e {
	GROUND_TARGET_HIT_NONE = 0,
	GROUND_TARGET_HIT_NORMAL,
	GROUND_TARGET_HIT_EXPLOSIVE
} ground_target_hit_type_t;

void ground_targets_init(void);
void ground_targets_update(uint16_t camera_world_x);
ground_target_hit_type_t ground_targets_check_hit(uint16_t proj_world_x, int16_t proj_center_y,
												  uint16_t *hit_world_x, int16_t *hit_center_y);

#endif // GROUND_TARGETS_H
