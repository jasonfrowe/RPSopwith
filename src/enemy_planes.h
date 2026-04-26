#ifndef ENEMY_PLANES_H
#define ENEMY_PLANES_H

#include <stdbool.h>
#include <stdint.h>

void enemy_planes_init(void);
void enemy_planes_set_enabled(bool enabled);
void enemy_planes_set_level(uint8_t level);
void enemy_planes_update(uint16_t camera_world_x);
bool enemy_planes_check_shot_hit(uint16_t shot_world_x, int16_t shot_center_y,
                                 uint16_t *hit_world_x, int16_t *hit_center_y,
                                 int16_t *score_delta, bool *big_explosion);
bool enemy_planes_check_fragment_hit(uint16_t world_x, int16_t center_y, uint8_t half_size,
                                     uint16_t *hit_world_x, int16_t *hit_center_y,
                                     bool *big_explosion);
bool enemy_planes_check_player_collision(uint16_t player_world_x, int16_t player_plane_y,
                                         uint16_t player_prev_world_x, int16_t player_prev_plane_y,
                                         uint16_t *hit_world_x, int16_t *hit_center_y,
                                         bool *big_explosion);

#endif // ENEMY_PLANES_H