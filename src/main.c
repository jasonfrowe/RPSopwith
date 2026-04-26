#include <rp6502.h>
#include <stdbool.h>
#include <stdint.h>
#include "constants.h"
#include "tile_mode2.h"
#include "sprite_mode5.h"
#include "text_mode1.h"

static bool init_graphics(void)
{
    // 320×240 canvas
    int rc;
    rc = xreg_vga_canvas(1);
    if (rc < 0) {
        return false;
    }

    tile_mode2_init();
    tile_hud_init();
    sprite_mode5_players_init();
    sprite_mode5_init_targets();
    text_mode1_init();

    return true;
}

uint8_t vsync_last = 0;

int main(void)
{

    // Initialize input
    xreg(0, 0, 0, KEYBOARD_INPUT);
    xreg(0, 0, 2, GAMEPAD_INPUT);

    // Initialise graphics
    if (!init_graphics()) {
        return 1;
    }

    // Main loop
    while (true) {
        // 1. SYNC
        if (RIA.vsync == vsync_last) continue;
        vsync_last = RIA.vsync;
    }

}