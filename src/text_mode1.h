#ifndef TEXT_MODE1_H
#define TEXT_MODE1_H

#include <stdint.h>

// EGA 16-color palette in RGB555 + alpha format (bit 5 = opaque).
// Color 0 is transparent black (alpha clear), matching ANSI convention.
static const uint16_t text_palette[16] = {
    0x0000,  //  0: Black        (transparent)
    0xA820,  //  1: Blue
    0x0560,  //  2: Green
    0xAD60,  //  3: Cyan
    0x0035,  //  4: Red
    0xA835,  //  5: Magenta
    0x02B5,  //  6: Brown
    0xAD75,  //  7: Light Gray
    0x52AA,  //  8: Dark Gray
    0xFAAA,  //  9: Light Blue
    0x57EA,  // 10: Light Green
    0xFFEA,  // 11: Light Cyan
    0x52BF,  // 12: Light Red
    0xFABF,  // 13: Light Magenta
    0x57FF,  // 14: Yellow
    0xFFFF,  // 15: White
};

extern void text_mode1_init(void);
extern void text_mode1_clear(void);
extern void text_mode1_put_string(uint8_t col, uint8_t row, uint8_t color, const char *str);
extern void text_mode1_render_score(void);
extern void text_mode1_reset_score(void);
extern void text_mode1_add_score(int16_t delta);
extern void text_mode1_score_crash(void);

#endif // TEXT_MODE1_H