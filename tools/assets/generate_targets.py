#!/usr/bin/env python3
"""
Generate RP6502 Mode-5 target sprites from SDL Sopwith source.

Extracts target sprites from swsymbol.c and emits a horizontal PNG strip
(one 16x16 frame per target state, standing then destroyed, left to right).
Feed the output PNG to convert_sprite.py to produce the 4bpp .bin + palette.
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


def parse_swsymbol_targets(path: pathlib.Path) -> dict:
    """
    Extract target sprites from swsymbol.c.
    Returns dict with keys 'standing' and 'destroyed', each containing array of target sprites.
    """
    src = path.read_text(encoding="utf-8")
    
    # Parse swtrgsym array (standing targets)
    start = src.find("static const char *swtrgsym[] = {")
    if start < 0:
        raise ValueError("swtrgsym array not found")
    
    end = src.find("};", start)
    if end < 0:
        raise ValueError("swtrgsym array end not found")
    
    swtrgsym_block = src[start:end]
    
    # Find all string literals in swtrgsym
    literals = re.findall(r'"([^"\\]*(?:\\.[^"\\]*)*)"', swtrgsym_block)
    
    # Group into 16-line sprites (skip non-string entries like custom_target_sym)
    standing = []
    i = 0
    while i < len(literals):
        # Try to collect 16 lines for a sprite
        sprite_lines = []
        for j in range(16):
            if i + j < len(literals):
                line = bytes(literals[i + j], "utf-8").decode("unicode_escape")
                line = line.rstrip("\n")
                sprite_lines.append(line)
            else:
                break
        
        # If we got 16 lines and they look like a sprite, keep them
        if len(sprite_lines) == 16 and all(len(line) > 20 for line in sprite_lines):
            standing.append(sprite_lines)
            i += 16
        else:
            # Skip single-line entries (like custom_target_sym references)
            i += 1
    
    # Parse destroyed_* strings for hit variants
    destroyed = {}
    
    destroyed_names = ["building", "truck", "flag", "tent"]
    
    for name in destroyed_names:
        pattern = rf"static const char destroyed_{name}\[\]\s*=(.*?);"
        match = re.search(pattern, src, re.DOTALL)
        if match:
            block = match.group(1)
            # Extract all string literals from this block
            literals = re.findall(r'"([^"\\]*(?:\\.[^"\\]*)*)"', block)
            lines = []
            for lit in literals:
                line = bytes(lit, "utf-8").decode("unicode_escape")
                line = line.rstrip("\n")
                if line:  # Skip empty lines from comments
                    lines.append(line)
            if lines:
                destroyed[name] = lines
    
    # Map destroyed variants to targets
    hit_variants = [
        destroyed.get("building", []),       # TARGET_HANGAR
        destroyed.get("building", []),       # TARGET_FACTORY
        destroyed.get("building", []),       # TARGET_OIL_TANK
        destroyed.get("building", []),       # TARGET_TANK
        destroyed.get("truck", []),          # TARGET_TRUCK
        destroyed.get("truck", []),          # TARGET_TANKER_TRUCK
        destroyed.get("flag", []),           # TARGET_FLAG
        destroyed.get("tent", []),           # TARGET_TENT
    ]
    
    return {
        "standing": standing[:8],  # Only first 8 main types
        "hit": hit_variants,
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

    standing_sprites = targets_data.get("standing", [])
    hit_sprites = targets_data.get("hit", [])

    if not standing_sprites:
        print("error: no standing target sprites found", file=sys.stderr)
        return 1

    # Build frame list: standing then destroyed, interleaved per target.
    # Layout: [target0_stand, target0_hit, target1_stand, target1_hit, ...]
    num_generated = min(args.num_targets, len(standing_sprites))
    frames: list[list[list[int]]] = []
    for i in range(num_generated):
        standing_pixels = ascii_to_pixels(standing_sprites[i]) if i < len(standing_sprites) else [[0]*SIZE for _ in range(SIZE)]
        hit_pixels = ascii_to_pixels(hit_sprites[i]) if i < len(hit_sprites) and hit_sprites[i] else [[0]*SIZE for _ in range(SIZE)]
        frames.append(standing_pixels)
        frames.append(hit_pixels)

    img = make_strip(frames)
    args.out_png.parent.mkdir(parents=True, exist_ok=True)
    img.save(args.out_png)

    total_frames = len(frames)
    print(f"Generated {num_generated} target types × 2 states = {total_frames} frames")
    print(f"Strip size: {img.width}×{img.height} → {args.out_png}")
    print(f"Next: python tools/convert_sprite.py --bpp 4 --mode tile --extract-palette {args.out_png}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
