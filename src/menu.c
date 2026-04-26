#include <stdbool.h>
#include <stdint.h>

#include "ambient_birds.h"
#include "ambient_flocks.h"
#include "enemy_planes.h"
#include "flight.h"
#include "ground_targets.h"
#include "menu.h"
#include "projectiles.h"
#include "resources.h"
#include "text_mode1.h"
#include "tile_mode2.h"

enum {
    MENU_MIN_LEVEL = 1,
    MENU_MAX_LEVEL = 99
};

typedef enum title_item_e {
    TITLE_ITEM_LEVEL = 0,
    TITLE_ITEM_ENEMIES = 1,
    TITLE_ITEM_START = 2,
    TITLE_ITEM_COUNT
} title_item_t;

static bool s_menu_active = true;
static title_item_t s_title_item = TITLE_ITEM_LEVEL;
static uint8_t s_start_level = 1u;
static bool s_computer_enemies_enabled = true;
static input_actions_t s_prev_actions;

static bool pressed_now(bool now, bool prev)
{
    return now && !prev;
}

static void build_level_line(char *line, bool selected, uint8_t level)
{
    line[0] = selected ? '>' : ' ';
    line[1] = ' ';
    line[2] = 'L';
    line[3] = 'e';
    line[4] = 'v';
    line[5] = 'e';
    line[6] = 'l';
    line[7] = ' ';
    line[8] = (char)('0' + ((level / 10u) % 10u));
    line[9] = (char)('0' + (level % 10u));
    line[10] = ' ';
    line[11] = selected ? '<' : ' ';
    line[12] = '\0';
}

static void build_enemies_line(char *line, bool selected, bool enemies_enabled)
{
    uint8_t i = 0u;

    line[i++] = selected ? '>' : ' ';
    line[i++] = ' ';
    line[i++] = 'C';
    line[i++] = 'o';
    line[i++] = 'm';
    line[i++] = 'p';
    line[i++] = 'u';
    line[i++] = 't';
    line[i++] = 'e';
    line[i++] = 'r';
    line[i++] = ' ';
    line[i++] = 'e';
    line[i++] = 'n';
    line[i++] = 'e';
    line[i++] = 'm';
    line[i++] = 'i';
    line[i++] = 'e';
    line[i++] = 's';
    line[i++] = ' ';
    line[i++] = enemies_enabled ? 'O' : 'O';
    line[i++] = enemies_enabled ? 'N' : 'F';
    line[i++] = enemies_enabled ? ' ' : 'F';
    line[i++] = ' ';
    line[i++] = selected ? '<' : ' ';
    line[i] = '\0';
}

static void build_start_line(char *line, bool selected)
{
    line[0] = selected ? '>' : ' ';
    line[1] = ' ';
    line[2] = 'S';
    line[3] = 't';
    line[4] = 'a';
    line[5] = 'r';
    line[6] = 't';
    line[7] = ' ';
    line[8] = selected ? '<' : ' ';
    line[9] = '\0';
}

static void draw_title_menu(void)
{
    char line[32];

    text_mode1_clear();

    text_mode1_put_string(12, 6, 15, "S O P W I T H");

    build_level_line(line, (s_title_item == TITLE_ITEM_LEVEL), s_start_level);
    text_mode1_put_string(12, 10,
                          (s_title_item == TITLE_ITEM_LEVEL) ? 14 : 11,
                          line);

    build_enemies_line(line, (s_title_item == TITLE_ITEM_ENEMIES), s_computer_enemies_enabled);
    text_mode1_put_string(7, 12,
                          (s_title_item == TITLE_ITEM_ENEMIES) ? 14 : 11,
                          line);

    build_start_line(line, (s_title_item == TITLE_ITEM_START));
    text_mode1_put_string(15, 14,
                          (s_title_item == TITLE_ITEM_START) ? 14 : 11,
                          line);
}

static void start_new_game(void)
{
    tile_mode2_reset_ground_map();
    resources_init();
    ground_targets_init();
    projectiles_init();
    enemy_planes_init();
    ambient_flocks_init();
    ambient_birds_init();

    flight_set_level(s_start_level);
    enemy_planes_set_level(s_start_level);
    enemy_planes_set_enabled(s_computer_enemies_enabled);

    flight_init();

    text_mode1_clear();
    text_mode1_reset_score();

    s_menu_active = false;
}

void menu_init(void)
{
    s_menu_active = true;
    s_title_item = TITLE_ITEM_LEVEL;
    s_start_level = 1u;
    s_computer_enemies_enabled = true;
    s_prev_actions = (input_actions_t){0};

    draw_title_menu();
}

void menu_update(const input_actions_t *actions)
{
    bool up_pressed = pressed_now(actions->up, s_prev_actions.up);
    bool down_pressed = pressed_now(actions->down, s_prev_actions.down);
    bool x_pressed = pressed_now(actions->fire, s_prev_actions.fire);
    bool start_pressed = pressed_now(actions->start, s_prev_actions.start);

    if (!s_menu_active) {
        s_prev_actions = *actions;
        return;
    }

    if (up_pressed) {
        if (s_title_item == 0) {
            s_title_item = (title_item_t)(TITLE_ITEM_COUNT - 1);
        } else {
            s_title_item = (title_item_t)(s_title_item - 1);
        }
        draw_title_menu();
    }

    if (down_pressed) {
        s_title_item = (title_item_t)((s_title_item + 1) % TITLE_ITEM_COUNT);
        draw_title_menu();
    }

    if (x_pressed) {
        if (s_title_item == TITLE_ITEM_LEVEL) {
            ++s_start_level;
            if (s_start_level > MENU_MAX_LEVEL) {
                s_start_level = MENU_MIN_LEVEL;
            }
            draw_title_menu();
        } else if (s_title_item == TITLE_ITEM_ENEMIES) {
            s_computer_enemies_enabled = !s_computer_enemies_enabled;
            draw_title_menu();
        } else {
            start_new_game();
        }
    }

    if (start_pressed) {
        start_new_game();
    }

    s_prev_actions = *actions;
}

bool menu_is_active(void)
{
    return s_menu_active;
}
