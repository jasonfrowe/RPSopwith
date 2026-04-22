#ifndef PLATFORM_INPUT_H
#define PLATFORM_INPUT_H

#include <stdbool.h>

typedef struct input_actions_s {
    bool up;
    bool down;
    bool left;
    bool right;
    bool land;
    bool flip;
    bool fire;
    bool bomb;
    bool start;
} input_actions_t;

void platform_input_init(void);
void platform_input_poll(input_actions_t *actions);

#endif