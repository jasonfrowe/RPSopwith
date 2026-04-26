#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "constants.h"
#include "tile_mode2.h"
#include "sprite_mode5.h"
#include "text_mode1.h"

unsigned TILE_GROUND_CONFIG;
unsigned TILE_HUD_CONFIG;

static uint8_t s_next_dynamic_tile = 255u;
static bool s_ground_map_backup_ready = false;

enum {
    TILE_SKY = 0,
    MIN_DYNAMIC_POOL_SIZE = 24
};

static uint8_t xram_read_u8(unsigned addr)
{
    RIA.addr0 = addr;
    RIA.step0 = 1;
    return RIA.rw0;
}

static void xram_write_u8(unsigned addr, uint8_t value)
{
    RIA.addr0 = addr;
    RIA.step0 = 1;
    RIA.rw0 = value;
}

static uint8_t tile_pixel_index(uint8_t tile_id, uint8_t x, uint8_t y)
{
    unsigned addr = GROUND_TILES + ((unsigned)tile_id * 32u) + ((unsigned)y * 4u) + (x >> 1);
    uint8_t packed = xram_read_u8(addr);

    if ((x & 1u) == 0u) {
        return (uint8_t)((packed >> 4) & 0x0Fu);
    }

    return (uint8_t)(packed & 0x0Fu);
}

static int16_t wrap_world_px(int32_t x)
{
    int32_t world_width_px = (int32_t)GROUND_WIDTH * 8;
    int32_t wrapped = x % world_width_px;
    if (wrapped < 0) {
        wrapped += world_width_px;
    }
    return (int16_t)wrapped;
}

static void build_edge_tile(const uint8_t *samples, uint8_t tile_row, uint8_t *out)
{
    uint8_t idx[64];
    uint8_t row_base = (uint8_t)(tile_row * 8u);

    for (uint8_t i = 0; i < 64u; ++i) {
        idx[i] = 1u;
    }

    for (uint8_t x = 0; x < 8u; ++x) {
        int16_t local_surface = (int16_t)samples[x] - (int16_t)row_base;
        uint8_t start_y;

        if (local_surface <= 0) {
            start_y = 0;
        } else if (local_surface >= 8) {
            continue;
        } else {
            start_y = (uint8_t)local_surface;
        }

        for (uint8_t y = start_y; y < 8u; ++y) {
            idx[(y * 8u) + x] = 2u;
        }
    }

    for (uint8_t y = 0; y < 8u; ++y) {
        for (uint8_t x = 0; x < 8u; x += 2u) {
            out[(y * 4u) + (x >> 1)] =
                (uint8_t)(((idx[(y * 8u) + x] & 0x0Fu) << 4) | (idx[(y * 8u) + x + 1u] & 0x0Fu));
        }
    }
}

static uint8_t find_surface_tile_row(uint16_t tile_x)
{
    for (uint8_t row = 0; row < GROUND_HEIGHT; ++row) {
        unsigned tile_addr = GROUND_DATA + ((unsigned)row * GROUND_WIDTH) + tile_x;
        uint8_t tile_id = xram_read_u8(tile_addr);
        if (tile_id != TILE_SKY) {
            return row;
        }
    }

    return (uint8_t)(GROUND_HEIGHT - 1u);
}

static uint8_t find_row_from_samples(const uint8_t *column_samples)
{
    uint8_t min_y = column_samples[0];

    for (uint8_t i = 1; i < 8u; ++i) {
        if (column_samples[i] < min_y) {
            min_y = column_samples[i];
        }
    }

    return (uint8_t)(min_y >> 3);
}

static uint8_t find_matching_tile_id(const uint8_t *tile_bytes)
{
    for (uint16_t tile_id = 0; tile_id < 256u; ++tile_id) {
        unsigned tile_addr = GROUND_TILES + ((unsigned)tile_id * 32u);
        uint8_t i;

        for (i = 0u; i < 32u; ++i) {
            if (xram_read_u8(tile_addr + i) != tile_bytes[i]) {
                break;
            }
        }

        if (i == 32u) {
            return (uint8_t)tile_id;
        }
    }

    return 0xFFu;
}

