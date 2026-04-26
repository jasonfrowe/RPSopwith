#ifndef FLIGHT_H
#define FLIGHT_H

#include <stdbool.h>
#include <stdint.h>

#include "input.h"

void flight_init(void);
void flight_update(const input_actions_t *actions);
uint16_t flight_world_x(void);
uint16_t flight_world_x_physics(void);
int16_t flight_plane_y(void);
int16_t flight_plane_y_physics(void);
uint8_t flight_plane_pitch(void);
uint8_t flight_plane_speed(void);
bool flight_plane_orient(void);
bool flight_is_airborne(void);
bool flight_is_at_home_base(void);
bool flight_can_start_landing(void);
bool flight_is_crashed(void);
bool flight_is_wounded(void);
bool flight_is_falling(void);
void flight_apply_debris_hit(void);
void flight_apply_bomb_hit(uint16_t impact_world_x, int16_t impact_center_y);
bool flight_consume_plane_explosion(uint16_t *world_x, int16_t *center_y,
									bool *apply_crater,
									bool *big_explosion);
int16_t flight_terrain_y_at(uint16_t world_x);
void flight_apply_bomb_crater(uint16_t impact_world_x);

#endif // FLIGHT_H