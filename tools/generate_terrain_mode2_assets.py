#!/usr/bin/env python3

import argparse
import pathlib
import re
import sys


GROUND_RE = re.compile(r"s_original_ground\[(\d+)\]\s*=\s*\{(.*?)\};", re.DOTALL)


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


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate RP6502 Mode-2 terrain tileset + world tilemap assets.")
    parser.add_argument("--ground-header", required=True, type=pathlib.Path)
    parser.add_argument("--tileset-bin", required=True, type=pathlib.Path)
    parser.add_argument("--tilemap-bin", required=True, type=pathlib.Path)
    parser.add_argument("--screen-height", required=True, type=int)
    parser.add_argument("--tile-size", required=True, type=int)
    parser.add_argument("--y-offset", type=int, default=None)
    args = parser.parse_args()

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
    y_offset = args.y_offset if args.y_offset is not None else (args.screen_height - 200)

    # Flip source Y to match original terrain orientation expected by this port,
    # then apply optional display offset.
    source_max_y = 199
    heights = [min(239, max(0, (source_max_y - h) + y_offset)) for h in ground]

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

    args.tileset_bin.parent.mkdir(parents=True, exist_ok=True)
    args.tilemap_bin.parent.mkdir(parents=True, exist_ok=True)
    args.tileset_bin.write_bytes(tileset)
    args.tilemap_bin.write_bytes(tilemap)

    print(f"Generated terrain tiles: {len(tiles)} / 256")
    print(f"World tilemap size: {world_width_tiles}x{world_height_tiles} = {len(tilemap)} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
