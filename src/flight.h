#ifndef FLIGHT_H
#define FLIGHT_H

#include "input.h"

void flight_init(void);
void flight_update(const input_actions_t *actions);
uint16_t flight_world_x(void);
int16_t flight_plane_y(void);
uint8_t flight_plane_pitch(void);
bool flight_plane_orient(void);
bool flight_is_crashed(void);

#endif // FLIGHT_H
