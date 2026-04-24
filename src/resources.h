#ifndef RESOURCES_H
#define RESOURCES_H

#include <stdbool.h>
#include <stdint.h>

void resources_init(void);
bool resources_tick_10hz(bool at_home, bool airborne, bool crashed, bool falling, uint8_t speed);

bool resources_try_consume_bullet(void);
bool resources_try_consume_bomb(void);
bool resources_try_consume_missile(void);

void resources_on_plane_lost(void);
bool resources_can_respawn(void);
void resources_on_respawn(void);

uint8_t resources_lives(void);
uint16_t resources_fuel(void);
uint16_t resources_bullets(void);
uint8_t resources_bombs(void);
uint8_t resources_missiles(void);

#endif // RESOURCES_H
