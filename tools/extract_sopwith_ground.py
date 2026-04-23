#!/usr/bin/env python3

import argparse
import pathlib
import re
import sys


ARRAY_RE = re.compile(
    r"original_ground\[\]\s*=\s*\{\s*/\* Original ground height by pixel \*/(.*?)\};",
    re.DOTALL,
)


def parse_ground_values(source_text: str) -> list[int]:
    match = ARRAY_RE.search(source_text)
    if match is None:
        raise ValueError("could not find original_ground[] in source")

    values = [int(token) for token in re.findall(r"\d+", match.group(1))]
    if not values:
        raise ValueError("original_ground[] was found, but no numeric samples were parsed")
    return values


def format_header(values: list[int]) -> str:
    lines = ["static const uint8_t s_original_ground[%d] = {" % len(values)]
    for start in range(0, len(values), 16):
        chunk = values[start:start + 16]
        suffix = "," if start + 16 < len(values) else ""
        lines.append("    " + ", ".join(str(value) for value in chunk) + suffix)
    lines.append("};")
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description="Extract Sopwith original_ground[] into a C header.")
    parser.add_argument("source", type=pathlib.Path, help="Path to original swgames.c")
    parser.add_argument(
        "-o",
        "--output",
        type=pathlib.Path,
        help="Header path to write. Defaults to stdout.",
    )
    parser.add_argument(
        "--expect-count",
        type=int,
        default=3000,
        help="Expected number of terrain samples. Defaults to 3000.",
    )
    args = parser.parse_args()

    try:
        values = parse_ground_values(args.source.read_text(encoding="utf-8"))
    except OSError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    if args.expect_count is not None and len(values) != args.expect_count:
        print(
            f"error: expected {args.expect_count} samples, parsed {len(values)}",
            file=sys.stderr,
        )
        return 1

    output_text = format_header(values)

    if args.output is None:
        sys.stdout.write(output_text)
    else:
        args.output.write_text(output_text, encoding="utf-8")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())