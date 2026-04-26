#!/usr/bin/env python3

import argparse
import pathlib
import re
import sys


ARRAY_RE = re.compile(
    r"s_original_ground\[[^\]]*\]\s*=\s*\{(.*?)\};",
    re.DOTALL,
)


def parse_values(header_text: str) -> list[int]:
    match = ARRAY_RE.search(header_text)
    if match is None:
        raise ValueError("could not find s_original_ground[] array in header")

    values = [int(token) for token in re.findall(r"\d+", match.group(1))]
    if not values:
        raise ValueError("no numeric terrain samples found in header")
    return values


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Export original_ground_3000.h values to a raw .bin blob"
    )
    parser.add_argument("input", type=pathlib.Path, help="Input header path")
    parser.add_argument("output", type=pathlib.Path, help="Output .bin path")
    parser.add_argument(
        "--expect-count",
        type=int,
        default=3000,
        help="Expected terrain sample count (default: 3000)",
    )
    args = parser.parse_args()

    try:
        text = args.input.read_text(encoding="utf-8")
        values = parse_values(text)
    except OSError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    if args.expect_count is not None and len(values) != args.expect_count:
        print(
            f"error: expected {args.expect_count} samples, got {len(values)}",
            file=sys.stderr,
        )
        return 1

    for v in values:
        if v < 0 or v > 255:
            print(f"error: sample out of byte range: {v}", file=sys.stderr)
            return 1

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(bytes(values))
    print(f"Wrote {len(values)} bytes to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())