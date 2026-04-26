#include <rp6502.h>
#include <stdbool.h>
#include <stdint.h>
#include "ambient_birds.h"
#include "ambient_flocks.h"
#include "constants.h"
#include "input.h"
#include "flight.h"
#include "enemy_planes.h"
#include "ground_targets.h"
#include "resources.h"
#include "projectiles.h"
#include "sprite_mode5.h"
#include "tile_mode2.h"
#include "text_mode1.h"
#include "menu.h"

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
    resources_init();
    ground_targets_init();
    projectiles_init();
    enemy_planes_init();
    ambient_flocks_init();
    ambient_birds_init();

    return true;
}

static uint8_t s_vsync_last = 0;

static void wait_for_vsync(void)
{
    while (RIA.vsync == s_vsync_last) {
    }
    s_vsync_last = RIA.vsync;
}

static void update_player_projectiles(const input_actions_t *actions)
{
    enemy_planes_update(flight_world_x());
    projectiles_update(flight_world_x(), actions);
    ground_targets_update(flight_world_x());
    ambient_flocks_update(flight_world_x());
    ambient_birds_update(flight_world_x());
}

static void update_player(const input_actions_t *actions)
{
    flight_update(actions);

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
}

int main(void)
{
    input_actions_t actions;

    input_init();

    if (!init_graphics()) {
        return 1;
    }

    menu_init();

    while (true) {
        wait_for_vsync();

        input_poll(&actions);

        if (menu_is_active()) {
            menu_update(&actions);
        } else {
            update_player(&actions);
            update_player_projectiles(&actions);
        }
    }
}