static uint8_t find_or_create_tile_id(const uint8_t *tile_bytes)
{
    uint8_t match = find_matching_tile_id(tile_bytes);
    if (match != 0xFFu) {
        return match;
    }

    if (s_next_dynamic_tile == 0u) {
        return 0xFFu;
    }

    unsigned tile_addr = GROUND_TILES + ((unsigned)s_next_dynamic_tile * 32u);
    for (uint8_t i = 0; i < 32u; ++i) {
        xram_write_u8(tile_addr + i, tile_bytes[i]);
    }

    return s_next_dynamic_tile--;
}

static void init_dynamic_pool_start(void)
{
    uint8_t max_used = 1u;

    for (uint16_t row = 0; row < GROUND_HEIGHT; ++row) {
        for (uint16_t col = 0; col < GROUND_WIDTH; ++col) {
            unsigned tile_addr = GROUND_DATA + ((unsigned)row * GROUND_WIDTH) + col;
            uint8_t tile_id = xram_read_u8(tile_addr);
            if (tile_id > max_used) {
                max_used = tile_id;
            }
        }
    }

    if ((uint16_t)255u - max_used < MIN_DYNAMIC_POOL_SIZE) {
        s_next_dynamic_tile = 0u;
        return;
    }

    s_next_dynamic_tile = 255u;
}

static void backup_ground_map_if_needed(void)
{
    if (s_ground_map_backup_ready) {
        return;
    }

    for (unsigned i = 0; i < GROUND_DATA_SIZE; ++i) {
        xram_write_u8((unsigned)GROUND_MAP_BACKUP_ADDR + i,
                      xram_read_u8(GROUND_DATA + i));
    }

    s_ground_map_backup_ready = true;
}

void tile_mode2_reset_ground_map(void)
{
    if (s_ground_map_backup_ready) {
        for (unsigned i = 0; i < GROUND_DATA_SIZE; ++i) {
            xram_write_u8(GROUND_DATA + i,
                          xram_read_u8((unsigned)GROUND_MAP_BACKUP_ADDR + i));
        }
    }

    init_dynamic_pool_start();
}

void tile_hud_init(void) {
    TILE_HUD_CONFIG = TILE_GROUND_CONFIG + sizeof(vga_mode2_config_t); // Add after ground config

    xram0_struct_set(TILE_HUD_CONFIG, vga_mode2_config_t, x_wrap, true);
    xram0_struct_set(TILE_HUD_CONFIG, vga_mode2_config_t, y_wrap, true);
    xram0_struct_set(TILE_HUD_CONFIG, vga_mode2_config_t, x_pos_px, 0);
    xram0_struct_set(TILE_HUD_CONFIG, vga_mode2_config_t, y_pos_px, 0);
    xram0_struct_set(TILE_HUD_CONFIG, vga_mode2_config_t, width_tiles,  HUD_MAP_WIDTH_TILES);
    xram0_struct_set(TILE_HUD_CONFIG, vga_mode2_config_t, height_tiles, HUD_MAP_HEIGHT_TILES);
    xram0_struct_set(TILE_HUD_CONFIG, vga_mode2_config_t, xram_data_ptr,    HUD_MAP_DATA); // tile ID grid
    xram0_struct_set(TILE_HUD_CONFIG, vga_mode2_config_t, xram_palette_ptr, HUD_PALETTE_ADDR);
    xram0_struct_set(TILE_HUD_CONFIG, vga_mode2_config_t, xram_tile_ptr,    HUD_TILES);        // tile bitmaps


    // Mode 2 args: MODE, OPTIONS, CONFIG, PLANE, BEGIN, END
    // OPTIONS: bit3=0 (8x8 tiles), bit[2:0]=2 (8-bit color index) => 0b0010 = 2
    // Plane 0 = background fill layer (behind sprite plane 1)
    if (xreg_vga_mode(2, 0x02, TILE_HUD_CONFIG, 1, 0, 0) < 0) {
        return;
    }

    RIA.addr0 = HUD_PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = hud_palette[i] & 0xFF;
        RIA.rw0 = hud_palette[i] >> 8;
    }

}

