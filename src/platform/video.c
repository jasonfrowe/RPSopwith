#include "platform/video.h"

#include <rp6502.h>
#include <stdint.h>
#include <string.h>

#include "constants.h"
#include "game/terrain/terrain.h"

typedef struct {
    int16_t x_pos_px;
    int16_t y_pos_px;
    uint16_t xram_sprite_ptr;
    uint16_t palette_ptr;
} vga_mode5_sprite_t;

enum {
    TILE_INDEX_SKY = 1,
    TILE_INDEX_GROUND = 2,

    PLAYER_COLOR_BODY = 3,
    PLAYER_COLOR_WING = 4,
    PLAYER_COLOR_TAIL = 5,
    PLAYER_COLOR_PROP = 6
};

static uint8_t s_surface_row_cache[RPS_TILEMAP_WIDTH_TILES];
static uint8_t s_surface_row_cache_valid;
static uint8_t s_player_last_frame = 0xFFu;
static uint8_t s_prop_palette_phase = 0;

static uint16_t wrap_world_x(int32_t x)
{
    int32_t wrapped = x % TERRAIN_WORLD_WIDTH;
    if (wrapped < 0) {
        wrapped += TERRAIN_WORLD_WIDTH;
    }
    return (uint16_t)wrapped;
}

static uint16_t interpolate_world_x(uint16_t prev_x, uint16_t curr_x, uint8_t subframe)
{
    int16_t diff = (int16_t)curr_x - (int16_t)prev_x;

    if (diff > (int16_t)(TERRAIN_WORLD_WIDTH / 2u)) {
        diff -= (int16_t)TERRAIN_WORLD_WIDTH;
    } else if (diff < -(int16_t)(TERRAIN_WORLD_WIDTH / 2u)) {
        diff += (int16_t)TERRAIN_WORLD_WIDTH;
    }

    return wrap_world_x((int32_t)prev_x + ((int32_t)diff * (int32_t)subframe) / 6);
}

static void write_palette_entry(uint8_t index, uint16_t rgb555)
{
    RIA.addr0 = RPS_XRAM_MODE2_PALETTE_ADDR + ((unsigned)index * 2u);
    RIA.step0 = 1;
    RIA.rw0 = (uint8_t)(rgb555 & 0xFFu);
    RIA.rw0 = (uint8_t)(rgb555 >> 8);
}

static void write_solid_4bpp_tile(uint8_t tile_index, uint8_t color_index)
{
    uint8_t packed = (uint8_t)((color_index << 4) | color_index);

    RIA.addr0 = RPS_XRAM_MODE2_TILESET_ADDR + ((unsigned)tile_index * RPS_MODE2_TILE_BYTES_4BPP);
    RIA.step0 = 1;
    for (uint8_t i = 0; i < RPS_MODE2_TILE_BYTES_4BPP; ++i) {
        RIA.rw0 = packed;
    }
}

static void write_mode5_palette_entry(uint8_t index, uint16_t rgb555)
{
    RIA.addr0 = RPS_XRAM_MODE5_PALETTE_ADDR + ((unsigned)index * 2u);
    RIA.step0 = 1;
    RIA.rw0 = (uint8_t)(rgb555 & 0xFFu);
    RIA.rw0 = (uint8_t)(rgb555 >> 8);
}

static void sprite_set_pixel(uint8_t *sprite, uint8_t x, uint8_t y, uint8_t color)
{
    uint8_t index;
    uint8_t *byte_ptr;

    if (x >= RPS_MODE5_SPRITE_SIZE_PX || y >= RPS_MODE5_SPRITE_SIZE_PX) {
        return;
    }

    index = (uint8_t)(y * (RPS_MODE5_SPRITE_SIZE_PX / 2u) + (x >> 1));
    byte_ptr = &sprite[index];
    if ((x & 1u) == 0) {
        *byte_ptr = (uint8_t)((*byte_ptr & 0x0Fu) | (color << 4));
    } else {
        *byte_ptr = (uint8_t)((*byte_ptr & 0xF0u) | (color & 0x0Fu));
    }
}

