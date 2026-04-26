#include <rp6502.h>
#include "text_mode1.h"
#include "constants.h"
#include "sprite_mode5.h"

unsigned TEXT_CONFIG;

// Score values for game events
#define SCORE_BUILDING           100
#define SCORE_EXPLOSIVE_BUILDING 200
#define SCORE_OX                (-200)
#define SCORE_CRASH              (-50)

void text_mode1_clear(void) {
    RIA.addr0 = TEXT_DATA;
    RIA.step0 = 1;
    for (uint16_t i = 0; i < (uint16_t)(TEXT_WIDTH_CHARS * TEXT_HEIGHT_CHARS); i++) {
        RIA.rw0 = 0x20;  // space glyph
        RIA.rw0 = 0x00;  // bg=0 (transparent), fg=0 (transparent)
    }
}

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
}