#include <rp6502.h>
#include <stdbool.h>
#include <stdint.h>
#include "tile_mode2.h"
#include "constants.h"

unsigned TILE_GROUND_CONFIG;
unsigned TILE_HUD_CONFIG;
static uint8_t s_ground_y_cache[GROUND_WIDTH * 8u];
static uint8_t s_ground_y_valid[GROUND_WIDTH * 8u];

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

static int16_t wrap_world_px(int16_t x)
{
    int16_t world_width_px = (int16_t)(GROUND_WIDTH * 8);

    while (x < 0) {
        x = (int16_t)(x + world_width_px);
    }
    while (x >= world_width_px) {
        x = (int16_t)(x - world_width_px);
    }

    return x;
}

static int16_t scan_ground_y_at_world_x(uint16_t world_x_px)
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

static void clear_ground_cache(void)
{
    for (uint16_t i = 0u; i < (uint16_t)(GROUND_WIDTH * 8u); ++i) {
        s_ground_y_cache[i] = 0u;
        s_ground_y_valid[i] = 0u;
    }
}

static uint8_t cached_ground_y(uint16_t world_x_px)
{
    uint16_t wrapped_x = (uint16_t)(world_x_px % (GROUND_WIDTH * 8u));

    if (s_ground_y_valid[wrapped_x] == 0u) {
        s_ground_y_cache[wrapped_x] = (uint8_t)scan_ground_y_at_world_x(wrapped_x);
        s_ground_y_valid[wrapped_x] = 1u;
    }

    return s_ground_y_cache[wrapped_x];
}

static uint8_t find_surface_row(uint16_t tile_x, uint8_t *tile_id)
{
    for (uint8_t row = 0u; row < GROUND_HEIGHT; ++row) {
        unsigned tile_addr = GROUND_DATA + ((unsigned)row * GROUND_WIDTH) + tile_x;
        uint8_t id = xram_read_u8(tile_addr);

        if (id != 0u) {
            *tile_id = id;
            return row;
        }
    }

    *tile_id = 0u;
    return (uint8_t)(GROUND_HEIGHT - 1u);
}

static void update_visual_column_from_cache(uint16_t tile_x)
{
    uint16_t base_x = (uint16_t)((tile_x * 8u) % (GROUND_WIDTH * 8u));
    uint8_t deepest_y = 0u;
    uint8_t surface_row;
    uint8_t old_row;
    uint8_t surface_tile_id;

    for (uint8_t i = 0u; i < 8u; ++i) {
        uint8_t y = cached_ground_y((uint16_t)(base_x + i));
        if (y > deepest_y) {
            deepest_y = y;
        }
    }

    surface_row = (uint8_t)(deepest_y >> 3);
    if (surface_row >= GROUND_HEIGHT) {
        surface_row = (uint8_t)(GROUND_HEIGHT - 1u);
    }

    old_row = find_surface_row(tile_x, &surface_tile_id);
    if (surface_tile_id == 0u) {
        return;
    }

    if (surface_row >= old_row) {
        for (uint8_t row = old_row; row < surface_row; ++row) {
            unsigned clear_addr = GROUND_DATA + ((unsigned)row * GROUND_WIDTH) + tile_x;
            xram_write_u8(clear_addr, 0u);
        }
    }

    {
        unsigned set_addr = GROUND_DATA + ((unsigned)surface_row * GROUND_WIDTH) + tile_x;
        xram_write_u8(set_addr, surface_tile_id);
    }
}

void tile_mode2_init(void) {
    TILE_GROUND_CONFIG = SPRITE_DATA_END; // Add after sprite config

    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, x_wrap, true);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, y_wrap, false);
    xram0_struct_set(TILE_GROUND_CONFIG, vga_mode2_config_t, x_pos_px, 1500);
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

    tile_mode2_set_scroll_x((int16_t)PLAYER_START_WORLD_X_PX);
    clear_ground_cache();
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
    return (int16_t)cached_ground_y(world_x_px);
}

void tile_mode2_apply_crater(uint16_t impact_world_x, uint8_t radius_px, uint8_t max_depth_px)
{
    static const uint8_t s_crater_profile_8[8] = {1u, 2u, 2u, 3u, 3u, 2u, 2u, 1u};
    uint16_t world_width = (uint16_t)(GROUND_WIDTH * 8u);
    uint16_t wrapped_impact_x = (uint16_t)(impact_world_x % world_width);
    int16_t start_dx;
    int16_t end_dx;

    if (max_depth_px == 0u) {
        return;
    }

    if (radius_px == 0u) {
        radius_px = 1u;
    }

    start_dx = (int16_t)(-(int16_t)radius_px);
    end_dx = (int16_t)radius_px;

    for (int16_t dx = start_dx; dx <= end_dx; ++dx) {
        uint16_t world_x = (uint16_t)wrap_world_px((int16_t)wrapped_impact_x + dx);
        uint16_t tile_x = (uint16_t)((world_x >> 3) % GROUND_WIDTH);
        uint8_t old_y = cached_ground_y(world_x);
        int16_t abs_dx = (dx < 0) ? (int16_t)(-dx) : dx;
        int16_t depth;

        if (radius_px == 4u && abs_dx <= 4) {
            uint8_t profile_depth = s_crater_profile_8[(uint8_t)(dx + 4)];
            depth = (int16_t)((profile_depth * max_depth_px) / 3u);
        } else {
            depth = (int16_t)max_depth_px -
                    (int16_t)(((uint16_t)abs_dx * max_depth_px) / (uint16_t)(radius_px + 1u));
        }

        if (depth < 0) {
            depth = 0;
        }
        if (depth > max_depth_px) {
            depth = max_depth_px;
        }

        if (depth > 0) {
            uint8_t new_y = (uint8_t)(old_y + (uint8_t)depth);
            if (new_y > (uint8_t)(SCREEN_HEIGHT - 1)) {
                new_y = (uint8_t)(SCREEN_HEIGHT - 1);
            }
            s_ground_y_cache[world_x] = new_y;
            s_ground_y_valid[world_x] = 1u;

            update_visual_column_from_cache(tile_x);
        }
    }
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