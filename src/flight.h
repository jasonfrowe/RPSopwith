#ifndef FLIGHT_H
#define FLIGHT_H

#include "input.h"

void flight_init(void);
void flight_update(const input_actions_t *actions);
uint16_t flight_world_x(void);

#endif // FLIGHT_H
