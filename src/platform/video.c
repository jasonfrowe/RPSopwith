#include "platform/video.h"

#include <rp6502.h>
#include <stdint.h>
#include <string.h>
#ifdef SOPWITH_DEBUG_XRAM
#include <stdio.h>
#endif

#include "constants.h"

#ifndef COLOR_FROM_RGB8
#define COLOR_FROM_RGB8(r,g,b) (((b >> 3) << 11) | ((g >> 3) << 6) | (r >> 3))
#endif

#ifndef COLOR_ALPHA_MASK
#define COLOR_ALPHA_MASK (1u << 5)
#endif

typedef struct {
    int16_t x_pos_px;
    int16_t y_pos_px;
    uint16_t xram_sprite_ptr;
    uint16_t palette_ptr;
} vga_mode5_sprite_t;

enum {
    PLAYER_COLOR_BODY = 3,
    PLAYER_COLOR_WING = 4,
    PLAYER_COLOR_TAIL = 5,
    PLAYER_COLOR_PROP = 6
};

static bool s_terrain_map_initialized = false;
static uint16_t s_terrain_loaded_left_tile = 0;
static uint8_t s_terrain_loaded_left_map_col = 0;
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

static int32_t positive_mod_i32(int32_t value, int32_t modulus)
{
    int32_t result = value % modulus;
    if (result < 0) {
        result += modulus;
    }
    return result;
}

