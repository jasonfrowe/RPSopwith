#include "platform/input.h"

#include <rp6502.h>
#include <string.h>

enum {
    KEYBOARD_INPUT_ADDR = 0xFFA0,
    GAMEPAD_INPUT_ADDR = 0xFF78
};

void platform_input_init(void)
{
    xreg(0, 0, 0, KEYBOARD_INPUT_ADDR);
    xreg(0, 0, 2, GAMEPAD_INPUT_ADDR);
}

void platform_input_poll(input_actions_t *actions)
{
    memset(actions, 0, sizeof(*actions));

    // Input mappings are implemented in the next milestone.
}