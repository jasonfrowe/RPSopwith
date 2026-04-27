# RPSopwith

RPSopwith is a Sopwith-style side-scrolling dogfight game for the Picocomputer RP6502.

![RPSopwith screenshot](Screenshots/Screenshot%202026-04-27%2000-12-49.png)

## The Pitch

Fly a biplane through hostile skies, strafe targets, drop bombs, and survive long enough to make it home.

## How To Play

- Keep your speed under control: too little and you stall, too much and you lose precision.
- Use flaps to climb or dive and line up gun runs.
- Fire guns for quick attacks and drop bombs for ground targets.
- If things go bad, turn for home and land.

## Controls

Keyboard defaults:
- Left/Right arrows or A/D: throttle down/up
- Up/Down arrows or W/S: flap up/down
- C: flip plane direction
- Home or H: return/request home landing
- Z or Space: fire gun
- X: drop bomb
- Enter: start or reset after crash

Gamepad defaults:
- Loaded from JOYSTICK_SH.DAT when available.
- Falls back to built-in defaults otherwise.

## Current Features

- Scrolling terrain world with sprite-based player, enemies, and projectiles.
- Deterministic 10 Hz gameplay simulation with smooth render interpolation.
- Keyboard and gamepad support.

## Run The Game

If you already have the project built:

```sh
python ./tools/rp6502.py run build/RPSopwith.rp6502
```

## Build From Source

Build requirements:

- LLVM-MOS SDK
- CMake
- Python 3

Optional but recommended:
- Local virtual environment at .venv

## Setup

```sh
python3 -m venv .venv
.venv/bin/python -m pip install --upgrade pip
.venv/bin/python -m pip install pillow
```

```sh
cmake -S . -B build
cmake --build build
```

Output package:
- build/RPSopwith.rp6502

## Project Layout

- src: game/runtime code
- images and Sprites: source art assets
- Music: music assets
- tools: build and conversion tooling
- Screenshots: gameplay captures