static int32_t floor_div_tile(int32_t x)
{
    int32_t d = x / (int32_t)RPS_TILE_SIZE_PX;
    int32_t r = x % (int32_t)RPS_TILE_SIZE_PX;
    if (r < 0) {
        d--;
    }
    return d;
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

static uint16_t wrap_world_tile_x(int32_t tile_x)
{
    int32_t wrapped = tile_x % (int32_t)TERRAIN_WORLD_WIDTH_TILES;
    if (wrapped < 0) {
        wrapped += (int32_t)TERRAIN_WORLD_WIDTH_TILES;
    }
    return (uint16_t)wrapped;
}

static int16_t cyclic_world_tile_delta(uint16_t from_tile, uint16_t to_tile)
{
    int16_t delta = (int16_t)to_tile - (int16_t)from_tile;
    int16_t half = (int16_t)(TERRAIN_WORLD_WIDTH_TILES / 2u);

    if (delta > half) {
        delta -= (int16_t)TERRAIN_WORLD_WIDTH_TILES;
    } else if (delta < -half) {
        delta += (int16_t)TERRAIN_WORLD_WIDTH_TILES;
    }

    return delta;
}

static void write_palette_entry(uint8_t index, uint16_t rgb555)
{
    RIA.addr0 = RPS_XRAM_MODE2_PALETTE_ADDR + ((unsigned)index * 2u);
    RIA.step0 = 1;
    RIA.rw0 = (uint8_t)(rgb555 & 0xFFu);
    RIA.rw0 = (uint8_t)(rgb555 >> 8);
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
    uint8_t angle = (uint8_t)state->plane_pitch & 0x0Fu;
    uint8_t a = (uint8_t)((16u - angle) & 0x0Fu);
    uint8_t frame_index;

    if (state->plane_orient) {
        // Match original Sopwith symbol choice for flipped orientation:
        // a = (16 - angle) % 16; use mirrored frame bank.
        frame_index = (uint8_t)(16u + a);
    } else {
        frame_index = angle;
    }

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
    int16_t sprite_y = state->plane_y;

    if (sprite_x < 0) {
        sprite_x = 0;
    }
    if (sprite_y < -(int16_t)RPS_MODE5_SPRITE_SIZE_PX) {
        sprite_y = -(int16_t)RPS_MODE5_SPRITE_SIZE_PX;
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

#ifdef SOPWITH_DEBUG_XRAM
static void log_xram_layout(void)
{
    printf("[XRAM] Mode2 cfg  0x%04X..0x%04X\n", RPS_XRAM_MODE2_CONFIG_ADDR, RPS_XRAM_MODE2_TILEMAP_ADDR - 1u);
    printf("[XRAM] Mode2 map  0x%04X..0x%04X\n", RPS_XRAM_MODE2_TILEMAP_ADDR, RPS_XRAM_MODE2_TILESET_ADDR - 1u);
    printf("[XRAM] Mode2 tile 0x%04X..0x%04X\n", RPS_XRAM_MODE2_TILESET_ADDR, RPS_XRAM_MODE2_PALETTE_ADDR - 1u);
    printf("[XRAM] Mode2 pal  0x%04X..0x%04X\n", RPS_XRAM_MODE2_PALETTE_ADDR, RPS_XRAM_MODE2_END - 1u);
    printf("[XRAM] Mode5 cfg  0x%04X..0x%04X\n", RPS_XRAM_MODE5_CONFIG_ADDR, RPS_XRAM_MODE5_SPRITE_DATA_ADDR - 1u);
    printf("[XRAM] Mode5 data 0x%04X..0x%04X\n", RPS_XRAM_MODE5_SPRITE_DATA_ADDR, RPS_XRAM_MODE5_PALETTE_ADDR - 1u);
    printf("[XRAM] Mode5 pal  0x%04X..0x%04X\n", RPS_XRAM_MODE5_PALETTE_ADDR, RPS_XRAM_VIDEO_END - 1u);
    printf("[XRAM] Input gp   0x%04X..0x%04X\n", RPS_GAMEPAD_INPUT_ADDR, (unsigned)(RPS_GAMEPAD_INPUT_ADDR + RPS_GAMEPAD_INPUT_BYTES - 1u));
    printf("[XRAM] Input kb   0x%04X..0x%04X\n", RPS_KEYBOARD_INPUT_ADDR, (unsigned)(RPS_KEYBOARD_INPUT_ADDR + RPS_KEYBOARD_INPUT_BYTES - 1u));
    printf("[ROM ] Terrain    0x%05X..0x%05X\n", (unsigned)RPS_XRAM_TERRAIN_TILESET_ADDR, (unsigned)(RPS_XRAM_TERRAIN_TILESET_ADDR + RPS_XRAM_TERRAIN_TILESET_BYTES - 1u));
    printf("[ROM ] World map  0x%05X..0x%05X\n", (unsigned)RPS_XRAM_WORLD_TILEMAP_ADDR, (unsigned)(RPS_XRAM_WORLD_TILEMAP_ADDR + RPS_XRAM_WORLD_TILEMAP_BYTES - 1u));
}
#endif

static void load_world_tile_column(uint8_t map_col, uint16_t world_tile_x)
{
    for (uint8_t row = 0; row < RPS_TILEMAP_HEIGHT_TILES; ++row) {
        unsigned src = RPS_XRAM_WORLD_TILEMAP_ADDR +
                       ((unsigned)row * TERRAIN_WORLD_WIDTH_TILES) +
                       world_tile_x;
        unsigned dst = RPS_XRAM_MODE2_TILEMAP_ADDR +
                       ((unsigned)row * RPS_TILEMAP_WIDTH_TILES) +
                       map_col;
        xram_write_u8(dst, xram_read_u8(src));
    }
}

static void render_terrain_tilemap(uint16_t camera_world_x)
{
    int32_t camera_left = (int32_t)camera_world_x - ((int32_t)RPS_SCREEN_WIDTH_PX / 2);
    uint16_t camera_left_tile = wrap_world_tile_x(floor_div_tile(camera_left));
    int16_t camera_x_subtile_px = (int16_t)positive_mod_i32(camera_left, (int32_t)RPS_TILE_SIZE_PX);

    if (!s_terrain_map_initialized) {
        s_terrain_loaded_left_tile = camera_left_tile;
        s_terrain_loaded_left_map_col = 0u;
        for (uint8_t map_col = 0; map_col < RPS_TILEMAP_WIDTH_TILES; ++map_col) {
            uint16_t world_tile = (uint16_t)((s_terrain_loaded_left_tile + map_col) % TERRAIN_WORLD_WIDTH_TILES);
            load_world_tile_column(map_col, world_tile);
        }
        s_terrain_map_initialized = true;
    }

    int16_t delta = cyclic_world_tile_delta(s_terrain_loaded_left_tile, camera_left_tile);

    while (delta > 0) {
        uint16_t new_right_tile = (uint16_t)((s_terrain_loaded_left_tile + RPS_TILEMAP_WIDTH_TILES) % TERRAIN_WORLD_WIDTH_TILES);
        load_world_tile_column(s_terrain_loaded_left_map_col, new_right_tile);
        s_terrain_loaded_left_tile = (uint16_t)((s_terrain_loaded_left_tile + 1u) % TERRAIN_WORLD_WIDTH_TILES);
        s_terrain_loaded_left_map_col = (uint8_t)((s_terrain_loaded_left_map_col + 1u) % RPS_TILEMAP_WIDTH_TILES);
        delta--;
    }

    while (delta < 0) {
        uint16_t new_left_tile = (uint16_t)((s_terrain_loaded_left_tile + TERRAIN_WORLD_WIDTH_TILES - 1u) % TERRAIN_WORLD_WIDTH_TILES);
        uint8_t new_left_map_col = (uint8_t)((s_terrain_loaded_left_map_col + RPS_TILEMAP_WIDTH_TILES - 1u) % RPS_TILEMAP_WIDTH_TILES);
        load_world_tile_column(new_left_map_col, new_left_tile);
        s_terrain_loaded_left_tile = new_left_tile;
        s_terrain_loaded_left_map_col = new_left_map_col;
        delta++;
    }

    xram0_struct_set(
        RPS_XRAM_MODE2_CONFIG_ADDR,
        vga_mode2_config_t,
        x_pos_px,
        (int16_t)(-((int16_t)s_terrain_loaded_left_map_col * (int16_t)RPS_TILE_SIZE_PX + camera_x_subtile_px))
    );
}

bool platform_video_init(void)
{
    // Mode-2 tile layer, 8x8 tiles, palette-indexed pixels.
    xram0_struct_set(RPS_XRAM_MODE2_CONFIG_ADDR, vga_mode2_config_t, x_wrap, true);
    xram0_struct_set(RPS_XRAM_MODE2_CONFIG_ADDR, vga_mode2_config_t, y_wrap, false);
    xram0_struct_set(RPS_XRAM_MODE2_CONFIG_ADDR, vga_mode2_config_t, x_pos_px, 0);
    xram0_struct_set(RPS_XRAM_MODE2_CONFIG_ADDR, vga_mode2_config_t, y_pos_px, 0);
    xram0_struct_set(RPS_XRAM_MODE2_CONFIG_ADDR, vga_mode2_config_t, width_tiles, RPS_TILEMAP_WIDTH_TILES);
    xram0_struct_set(RPS_XRAM_MODE2_CONFIG_ADDR, vga_mode2_config_t, height_tiles, RPS_TILEMAP_HEIGHT_TILES);
    xram0_struct_set(RPS_XRAM_MODE2_CONFIG_ADDR, vga_mode2_config_t, xram_data_ptr, RPS_XRAM_MODE2_TILEMAP_ADDR);
    xram0_struct_set(RPS_XRAM_MODE2_CONFIG_ADDR, vga_mode2_config_t, xram_palette_ptr, RPS_XRAM_MODE2_PALETTE_ADDR);
    xram0_struct_set(RPS_XRAM_MODE2_CONFIG_ADDR, vga_mode2_config_t, xram_tile_ptr, RPS_XRAM_TERRAIN_TILESET_ADDR);

    if (xreg_vga_mode(2, 0x02, RPS_XRAM_MODE2_CONFIG_ADDR, 0, 0, 0) < 0) {
        return false;
    }

    write_palette_entry(0, 0x0000);
    // Use documented RGB packing + alpha bit to avoid tint artifacts.
    write_palette_entry(1, COLOR_FROM_RGB8(120, 190, 255) | COLOR_ALPHA_MASK);
    write_palette_entry(2, COLOR_FROM_RGB8(96, 176, 92) | COLOR_ALPHA_MASK);
    s_terrain_map_initialized = false;

    if (!init_player_sprite_layer()) {
        return false;
    }

#ifdef SOPWITH_DEBUG_XRAM
    log_xram_layout();
#endif

    return true;
}

void platform_video_render(const game_state_t *state, uint8_t subframe)
{
    uint16_t camera_world_x = interpolate_world_x(state->prev_world_x, state->world_x, subframe);

    render_terrain_tilemap(camera_world_x);
    update_player_sprite_animation(state);
    update_player_sprite_position(state);
}