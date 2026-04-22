#include "platform/input.h"

#include <rp6502.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "constants.h"

enum {
    KEYBOARD_INPUT_ADDR = RPS_KEYBOARD_INPUT_ADDR,
    GAMEPAD_INPUT_ADDR = RPS_GAMEPAD_INPUT_ADDR
};

#define JOYSTICK_CONFIG_FILE "JOYSTICK_SH.DAT"

enum {
    KEYBOARD_BYTES = 32,

    GP_DPAD_UP = 0x01,
    GP_DPAD_DOWN = 0x02,
    GP_DPAD_LEFT = 0x04,
    GP_DPAD_RIGHT = 0x08,
    GP_CONNECTED = 0x80,

    GP_LSTICK_UP = 0x01,
    GP_LSTICK_DOWN = 0x02,
    GP_LSTICK_LEFT = 0x04,
    GP_LSTICK_RIGHT = 0x08,

    GP_BTN_A = 0x01,
    GP_BTN_B = 0x02,
    GP_BTN_Y = 0x10,

    GP_BTN_START = 0x08
};

enum {
    GP_FIELD_DPAD = 0,
    GP_FIELD_STICKS = 1,
    GP_FIELD_BTN0 = 2,
    GP_FIELD_BTN1 = 3
};

enum {
    KEY_A = 0x04,
    KEY_C = 0x06,
    KEY_D = 0x07,
    KEY_S = 0x16,
    KEY_W = 0x1A,
    KEY_X = 0x1B,
    KEY_Y = 0x1C,
    KEY_Z = 0x1D,
    KEY_ENTER = 0x28,
    KEY_SPACE = 0x2C,
    KEY_RIGHT = 0x4F,
    KEY_LEFT = 0x50,
    KEY_DOWN = 0x51,
    KEY_UP = 0x52
};

typedef struct gamepad_state_s {
    uint8_t dpad;
    uint8_t sticks;
    uint8_t btn0;
    uint8_t btn1;
    int8_t lx;
    int8_t ly;
    int8_t rx;
    int8_t ry;
    uint8_t l2;
    uint8_t r2;
} gamepad_state_t;

typedef struct button_mapping_s {
    uint8_t keyboard_key;
    uint8_t gamepad_button;
    uint8_t gamepad_mask;
    uint8_t gamepad_button2;
    uint8_t gamepad_mask2;
} button_mapping_t;

typedef struct joystick_mapping_s {
    uint8_t action_id;
    uint8_t field;
    uint8_t mask;
} joystick_mapping_t;

enum {
    ACTION_MOVE_UP = 0,
    ACTION_MOVE_DOWN,
    ACTION_MOVE_LEFT,
    ACTION_MOVE_RIGHT,
    ACTION_BTN_FIRE,
    ACTION_BTN_BOMB,
    ACTION_UNUSED_BTN_X,
    ACTION_BTN_FLIP,
    ACTION_UNUSED_BTN_LT,
    ACTION_UNUSED_BTN_RT,
    ACTION_UNUSED_BTN_SELECT,
    ACTION_BTN_START,
    ACTION_COUNT
};

static uint8_t s_keystate[KEYBOARD_BYTES];
static gamepad_state_t s_gamepad0;
static button_mapping_t s_button_mappings[ACTION_COUNT];

static bool key_is_down(uint8_t hid_key)
{
    return (s_keystate[hid_key >> 3] & (1u << (hid_key & 7))) != 0;
}

static bool gamepad_connected(void)
{
    return (s_gamepad0.dpad & GP_CONNECTED) != 0;
}

static uint8_t gamepad_field_value(uint8_t field)
{
    switch (field) {
        case GP_FIELD_DPAD:
            return s_gamepad0.dpad;
        case GP_FIELD_STICKS:
            return s_gamepad0.sticks;
        case GP_FIELD_BTN0:
            return s_gamepad0.btn0;
        case GP_FIELD_BTN1:
            return s_gamepad0.btn1;
        default:
            return 0;
    }
}

