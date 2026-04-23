#include <rp6502.h>
#include <stdbool.h>
#include <stdint.h>
#include "constants.h"
#include "input.h"
#include "sprite_mode5.h"
#include "tile_mode2.h"

static bool init_graphics(void)
{
    // 320×240 canvas
    int rc;
    rc = xreg_vga_canvas(1);
    if (rc < 0) {
        return false;
    }

    sprite_mode5_init();
    tile_mode2_init();

    return true;
}

uint8_t vsync_last = 0;

int main(void)
{
    input_init();

    // Initialise graphics
    if (!init_graphics()) {
        return 1;
    }

    input_flight_init();

    // Main loop
    while (true) {
        // 1. SYNC
        if (RIA.vsync == vsync_last) continue;
        vsync_last = RIA.vsync;

        input_flight_update();

    }

    return 0;
}