static void write_player_frame_bitmap(uint8_t frame_index, int8_t bank)
{
    uint8_t sprite[RPS_MODE5_SPRITE_BYTES_4BPP];
    uint8_t wing_left = (uint8_t)(3 + (bank < 0 ? -bank : 0));
    uint8_t wing_right = (uint8_t)(12 - (bank > 0 ? bank : 0));
    uint8_t prop_x = (uint8_t)(13 + ((frame_index >> 1) & 1u));

    memset(sprite, 0, sizeof(sprite));

    for (uint8_t y = 4; y <= 11; ++y) {
        for (uint8_t x = 6; x <= 9; ++x) {
            sprite_set_pixel(sprite, x, y, PLAYER_COLOR_BODY);
        }
    }

    for (uint8_t x = wing_left; x <= wing_right; ++x) {
        int16_t dy = ((int16_t)bank * ((int16_t)x - 8)) / 16;
        uint8_t y = (uint8_t)(7 + dy);
        sprite_set_pixel(sprite, x, y, PLAYER_COLOR_WING);
    }

    for (uint8_t y = 10; y <= 14; ++y) {
        sprite_set_pixel(sprite, 5, y, PLAYER_COLOR_TAIL);
        sprite_set_pixel(sprite, 10, y, PLAYER_COLOR_TAIL);
    }
    sprite_set_pixel(sprite, 4, 7, PLAYER_COLOR_TAIL);

    sprite_set_pixel(sprite, prop_x, 6, PLAYER_COLOR_PROP);
    sprite_set_pixel(sprite, prop_x, 7, PLAYER_COLOR_PROP);
    sprite_set_pixel(sprite, prop_x, 8, PLAYER_COLOR_PROP);

    RIA.addr0 = RPS_XRAM_MODE5_SPRITE_DATA_ADDR + ((unsigned)frame_index * RPS_MODE5_SPRITE_BYTES_4BPP);
    RIA.step0 = 1;
    for (uint8_t i = 0; i < RPS_MODE5_SPRITE_BYTES_4BPP; ++i) {
        RIA.rw0 = sprite[i];
    }
}

static void write_player_sprite_frames(void)
{
    for (uint8_t i = 0; i < RPS_PLAYER_BANK_FRAME_COUNT; ++i) {
        int8_t bank = (int8_t)i + RPS_PLAYER_BANK_MIN;
        write_player_frame_bitmap(i, bank);
    }
}

static bool init_player_sprite_layer(void)
{
    int16_t x = (int16_t)((RPS_SCREEN_WIDTH_PX - RPS_MODE5_SPRITE_SIZE_PX) / 2u);
    int16_t y = (int16_t)((RPS_SCREEN_HEIGHT_PX - RPS_MODE5_SPRITE_SIZE_PX) / 2u);
    uint16_t frame_center_ptr = (uint16_t)(RPS_XRAM_MODE5_SPRITE_DATA_ADDR + ((RPS_PLAYER_BANK_FRAME_COUNT / 2u) * RPS_MODE5_SPRITE_BYTES_4BPP));

    xram0_struct_set(RPS_XRAM_MODE5_CONFIG_ADDR, vga_mode5_sprite_t, x_pos_px, x);
    xram0_struct_set(RPS_XRAM_MODE5_CONFIG_ADDR, vga_mode5_sprite_t, y_pos_px, y);
    xram0_struct_set(RPS_XRAM_MODE5_CONFIG_ADDR, vga_mode5_sprite_t, xram_sprite_ptr, frame_center_ptr);
    xram0_struct_set(RPS_XRAM_MODE5_CONFIG_ADDR, vga_mode5_sprite_t, palette_ptr, RPS_XRAM_MODE5_PALETTE_ADDR);

    if (xreg_vga_mode(5, 0x0A, RPS_XRAM_MODE5_CONFIG_ADDR, 1, 1, 0, 0) < 0) {
        return false;
    }

    // Palette is preloaded from generated asset at boot.
    // Force index 0 transparent in case stale generated assets are present.
    write_mode5_palette_entry(0, 0x0000);
    // We only animate prop color at runtime.
    write_mode5_palette_entry(PLAYER_COLOR_PROP, 0x7FE0);
    s_player_last_frame = 0xFFu;
    s_prop_palette_phase = 0;

    return true;
}

