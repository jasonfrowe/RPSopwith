#include <rp6502.h>
#include <stdint.h>
#include "constants.h"
#include "sprite_mode5.h"
#include "tile_mode2.h"
#include "text_mode1.h"

unsigned TEXT_CONFIG;

void text_mode1_init(void) {
    // Text mode not implemented yet, but reserve config space for it
    TEXT_CONFIG = PROJECTILES_CONFIG + MAX_PROJECTILES * sizeof(vga_mode5_sprite_t) ; // Just after the end of sprite data

    xram0_struct_set(TEXT_CONFIG, vga_mode1_config_t, x_wrap, 0);
    xram0_struct_set(TEXT_CONFIG, vga_mode1_config_t, y_wrap, 0);
    xram0_struct_set(TEXT_CONFIG, vga_mode1_config_t, x_pos_px, 0); //Bug: first char duplicated if not set to zero
    xram0_struct_set(TEXT_CONFIG, vga_mode1_config_t, y_pos_px, 0);
    xram0_struct_set(TEXT_CONFIG, vga_mode1_config_t, width_chars, TEXT_WIDTH_CHARS);
    xram0_struct_set(TEXT_CONFIG, vga_mode1_config_t, height_chars, TEXT_HEIGHT_CHARS);
    xram0_struct_set(TEXT_CONFIG, vga_mode1_config_t, xram_data_ptr, TEXT_DATA);
    xram0_struct_set(TEXT_CONFIG, vga_mode1_config_t, xram_palette_ptr, TEXT_PALETTE_ADDR);
    xram0_struct_set(TEXT_CONFIG, vga_mode1_config_t, xram_font_ptr, 0xFFFF);

    // Mode 2 args: MODE, OPTIONS, CONFIG, PLANE, BEGIN, END
    if (xreg_vga_mode(1, 0x0020, TEXT_CONFIG, 2, 0, 0) < 0) {
        return;
    }

    RIA.addr0 = TEXT_PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = text_palette[i] & 0xFF;
        RIA.rw0 = text_palette[i] >> 8;
    }

}