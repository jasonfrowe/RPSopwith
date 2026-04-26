#ifndef MENU_H
#define MENU_H

#include <stdbool.h>

#include "input.h"

void menu_init(void);
void menu_activate(void);
void menu_update(const input_actions_t *actions);
bool menu_is_active(void);
bool menu_consume_start_request(uint8_t *level, bool *enemies_enabled);

#endif // MENU_H