static void update_player_sprite_animation(const game_state_t *state)
{
    static const uint16_t prop_colors[4] = {
        0x0010,
        0x021F,
        0x03FF,
        0x7FFF
    };
    uint8_t frame_index = (uint8_t)(state->plane_bank - RPS_PLAYER_BANK_MIN);

    if (frame_index >= RPS_PLAYER_BANK_FRAME_COUNT) {
        frame_index = (uint8_t)(RPS_PLAYER_BANK_FRAME_COUNT / 2u);
    }

    if (frame_index != s_player_last_frame) {
        xram0_struct_set(
            RPS_XRAM_MODE5_CONFIG_ADDR,
            vga_mode5_sprite_t,
            xram_sprite_ptr,
            (uint16_t)(RPS_XRAM_MODE5_SPRITE_DATA_ADDR + ((unsigned)frame_index * RPS_MODE5_SPRITE_BYTES_4BPP))
        );
        s_player_last_frame = frame_index;
    }

    s_prop_palette_phase = (uint8_t)((s_prop_palette_phase + 1u) & 3u);
    write_mode5_palette_entry(PLAYER_COLOR_PROP, prop_colors[s_prop_palette_phase]);
}

static void update_player_sprite_position(const game_state_t *state)
{
    int16_t sprite_x = (int16_t)(state->plane_x - (int16_t)(RPS_MODE5_SPRITE_SIZE_PX / 2u));
    int16_t sprite_y = (int16_t)(state->plane_y - (int16_t)(RPS_MODE5_SPRITE_SIZE_PX / 2u));

    if (sprite_x < 0) {
        sprite_x = 0;
    }
    if (sprite_y < 0) {
        sprite_y = 0;
    }
    if (sprite_x > (int16_t)(RPS_SCREEN_WIDTH_PX - RPS_MODE5_SPRITE_SIZE_PX)) {
        sprite_x = (int16_t)(RPS_SCREEN_WIDTH_PX - RPS_MODE5_SPRITE_SIZE_PX);
    }
    if (sprite_y > (int16_t)(RPS_SCREEN_HEIGHT_PX - RPS_MODE5_SPRITE_SIZE_PX)) {
        sprite_y = (int16_t)(RPS_SCREEN_HEIGHT_PX - RPS_MODE5_SPRITE_SIZE_PX);
    }

    xram0_struct_set(RPS_XRAM_MODE5_CONFIG_ADDR, vga_mode5_sprite_t, x_pos_px, sprite_x);
    xram0_struct_set(RPS_XRAM_MODE5_CONFIG_ADDR, vga_mode5_sprite_t, y_pos_px, sprite_y);
}

