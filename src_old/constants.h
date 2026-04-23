#ifndef CONSTANTS_H
#define CONSTANTS_H

// -----------------------------------------------------------------------------
// Display and terrain dimensions
// -----------------------------------------------------------------------------

#define RPS_SCREEN_WIDTH_PX 320u
#define RPS_SCREEN_HEIGHT_PX 240u
#define RPS_TILE_SIZE_PX 8u

#define RPS_SOURCE_SCREEN_HEIGHT_PX 200u
#define RPS_TERRAIN_SCREEN_Y_OFFSET_PX 0u

#define RPS_TILEMAP_WIDTH_TILES 64u
#define RPS_TILEMAP_HEIGHT_TILES (RPS_SCREEN_HEIGHT_PX / RPS_TILE_SIZE_PX)

#define TERRAIN_WORLD_WIDTH 3000u
#define TERRAIN_WORLD_WIDTH_TILES (TERRAIN_WORLD_WIDTH / RPS_TILE_SIZE_PX)
#define TERRAIN_WORLD_HEIGHT_TILES RPS_TILEMAP_HEIGHT_TILES

#define RPS_PLAYER1_START_WORLD_X 1270u
#define RPS_PLAYER_RUNWAY_SPAN_PX 20u
#define RPS_PLANE_GROUND_CONTACT_FROM_TOP_PX 13u

// -----------------------------------------------------------------------------
// Flight model (Sopwith-inspired 10 Hz tuning)
// -----------------------------------------------------------------------------

#define RPS_FPS 10u
#define RPS_MIN_SPEED 4
#define RPS_MAX_SPEED 8
#define RPS_MAX_THROTTLE 4

#define RPS_FLAP_PITCH_MIN (-4)
#define RPS_FLAP_PITCH_MAX 4

#define RPS_TAKEOFF_SPEED 6
#define RPS_TAKEOFF_LIFT_VY (-3)
#define RPS_GRAVITY_PER_TICK 1

// -----------------------------------------------------------------------------
// XRAM/ROM layout (RPDemo-style: explicit ADDR + SIZE tracking)
// -----------------------------------------------------------------------------
// Code uses helper-window addresses (0x0000-0xFFFF). CMake holds full ROM addrs.

// Mode-2 tile resources
#define RPS_MODE2_TILE_BYTES_4BPP 32u
#define RPS_MODE2_TILE_COUNT 256u
#define RPS_MODE2_TILESET_BYTES (RPS_MODE2_TILE_BYTES_4BPP * RPS_MODE2_TILE_COUNT)
#define RPS_MODE2_TILEMAP_BYTES (RPS_TILEMAP_WIDTH_TILES * RPS_TILEMAP_HEIGHT_TILES)

// Mode-5 sprite resources
#define RPS_MODE5_SPRITE_SIZE_PX 16u
#define RPS_MODE5_SPRITE_BYTES_4BPP 128u
#define RPS_MODE5_SPRITE_CONFIG_BYTES 8u
#define RPS_MODE5_SPRITE_CAPACITY 16u

#define RPS_PLAYER_BANK_MIN (-4)
#define RPS_PLAYER_BANK_MAX 4
#define RPS_PLAYER_BANK_FRAME_COUNT 32u

// Runtime XRAM layout
#define RPS_MODE2_CONFIG_ADDR 0x0200u
#define RPS_MODE2_CONFIG_SIZE 0x0020u

#define RPS_MODE2_TILEMAP_ADDR (RPS_MODE2_CONFIG_ADDR + RPS_MODE2_CONFIG_SIZE)
#define RPS_MODE2_TILEMAP_SIZE RPS_MODE2_TILEMAP_BYTES

// Mode-2 tileset location in helper window (matches ROM helper alias 0x0A00)
#define RPS_MODE2_TILESET_ADDR (((RPS_MODE2_TILEMAP_ADDR + RPS_MODE2_TILEMAP_SIZE) + 0x00FFu) & ~0x00FFu)
#define RPS_MODE2_TILESET_SIZE RPS_MODE2_TILESET_BYTES

#define RPS_MODE2_PALETTE_ADDR (RPS_MODE2_TILESET_ADDR + RPS_MODE2_TILESET_SIZE)
#define RPS_MODE2_PALETTE_SIZE 0x0020u