void tile_mode2_init(void) {
    TILE_GROUND_CONFIG = SPRITE_DATA_END; // Add after asset data block

    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, x_wrap, true);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, y_wrap, true);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, x_pos_px, 0);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, y_pos_px, 0);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, width_tiles,  GROUND_WIDTH);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, height_tiles, GROUND_HEIGHT);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, xram_data_ptr,    GROUND_DATA); // tile ID grid
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, xram_palette_ptr, TILE_GROUND_PALETTE_ADDR);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, xram_tile_ptr,    GROUND_TILES);        // tile bitmaps


    // Mode 2 args: MODE, OPTIONS, CONFIG, PLANE, BEGIN, END
    // OPTIONS: bit3=0 (8x8 tiles), bit[2:0]=2 (8-bit color index) => 0b0010 = 2
    // Plane 0 = background fill layer (behind sprite plane 1)
    if (xreg_vga_mode(2, 0x02, TILE_GROUND_CONFIG, 0, 0, 0) < 0) {
        return;
    }

    RIA.addr0 = TILE_GROUND_PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = tile_bg_palette[i] & 0xFF;
        RIA.rw0 = tile_bg_palette[i] >> 8;
    }

    backup_ground_map_if_needed();
    init_dynamic_pool_start();

    // Start at the player's home position so the terrain is correct on the first frame
    tile_mode2_set_scroll_x(PLAYER_START_WORLD_X_PX);
}

void tile_mode2_set_scroll_x(int16_t world_scroll_px)
{
    int16_t wrapped_world_x = wrap_world_px(world_scroll_px);
    int16_t camera_left = (int16_t)(wrapped_world_x - (SCREEN_WIDTH / 2));
    int16_t wrapped_left = wrap_world_px(camera_left);

    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, x_pos_px, (int16_t)(-wrapped_left));
}

int16_t tile_mode2_ground_y_at_world_x(uint16_t world_x_px)
{
    uint16_t tile_x = (uint16_t)((world_x_px >> 3) % GROUND_WIDTH);
    uint8_t local_x = (uint8_t)(world_x_px & 0x07u);

    for (uint16_t row = 0; row < GROUND_HEIGHT; ++row) {
        unsigned tile_addr = GROUND_DATA + ((unsigned)row * GROUND_WIDTH) + tile_x;
        uint8_t tile_id = xram_read_u8(tile_addr);

        if (tile_id != 0u) {
            for (uint8_t y = 0; y < 8u; ++y) {
                if (tile_pixel_index(tile_id, local_x, y) == 2u) {
                    return (int16_t)((row * 8u) + y);
                }
            }
        }
    }

    return (int16_t)(SCREEN_HEIGHT - 1);
}

void tile_mode2_update_ground_column(uint16_t world_x_px, const uint8_t *column_samples)
{
    uint16_t tile_x = (uint16_t)((world_x_px >> 3) % GROUND_WIDTH);
    uint8_t row = find_row_from_samples(column_samples);
    uint8_t surface_row = find_surface_tile_row(tile_x);
    uint8_t tile_bytes[32];
    uint8_t tile_id;
    unsigned tile_addr;

    if (row < surface_row) {
        row = surface_row;
    }

    build_edge_tile(column_samples, row, tile_bytes);

    tile_id = find_or_create_tile_id(tile_bytes);
    if (tile_id == 0xFFu) {
        uint8_t candidate_rows[4];
        uint8_t candidate_count = 0;

        candidate_rows[candidate_count++] = surface_row;
        if (row + 1u < GROUND_HEIGHT) {
            candidate_rows[candidate_count++] = (uint8_t)(row + 1u);
        }
        if (row > 0u) {
            candidate_rows[candidate_count++] = (uint8_t)(row - 1u);
        }
        if (surface_row > 0u) {
            candidate_rows[candidate_count++] = (uint8_t)(surface_row - 1u);
        }

        for (uint8_t i = 0; i < candidate_count; ++i) {
            uint8_t r = candidate_rows[i];
            build_edge_tile(column_samples, r, tile_bytes);
            tile_id = find_or_create_tile_id(tile_bytes);
            if (tile_id != 0xFFu) {
                row = r;
                break;
            }
        }

        if (tile_id == 0xFFu) {
            return;
        }
    }

    // If the surface has moved into a lower tile row, clear the stale cap rows first.
    if (row > surface_row) {
        for (uint8_t clear_row = surface_row; clear_row < row; ++clear_row) {
            unsigned clear_addr = GROUND_DATA + ((unsigned)clear_row * GROUND_WIDTH) + tile_x;
            xram_write_u8(clear_addr, TILE_SKY);
        }
    }

    tile_addr = GROUND_DATA + ((unsigned)row * GROUND_WIDTH) + tile_x;
    xram_write_u8(tile_addr, tile_id);
}