static void render_terrain_tilemap(uint16_t camera_world_x)
{
    int32_t camera_left = (int32_t)camera_world_x - ((int32_t)RPS_SCREEN_WIDTH_PX / 2);

    for (uint8_t tx = 0; tx < RPS_TILEMAP_WIDTH_TILES; ++tx) {
        uint16_t sample_world_x = wrap_world_x(camera_left + ((int32_t)tx * RPS_TILE_SIZE_PX) + (RPS_TILE_SIZE_PX / 2));
        uint8_t surface_px = terrain_height_at_world_x(sample_world_x);
        uint8_t surface_row = (uint8_t)(surface_px / RPS_TILE_SIZE_PX);
        uint8_t old_row = s_surface_row_cache[tx];

        if (!s_surface_row_cache_valid || old_row == 0xFFu) {
            RIA.addr0 = RPS_XRAM_MODE2_TILEMAP_ADDR + tx;
            RIA.step0 = RPS_TILEMAP_WIDTH_TILES;
            for (uint8_t ty = 0; ty < RPS_TILEMAP_HEIGHT_TILES; ++ty) {
                RIA.rw0 = (ty >= surface_row) ? TILE_INDEX_GROUND : TILE_INDEX_SKY;
            }
            s_surface_row_cache[tx] = surface_row;
            continue;
        }

        if (old_row == surface_row) {
            continue;
        }

        if (surface_row > old_row) {
            // Ground moved down: previous ground strip becomes sky.
            RIA.addr0 = RPS_XRAM_MODE2_TILEMAP_ADDR + tx + ((unsigned)old_row * RPS_TILEMAP_WIDTH_TILES);
            RIA.step0 = RPS_TILEMAP_WIDTH_TILES;
            for (uint8_t ty = old_row; ty < surface_row; ++ty) {
                RIA.rw0 = TILE_INDEX_SKY;
            }
        } else {
            // Ground moved up: previous sky strip becomes ground.
            RIA.addr0 = RPS_XRAM_MODE2_TILEMAP_ADDR + tx + ((unsigned)surface_row * RPS_TILEMAP_WIDTH_TILES);
            RIA.step0 = RPS_TILEMAP_WIDTH_TILES;
            for (uint8_t ty = surface_row; ty < old_row; ++ty) {
                RIA.rw0 = TILE_INDEX_GROUND;
            }
        }

        s_surface_row_cache[tx] = surface_row;
    }

    s_surface_row_cache_valid = 1u;
}

bool platform_video_init(void)
{
    // Mode-2 tile layer, 8x8 tiles, palette-indexed pixels.
    xram0_struct_set(RPS_XRAM_MODE2_CONFIG_ADDR, vga_mode2_config_t, x_wrap, false);
    xram0_struct_set(RPS_XRAM_MODE2_CONFIG_ADDR, vga_mode2_config_t, y_wrap, false);
    xram0_struct_set(RPS_XRAM_MODE2_CONFIG_ADDR, vga_mode2_config_t, x_pos_px, 0);
    xram0_struct_set(RPS_XRAM_MODE2_CONFIG_ADDR, vga_mode2_config_t, y_pos_px, 0);
    xram0_struct_set(RPS_XRAM_MODE2_CONFIG_ADDR, vga_mode2_config_t, width_tiles, RPS_TILEMAP_WIDTH_TILES);
    xram0_struct_set(RPS_XRAM_MODE2_CONFIG_ADDR, vga_mode2_config_t, height_tiles, RPS_TILEMAP_HEIGHT_TILES);
    xram0_struct_set(RPS_XRAM_MODE2_CONFIG_ADDR, vga_mode2_config_t, xram_data_ptr, RPS_XRAM_MODE2_TILEMAP_ADDR);
    xram0_struct_set(RPS_XRAM_MODE2_CONFIG_ADDR, vga_mode2_config_t, xram_palette_ptr, RPS_XRAM_MODE2_PALETTE_ADDR);
    xram0_struct_set(RPS_XRAM_MODE2_CONFIG_ADDR, vga_mode2_config_t, xram_tile_ptr, RPS_XRAM_MODE2_TILESET_ADDR);

    if (xreg_vga_mode(2, 0x02, RPS_XRAM_MODE2_CONFIG_ADDR, 0, 0, 0) < 0) {
        return false;
    }

    write_palette_entry(0, 0x0000);
    write_palette_entry(1, 0x7E1F);
    write_palette_entry(2, 0x0154);

    write_solid_4bpp_tile(TILE_INDEX_SKY, 1);
    write_solid_4bpp_tile(TILE_INDEX_GROUND, 2);

    memset(s_surface_row_cache, 0xFF, sizeof(s_surface_row_cache));
    s_surface_row_cache_valid = 0u;

    if (!init_player_sprite_layer()) {
        return false;
    }

    return true;
}

void platform_video_render(const game_state_t *state, uint8_t subframe)
{
    uint16_t camera_world_x = interpolate_world_x(state->prev_world_x, state->world_x, subframe);

    render_terrain_tilemap(camera_world_x);
    update_player_sprite_animation(state);
    update_player_sprite_position(state);
}