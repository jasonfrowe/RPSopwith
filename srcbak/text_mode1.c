#include <rp6502.h>
#include <stdint.h>
#include "constants.h"
#include "sprite_mode5.h"
#include "tile_mode2.h"
#include "text_mode1.h"

unsigned TEXT_CONFIG;

static int16_t s_score = 0;

// Score values for game events
#define SCORE_BUILDING           100
#define SCORE_EXPLOSIVE_BUILDING 200
#define SCORE_OX                (-200)
#define SCORE_CRASH              (-50)

void text_mode1_init(void) {
    TEXT_CONFIG = PROJECTILES_CONFIG + MAX_PROJECTILES * sizeof(vga_mode5_sprite_t);

    xram0_struct_set(TEXT_CONFIG, vga_mode1_config_t, x_wrap, 0);
    xram0_struct_set(TEXT_CONFIG, vga_mode1_config_t, y_wrap, 0);
    xram0_struct_set(TEXT_CONFIG, vga_mode1_config_t, x_pos_px, 0); //Bug: first char duplicated if not set to zero
    xram0_struct_set(TEXT_CONFIG, vga_mode1_config_t, y_pos_px, 0);
    xram0_struct_set(TEXT_CONFIG, vga_mode1_config_t, width_chars, TEXT_WIDTH_CHARS);
    xram0_struct_set(TEXT_CONFIG, vga_mode1_config_t, height_chars, TEXT_HEIGHT_CHARS);
    xram0_struct_set(TEXT_CONFIG, vga_mode1_config_t, xram_data_ptr, TEXT_DATA);
    xram0_struct_set(TEXT_CONFIG, vga_mode1_config_t, xram_palette_ptr, TEXT_PALETTE_ADDR);
    xram0_struct_set(TEXT_CONFIG, vga_mode1_config_t, xram_font_ptr, 0xFFFF);

    // Mode 1 args: MODE, OPTIONS, CONFIG, PLANE, BEGIN, END
    // OPTIONS: bit 3=0 (8x8 font), bits 2:0=2 (4-bit color) => 0x0002
    if (xreg_vga_mode(1, 0x0002, TEXT_CONFIG, 2, 0, 0) < 0) {
        return;
    }

    RIA.addr0 = TEXT_PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = text_palette[i] & 0xFF;
        RIA.rw0 = text_palette[i] >> 8;
    }

    text_mode1_clear();
    text_mode1_render_score();
}

void text_mode1_clear(void) {
    RIA.addr0 = TEXT_DATA;
    RIA.step0 = 1;
    for (uint16_t i = 0; i < (uint16_t)(TEXT_WIDTH_CHARS * TEXT_HEIGHT_CHARS); i++) {
        RIA.rw0 = 0x20;  // space glyph
        RIA.rw0 = 0x00;  // bg=0 (transparent), fg=0 (transparent)
    }
}

void text_mode1_put_string(uint8_t col, uint8_t row, uint8_t color, const char *str) {
    unsigned addr = TEXT_DATA + ((unsigned)row * TEXT_WIDTH_CHARS + col) * 2u;
    RIA.addr0 = addr;
    RIA.step0 = 1;
    while (*str && col < TEXT_WIDTH_CHARS) {
        RIA.rw0 = (uint8_t)*str++;
        RIA.rw0 = color;
        col++;
    }
}

void text_mode1_render_score(void) {
    char buf[8];
    int16_t val = s_score;
    bool negative = (val < 0);
    if (negative) { val = (int16_t)-val; }

    uint8_t pos = 7;
    buf[pos] = '\0';
    if (val == 0) {
        buf[--pos] = '0';
    } else {
        while (val > 0 && pos > 0) {
            buf[--pos] = (char)('0' + (val % 10));
            val = (int16_t)(val / 10);
        }
    }
    if (negative && pos > 0) {
        buf[--pos] = '-';
    }

    // Bottom-left numeric score only (no label), matching Sopwith layout.
    // color byte: high nibble=bg index, low nibble=fg index
    uint8_t score_col = 0;
    uint8_t score_row = (uint8_t)(TEXT_HEIGHT_CHARS - 4u);

    // Clear the field first so old digits don't linger.
    text_mode1_put_string(0, score_row, 0x0B, "       ");

    uint8_t score_len = (uint8_t)(7 - pos);  // number of chars in buf
    for (uint8_t p = score_len; p < 7; p++) {
        text_mode1_put_string(score_col++, score_row, 0x0B, " ");
    }
    text_mode1_put_string(score_col, score_row, 0x0B, &buf[pos]);
}

void text_mode1_add_score(int16_t delta) {
    s_score = (int16_t)(s_score + delta);
    text_mode1_render_score();
}

void text_mode1_reset_score(void) {
    s_score = 0;
    text_mode1_render_score();
}

void text_mode1_score_building(void) {
    text_mode1_add_score(SCORE_BUILDING);
}

void text_mode1_score_explosive_building(void) {
    text_mode1_add_score(SCORE_EXPLOSIVE_BUILDING);
}

void text_mode1_score_ox(void) {
    text_mode1_add_score(SCORE_OX);
}

void text_mode1_score_crash(void) {
    text_mode1_add_score(SCORE_CRASH);
}