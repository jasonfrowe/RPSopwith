#include <rp6502.h>
#include <stdbool.h>
#include <stdint.h>
#include "constants.h"
#include "input.h"
#include "flight.h"
#include "tile_mode2.h"
#include "sprite_mode5.h"
#include "text_mode1.h"

static bool init_graphics(void)
{
    int rc;

    rc = xreg_vga_canvas(1);
    if (rc < 0) {
        return false;
    }

    tile_mode2_init();
    tile_hud_init();
    sprite_mode5_players_init();
    sprite_mode5_init_targets();
    sprite_mode5_init_projectiles();
    text_mode1_init();

    return true;
}

static uint8_t s_vsync_last = 0;

static void wait_for_vsync(void)
{
    while (RIA.vsync == s_vsync_last) {
    }
    s_vsync_last = RIA.vsync;
}

int main(void)
{
    input_actions_t actions;

    input_init();

    if (!init_graphics()) {
        return 1;
    }

    flight_init();

    while (true) {
        wait_for_vsync();

        input_poll(&actions);
        flight_update(&actions);
    }
}