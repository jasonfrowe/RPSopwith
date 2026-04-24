#!/usr/bin/env python3

import argparse
import pathlib
import re
import sys

from PIL import Image


GROUND_RE = re.compile(r"s_original_ground\[(\d+)\]\s*=\s*\{(.*?)\};", re.DOTALL)
TARGET_BLOCK_RE = re.compile(r"static const ground_target_t s_targets\[\]\s*=\s*\{(.*?)\};", re.DOTALL)
TARGET_ENTRY_RE = re.compile(r"\{\s*(\d+)\s*,")


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


def parse_target_x_positions(c_path: pathlib.Path) -> list[int]:
    text = c_path.read_text(encoding="utf-8")
    block = TARGET_BLOCK_RE.search(text)
    if block is None:
        return []

    return [int(m.group(1)) for m in TARGET_ENTRY_RE.finditer(block.group(1))]


def parse_rgba(value: str) -> tuple[int, int, int, int]:
    v = value.strip().lstrip("#")
    if len(v) == 6:
        v = v + "FF"
    if len(v) != 8:
        raise ValueError(f"invalid color '{value}', expected RRGGBB or RRGGBBAA")

    return (
        int(v[0:2], 16),
        int(v[2:4], 16),
        int(v[4:6], 16),
        int(v[6:8], 16),
    )


def downsample(values: list[int], out_width: int) -> list[int]:
    if out_width <= 0:
        raise ValueError("output width must be > 0")

    n = len(values)
    out: list[int] = []

    for x in range(out_width):
        start = (x * n) // out_width
        end = ((x + 1) * n) // out_width
        if end <= start:
            end = min(n, start + 1)

        bucket = values[start:end]
        out.append(sum(bucket) // len(bucket))

    return out


def map_y(sample_y: int, source_max_y: int, output_height: int) -> int:
    # Source values are top-origin pixels in range [0..source_max_y],
    # but minimap rendering expects inverted vertical orientation.
    return ((source_max_y - sample_y) * (output_height - 1)) // source_max_y


def draw_minimap(
    output_path: pathlib.Path,
    ground_values: list[int],
    output_width: int,
    output_height: int,
    background: tuple[int, int, int, int],
    line_color: tuple[int, int, int, int],
    fill_color: tuple[int, int, int, int],
    show_fill: bool,
    target_world_x: list[int],
    target_color: tuple[int, int, int, int],
    source_max_y: int,
) -> None:
    img = Image.new("RGBA", (output_width, output_height), background)
    px = img.load()

    reduced = downsample(ground_values, output_width)

    # Draw terrain profile line and optional ground fill.
    for x, src_y in enumerate(reduced):
        y = map_y(src_y, source_max_y, output_height)
        y = max(0, min(output_height - 1, y))
        px[x, y] = line_color

        if show_fill:
            for yy in range(y + 1, output_height):
                px[x, yy] = fill_color

    # Optional target dots projected onto minimap x-axis.
    world_width = len(ground_values)
    if target_world_x:
        for tx in target_world_x:
            x = (tx * output_width) // world_width
            x = max(0, min(output_width - 1, x))
            if output_height >= 2:
                px[x, 0] = target_color
                px[x, 1] = target_color
            else:
                px[x, 0] = target_color

    output_path.parent.mkdir(parents=True, exist_ok=True)
    img.save(output_path)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate a minimap PNG by scaling the 3000px Sopwith terrain profile.",
    )
    parser.add_argument(
        "--ground-header",
        type=pathlib.Path,
        default=pathlib.Path("src/original_ground_3000.h"),
        help="Input header containing s_original_ground[]",
    )
    parser.add_argument(
        "--targets-c",
        type=pathlib.Path,
        default=pathlib.Path("src/ground_targets.c"),
        help="Input C file for optional target marker extraction",
    )
    parser.add_argument(
        "--output",
        type=pathlib.Path,
        default=pathlib.Path("Sprites/minimap_terrain_160x32.png"),
        help="Output PNG path",
    )
    parser.add_argument(
        "--width",
        type=int,
        default=160,
        help="Output width in pixels",
    )
    parser.add_argument(
        "--height",
        type=int,
        default=32,
        help="Output height in pixels",
    )
    parser.add_argument(
        "--source-max-y",
        type=int,
        default=199,
        help="Maximum Y value in the source profile",
    )
    parser.add_argument(
        "--line-color",
        default="BFE27AFF",
        help="Terrain line color as RRGGBB or RRGGBBAA",
    )
    parser.add_argument(
        "--fill-color",
        default="5A7D35FF",
        help="Terrain fill color as RRGGBB or RRGGBBAA",
    )
    parser.add_argument(
        "--background",
        default="00000000",
        help="Background color as RRGGBB or RRGGBBAA",
    )
    parser.add_argument(
        "--target-color",
        default="FF6E40FF",
        help="Target marker color as RRGGBB or RRGGBBAA",
    )
    parser.add_argument(
        "--no-fill",
        action="store_true",
        help="Disable filling below terrain profile",
    )
    parser.add_argument(
        "--show-targets",
        action="store_true",
        help="Draw target markers from ground_targets.c at top of minimap",
    )
    args = parser.parse_args()

    if args.width <= 0 or args.height <= 0:
        print("error: --width and --height must be > 0", file=sys.stderr)
        return 1

    try:
        ground_values = parse_ground_samples(args.ground_header)
        background = parse_rgba(args.background)
        line_color = parse_rgba(args.line_color)
        fill_color = parse_rgba(args.fill_color)
        target_color = parse_rgba(args.target_color)
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    target_x: list[int] = []
    if args.show_targets:
        try:
            target_x = parse_target_x_positions(args.targets_c)
        except OSError as exc:
            print(f"error: {exc}", file=sys.stderr)
            return 1

    try:
        draw_minimap(
            output_path=args.output,
            ground_values=ground_values,
            output_width=args.width,
            output_height=args.height,
            background=background,
            line_color=line_color,
            fill_color=fill_color,
            show_fill=not args.no_fill,
            target_world_x=target_x,
            target_color=target_color,
            source_max_y=args.source_max_y,
        )
    except Exception as exc:
        print(f"error: failed generating minimap: {exc}", file=sys.stderr)
        return 1

    print(f"Wrote minimap PNG: {args.output}")
    print(f"Source samples: {len(ground_values)}")
    print(f"Output size: {args.width}x{args.height}")
    if args.show_targets:
        print(f"Target markers: {len(target_x)}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
