#include <rp6502.h>
#include <stdbool.h>
#include <stdint.h>
#include "ambient_birds.h"
#include "ambient_flocks.h"
#include "constants.h"
#include "ground_targets.h"
#include "input.h"
#include "projectiles.h"
#include "sprite_mode5.h"
#include "tile_mode2.h"
#include "flight.h"
#include "text_mode1.h"

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
    sprite_mode5_init_targets();
    sprite_mode5_init_projectiles();
    ground_targets_init();
    projectiles_init();
    ambient_flocks_init();
    ambient_birds_init();
    text_mode1_init();

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

        if (!flight_is_crashed()) {
            int16_t plane_center_y = (int16_t)(flight_plane_y_physics() + (PLAYER_SPRITE_SIZE_PX / 2));
            bool flock_hit = ambient_flocks_scatter_at(
                flight_world_x_physics(),
                plane_center_y,
                (uint8_t)(PLAYER_SPRITE_SIZE_PX / 2));
            bool bird_hit = ambient_birds_check_plane_hit(
                flight_world_x_physics(),
                plane_center_y,
                (uint8_t)(PLAYER_SPRITE_SIZE_PX / 2));

            if (flock_hit || bird_hit) {
                flight_apply_debris_hit();
                if (flock_hit) {
                    ambient_birds_spawn_splat(flight_world_x_physics(), plane_center_y);
                }
            }
        }

        {
            uint16_t crash_wx = 0;
            int16_t crash_cy = 0;
            bool apply_crater = false;
            bool big_explosion = false;
            if (flight_consume_plane_explosion(&crash_wx, &crash_cy, &apply_crater,
                                               &big_explosion)) {
                projectiles_spawn_crash_explosion(crash_wx, crash_cy, big_explosion);
                text_mode1_score_crash();
                if (apply_crater) {
                    flight_apply_bomb_crater(crash_wx);
                }
            }
        }

        projectiles_update(flight_world_x(), input_last_actions());
        ground_targets_update(flight_world_x());
        ambient_flocks_update(flight_world_x());
        ambient_birds_update(flight_world_x());

    }

    return 0;
}