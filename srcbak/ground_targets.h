#ifndef GROUND_TARGETS_H
#define GROUND_TARGETS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum ground_target_hit_type_e {
	GROUND_TARGET_HIT_NONE = 0,
	GROUND_TARGET_HIT_NO_EXPLOSION,
	GROUND_TARGET_HIT_NORMAL,
	GROUND_TARGET_HIT_EXPLOSIVE
} ground_target_hit_type_t;

void ground_targets_init(void);
void ground_targets_update(uint16_t camera_world_x);
bool ground_targets_all_enemy_targets_destroyed(void);
ground_target_hit_type_t ground_targets_check_hit(uint16_t proj_world_x, int16_t proj_center_y,
								  uint16_t *hit_world_x, int16_t *hit_center_y,
								  int16_t *score_delta);
ground_target_hit_type_t ground_targets_check_shot_hit(uint16_t shot_world_x, int16_t shot_center_y,
								   uint16_t *hit_world_x, int16_t *hit_center_y,
								   int16_t *score_delta);
ground_target_hit_type_t ground_targets_check_plane_collision(uint16_t plane_world_x, int16_t plane_top_y,
								  uint16_t *hit_world_x, int16_t *hit_center_y,
								  int16_t *score_delta);

#endif // GROUND_TARGETS_H
