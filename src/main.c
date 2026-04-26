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

typedef enum game_mode_e {
    GAME_MODE_MENU = 0,
    GAME_MODE_PLAYING,
    GAME_MODE_LEVEL_COMPLETE,
    GAME_MODE_GAME_OVER
} game_mode_t;

enum {
    GAME_TICK_DIV_10HZ = 6u,
    WIN_TIMEOUT_TICKS_10HZ = 72u,
    WIN_FRAME_HOLD_TICKS_10HZ = 18u,
    WIN_FRAME_RIGHT_BASE = 40u,
    WIN_FRAME_FLIPPED_BASE = 44u,
    GAME_OVER_DELAY_TICKS_10HZ = 40u,
    MAX_GAME_LEVEL = 10u,
    STATUS_ROW_MAIN = 14u,
    STATUS_ROW_SUB = 15u
};

static game_mode_t s_game_mode = GAME_MODE_MENU;
static uint8_t s_mode_tick_div = 0u;
static uint8_t s_mode_timer_10hz = 0u;
static uint8_t s_current_level = 1u;
static bool s_current_enemies_enabled = true;
static bool s_crash_accounted = false;
static bool s_win_flipped = false;

static bool init_video(void)
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

static void init_world(void)
{
    resources_init();
    ground_targets_init();
    projectiles_init();
    enemy_planes_init();
    ambient_flocks_init();
    ambient_birds_init();
}

static void reset_level_world(void)
{
    flight_set_level(s_current_level);
    enemy_planes_set_level(s_current_level);
    enemy_planes_set_enabled(s_current_enemies_enabled);
    ground_targets_set_level(s_current_level);
    ground_targets_init();
    projectiles_init();
    enemy_planes_init();
    ambient_flocks_init();
    ambient_birds_init();
    flight_init();
}

static uint8_t s_vsync_last = 0;

static void wait_for_vsync(void)
{
    while (RIA.vsync == s_vsync_last) {
    }
    s_vsync_last = RIA.vsync;
}

static void clear_status_rows(void)
{
    text_mode1_put_string(0, STATUS_ROW_MAIN, 0, "                                        ");
    text_mode1_put_string(0, STATUS_ROW_SUB, 0, "                                        ");
}

static void show_game_over_text(void)
{
    clear_status_rows();
    text_mode1_put_string(16, STATUS_ROW_MAIN, 13, "THE END");
}

static void show_level_complete_text(void)
{
    char level_line[9];

    clear_status_rows();
    text_mode1_put_string(13, STATUS_ROW_MAIN, 10, "LEVEL COMPLETE");

    level_line[0] = 'L';
    level_line[1] = 'E';
    level_line[2] = 'V';
    level_line[3] = 'E';
    level_line[4] = 'L';
    level_line[5] = ' ';
    level_line[6] = (char)('0' + ((s_current_level / 10u) % 10u));
    level_line[7] = (char)('0' + (s_current_level % 10u));
    level_line[8] = '\0';
    text_mode1_put_string(16, STATUS_ROW_SUB, 11, level_line);
}

static void activate_menu_scene(void)
{
    resources_init();
    reset_level_world();
    enemy_planes_set_enabled(false);
    clear_status_rows();
    menu_activate();
    s_game_mode = GAME_MODE_MENU;
    s_mode_tick_div = 0u;
    s_mode_timer_10hz = 0u;
    s_crash_accounted = false;
}

static void start_game_session(uint8_t level, bool enemies_enabled)
{
    s_current_level = level;
    s_current_enemies_enabled = enemies_enabled;
    resources_init();
    reset_level_world();
    text_mode1_clear();
    text_mode1_reset_score();
    clear_status_rows();
    s_game_mode = GAME_MODE_PLAYING;
    s_mode_tick_div = 0u;
    s_mode_timer_10hz = 0u;
    s_crash_accounted = false;
}

static void advance_to_next_level(void)
{
    if (s_current_level < MAX_GAME_LEVEL) {
        ++s_current_level;
    }

    resources_on_respawn();
    reset_level_world();
    clear_status_rows();
    s_game_mode = GAME_MODE_PLAYING;
    s_mode_tick_div = 0u;
    s_mode_timer_10hz = 0u;
    s_crash_accounted = false;
}

static void update_win_animation_frame(void)
{
    uint8_t ticks_left = s_mode_timer_10hz;
    uint8_t block;
    uint8_t frame_index;

    if (ticks_left == 0u) { ticks_left = 1u; }
    block = (uint8_t)((ticks_left - 1u) / WIN_FRAME_HOLD_TICKS_10HZ);
    if (block > 3u) { block = 3u; }
    frame_index = (uint8_t)((s_win_flipped ? WIN_FRAME_FLIPPED_BASE : WIN_FRAME_RIGHT_BASE) + block);
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, xram_sprite_ptr,
                     (uint16_t)(PLAYER_DATA + ((unsigned)frame_index * PLAYER_FRAME_SIZE)));
}

