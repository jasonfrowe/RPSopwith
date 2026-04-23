#!/usr/bin/env python3

import argparse
import pathlib
import re
import sys

from PIL import Image


GROUND_RE = re.compile(r"s_original_ground\[(\d+)\]\s*=\s*\{(.*?)\};", re.DOTALL)

# SDL Sopwith swinit.c inittarget()/Flatten() uses these same target X positions
# from swgames.c and flattens [x .. x + symbol_w - 1] with headroom=symbol_h.
GROUND_TARGET_X = [
    191, 284, 409, 539, 685, 807, 934,
    1210, 1240, 1376, 1440,
    1550, 1608, 1750, 1780, 2024,
    2159, 2279, 2390, 2549, 2678, 2763,
]

TARGET_SYMBOL_WIDTH = 16
TARGET_SYMBOL_HEIGHT = 16
SOURCE_MAX_Y = 199


def parse_ground_samples(header_path: pathlib.Path) -> list[int]:
    text = header_path.read_text(encoding="utf-8")
    match = GROUND_RE.search(text)
    if match is None:
        raise ValueError("could not find s_original_ground[] in header")

    declared_count = int(match.group(1))
    values = [int(tok) for tok in re.findall(r"\d+", match.group(2))]
    if len(values) != declared_count:
        raise ValueError(f"declared {declared_count} samples, parsed {len(values)}")
    return values


def sopwith_flatten(ground: list[int], min_x: int, max_x: int, headroom: int) -> int:
    """Mirror SDL Sopwith swinit.c: Flatten(minx, maxx, headroom)."""
    min_h = min(ground[min_x:max_x + 1])
    max_h = max(ground[min_x:max_x + 1])
    ave_h = (min_h + max_h) // 2
    ave_h = min(ave_h, SOURCE_MAX_Y - headroom - 1)

    for x in range(min_x, max_x + 1):
        ground[x] = ave_h

    return ave_h


def apply_sopwith_target_flatten(ground: list[int]) -> list[int]:
    flattened = list(ground)
    max_x = len(flattened) - 1

    for x in GROUND_TARGET_X:
        min_x = max(0, x)
        max_span_x = min(max_x, x + TARGET_SYMBOL_WIDTH - 1)
        sopwith_flatten(flattened, min_x, max_span_x, TARGET_SYMBOL_HEIGHT)

    return flattened


def tile_indices_to_4bpp_bytes(indices: list[int]) -> bytes:
    out = bytearray()
    for y in range(8):
        row = indices[y * 8:(y + 1) * 8]
        for x in range(0, 8, 2):
            out.append(((row[x] & 0x0F) << 4) | (row[x + 1] & 0x0F))
    return bytes(out)


def build_edge_tile(samples: list[int], tile_row: int) -> bytes:
    idx = [1] * 64  # sky index
    row_base = tile_row * 8
    for x in range(8):
        local_surface = samples[x] - row_base
        if local_surface <= 0:
            start_y = 0
        elif local_surface >= 8:
            continue
        else:
            start_y = local_surface
        for y in range(start_y, 8):
            idx[y * 8 + x] = 2  # ground index
    return tile_indices_to_4bpp_bytes(idx)


def all_byte(value: int, size: int) -> bytes:
    return bytes([value] * size)


def tile_bytes_to_indices(tile_bytes: bytes) -> list[int]:
    indices: list[int] = []
    for b in tile_bytes:
        indices.append((b >> 4) & 0x0F)
        indices.append(b & 0x0F)
    return indices


def write_tileset_png(path: pathlib.Path, tiles: list[bytes]) -> None:
    # Convert-script tile mode expects image height == tile size (8px).
    # Emit a horizontal strip: one 8x8 tile per frame (256 frames total).
    # Palette:
    #   index 0 = transparent / sky (black, treated as transparent by runtime)
    #   index 1 = sky fill (light blue — pure sky tiles)
    #   index 2 = ground fill (earthy brown-green)
    # Edge tiles use indices 1 (sky) and 2 (ground) per pixel.
    tile_count = 256
    atlas = Image.new("P", (tile_count * 8, 8), 0)
    palette = [0] * (256 * 3)
    palette[0:3]  = [0, 0, 0]        # index 0: transparent / unused
    palette[3:6]  = [100, 160, 220]  # index 1: sky blue
    palette[6:9]  = [100, 130, 60]   # index 2: ground green-brown
    atlas.putpalette(palette)

    for i in range(tile_count):
        tx = i * 8
        ty = 0
        tile = tiles[i] if i < len(tiles) else bytes([0x11] * 32)
        idx = tile_bytes_to_indices(tile)
        for y in range(8):
            for x in range(8):
                atlas.putpixel((tx + x, ty + y), idx[y * 8 + x])

    path.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(path)


