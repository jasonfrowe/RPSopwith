#ifndef CONSTANTS_H
#define CONSTANTS_H

// Screen dimensions
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// Sprite data configuration
#define SPRITE_DATA_START       0x0000U            // Starting address in XRAM for sprite data

#define PLAYER_DATA            (SPRITE_DATA_START) // Address for main tile bitmap data
#define PLAYER_DATA_SIZE        0x1800U            // 6144 bytes (48 frames 16x16 at 4bpp)
#define PLAYER_SPRITE_SIZE_PX   16                 // Player sprite is 16x16 pixels
#define PLAYER_FRAME_SIZE       0x0080U            // 128 bytes per 16x16 4bpp frame
#define PLAYER_FRAME_COUNT      48                 // 48 frames (Rotation and flipping and flying away)
#define MAX_ENEMIES             2                  // Max number of enemy planes on screen

#define GROUND_DATA            (PLAYER_DATA + PLAYER_DATA_SIZE) // Address for terrain tile map
#define GROUND_DATA_SIZE        0x2BF2U            // 11250 bytes (375 x 30)
#define GROUND_WIDTH            375                // Width of terrain tileset in pixels
#define GROUND_HEIGHT           30                 // Height of terrain tileset in pixels

#define GROUND_TILES           (GROUND_DATA + GROUND_DATA_SIZE) // Address for terrain tilemap (tile ID grid)
#define GROUND_TILES_SIZE       0x2000U            // 8192 bytes (256 x 32 tile indices for 8x8 tiles)

#define TARGETS_DATA           (GROUND_TILES + GROUND_TILES_SIZE) // Address for target sprite graphics (not used yet)
#define TARGETS_DATA_SIZE       0x0F00U            // 3840 bytes (30 frames of 16x16 at 4bpp)
#define TARGETS_SPRITE_SIZE_PX  16                 // Target sprite is 16x16 pixels
#define TARGETS_FRAME_SIZE      0x0080U            // 128 bytes per 16x16 4bpp frame
#define TARGETS_FRAME_COUNT     30                 // 30 frames different target graphics
#define MAX_TARGETS             24                 // 22 authored ground targets in the current level, plus 2 spare slots

#define PROJECTILE_DATA          (TARGETS_DATA + TARGETS_DATA_SIZE) // Address for projectile sprite graphics (not used yet)
#define PROJECTILE_DATA_SIZE      0x0E80U            // 3712 bytes (29 frames of 16x16 at 4bpp)
#define PROJECTILE_SPRITE_SIZE_PX 16                // Projectile sprite is 16x16 pixels
#define PROJECTILE_FRAME_SIZE     0x0080U            // 128 bytes per 16x16 4bpp frame
#define PROJECTILE_FRAME_COUNT    29                 // 29 frames different projectile graphics
#define MAX_PROJECTILES           42                 // Max number of projectiles on screen
#define MAX_FLOCK_SPRITES          4                 // Reserved ambient flock sprite slots
#define MAX_BIRD_SPRITES          12                 // Reserved ambient single-bird sprite slots
#define MAX_SPLAT_SPRITES          2                 // Reserved bird-splat cue sprite slots
#define MAX_COMBAT_PROJECTILES   (MAX_PROJECTILES - MAX_FLOCK_SPRITES - MAX_BIRD_SPRITES - MAX_SPLAT_SPRITES)

#define TEXT_DATA                (PROJECTILE_DATA + PROJECTILE_DATA_SIZE) // Address for text glyph graphics (not used yet)
#define TEXT_DATA_SIZE            0x0960U            // 2400 bytes (40x30 glyphs of 8x8 at 4bpp)
#define TEXT_WIDTH_CHARS          40                 // Number of glyphs per row in text data
#define TEXT_HEIGHT_CHARS         30                 // Number of glyph rows in text data

