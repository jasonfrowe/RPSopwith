#ifndef MENU_H
#define MENU_H

#include <stdbool.h>

#include "input.h"

void menu_init(void);
void menu_update(const input_actions_t *actions);
bool menu_is_active(void);

#endif // MENU_H
