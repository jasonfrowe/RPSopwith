#ifndef CONSTANTS_H
#define CONSTANTS_H

// Screen dimensions
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// Sprite data configuration
#define SPRITE_DATA_START       0x0000U            // Starting address in XRAM for sprite data

#define PLAYER_DATA            (SPRITE_DATA_START) // Address for main tile bitmap data
#define PLAYER_DATA_SIZE        0x1000U            // 4096 bytes (32 frames 16x16 at 4bpp)
#define PLAYER_SPRITE_SIZE_PX   16                 // Player sprite is 16x16 pixels
#define PLAYER_FRAME_SIZE       0x0080U            // 128 bytes per 16x16 4bpp frame
#define PLAYER_FRAME_COUNT      32                 // 32 frames (Rotation and flipping)

#define GROUND_DATA            (PLAYER_DATA + PLAYER_DATA_SIZE) // Address for terrain tile graphics
#define GROUND_DATA_SIZE        0x2BF2U            // 11250 bytes (375 x 30)
#define GROUND_WIDTH            375                // Width of terrain tileset in pixels
#define GROUND_HEIGHT           30                 // Height of terrain tileset in pixels

#define GROUND_TILES           (GROUND_DATA + GROUND_DATA_SIZE) // Address for terrain tilemap (tile ID grid)
#define GROUND_TILES_SIZE       0x2000U            // 8192 bytes (256 x 32 tile indices for 8x8 tiles)

#define SPRITE_DATA_END        (GROUND_TILES + GROUND_TILES_SIZE)

// Palette configurations
#define PLAYER_PALETTE_ADDR      0xFC00  // 16-color palette (32 bytes, 0xFC00-0xFC1F)
#define PLAYER_PALETTE_SIZE      0x0020
#define TILE_GROUND_PALETTE_ADDR 0xFC20  // 16-color palette for terrain tiles (32 bytes, 0xFC20-0xFC3F)
#define TILE_GROUND_PALETTE_SIZE 0x0020

// OPL2 sound chip configuration
#define OPL_XRAM_ADDR   0xFE00  // Native RIA OPL2 register page
#define OPL_SIZE        0x0100

// RIA input buffers are provided at fixed XRAM addresses.
#define GAMEPAD_INPUT   0xFF78  // 40 bytes for 4 gamepads
#define KEYBOARD_INPUT  0xFFA0  // 32 bytes keyboard bitfield

// Configs 
extern unsigned PLAYER_CONFIG; // Address in XRAM where player sprite config is stored, for updates

#endif // CONSTANTS_H