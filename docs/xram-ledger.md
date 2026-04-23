# RPSopwith XRAM Ownership Ledger (Baseline)

This document describes the current known-good memory ownership model.
All addresses are from `src/constants.h` and `CMakeLists.txt`.

## Runtime XRAM Regions (0x0000-0xFFFF)

| Region | Start | End | Owner | Mutability | Notes |
| --- | --- | --- | --- | --- | --- |
| Mode-2 config | 0x0200 | 0x021F | video.c | boot + frame | Tile layer config and camera x scroll |
| Mode-2 tilemap | 0x0220 | 0x09BF | video.c | boot + frame | 64x30 visible map ring buffer |
| Mode-2 tileset | 0x0A00 | 0x29FF | ROM asset data | read-only | Terrain tileset loaded at 0x10A00, pointer into XRAM |
| Mode-2 palette | 0x2A00 | 0x2A1F | video.c | boot | Sky/ground entries |
| Mode-5 config | 0x2A20 | 0x2A9F | video.c | boot + frame | Sprite slot config array |
| Mode-5 sprite data | 0x2B00 | 0x3AFF | video.c | boot | Generated player frames |
| Mode-5 palette | 0x3B00 | 0x3B1F | video.c | boot + frame | Player palette + prop animation |
| OPL2 register page | 0xFE00 | 0xFEFF | RIA OPL2 | runtime audio | Reserved for OPL2 register interface |

## Firmware-Owned XRAM Buffers (Do Not Write)

| Buffer | Start | End | Owner | Access Rule |
| --- | --- | --- | --- | --- |
| Gamepad snapshot | 0xFF78 | 0xFF81 | RP6502 firmware | Read-only from game code |
| Keyboard bitfield | 0xFFA0 | 0xFFBF | RP6502 firmware | Read-only from game code |

## ROM Asset Placement (CMake rp6502_asset)

| Asset | Address | Bytes | Source |
| --- | --- | --- | --- |
| Terrain tileset | 0x10A00 | 8192 | `build/assets/terrain_tileset_4bpp.bin` |
| Player strip | 0x12B00 | 4096 | `build/assets/player_bank_strip_4bpp.bin` |
| Player palette | 0x13B00 | 32 | `build/assets/player_bank_strip_4bpp_palette.bin` |
| World tilemap | 0x13C00 | 11250 | `build/assets/terrain_world_tilemap.bin` |

## Ownership Rules

1. Only `src/platform/video.c` writes runtime video regions.
2. `src/platform/input.c` only reads firmware input buffers.
3. Any new sprite/effect region must be allocated in `src/constants.h` and covered by compile-time overlap checks.
4. Any new ROM asset address must be listed here and reviewed for overlap with existing assets.
5. `0xFE00..0xFEFF` is reserved for OPL2 and must not be used for general-purpose runtime buffers.

## Debug Support

Set CMake option `SOPWITH_ENABLE_XRAM_DEBUG=ON` to print resolved XRAM/ROM ranges at boot from `platform_video_init()`.

Example configure:

```sh
cmake -S . -B build -DSOPWITH_ENABLE_XRAM_DEBUG=ON
```
