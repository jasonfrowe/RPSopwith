#include <rp6502.h>
#include <stdint.h>
#include "constants.h"
#include "sfx.h"

// OPL2 patch structure (mirrors RPMegaRaider instruments.h)
typedef struct {
    uint8_t m_ave, m_ksl, m_atdec, m_susrel, m_wave;
    uint8_t c_ave, c_ksl, c_atdec, c_susrel, c_wave;
    uint8_t feedback;
} sfx_patch_t;

#define SFX_SHOT_CH   4u

// Operator slot offset tables (same as RPMegaRaider instruments.c)
static const uint8_t s_mod[9] = {0x00,0x01,0x02,0x08,0x09,0x0A,0x10,0x11,0x12};
static const uint8_t s_car[9] = {0x03,0x04,0x05,0x0B,0x0C,0x0D,0x13,0x14,0x15};

// Gunshot patch: instant attack, fast decay to near-silent sustain.
// Mirrors impact_patch from RPMegaRaider/src/sound.c.
// susrel=0xC8 means sustain=C (nearly silent), release=8 — note dies on its own.
static const sfx_patch_t s_shot_patch = {
    .m_ave=0x11, .m_ksl=0x14, .m_atdec=0xF4, .m_susrel=0xC8, .m_wave=0x00,
    .c_ave=0x21, .c_ksl=0x00, .c_atdec=0xF3, .c_susrel=0xC6, .c_wave=0x00,
    .feedback=0x0E,
};

// F-number table for octave calculation (same as RPMegaRaider opl.c)
static const uint16_t s_fnum[12] = {
    308, 325, 345, 365, 387, 410, 434, 460, 487, 516, 547, 579
};

static uint8_t s_shadow_b0 = 0;
static uint8_t s_shadow_ksl_c = 0;

static void sfx_opl_write(uint8_t reg, uint8_t value)
{
    RIA.addr1 = (uint16_t)(OPL_XRAM_ADDR + reg);
    RIA.rw1 = value;
}

static void sfx_play_note(const sfx_patch_t *p, uint8_t ch,
                          uint8_t *shadow_b0_ptr, uint8_t *shadow_ksl_ptr,
                          uint8_t midi_note, uint8_t velocity)
{
    uint8_t note  = midi_note < 12u ? 12u : midi_note;
    uint8_t block = (uint8_t)((note - 12u) / 12u);
    uint8_t idx   = (uint8_t)((note - 12u) % 12u);
    uint16_t f    = s_fnum[idx];
    uint8_t b0hi;
    uint8_t m     = s_mod[ch];
    uint8_t c     = s_car[ch];
    uint8_t vol;

    if (block > 7u) { block = 7u; }
    b0hi = (uint8_t)(0x20u | (block << 2u) | ((f >> 8u) & 0x03u));

    // NoteOff first
    sfx_opl_write((uint8_t)(0xB0u + ch), *shadow_b0_ptr);

    // Program patch
    *shadow_ksl_ptr = p->c_ksl;
    sfx_opl_write((uint8_t)(0x20u + m), p->m_ave);
    sfx_opl_write((uint8_t)(0x20u + c), p->c_ave);
    sfx_opl_write((uint8_t)(0x40u + m), p->m_ksl);
    sfx_opl_write((uint8_t)(0x40u + c), p->c_ksl);
    sfx_opl_write((uint8_t)(0x60u + m), p->m_atdec);
    sfx_opl_write((uint8_t)(0x60u + c), p->c_atdec);
    sfx_opl_write((uint8_t)(0x80u + m), p->m_susrel);
    sfx_opl_write((uint8_t)(0x80u + c), p->c_susrel);
    sfx_opl_write((uint8_t)(0xE0u + m), p->m_wave);
    sfx_opl_write((uint8_t)(0xE0u + c), p->c_wave);
    sfx_opl_write((uint8_t)(0xC0u + ch), p->feedback);

    // Volume
    vol = (uint8_t)(63u - (velocity >> 1u));
    sfx_opl_write((uint8_t)(0x40u + c), (uint8_t)((*shadow_ksl_ptr & 0xC0u) | vol));

    // NoteOn
    sfx_opl_write((uint8_t)(0xA0u + ch), (uint8_t)(f & 0xFFu));
    sfx_opl_write((uint8_t)(0xB0u + ch), b0hi);
    *shadow_b0_ptr = (uint8_t)(b0hi & 0x1Fu);
}

void sfx_init(void)
{
    s_shadow_b0    = 0u;
    s_shadow_ksl_c = 0u;
    sfx_opl_write((uint8_t)(0xB0u + SFX_SHOT_CH), 0x00u);
}

void sfx_play_gunshot(void)
{
    sfx_play_note(&s_shot_patch, SFX_SHOT_CH,
                  &s_shadow_b0, &s_shadow_ksl_c,
                  72u, 110u);
}

void sfx_play_enemy_gunshot(void)
{
    sfx_play_note(&s_shot_patch, SFX_SHOT_CH,
                  &s_shadow_b0, &s_shadow_ksl_c,
                  64u, 96u);
}

void sfx_play_bomb_drop(void)
{
    sfx_play_note(&s_shot_patch, SFX_SHOT_CH,
                  &s_shadow_b0, &s_shadow_ksl_c,
                  52u, 88u);
}

void sfx_play_collision(void)
{
    sfx_play_note(&s_shot_patch, SFX_SHOT_CH,
                  &s_shadow_b0, &s_shadow_ksl_c,
                  40u, 120u);
}
