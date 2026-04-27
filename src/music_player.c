#include <rp6502.h>
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include "constants.h"
#include "music_player.h"

enum {
    MUSIC_BUFFER_SIZE = 256u,
    MUSIC_SEEK_SET = 0u
};

static const char s_menu_music_filename[] = "ROM:SOPWITH.BIN";

static int s_music_fd = -1;
static uint8_t s_music_buffer[MUSIC_BUFFER_SIZE];
static uint16_t s_music_buf_idx = 0u;
static uint16_t s_music_bytes_ready = 0u;
static uint16_t s_music_wait_ticks = 0u;
static bool s_music_enabled = false;
static bool s_music_error = false;
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

static void close_music_file(void)
{
    if (s_music_fd >= 0) {
        close(s_music_fd);
        s_music_fd = -1;
    }
}

static bool refill_music_buffer(void)
{
    uint16_t remaining = (uint16_t)(s_music_bytes_ready - s_music_buf_idx);

    if (remaining > 0u) {
        for (uint16_t i = 0u; i < remaining; ++i) {
            s_music_buffer[i] = s_music_buffer[(uint16_t)(s_music_buf_idx + i)];
        }
    }

    s_music_buf_idx = 0u;
    s_music_bytes_ready = remaining;

    while (s_music_bytes_ready < 4u) {
        int bytes_read = read(s_music_fd,
                              &s_music_buffer[s_music_bytes_ready],
                              (unsigned)(MUSIC_BUFFER_SIZE - s_music_bytes_ready));
        if (bytes_read <= 0) {
            return false;
        }
        s_music_bytes_ready = (uint16_t)(s_music_bytes_ready + (uint16_t)bytes_read);
    }

    return true;
}

static bool read_next_packet(uint8_t *reg, uint8_t *value, uint16_t *delay)
{
    if (s_music_fd < 0) {
        return false;
    }

    if ((uint16_t)(s_music_bytes_ready - s_music_buf_idx) < 4u && !refill_music_buffer()) {
        return false;
    }

    *reg = s_music_buffer[s_music_buf_idx++];
    *value = s_music_buffer[s_music_buf_idx++];
    *delay = (uint16_t)s_music_buffer[s_music_buf_idx++];
    *delay |= (uint16_t)((uint16_t)s_music_buffer[s_music_buf_idx++] << 8);
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

    s_music_fd = open(s_menu_music_filename, O_RDONLY);
    if (s_music_fd < 0) {
        s_music_error = true;
        return;
    }

    s_music_buf_idx = 0u;
    s_music_bytes_ready = 0u;
    s_music_wait_ticks = 0u;
    s_music_enabled = true;
    s_music_error = false;
    s_music_just_looped = false;

    opl_reset();
}

void music_player_stop(void)
{
    s_music_enabled = false;
    s_music_error = false;
    s_music_just_looped = false;
    s_music_wait_ticks = 0u;
    s_music_buf_idx = 0u;
    s_music_bytes_ready = 0u;
    close_music_file();
    opl_reset();
}

void music_player_update(void)
{
    uint8_t reg;
    uint8_t value;
    uint16_t delay;

    if (!s_music_enabled || s_music_error || s_music_fd < 0) {
        return;
    }

    if (s_music_wait_ticks > 0u) {
        --s_music_wait_ticks;
    }

    while (s_music_wait_ticks == 0u) {
        if (!read_next_packet(&reg, &value, &delay)) {
            s_music_error = true;
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
            if (lseek(s_music_fd, 0, MUSIC_SEEK_SET) < 0) {
                s_music_error = true;
                music_player_stop();
                return;
            }

            s_music_buf_idx = 0u;
            s_music_bytes_ready = 0u;
            s_music_just_looped = true;
            continue;
        }

        opl_write(reg, value);
        s_music_wait_ticks = delay;
    }
}