static void reset_default_mappings(void)
{
    memset(s_button_mappings, 0, sizeof(s_button_mappings));

    s_button_mappings[ACTION_MOVE_UP].keyboard_key = KEY_UP;
    s_button_mappings[ACTION_MOVE_UP].gamepad_button = GP_FIELD_STICKS;
    s_button_mappings[ACTION_MOVE_UP].gamepad_mask = GP_LSTICK_UP;
    s_button_mappings[ACTION_MOVE_UP].gamepad_button2 = GP_FIELD_DPAD;
    s_button_mappings[ACTION_MOVE_UP].gamepad_mask2 = GP_DPAD_UP;

    s_button_mappings[ACTION_MOVE_DOWN].keyboard_key = KEY_DOWN;
    s_button_mappings[ACTION_MOVE_DOWN].gamepad_button = GP_FIELD_STICKS;
    s_button_mappings[ACTION_MOVE_DOWN].gamepad_mask = GP_LSTICK_DOWN;
    s_button_mappings[ACTION_MOVE_DOWN].gamepad_button2 = GP_FIELD_DPAD;
    s_button_mappings[ACTION_MOVE_DOWN].gamepad_mask2 = GP_DPAD_DOWN;

    s_button_mappings[ACTION_MOVE_LEFT].keyboard_key = KEY_LEFT;
    s_button_mappings[ACTION_MOVE_LEFT].gamepad_button = GP_FIELD_STICKS;
    s_button_mappings[ACTION_MOVE_LEFT].gamepad_mask = GP_LSTICK_LEFT;
    s_button_mappings[ACTION_MOVE_LEFT].gamepad_button2 = GP_FIELD_DPAD;
    s_button_mappings[ACTION_MOVE_LEFT].gamepad_mask2 = GP_DPAD_LEFT;

    s_button_mappings[ACTION_MOVE_RIGHT].keyboard_key = KEY_RIGHT;
    s_button_mappings[ACTION_MOVE_RIGHT].gamepad_button = GP_FIELD_STICKS;
    s_button_mappings[ACTION_MOVE_RIGHT].gamepad_mask = GP_LSTICK_RIGHT;
    s_button_mappings[ACTION_MOVE_RIGHT].gamepad_button2 = GP_FIELD_DPAD;
    s_button_mappings[ACTION_MOVE_RIGHT].gamepad_mask2 = GP_DPAD_RIGHT;

    s_button_mappings[ACTION_BTN_FLIP].keyboard_key = KEY_C;
    s_button_mappings[ACTION_BTN_FLIP].gamepad_button = GP_FIELD_BTN0;
    s_button_mappings[ACTION_BTN_FLIP].gamepad_mask = GP_BTN_Y;

    s_button_mappings[ACTION_BTN_FIRE].keyboard_key = KEY_Z;
    s_button_mappings[ACTION_BTN_FIRE].gamepad_button = GP_FIELD_BTN0;
    s_button_mappings[ACTION_BTN_FIRE].gamepad_mask = GP_BTN_A;

    s_button_mappings[ACTION_BTN_BOMB].keyboard_key = KEY_X;
    s_button_mappings[ACTION_BTN_BOMB].gamepad_button = GP_FIELD_BTN0;
    s_button_mappings[ACTION_BTN_BOMB].gamepad_mask = GP_BTN_B;

    s_button_mappings[ACTION_BTN_START].keyboard_key = KEY_ENTER;
    s_button_mappings[ACTION_BTN_START].gamepad_button = GP_FIELD_BTN1;
    s_button_mappings[ACTION_BTN_START].gamepad_mask = GP_BTN_START;
}

static bool load_button_mappings(void)
{
    FILE *fp;
    int count;

    fp = fopen(JOYSTICK_CONFIG_FILE, "rb");
    if (!fp) {
        return false;
    }

    count = fgetc(fp);
    if (count <= 0 || count > ACTION_COUNT) {
        fclose(fp);
        return false;
    }

    for (int i = 0; i < count; ++i) {
        joystick_mapping_t mapping;

        if (fread(&mapping, sizeof(mapping), 1, fp) != 1) {
            fclose(fp);
            return false;
        }

        if (mapping.action_id >= ACTION_COUNT) {
            continue;
        }
        if (mapping.field > GP_FIELD_BTN1) {
            continue;
        }

        s_button_mappings[mapping.action_id].gamepad_button = mapping.field;
        s_button_mappings[mapping.action_id].gamepad_mask = mapping.mask;
        s_button_mappings[mapping.action_id].gamepad_button2 = 0;
        s_button_mappings[mapping.action_id].gamepad_mask2 = 0;
    }

    fclose(fp);
    return true;
}

static bool action_pressed(uint8_t action_id)
{
    button_mapping_t *mapping = &s_button_mappings[action_id];

    if (key_is_down(mapping->keyboard_key)) {
        return true;
    }

    if (!gamepad_connected()) {
        return false;
    }

    if ((gamepad_field_value(mapping->gamepad_button) & mapping->gamepad_mask) != 0) {
        return true;
    }

    if (mapping->gamepad_mask2 != 0 &&
        (gamepad_field_value(mapping->gamepad_button2) & mapping->gamepad_mask2) != 0) {
        return true;
    }

    return false;
}

void platform_input_init(void)
{
    xreg(0, 0, 0, KEYBOARD_INPUT_ADDR);
    xreg(0, 0, 2, GAMEPAD_INPUT_ADDR);

    reset_default_mappings();
    (void)load_button_mappings();
}

void platform_input_poll(input_actions_t *actions)
{
    memset(actions, 0, sizeof(*actions));

    RIA.addr0 = KEYBOARD_INPUT_ADDR;
    RIA.step0 = 1;
    for (uint8_t i = 0; i < KEYBOARD_BYTES; ++i) {
        s_keystate[i] = RIA.rw0;
    }

    RIA.addr0 = GAMEPAD_INPUT_ADDR;
    RIA.step0 = 1;
    s_gamepad0.dpad = RIA.rw0;
    s_gamepad0.sticks = RIA.rw0;
    s_gamepad0.btn0 = RIA.rw0;
    s_gamepad0.btn1 = RIA.rw0;
    s_gamepad0.lx = (int8_t)RIA.rw0;
    s_gamepad0.ly = (int8_t)RIA.rw0;
    s_gamepad0.rx = (int8_t)RIA.rw0;
    s_gamepad0.ry = (int8_t)RIA.rw0;
    s_gamepad0.l2 = RIA.rw0;
    s_gamepad0.r2 = RIA.rw0;

    actions->up = action_pressed(ACTION_MOVE_UP) || key_is_down(KEY_W);
    actions->down = action_pressed(ACTION_MOVE_DOWN) || key_is_down(KEY_S);
    actions->left = action_pressed(ACTION_MOVE_LEFT) || key_is_down(KEY_A);
    actions->right = action_pressed(ACTION_MOVE_RIGHT) || key_is_down(KEY_D);
    actions->flip = action_pressed(ACTION_BTN_FLIP);
    actions->fire = action_pressed(ACTION_BTN_FIRE) || key_is_down(KEY_SPACE);
    actions->bomb = action_pressed(ACTION_BTN_BOMB);
    actions->start = action_pressed(ACTION_BTN_START);
}