def write_tilemap_png(path: pathlib.Path, tilemap: bytes, width_tiles: int, height_tiles: int) -> None:
    img = Image.new("P", (width_tiles, height_tiles), 0)
    # Identity-ish palette for index visualization.
    palette = [0] * (256 * 3)
    for i in range(256):
        palette[i * 3 + 0] = i
        palette[i * 3 + 1] = i
        palette[i * 3 + 2] = i
    img.putpalette(palette)

    for y in range(height_tiles):
        row_start = y * width_tiles
        for x in range(width_tiles):
            img.putpixel((x, y), tilemap[row_start + x])

    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path)


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate RP6502 Mode-2 terrain tileset + world tilemap assets.")
    parser.add_argument("--ground-header", required=True, type=pathlib.Path)
    parser.add_argument("--tileset-bin", type=pathlib.Path)
    parser.add_argument(
        "--tilemap-bin",
        type=pathlib.Path,
        default=pathlib.Path("images/terrain_world_tilemap.bin"),
        help="Tilemap binary output path (default: images/terrain_world_tilemap.bin)",
    )
    parser.add_argument(
        "--tileset-png",
        type=pathlib.Path,
        default=pathlib.Path("Sprites/terrain_tileset.png"),
        help="Optional tileset atlas PNG output path (default: Sprites/terrain_tileset.png)",
    )
    parser.add_argument(
        "--tilemap-png",
        type=pathlib.Path,
        default=pathlib.Path("Sprites/terrain_world_tilemap.png"),
        help="Optional tilemap PNG output path (default: Sprites/terrain_world_tilemap.png)",
    )
    parser.add_argument("--screen-height", required=True, type=int)
    parser.add_argument("--tile-size", required=True, type=int)
    parser.add_argument("--y-offset", type=int, default=None)
    args = parser.parse_args()

    if args.tileset_bin is None and args.tilemap_bin is None and args.tileset_png is None and args.tilemap_png is None:
        print("error: no outputs requested", file=sys.stderr)
        return 1

    if args.tile_size != 8:
        print("error: only tile-size 8 is supported", file=sys.stderr)
        return 1

    try:
        ground = parse_ground_samples(args.ground_header)
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    world_width_px = len(ground)
    if world_width_px % args.tile_size != 0:
        print("error: ground width is not divisible by tile-size", file=sys.stderr)
        return 1

    world_width_tiles = world_width_px // args.tile_size
    world_height_tiles = args.screen_height // args.tile_size
    # Repository pipeline uses y_offset=0 as the baseline placement.
    # Keep default aligned so a requested +8 means exactly 8 pixels from that baseline.
    y_offset = args.y_offset if args.y_offset is not None else 0

    ground = apply_sopwith_target_flatten(ground)

    # Original terrain samples are bottom-origin; convert to top-origin screen Y.
    # This matches the historical port mapping and DOS visual orientation.
    heights = [min(239, max(0, (SOURCE_MAX_Y - h) + y_offset)) for h in ground]

    sky_tile = all_byte(0x11, 32)
    ground_tile = all_byte(0x22, 32)

    tiles: list[bytes] = [sky_tile, ground_tile]
    tile_index_by_bytes: dict[bytes, int] = {
        sky_tile: 0,
        ground_tile: 1,
    }

    tilemap = bytearray(world_width_tiles * world_height_tiles)

    for tx in range(world_width_tiles):
        samples = heights[tx * 8:(tx * 8) + 8]
        min_row = min(samples) // 8
        max_row = max(samples) // 8

        for ty in range(world_height_tiles):
            if ty < min_row:
                tile_index = 0
            elif ty > max_row:
                tile_index = 1
            else:
                tile_bytes = build_edge_tile(samples, ty)
                if tile_bytes in tile_index_by_bytes:
                    tile_index = tile_index_by_bytes[tile_bytes]
                else:
                    if len(tiles) >= 256:
                        print(
                            f"error: unique terrain tiles exceeded 256 at column {tx}, row {ty} (need >255)",
                            file=sys.stderr,
                        )
                        return 1
                    tile_index = len(tiles)
                    tiles.append(tile_bytes)
                    tile_index_by_bytes[tile_bytes] = tile_index

            tilemap[(ty * world_width_tiles) + tx] = tile_index

    tileset = bytearray(256 * 32)
    for i, tile in enumerate(tiles):
        start = i * 32
        tileset[start:start + 32] = tile

    if args.tileset_bin is not None:
        args.tileset_bin.parent.mkdir(parents=True, exist_ok=True)
        args.tileset_bin.write_bytes(tileset)

    if args.tilemap_bin is not None:
        args.tilemap_bin.parent.mkdir(parents=True, exist_ok=True)
        args.tilemap_bin.write_bytes(tilemap)

    if args.tileset_png is not None:
        write_tileset_png(args.tileset_png, tiles)

    if args.tilemap_png is not None:
        write_tilemap_png(args.tilemap_png, bytes(tilemap), world_width_tiles, world_height_tiles)

    print(f"Generated terrain tiles: {len(tiles)} / 256")
    print(f"World tilemap size: {world_width_tiles}x{world_height_tiles} = {len(tilemap)} bytes")
    if args.tileset_png is not None:
        print(f"Tileset PNG: {args.tileset_png}")
    if args.tilemap_png is not None:
        print(f"Tilemap PNG: {args.tilemap_png}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
