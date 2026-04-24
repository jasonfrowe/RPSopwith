#include <rp6502.h>
#include <stdbool.h>
#include <stdint.h>
#include "ambient_birds.h"
#include "ambient_flocks.h"
#include "constants.h"
#include "ground_targets.h"
#include "input.h"
#include "projectiles.h"
#include "resources.h"
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
    tile_hud_init();
    resources_init();

    return true;
}

typedef enum game_mode_s {
    GAME_MODE_PLAYING = 0,
    GAME_MODE_END,
    GAME_MODE_WIN
} game_mode_t;

enum {
    GAME_TICK_DIV_10HZ = 6u,
    END_TIMEOUT_TICKS_10HZ = 20u,
    WIN_TIMEOUT_TICKS_10HZ = 72u,
    WIN_FRAME_HOLD_TICKS_10HZ = 18u,
    END_TEXT_COL = ((SCREEN_WIDTH / 16) - 4),
    END_TEXT_ROW = 12,
    WIN_FRAME_RIGHT_BASE = 40u,
    WIN_FRAME_FLIPPED_BASE = 44u
};

static game_mode_t s_game_mode = GAME_MODE_PLAYING;
static uint8_t s_game_tick_div = 0;
static uint8_t s_mode_timer_10hz = 0;
static bool s_win_flipped = false;

static void set_player_frame(uint8_t frame_index)
{
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, xram_sprite_ptr,
                     (uint16_t)(PLAYER_DATA + ((unsigned)frame_index * PLAYER_FRAME_SIZE)));
}

static void reset_level(void)
{
    tile_mode2_reset_ground_map();
    ground_targets_init();
    projectiles_init();
    ambient_flocks_init();
    ambient_birds_init();
    resources_init();
    text_mode1_reset_score();
    text_mode1_put_string(END_TEXT_COL, END_TEXT_ROW, 0x00, "       ");
    flight_init();

    s_game_mode = GAME_MODE_PLAYING;
    s_mode_timer_10hz = 0;
    s_game_tick_div = 0;
}

static void enter_end_mode(void)
{
    s_game_mode = GAME_MODE_END;
    s_mode_timer_10hz = END_TIMEOUT_TICKS_10HZ;
    s_game_tick_div = 0;
    text_mode1_put_string(END_TEXT_COL, END_TEXT_ROW, 0x0B, "THE END");
}

static void enter_win_mode(void)
{
    s_game_mode = GAME_MODE_WIN;
    s_mode_timer_10hz = WIN_TIMEOUT_TICKS_10HZ;
    s_game_tick_div = 0;
    s_win_flipped = flight_plane_orient();
}

static void update_win_animation_frame(void)
{
    uint8_t ticks_left = s_mode_timer_10hz;
    uint8_t block;
    uint8_t frame_index;

    if (ticks_left == 0u) {
        ticks_left = 1u;
    }

    block = (uint8_t)((ticks_left - 1u) / WIN_FRAME_HOLD_TICKS_10HZ);
    if (block > 3u) {
        block = 3u;
    }

    frame_index = (uint8_t)((s_win_flipped ? WIN_FRAME_FLIPPED_BASE : WIN_FRAME_RIGHT_BASE) + block);
    set_player_frame(frame_index);
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

        if (s_game_mode == GAME_MODE_PLAYING) {
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

            if (flight_is_crashed() && !resources_can_respawn()) {
                enter_end_mode();
            } else if (!flight_is_crashed() && ground_targets_all_enemy_targets_destroyed()) {
                enter_win_mode();
            }
        } else {
            if (++s_game_tick_div >= GAME_TICK_DIV_10HZ) {
                s_game_tick_div = 0;
                if (s_mode_timer_10hz > 0u) {
                    --s_mode_timer_10hz;
                }
            }

            if (s_game_mode == GAME_MODE_WIN) {
                update_win_animation_frame();
            }

            if (s_mode_timer_10hz == 0u) {
                reset_level();
            }
        }

    }

    return 0;
}