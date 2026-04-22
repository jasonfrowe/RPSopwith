#ifndef CONSTANTS_H
#define CONSTANTS_H

// -----------------------------------------------------------------------------
// Display and terrain dimensions
// -----------------------------------------------------------------------------

#define RPS_SCREEN_WIDTH_PX 320u
#define RPS_SCREEN_HEIGHT_PX 240u
#define RPS_TILE_SIZE_PX 8u

#define RPS_TILEMAP_WIDTH_TILES (RPS_SCREEN_WIDTH_PX / RPS_TILE_SIZE_PX)
#define RPS_TILEMAP_HEIGHT_TILES (RPS_SCREEN_HEIGHT_PX / RPS_TILE_SIZE_PX)

#define TERRAIN_WORLD_WIDTH 1024u

// -----------------------------------------------------------------------------
// XRAM layout (Phase 0/1): packed Mode-2 terrain resources
// -----------------------------------------------------------------------------
// Keeping this centralized mirrors the RPDemo style and makes growth visible.

#define RPS_XRAM_MODE2_CONFIG_ADDR 0x0200u
#define RPS_XRAM_MODE2_CONFIG_BYTES 0x0020u

#define RPS_MODE2_TILE_BYTES_4BPP 32u
#define RPS_MODE2_TILE_COUNT 256u
#define RPS_MODE2_TILESET_BYTES (RPS_MODE2_TILE_BYTES_4BPP * RPS_MODE2_TILE_COUNT)

#define RPS_MODE2_TILEMAP_BYTES (RPS_TILEMAP_WIDTH_TILES * RPS_TILEMAP_HEIGHT_TILES)

#define RPS_XRAM_MODE2_TILEMAP_ADDR (RPS_XRAM_MODE2_CONFIG_ADDR + RPS_XRAM_MODE2_CONFIG_BYTES)

// Align tile bitmap bank to 256-byte boundary for easier future tooling/streaming.
#define RPS_XRAM_MODE2_TILESET_ADDR (((RPS_XRAM_MODE2_TILEMAP_ADDR + RPS_MODE2_TILEMAP_BYTES) + 0x00FFu) & ~0x00FFu)
#define RPS_XRAM_MODE2_PALETTE_ADDR (RPS_XRAM_MODE2_TILESET_ADDR + RPS_MODE2_TILESET_BYTES)
#define RPS_XRAM_MODE2_PALETTE_BYTES 0x0020u

#define RPS_XRAM_MODE2_END (RPS_XRAM_MODE2_PALETTE_ADDR + RPS_XRAM_MODE2_PALETTE_BYTES)

// -----------------------------------------------------------------------------
// Mode-5 placeholder sprite layout (single player sprite now, room to scale)
// -----------------------------------------------------------------------------

#define RPS_MODE5_SPRITE_SIZE_PX 16u
#define RPS_MODE5_SPRITE_BYTES_4BPP 128u
#define RPS_MODE5_SPRITE_CONFIG_BYTES 8u
#define RPS_MODE5_SPRITE_CAPACITY 16u

#define RPS_XRAM_MODE5_CONFIG_ADDR (((RPS_XRAM_MODE2_END + 0x000Fu) & ~0x000Fu))
#define RPS_XRAM_MODE5_CONFIG_BYTES (RPS_MODE5_SPRITE_CONFIG_BYTES * RPS_MODE5_SPRITE_CAPACITY)

#define RPS_XRAM_MODE5_SPRITE_DATA_ADDR (((RPS_XRAM_MODE5_CONFIG_ADDR + RPS_XRAM_MODE5_CONFIG_BYTES) + 0x00FFu) & ~0x00FFu)
#define RPS_XRAM_MODE5_SPRITE_DATA_BYTES (RPS_MODE5_SPRITE_BYTES_4BPP * RPS_MODE5_SPRITE_CAPACITY)

#define RPS_XRAM_MODE5_PALETTE_ADDR (RPS_XRAM_MODE5_SPRITE_DATA_ADDR + RPS_XRAM_MODE5_SPRITE_DATA_BYTES)
#define RPS_XRAM_MODE5_PALETTE_BYTES 0x0020u

#define RPS_XRAM_VIDEO_END (RPS_XRAM_MODE5_PALETTE_ADDR + RPS_XRAM_MODE5_PALETTE_BYTES)

// -----------------------------------------------------------------------------
// RIA fixed input buffers (provided by firmware in high XRAM)
// -----------------------------------------------------------------------------

#define RPS_GAMEPAD_INPUT_ADDR 0xFF78u
#define RPS_KEYBOARD_INPUT_ADDR 0xFFA0u

#endif