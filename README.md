# RPSopwith

RPSopwith is an in-progress port of SDL Sopwith to the Picocomputer RP6502.

## Current Status

Implemented and stable enough for continued feature work:
- Fixed-step gameplay loop at 10 Hz with interpolated rendering on VSYNC.
- Single ground layer using Mode-2 tilemap scroll.
- Player sprite layer using Mode-5 4bpp sprites.
- Prebuilt terrain pipeline:
  - source ground profile from original Sopwith data,
  - generated Mode-2 terrain tileset,
  - generated full world tilemap,
  - runtime ring-buffer column streaming from world map into visible map.
- Input path for keyboard and gamepad with Sopwith-oriented action mapping.
- Flight, landing, stall, crash, restart loop working at MVP level.

Not done yet:
- Core combat entities (shots, bombs, explosions as full gameplay systems).
- AI/autopilot, bases, scoring, and mission loop parity.
- HUD/audio/content parity polish.

## Rendering Architecture

Layer model:
- Ground: Mode-2 tile layer only.
- Player: Mode-5 sprite layer.

Terrain model:
- Canonical world terrain width is 3000 px.
- Build step generates:
  - 256-tile 4bpp terrain tileset,
  - 375 x 30 world tilemap for 320x240 with 8x8 tiles.
- Runtime updates only the entering columns of a 64-column visible ring tilemap.

Key files:
- src/platform/video.c
- src/game/terrain/terrain.c
- tools/generate_terrain_mode2_assets.py
- src/constants.h

## Build Requirements

- LLVM-MOS SDK and toolchain
- CMake
- Python virtual environment at .venv

The build prefers .venv/bin/python automatically when present.

## Python Setup

Create and populate the local environment:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install --upgrade pip
.venv/bin/python -m pip install pillow
```

## Build

```sh
cmake -S . -B build
cmake --build build
```

Output ROM:
- build/RPSopwith.rp6502

## Run On Device

```sh
python ./tools/rp6502.py run build/RPSopwith.rp6502
```

## Controls (Current)

Keyboard defaults:
- Left/Right arrows or A/D: throttle up/down
- Up/Down arrows or W/S: flap down/up input
- C: flip orientation
- Home or H: request landing/home
- Z or Space: fire
- X: bomb
- Enter: start/reset from crash

Gamepad defaults are loaded from JOYSTICK_SH.DAT when present, otherwise built-in defaults are used.

## Asset Pipeline

Player assets:
- tools/assets/generate_player_strip.py
- tools/assets/convert_sprite.py

Terrain assets:
- tools/generate_terrain_mode2_assets.py
- src/game/terrain/original_ground_3000.h

Generated outputs:
- build/assets/player_bank_strip_4bpp.bin
- build/assets/player_bank_strip_4bpp_palette.bin
- build/assets/terrain_tileset_4bpp.bin
- build/assets/terrain_world_tilemap.bin

## XRAM Layout Notes

Keep addresses centralized in src/constants.h.

Current packed assets:
- Terrain tileset at 0x10A00
- Player strip data at 0x12B00
- Player palette at 0x13B00
- Terrain world tilemap at 0x13C00

## Roadmap (Revisited Plan)

Completed foundation phases:
1. Project skeleton, feature flags, phased bring-up.
2. Platform video backend with Mode-2 and Mode-5.
3. Build-integrated Python asset toolchain.
4. Terrain + camera MVP with prebuilt world map streaming.
5. Flight model MVP with interpolation and landing/crash loop.

Next steps (recommended order):
1. Milestone 1 hardening
  - lock in spawn/runway tuning,
  - add repeatable terrain/flight regression checks,
  - verify no scroll artifacts across full world wrap cycles.
2. Combat entity slice
  - projectile and bomb entities,
  - impact/explosion visuals,
  - collision integration with terrain and player state.
3. Core game loop parity
  - base/objective interactions,
  - score and state transitions,
  - mission flow scaffolding.
4. AI/autopilot port
  - incremental import of classic behavior,
  - cap entity counts for RP6502 budget.
5. Performance and polish
  - reduce XRAM writes with dirty-region batching,
  - HUD and audio pass,
  - behavior parity pass against selected original scenarios.

## Project Notes

- Keep XRAM allocations and layer ownership explicit.
- Prefer build-generated assets over runtime synthesis.
- Keep gameplay deterministic at 10 Hz; rendering may interpolate only.



# 1. Player (already exists — re-run to regenerate from source)
python tools/assets/generate_player_strip.py
python tools/convert_sprite.py --bpp 4 --mode tile --extract-palette Sprites/player_bank_strip.png

# 2. Terrain
python tools/generate_terrain_mode2_assets.py --ground-header src/original_ground_3000.h --screen-height 240 --tile-size 8 --y-offset 0
python tools/convert_sprite.py --bpp 4 --mode tile --extract-palette Sprites/terrain_tileset.png

# 3. Targets
python tools/assets/generate_targets.py
python tools/convert_sprite.py --bpp 4 --mode tile --extract-palette Sprites/targets_strip.png