#define HUD_MAP_DATA             (TEXT_DATA + TEXT_DATA_SIZE) // Address for HUD map 
#define HUD_MAP_DATA_SIZE         0x04B0U            // 1200 bytes (40x30 tiles of 8x8)
#define HUD_MAP_WIDTH_TILES       40                 // Width of HUD map in tiles
#define HUD_MAP_HEIGHT_TILES      30                 // Height of HUD map in tiles

#define HUD_TILES                (HUD_MAP_DATA + HUD_MAP_DATA_SIZE) // Address for HUD tile graphics
#define HUD_TILES_SIZE            0x0D60U            // 3424 bytes (107 tiles of 8x8 at 4bpp)

#define SPRITE_DATA_END        (HUD_TILES + HUD_TILES_SIZE)

// Palette configurations
#define PLAYER_PALETTE_ADDR      0xFC00  // 16-color palette (32 bytes, 0xFC00-0xFC1F)
#define PLAYER_PALETTE_SIZE      0x0020
#define TILE_GROUND_PALETTE_ADDR 0xFC20  // 16-color palette for terrain tiles (32 bytes, 0xFC20-0xFC3F)
#define TILE_GROUND_PALETTE_SIZE 0x0020
#define TARGETS_PALETTE_ADDR     0xFC40  // 16-color palette for target tiles (32 bytes, 0xFC40-0xFC5F)
#define TARGETS_PALETTE_SIZE     0x0020
#define PROJECTILE_PALETTE_ADDR  0xFC60  // 16-color palette for projectiles (32 bytes, 0xFC60-0xFC7F)
#define PROJECTILE_PALETTE_SIZE  0x0020
#define TEXT_PALETTE_ADDR        0xFC80  // 16-color palette for text (32 bytes, 0xFC80-0xFC9F)
#define TEXT_PALETTE_SIZE        0x0020  // 32 bytes
#define PLAYER_TARGETS_PALETTE_ADDR 0xFCA0  // 16-color palette for player-owned buildings (32 bytes, 0xFCA0-0xFCBF)
#define PLAYER_TARGETS_PALETTE_SIZE 0x0020
#define HUD_PALETTE_ADDR         0xFCC0  // 16-color palette for HUD tiles (32 bytes, 0xFCC0-0xFCDFF)
#define HUD_PALETTE_SIZE         0x0020
#define ENEMY_PALETTE_ADDR       0xFCE0  // 16-color palette for enemy planes (32 bytes, 0xFCE0-0xFCFF)
#define ENEMY_PALETTE_SIZE       0x0020


// OPL2 sound chip configuration
#define OPL_XRAM_ADDR   0xFE00  // Native RIA OPL2 register page
#define OPL_SIZE        0x0100

// RIA input buffers are provided at fixed XRAM addresses.
#define GAMEPAD_INPUT   0xFF78  // 40 bytes for 4 gamepads
#define KEYBOARD_INPUT  0xFFA0  // 32 bytes keyboard bitfield

// Gameplay positioning and movement
#define PLAYER_START_WORLD_X_PX            1270
#define PLAYER_GROUND_CONTACT_FROM_TOP_PX  13
#define PLAYER_FLIGHT_SPEED_X_PX           2
#define PLAYER_FLIGHT_SPEED_Y_PX           2

// Gameplay features (toggles for classic vs enhanced behavior)
// When enabled, hit planes enter a wounded state with reduced control before crashing.
// When disabled, hit planes immediately enter uncontrolled falling (classic DOS behavior).
#define ENABLE_WOUNDED_STATE               0

// Configs 
extern unsigned PLAYER_CONFIG; // Address in XRAM where player sprite config is stored, for updates
extern unsigned TILE_GROUND_CONFIG; // Address in XRAM where tilemap config is stored, for updates
extern unsigned TARGETS_CONFIG; // Address in XRAM where target sprites config is stored, for
extern unsigned PROJECTILES_CONFIG; // Address in XRAM where projectile sprites config is stored, for updates
extern unsigned TEXT_CONFIG; // Address in XRAM where text sprites config is stored, for updates
extern unsigned ENEMY_CONFIG; // Address in XRAM where enemy plane sprites config is stored, for updates

#endif // CONSTANTS_H