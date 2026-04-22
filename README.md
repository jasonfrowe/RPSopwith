# RPSopwith

RPSopwith is an in-progress port of SDL Sopwith to the Picocomputer RP6502.

Current rendering direction:
- Mode-2 tile terrain layer
- Mode-5 player sprite layer (4bpp)
- XRAM layout centralized in src/constants.h

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

## Asset Conversion Pipeline

Player sprite frames are generated and converted during build.

Tools:
- tools/assets/generate_player_strip.py
- tools/assets/convert_sprite.py

Intermediate/generated files:
- assets/sprites/player_bank_strip.png
- build/assets/player_bank_strip_4bpp.bin
- build/assets/player_bank_strip_4bpp_palette.bin

Packaging:
- Sprite frame data is packed into ROM and loaded into XRAM at boot.
- Palette index 0 is forced to 0x0000 (transparent).

## Manual Asset Regeneration

```sh
.venv/bin/python tools/assets/generate_player_strip.py
.venv/bin/python tools/assets/convert_sprite.py \
  assets/sprites/player_bank_strip.png \
  --mode tile --bpp 4 --out-dir build/assets --extract-palette
```

## Project Notes

- Keep XRAM allocations in src/constants.h.
- Use Mode-2 and Mode-5 indexed formats (1/2/4 bpp) where possible.
- Prefer build-integrated conversion over ad-hoc runtime generation.
