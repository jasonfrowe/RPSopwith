#include <rp6502.h>
#include <stdbool.h>
#include <stdint.h>
#include "constants.h"
#include "music_player.h"

enum {
    MUSIC_END_ADDR = (MUSIC_DATA + MUSIC_DATA_SIZE)
};

static uint16_t s_music_addr = MUSIC_DATA;
static uint16_t s_music_wait_ticks = 0u;
static bool s_music_enabled = false;
static bool s_music_just_looped = false;

static void opl_write(uint8_t reg, uint8_t value)
{
    RIA.addr1 = (uint16_t)(OPL_XRAM_ADDR + reg);
    RIA.rw1 = value;
}

static void opl_reset(void)
{
    for (uint16_t reg = 0u; reg < 256u; ++reg) {
        opl_write((uint8_t)reg, 0u);
    }
    opl_write(0x01u, 0x20u);
    opl_write(0xBDu, 0x00u);
}

static uint8_t xram_read_u8(uint16_t addr)
{
    RIA.addr0 = addr;
    RIA.step0 = 1;
    return RIA.rw0;
}

static bool read_next_packet(uint8_t *reg, uint8_t *value, uint16_t *delay)
{
    if ((uint16_t)(s_music_addr + 3u) >= MUSIC_END_ADDR) {
        return false;
    }

    *reg = xram_read_u8(s_music_addr++);
    *value = xram_read_u8(s_music_addr++);
    *delay = (uint16_t)xram_read_u8(s_music_addr++);
    *delay |= (uint16_t)((uint16_t)xram_read_u8(s_music_addr++) << 8);
    return true;
}

void music_player_init(void)
{
    xreg(0, 1, 0x01, OPL_XRAM_ADDR);
    music_player_stop();
}

void music_player_play_menu(void)
{
    music_player_stop();

    s_music_addr = MUSIC_DATA;
    s_music_wait_ticks = 0u;
    s_music_enabled = true;
    s_music_just_looped = false;

    opl_reset();
}

void music_player_stop(void)
{
    s_music_enabled = false;
    s_music_just_looped = false;
    s_music_addr = MUSIC_DATA;
    s_music_wait_ticks = 0u;
    opl_reset();
}

void music_player_update(void)
{
    uint8_t reg;
    uint8_t value;
    uint16_t delay;

    if (!s_music_enabled) {
        return;
    }

    if (s_music_wait_ticks > 0u) {
        --s_music_wait_ticks;
    }

    while (s_music_wait_ticks == 0u) {
        if (!read_next_packet(&reg, &value, &delay)) {
            music_player_stop();
            return;
        }

        if (s_music_just_looped &&
            reg >= 0xB0u && reg <= 0xB8u &&
            (value & 0x20u) == 0u &&
            delay <= 1u) {
            s_music_just_looped = false;
            continue;
        }
        s_music_just_looped = false;

        if (reg == 0xFFu && value == 0xFFu) {
            s_music_addr = MUSIC_DATA;
            s_music_just_looped = true;
            continue;
        }

        opl_write(reg, value);
        s_music_wait_ticks = delay;
    }
}