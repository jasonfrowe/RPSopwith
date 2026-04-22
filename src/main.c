#include <rp6502.h>
#include <stdio.h>

#include "game/game.h"
#include "platform/frame_sync.h"
#include "platform/input.h"

int main(void)
{
    input_actions_t actions;
    game_state_t game;
    uint8_t subframe;

    if (xreg_vga_canvas(1) < 0) {
        puts("VGA init failed");
        return 1;
    }

    platform_input_init();
    frame_sync_init();
    game_init(&game);

    puts("RPSopwith Phase 0 runtime");

    while (1) {
        frame_sync_wait_vblank();
        subframe = frame_sync_subframe_60_to_10hz();

        platform_input_poll(&actions);

        if (subframe == 0) {
            game_tick_10hz(&game, &actions);
        }

        game_render_interpolated(&game, subframe);
    }
}