#define RPS_MODE2_END (RPS_MODE2_PALETTE_ADDR + RPS_MODE2_PALETTE_SIZE)

#define RPS_MODE5_CONFIG_ADDR (((RPS_MODE2_END + 0x000Fu) & ~0x000Fu))
#define RPS_MODE5_CONFIG_SIZE (RPS_MODE5_SPRITE_CONFIG_BYTES * RPS_MODE5_SPRITE_CAPACITY)

// RPDemo-style: no extra 256-byte alignment here; data follows config directly.
#define RPS_MODE5_SPRITE_DATA_ADDR (RPS_MODE5_CONFIG_ADDR + RPS_MODE5_CONFIG_SIZE)
#define RPS_MODE5_SPRITE_DATA_SIZE (RPS_MODE5_SPRITE_BYTES_4BPP * RPS_PLAYER_BANK_FRAME_COUNT)

#define RPS_MODE5_PALETTE_ADDR (RPS_MODE5_SPRITE_DATA_ADDR + RPS_MODE5_SPRITE_DATA_SIZE)
#define RPS_MODE5_PALETTE_SIZE 0x0020u

#define RPS_VIDEO_END (RPS_MODE5_PALETTE_ADDR + RPS_MODE5_PALETTE_SIZE)

// Backward-compatible aliases used by current code
#define RPS_XRAM_MODE2_CONFIG_ADDR RPS_MODE2_CONFIG_ADDR
#define RPS_XRAM_MODE2_CONFIG_BYTES RPS_MODE2_CONFIG_SIZE
#define RPS_XRAM_MODE2_TILEMAP_ADDR RPS_MODE2_TILEMAP_ADDR
#define RPS_XRAM_MODE2_TILESET_ADDR RPS_MODE2_TILESET_ADDR
#define RPS_XRAM_MODE2_PALETTE_ADDR RPS_MODE2_PALETTE_ADDR
#define RPS_XRAM_MODE2_PALETTE_BYTES RPS_MODE2_PALETTE_SIZE
#define RPS_XRAM_MODE2_END RPS_MODE2_END
#define RPS_XRAM_MODE5_CONFIG_ADDR RPS_MODE5_CONFIG_ADDR
#define RPS_XRAM_MODE5_CONFIG_BYTES RPS_MODE5_CONFIG_SIZE
#define RPS_XRAM_MODE5_SPRITE_DATA_ADDR RPS_MODE5_SPRITE_DATA_ADDR
#define RPS_XRAM_MODE5_SPRITE_DATA_BYTES RPS_MODE5_SPRITE_DATA_SIZE
#define RPS_XRAM_MODE5_PALETTE_ADDR RPS_MODE5_PALETTE_ADDR
#define RPS_XRAM_MODE5_PALETTE_BYTES RPS_MODE5_PALETTE_SIZE
#define RPS_XRAM_VIDEO_END RPS_VIDEO_END

// ROM helper-window aliases (actual asset sizes tracked here)
// CMake full addresses:
//   terrain tileset -> 0x10A00 (helper 0x0A00, size 0x2000)
//   player strip    -> 0x12B00 (helper 0x2B00, size 0x1000)
//   player palette  -> 0x13B00 (helper 0x3B00, size 0x0020)
//   world tilemap   -> 0x13C00 (helper 0x3C00, size 375*30 = 11250)
#define RPS_XRAM_TERRAIN_TILESET_ADDR 0x0A00u
#define RPS_XRAM_TERRAIN_TILESET_BYTES RPS_MODE2_TILESET_BYTES
#define RPS_XRAM_PLAYER_STRIP_ADDR 0x2B00u
#define RPS_XRAM_PLAYER_STRIP_BYTES (RPS_MODE5_SPRITE_BYTES_4BPP * RPS_PLAYER_BANK_FRAME_COUNT)
#define RPS_XRAM_PLAYER_PALETTE_ADDR 0x3B00u
#define RPS_XRAM_PLAYER_PALETTE_BYTES 0x0020u
#define RPS_XRAM_WORLD_TILEMAP_ADDR 0x3C00u
#define RPS_XRAM_WORLD_TILEMAP_BYTES (TERRAIN_WORLD_WIDTH_TILES * TERRAIN_WORLD_HEIGHT_TILES)

