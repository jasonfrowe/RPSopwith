#!/usr/bin/env python3
"""
Generate RP6502 Mode-5 target sprites from SDL Sopwith source.

Extracts target sprites from swsymbol.c and converts to 16x16 4bpp binary format.
Each target has a standing variant and a destroyed/hit variant.
"""

import argparse
import pathlib
import re
import sys


SIZE = 16
SOPWITH_SYMBOL_PATH = pathlib.Path("/Users/rowe/Software/games/sdl-sopwith/src/swsymbol.c")
COLOR_CHARS = " *-#"


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
    ' '=0 (transparent), '*'=1 (body), '-'=2 (edge/light), '#'=3 (dark/shadow).
    Each line is 32 chars representing 16 pixels (2 chars per pixel for clarity).
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


def pixels_to_4bpp_bytes(pixels: list[list[int]]) -> bytes:
    """
    Convert 16x16 pixel grid (4bpp) to RP6502 "tall" format.
    Each row: SIZE bytes, each byte holds 2 pixels (4bpp each, left=high, right=low).
    """
    out = bytearray()
    for y in range(SIZE):
        for x in range(0, SIZE, 2):
            high_nibble = pixels[y][x] & 0x0F
            low_nibble = pixels[y][x + 1] & 0x0F
            out.append((high_nibble << 4) | low_nibble)
    return bytes(out)


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
        "--targets-bin",
        type=pathlib.Path,
        required=True,
        help="Output binary for all target sprites (standing + hit variants)"
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

    # Generate output: 2 variants (standing + hit) × num_targets × 128 bytes per sprite
    all_sprites = bytearray()
    num_generated = min(args.num_targets, len(standing_sprites))
    
    for i in range(num_generated):
        # Standing variant
        if i < len(standing_sprites):
            pixels = ascii_to_pixels(standing_sprites[i])
        else:
            pixels = [[0 for _ in range(SIZE)] for _ in range(SIZE)]
        all_sprites.extend(pixels_to_4bpp_bytes(pixels))
        
        # Hit/destroyed variant
        if i < len(hit_sprites) and hit_sprites[i]:
            pixels = ascii_to_pixels(hit_sprites[i])
        else:
            pixels = [[0 for _ in range(SIZE)] for _ in range(SIZE)]
        all_sprites.extend(pixels_to_4bpp_bytes(pixels))
    
    args.targets_bin.parent.mkdir(parents=True, exist_ok=True)
    args.targets_bin.write_bytes(all_sprites)
    
    bytes_per_sprite = SIZE * SIZE // 2
    total_sprites = num_generated * 2  # standing + hit per target
    print(f"Generated {num_generated} target types (standing + destroyed)")
    print(f"Total sprites: {total_sprites} × {bytes_per_sprite} bytes = {len(all_sprites)} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