static void enter_level_complete_mode(void)
{
    s_win_flipped = flight_plane_orient();
    s_game_mode = GAME_MODE_LEVEL_COMPLETE;
    s_mode_tick_div = 0u;
    s_mode_timer_10hz = WIN_TIMEOUT_TICKS_10HZ;
    show_level_complete_text();
}

static void enter_game_over_mode(void)
{
    s_game_mode = GAME_MODE_GAME_OVER;
    s_mode_tick_div = 0u;
    s_mode_timer_10hz = GAME_OVER_DELAY_TICKS_10HZ;
    show_game_over_text();
}

static void update_player_projectiles(const input_actions_t *actions)
{
    projectiles_update(flight_world_x(), actions);
    ground_targets_update(flight_world_x());
    ambient_flocks_update(flight_world_x());
    ambient_birds_update(flight_world_x());
}

static void render_menu_scene(void)
{
    uint16_t camera_world_x = flight_world_x();

    enemy_planes_update(camera_world_x);
    ground_targets_update(camera_world_x);
    ambient_flocks_update(camera_world_x);
    ambient_birds_update(camera_world_x);
}

static void update_player(const input_actions_t *actions)
{
    flight_update(actions);

    if (!flight_is_crashed()) {
        int16_t plane_center_y = (int16_t)(flight_plane_y_physics() + PLANE_HITBOX_CENTER_Y_OFFSET_PX);
        bool flock_hit = ambient_flocks_scatter_at(
            flight_world_x_physics(),
            plane_center_y,
            (uint8_t)PLANE_HITBOX_HALF_WIDTH_LEFT_PX);
        bool bird_hit = ambient_birds_check_plane_hit(
            flight_world_x_physics(),
            plane_center_y,
            (uint8_t)PLANE_HITBOX_HALF_WIDTH_LEFT_PX,
            (uint8_t)PLANE_HITBOX_HALF_HEIGHT_UP_PX);

        if (flock_hit || bird_hit) {
            flight_apply_debris_hit();
            if (flock_hit) {
                ambient_birds_spawn_splat(flight_world_x_physics(), plane_center_y);
            }
        }
    }
}

static void update_playing_mode(const input_actions_t *actions)
{
    enemy_planes_update(flight_world_x());
    update_player(actions);
    update_player_projectiles(actions);

    if (!s_crash_accounted && flight_is_crashed()) {
        resources_on_plane_lost();
        text_mode1_score_crash();
        s_crash_accounted = true;

        if (!resources_can_respawn()) {
            enter_game_over_mode();
            return;
        }
    }

    if (s_crash_accounted && flight_is_crashed() && actions->start && resources_can_respawn()) {
        resources_on_respawn();
        flight_respawn();
        s_crash_accounted = false;
        clear_status_rows();
    }

    if (!flight_is_crashed() && !flight_is_falling() &&
        ground_targets_all_enemy_targets_destroyed()) {
        enter_level_complete_mode();
    }
}

static void update_nonplaying_mode(void)
{
    if (++s_mode_tick_div >= GAME_TICK_DIV_10HZ) {
        s_mode_tick_div = 0u;
        if (s_mode_timer_10hz > 0u) {
            --s_mode_timer_10hz;
        }
    }

    if (s_mode_timer_10hz != 0u) {
        return;
    }

    if (s_game_mode == GAME_MODE_LEVEL_COMPLETE) {
        advance_to_next_level();
    } else if (s_game_mode == GAME_MODE_GAME_OVER) {
        activate_menu_scene();
    }
}

int main(void)
{
    input_actions_t actions;
    uint8_t start_level;
    bool enemies_enabled;

    input_init();

    if (!init_video()) {
        return 1;
    }

    activate_menu_scene();
    menu_init();

    while (true) {
        wait_for_vsync();

        input_poll(&actions);

        if (s_game_mode == GAME_MODE_MENU) {
            render_menu_scene();
            menu_update(&actions);

            if (menu_consume_start_request(&start_level, &enemies_enabled)) {
                start_game_session(start_level, enemies_enabled);
            }
        } else if (s_game_mode == GAME_MODE_PLAYING) {
            update_playing_mode(&actions);
        } else {
            if (s_game_mode == GAME_MODE_LEVEL_COMPLETE) {
                update_win_animation_frame();
            }
            update_nonplaying_mode();
        }
    }
}