// -----------------------------------------------------------------------------
// RIA fixed input buffers (provided by firmware in high XRAM)
// -----------------------------------------------------------------------------

// OPL2 sound chip register page (native RIA mapping).
#define OPL_XRAM_ADDR 0xFE00u
#define OPL_SIZE 0x0100u

#define RPS_GAMEPAD_INPUT_ADDR 0xFF78u
#define RPS_KEYBOARD_INPUT_ADDR 0xFFA0u

// -----------------------------------------------------------------------------
// Compile-time XRAM safety checks
// -----------------------------------------------------------------------------

#define RPS_GAMEPAD_INPUT_BYTES 10u
#define RPS_KEYBOARD_INPUT_BYTES 32u

_Static_assert(
	RPS_XRAM_MODE2_CONFIG_ADDR + RPS_XRAM_MODE2_CONFIG_BYTES <= RPS_XRAM_MODE2_TILEMAP_ADDR,
	"Mode-2 config overlaps tilemap"
);
_Static_assert(
	RPS_XRAM_MODE2_TILEMAP_ADDR + RPS_MODE2_TILEMAP_BYTES <= RPS_XRAM_MODE2_TILESET_ADDR,
	"Mode-2 tilemap overlaps tileset"
);
_Static_assert(
	RPS_XRAM_MODE2_TILESET_ADDR + RPS_MODE2_TILESET_BYTES <= RPS_XRAM_MODE2_PALETTE_ADDR,
	"Mode-2 tileset overlaps palette"
);
_Static_assert(
	RPS_XRAM_MODE2_PALETTE_ADDR + RPS_XRAM_MODE2_PALETTE_BYTES <= RPS_XRAM_MODE5_CONFIG_ADDR,
	"Mode-2 palette overlaps Mode-5 config"
);
_Static_assert(
	RPS_XRAM_MODE5_CONFIG_ADDR + RPS_XRAM_MODE5_CONFIG_BYTES <= RPS_XRAM_MODE5_SPRITE_DATA_ADDR,
	"Mode-5 config overlaps sprite data"
);
_Static_assert(
	RPS_XRAM_MODE5_SPRITE_DATA_ADDR + RPS_XRAM_MODE5_SPRITE_DATA_BYTES <= RPS_XRAM_MODE5_PALETTE_ADDR,
	"Mode-5 sprite data overlaps palette"
);
_Static_assert(
	RPS_XRAM_TERRAIN_TILESET_ADDR + RPS_XRAM_TERRAIN_TILESET_BYTES <= RPS_XRAM_PLAYER_STRIP_ADDR,
	"Terrain tileset overlaps player strip in helper window"
);
_Static_assert(
	RPS_XRAM_PLAYER_STRIP_ADDR + RPS_XRAM_PLAYER_STRIP_BYTES <= RPS_XRAM_PLAYER_PALETTE_ADDR,
	"Player strip overlaps player palette in helper window"
);
_Static_assert(
	RPS_XRAM_PLAYER_PALETTE_ADDR + RPS_XRAM_PLAYER_PALETTE_BYTES <= RPS_XRAM_WORLD_TILEMAP_ADDR,
	"Player palette overlaps world tilemap in helper window"
);
_Static_assert(
	RPS_XRAM_VIDEO_END <= RPS_GAMEPAD_INPUT_ADDR,
	"Runtime video XRAM overlaps firmware gamepad buffer"
);
_Static_assert(
	RPS_XRAM_VIDEO_END <= OPL_XRAM_ADDR,
	"Runtime video XRAM overlaps OPL2 register page"
);
_Static_assert(
	OPL_XRAM_ADDR + OPL_SIZE <= RPS_GAMEPAD_INPUT_ADDR,
	"OPL2 register page overlaps firmware gamepad buffer"
);
_Static_assert(
	RPS_GAMEPAD_INPUT_ADDR + RPS_GAMEPAD_INPUT_BYTES <= RPS_KEYBOARD_INPUT_ADDR,
	"Gamepad and keyboard firmware buffers overlap"
);
_Static_assert(
	RPS_KEYBOARD_INPUT_ADDR + RPS_KEYBOARD_INPUT_BYTES <= 0x10000u,
	"Keyboard firmware buffer exceeds XRAM range"
);

#endif