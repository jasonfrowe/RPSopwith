#!/usr/bin/env python3
"""
Generate RP6502 Mode-5 projectiles/effects sprites from SDL Sopwith source.

Outputs a 16x16 horizontal PNG strip that includes bombs, missiles, bursts,
birds/flocks, bird splat, debris, plus explicit bullet/smoke pixel sprites.
"""

import argparse
from pathlib import Path
import re
import sys

from PIL import Image


SIZE = 16
SOPWITH_SYMBOL_PATH = Path("/Users/rowe/Software/games/sdl-sopwith/src/swsymbol.c")
COLOR_CHARS = " *-#"

PROJECTILE_COLORS = {
    0: (0, 0, 0),
    1: (240, 240, 240),
    2: (180, 180, 180),
    3: (80, 80, 80),
    4: (255, 255, 120),
    5: (140, 140, 140),
}


def extract_text_array(src: str, array_name: str) -> list[str]:
    start = src.find(f"static const char *{array_name}[] = {{")
    if start < 0:
        raise ValueError(f"{array_name} array not found")

    end = src.find("};", start)
    if end < 0:
        raise ValueError(f"{array_name} array end not found")

    block = src[start:end]
    return re.findall(r'"([^"\\]*(?:\\.[^"\\]*)*)"', block)


def parse_symbol_array(src: str, array_name: str, rows_per_symbol: int) -> list[list[str]]:
    literals = extract_text_array(src, array_name)
    decoded = [bytes(lit, "utf-8").decode("unicode_escape").rstrip("\n") for lit in literals]
    symbols: list[list[str]] = []

    if rows_per_symbol <= 0:
        return symbols

    for i in range(0, len(decoded), rows_per_symbol):
        chunk = decoded[i:i + rows_per_symbol]
        if len(chunk) == rows_per_symbol:
            symbols.append(chunk)

    return symbols


def ascii_to_pixels(lines: list[str]) -> list[list[int]]:
    # Symbols in swsymbol.c are variable-sized ASCII art (2 chars per pixel).
    # Normalize each source symbol to centered 16x16 output for uniform strips.
    out = [[0 for _ in range(SIZE)] for _ in range(SIZE)]
    src_h = len(lines)
    src_w = 0
    for row in lines:
        src_w = max(src_w, len(row) // 2)

    if src_w <= 0 or src_h <= 0:
        return out

    off_x = max(0, (SIZE - src_w) // 2)
    off_y = max(0, (SIZE - src_h) // 2)

    for y in range(min(src_h, SIZE)):
        row = lines[y]
        for x in range(min(src_w, SIZE)):
            src_x = x * 2
            if src_x >= len(row):
                continue
            ch = row[src_x]
            c = COLOR_CHARS.find(ch)
            if c < 0:
                c = 0

            dx = off_x + x
            dy = off_y + y
            if 0 <= dx < SIZE and 0 <= dy < SIZE:
                out[dy][dx] = c
    return out


def make_single_pixel_sprite(color_index: int) -> list[list[int]]:
    out = [[0 for _ in range(SIZE)] for _ in range(SIZE)]
    out[SIZE // 2][SIZE // 2] = color_index
    return out


def blit_pixels(img: Image.Image, ox: int, pixels: list[list[int]]) -> None:
    for y in range(SIZE):
        for x in range(SIZE):
            img.putpixel((ox + x, y), pixels[y][x])


def make_strip(frames: list[list[list[int]]]) -> Image.Image:
    img = Image.new("P", (SIZE * len(frames), SIZE), 0)
    palette = [0] * (256 * 3)
    for idx, (r, g, b) in PROJECTILE_COLORS.items():
        palette[idx * 3 + 0] = r
        palette[idx * 3 + 1] = g
        palette[idx * 3 + 2] = b
    img.putpalette(palette)

    for i, frame in enumerate(frames):
        blit_pixels(img, i * SIZE, frame)

    return img


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate projectiles/effects strip from SDL Sopwith symbols."
    )
    parser.add_argument(
        "--sopwith-symbol-path",
        type=Path,
        default=SOPWITH_SYMBOL_PATH,
        help="Path to SDL Sopwith swsymbol.c",
    )
    parser.add_argument(
        "--out-png",
        type=Path,
        default=Path("Sprites/projectiles_strip.png"),
        help="Output PNG strip path",
    )
    args = parser.parse_args()

    if not args.sopwith_symbol_path.exists():
        print(f"error: Sopwith symbols not found at {args.sopwith_symbol_path}", file=sys.stderr)
        return 1

    src = args.sopwith_symbol_path.read_text(encoding="utf-8")

    try:
        bomb = parse_symbol_array(src, "swbmbsym", 8)
        missile = parse_symbol_array(src, "swmscsym", 8)
        burst = parse_symbol_array(src, "swbstsym", 8)
        flock = parse_symbol_array(src, "swflksym", 16)
        bird = parse_symbol_array(src, "swbrdsym", 2)
        birdsplat = parse_symbol_array(src, "swsplsym", 32)
        debris = parse_symbol_array(src, "swexpsym", 8)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    frames: list[list[list[int]]] = []

    # Explicit tiny sprites for shot/smoke.
    frames.append(make_single_pixel_sprite(4))
    frames.append(make_single_pixel_sprite(5))

    for arr in (bomb, missile, burst, flock, bird, birdsplat, debris):
        for sym in arr:
            frames.append(ascii_to_pixels(sym))

    img = make_strip(frames)
    args.out_png.parent.mkdir(parents=True, exist_ok=True)
    img.save(args.out_png)

    print(f"Generated projectile strip: {len(frames)} frames")
    print(f"- shot/smoke: 2")
    print(f"- bombs: {len(bomb)} missiles: {len(missile)} bursts: {len(burst)}")
    print(f"- flocks: {len(flock)} birds: {len(bird)} birdsplat: {len(birdsplat)} debris: {len(debris)}")
    print(f"Strip size: {img.width}x{img.height} -> {args.out_png}")
    print(f"Next: python tools/convert_sprite.py --bpp 4 --mode tile --extract-palette {args.out_png}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
