#!/usr/bin/env python3

from pathlib import Path
import re

from PIL import Image, ImageDraw

BASE_FRAME_COUNT = 32
HIT_FRAME_COUNT = 8
WIN_FRAME_COUNT = 8
FRAME_COUNT = BASE_FRAME_COUNT + HIT_FRAME_COUNT + WIN_FRAME_COUNT
SIZE = 16

SOPWITH_SYMBOL_PATH = Path("/Users/rowe/Software/games/sdl-sopwith/src/swsymbol.c")

COLOR_CHARS = " *-#"


def parse_symbol_array(path: Path, array_name: str) -> list[list[str]]:
    src = path.read_text(encoding="utf-8")
    start = src.find(f"static const char *{array_name}[] = {{")
    if start < 0:
        raise RuntimeError(f"{array_name} block not found")

    end = src.find("};", start)
    if end < 0:
        raise RuntimeError(f"{array_name} block end not found")

    block = src[start:end]
    literals = re.findall(r'"([^"\\]*(?:\\.[^"\\]*)*)"', block)

    if len(literals) % SIZE != 0:
        raise RuntimeError(f"Unexpected {array_name} literal count")

    symbols: list[list[str]] = []
    for i in range(0, len(literals), SIZE):
        lines = []
        for lit in literals[i : i + SIZE]:
            line = bytes(lit, "utf-8").decode("unicode_escape")
            line = line.rstrip("\n")
            lines.append(line)
        symbols.append(lines)

    return symbols


def symbol_to_pixels(lines: list[str]) -> list[list[int]]:
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

            # Map Sopwith levels into sprite palette slots.
            if c == 1:
                c = 3
            elif c == 2:
                c = 4
            elif c == 3:
                c = 5

            out[y][x] = c
    return out


def rotate_point(x: int, y: int, w: int, h: int, rotations: int, mirror: bool) -> tuple[int, int]:
    for _ in range(rotations):
        x, y = y, w - 1 - x
        w, h = h, w

    if mirror:
        y = h - 1 - y

    return x, y


def blit_pixels(img: Image.Image, ox: int, pixels: list[list[int]]) -> None:
    for y in range(SIZE):
        for x in range(SIZE):
            c = pixels[y][x]
            if c:
                img.putpixel((ox + x, y), c)


def find_nose(pixels: list[list[int]], facing_left: bool) -> tuple[int, int]:
    coords = []
    for y in range(SIZE):
        for x in range(SIZE):
            if pixels[y][x] != 0:
                coords.append((x, y))

    if not coords:
        return 8, 8

    if facing_left:
        nose_x = min(x for x, _ in coords)
    else:
        nose_x = max(x for x, _ in coords)

    nose_y_samples = [y for x, y in coords if x == nose_x]
    if nose_y_samples:
        nose_y = int(sum(nose_y_samples) / len(nose_y_samples))
    else:
        nose_y = 7

    return nose_x, nose_y


def transform_symbol(symbol: list[list[int]], rotations: int, mirror: bool) -> list[list[int]]:
    out = [[0 for _ in range(SIZE)] for _ in range(SIZE)]
    for y in range(SIZE):
        for x in range(SIZE):
            c = symbol[y][x]
            if c == 0:
                continue
            dx, dy = rotate_point(x, y, SIZE, SIZE, rotations, mirror)
            if 0 <= dx < SIZE and 0 <= dy < SIZE:
                out[dy][dx] = c
    return out


def draw_base_frame(img: Image.Image, frame_index: int, base_symbols: list[list[list[int]]]) -> None:
    ox = frame_index * SIZE
    draw = ImageDraw.Draw(img)

    # 0..15: normal orientation, 16..31: mirrored orientation.
    mirrored = frame_index >= 16
    angle = frame_index & 0x0F
    symbol_index = angle & 0x03
    rotations = angle >> 2
    pixels = transform_symbol(base_symbols[symbol_index], rotations, mirrored)
    blit_pixels(img, ox, pixels)

    prop_color = 6
    facing_left = 4 <= angle <= 12
    nose_x, nose_y = find_nose(pixels, facing_left)

    if facing_left:
        prop_x = nose_x - 1
    else:
        prop_x = nose_x + 1

    prop_x = max(0, min(SIZE - 1, prop_x))

    y0 = max(0, nose_y - 1)
    y1 = min(SIZE - 1, nose_y + 1)
    draw.line([(ox + prop_x, y0), (ox + prop_x, y1)], fill=prop_color)


def draw_mirrored_pair_frames(
    img: Image.Image,
    frame_base: int,
    symbols: list[list[list[int]]],
) -> None:
    for i in range(4):
        if i >= len(symbols):
            break
        a = transform_symbol(symbols[i], 0, False)
        b = transform_symbol(symbols[i], 0, True)
        blit_pixels(img, (frame_base + i) * SIZE, a)
        blit_pixels(img, (frame_base + 4 + i) * SIZE, b)


def main() -> None:
    if not SOPWITH_SYMBOL_PATH.exists():
        raise RuntimeError(f"Sopwith symbols not found at {SOPWITH_SYMBOL_PATH}")

    symbols_text = parse_symbol_array(SOPWITH_SYMBOL_PATH, "swplnsym")
    if len(symbols_text) < 4:
        raise RuntimeError("Expected at least 4 plane symbol definitions")
    hit_text = parse_symbol_array(SOPWITH_SYMBOL_PATH, "swhitsym")
    win_text = parse_symbol_array(SOPWITH_SYMBOL_PATH, "swwinsym")

    base_symbols = [symbol_to_pixels(s) for s in symbols_text[:4]]
    hit_symbols = [symbol_to_pixels(s) for s in hit_text[:4]]
    win_symbols = [symbol_to_pixels(s) for s in win_text[:4]]

    img = Image.new("P", (SIZE * FRAME_COUNT, SIZE), 0)

    palette = [0] * (256 * 3)
    colors = {
        0: (0, 0, 0),
        1: (112, 184, 248),
        2: (80, 56, 24),
        3: (252, 252, 252),
        4: (128, 0, 0),
        5: (0, 160, 0),
        6: (255, 255, 80),
    }
    for idx, (r, g, b) in colors.items():
        palette[idx * 3 + 0] = r
        palette[idx * 3 + 1] = g
        palette[idx * 3 + 2] = b
    img.putpalette(palette)

    for i in range(BASE_FRAME_COUNT):
        draw_base_frame(img, i, base_symbols)

    draw_mirrored_pair_frames(img, BASE_FRAME_COUNT, hit_symbols)
    draw_mirrored_pair_frames(img, BASE_FRAME_COUNT + HIT_FRAME_COUNT, win_symbols)

    img.save("Sprites/player_bank_strip.png")
    print(f"Generated player strip: {FRAME_COUNT} frames ({SIZE * FRAME_COUNT}x{SIZE})")


if __name__ == "__main__":
    main()