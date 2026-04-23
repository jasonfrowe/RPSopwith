#!/usr/bin/env python3
"""
Generate RP6502 Mode-5 ground/target sprites from SDL Sopwith source.

Emits one 16x16 horizontal PNG strip containing:
1) target standing+destroyed pairs (first N TARGET_* types)
2) ox standing+dead
3) powerups
4) balloons

Feed output PNG to convert_sprite.py to produce 4bpp .bin + palette.
"""

import argparse
import pathlib
import re
import sys

from PIL import Image


SIZE = 16
SOPWITH_SYMBOL_PATH = pathlib.Path("/Users/rowe/Software/games/sdl-sopwith/src/swsymbol.c")
COLOR_CHARS = " *-#"

# Palette: index → RGB tuple.
# 0 = transparent (black), 1 = khaki body, 2 = light tan highlight, 3 = dark shadow.
TARGET_COLORS = {
    0: (0, 0, 0),
    1: (180, 160, 120),
    2: (220, 200, 160),
    3: (100, 80, 60),
}


def _extract_text_array(src: str, array_name: str) -> list[str]:
    start = src.find(f"static const char *{array_name}[] = {{")
    if start < 0:
        raise ValueError(f"{array_name} array not found")

    end = src.find("};", start)
    if end < 0:
        raise ValueError(f"{array_name} array end not found")

    block = src[start:end]
    return re.findall(r'"([^"\\]*(?:\\.[^"\\]*)*)"', block)


def _array_to_symbols(src: str, array_name: str) -> list[list[str]]:
    literals = _extract_text_array(src, array_name)
    symbols: list[list[str]] = []
    i = 0
    while i < len(literals):
        lines: list[str] = []
        for j in range(SIZE):
            if i + j >= len(literals):
                break
            line = bytes(literals[i + j], "utf-8").decode("unicode_escape")
            lines.append(line.rstrip("\n"))

        # A symbol line is 2 chars per pixel, so 16px symbols are usually 32 chars.
        if len(lines) == SIZE and all(len(line) >= (SIZE * 2 - 2) for line in lines):
            symbols.append(lines)
            i += SIZE
        else:
            i += 1

    return symbols


def parse_swsymbol_targets(path: pathlib.Path) -> dict:
    src = path.read_text(encoding="utf-8")

    standing_targets = _array_to_symbols(src, "swtrgsym")
    hit_targets = _array_to_symbols(src, "swhtrsym")
    ox_symbols = _array_to_symbols(src, "swoxsym")
    powerup_symbols = _array_to_symbols(src, "swpowerupsym")
    balloon_symbols = _array_to_symbols(src, "swballoonsym")

    return {
        "target_standing": standing_targets,
        "target_hit": hit_targets,
        "ox": ox_symbols,
        "powerups": powerup_symbols,
        "balloons": balloon_symbols,
    }


def ascii_to_pixels(lines: list[str]) -> list[list[int]]:
    """
    Convert ASCII art lines to pixel palette indices.
    ' '=0 (transparent), '*'=1 (body), '-'=2 (highlight), '#'=3 (shadow).
    Each line is 32 chars representing 16 pixels (2 chars per pixel).
    """
    out = [[0 for _ in range(SIZE)] for _ in range(SIZE)]
    for y in range(min(SIZE, len(lines))):
        row = lines[y]
        for x in range(SIZE):
            src_x = x * 2
            if src_x >= len(row):
                continue
            ch = row[src_x]
            c = COLOR_CHARS.find(ch)
            if c < 0:
                c = 0
            out[y][x] = c
    return out


def blit_pixels(img: Image.Image, ox: int, pixels: list[list[int]]) -> None:
    for y in range(SIZE):
        for x in range(SIZE):
            c = pixels[y][x]
            img.putpixel((ox + x, y), c)


def make_strip(frames: list[list[list[int]]]) -> Image.Image:
    """Build a horizontal PNG strip from a list of 16x16 pixel grids."""
    img = Image.new("P", (SIZE * len(frames), SIZE), 0)
    palette = [0] * (256 * 3)
    for idx, (r, g, b) in TARGET_COLORS.items():
        palette[idx * 3 + 0] = r
        palette[idx * 3 + 1] = g
        palette[idx * 3 + 2] = b
    img.putpalette(palette)
    for i, pixels in enumerate(frames):
        blit_pixels(img, i * SIZE, pixels)
    return img


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate RP6502 Mode-5 target sprites from SDL Sopwith."
    )
    parser.add_argument(
        "--sopwith-symbol-path",
        type=pathlib.Path,
        default=SOPWITH_SYMBOL_PATH,
        help="Path to SDL Sopwith swsymbol.c"
    )
    parser.add_argument(
        "--out-png",
        type=pathlib.Path,
        default=pathlib.Path("Sprites/targets_strip.png"),
        help="Output PNG horizontal strip (default: Sprites/targets_strip.png)",
    )
    parser.add_argument(
        "--num-targets",
        type=int,
        default=8,
        help="Number of target types to generate (default: 8 main types)"
    )
    args = parser.parse_args()

    if not args.sopwith_symbol_path.exists():
        print(f"error: Sopwith symbols not found at {args.sopwith_symbol_path}", file=sys.stderr)
        return 1

    try:
        targets_data = parse_swsymbol_targets(args.sopwith_symbol_path)
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    standing_sprites = targets_data.get("target_standing", [])
    hit_sprites = targets_data.get("target_hit", [])
    ox_sprites = targets_data.get("ox", [])
    powerup_sprites = targets_data.get("powerups", [])
    balloon_sprites = targets_data.get("balloons", [])

    if not standing_sprites:
        print("error: no standing target sprites found", file=sys.stderr)
        return 1

    # Build frame list in deterministic order.
    # Layout:
    # [target0_stand, target0_hit, ..., targetN_stand, targetN_hit,
    #  ox_stand, ox_dead,
    #  powerup0..powerupN,
    #  balloon0..balloonN]
    num_generated = min(args.num_targets, len(standing_sprites))
    frames: list[list[list[int]]] = []
    for i in range(num_generated):
        standing_pixels = ascii_to_pixels(standing_sprites[i]) if i < len(standing_sprites) else [[0]*SIZE for _ in range(SIZE)]
        hit_pixels = ascii_to_pixels(hit_sprites[i]) if i < len(hit_sprites) and hit_sprites[i] else [[0]*SIZE for _ in range(SIZE)]
        frames.append(standing_pixels)
        frames.append(hit_pixels)

    if len(ox_sprites) >= 2:
        frames.append(ascii_to_pixels(ox_sprites[0]))
        frames.append(ascii_to_pixels(ox_sprites[1]))

    for sym in powerup_sprites:
        frames.append(ascii_to_pixels(sym))

    for sym in balloon_sprites:
        frames.append(ascii_to_pixels(sym))

    img = make_strip(frames)
    args.out_png.parent.mkdir(parents=True, exist_ok=True)
    img.save(args.out_png)

    total_frames = len(frames)
    print(f"Generated frames: {total_frames}")
    print(f"- Targets (standing+destroyed): {num_generated * 2}")
    print(f"- Ox: {2 if len(ox_sprites) >= 2 else 0}")
    print(f"- Powerups: {len(powerup_sprites)}")
    print(f"- Balloons: {len(balloon_sprites)}")
    print(f"Strip size: {img.width}×{img.height} → {args.out_png}")
    print(f"Next: python tools/convert_sprite.py --bpp 4 --mode tile --extract-palette {args.out_png}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
