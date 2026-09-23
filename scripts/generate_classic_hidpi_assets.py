#!/usr/bin/env python3
"""Generate deterministic alpha-aware Classic HiDPI glyph BMPs.

The 25x25 legacy Classic Logo/X files remain the immutable 100% resources.
Higher-DPI assets are derived only from those pixels. Black is treated as the
legacy transparent key, then the image is bilinearly resampled in premultiplied
BGRA space so GDI AlphaBlend can preserve smooth edges without runtime decode.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
RESOURCE_DIR = ROOT / "src" / "resources"
SOURCE_SIZE = 25
TARGETS = (("125", 31), ("150", 38), ("175", 44), ("200", 50))
KINDS = ("shortcut", "close")


def read_u16(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset : offset + 2], "little")


def read_u32(data: bytes, offset: int, *, signed: bool = False) -> int:
    return int.from_bytes(
        data[offset : offset + 4],
        "little",
        signed=signed,
    )


def parse_source_bmp(path: Path) -> list[list[tuple[int, int, int, int]]]:
    data = path.read_bytes()
    if data[:2] != b"BM":
        raise ValueError(f"{path} is not a BMP")

    pixel_offset = read_u32(data, 10)
    width = read_u32(data, 18, signed=True)
    height = read_u32(data, 22, signed=True)
    bpp = read_u16(data, 28)
    compression = read_u32(data, 30)

    if (
        width != SOURCE_SIZE
        or abs(height) != SOURCE_SIZE
        or bpp not in (24, 32)
        or compression != 0
    ):
        raise ValueError(
            f"{path} must remain an uncompressed 25x25 24/32-bit BMP"
        )

    bytes_per_pixel = bpp // 8
    row_stride = ((width * bpp + 31) // 32) * 4
    top_down = height < 0
    pixels: list[list[tuple[int, int, int, int]]] = []

    for y in range(SOURCE_SIZE):
        source_y = y if top_down else SOURCE_SIZE - 1 - y
        row: list[tuple[int, int, int, int]] = []
        for x in range(SOURCE_SIZE):
            offset = (
                pixel_offset
                + source_y * row_stride
                + x * bytes_per_pixel
            )
            blue, green, red = data[offset : offset + 3]
            alpha = 0 if (red == 0 and green == 0 and blue == 0) else 255
            row.append((blue, green, red, alpha))
        pixels.append(row)

    return pixels


def axis_weights(destination: int, destination_size: int) -> tuple[int, int, int, int, int]:
    denominator = 2 * destination_size
    position = (2 * destination + 1) * SOURCE_SIZE - destination_size
    first = position // denominator
    remainder = position - first * denominator

    if first < 0:
        first = 0
        remainder = 0
    if first >= SOURCE_SIZE - 1:
        first = SOURCE_SIZE - 1
        remainder = 0

    second = min(first + 1, SOURCE_SIZE - 1)
    return (
        first,
        second,
        denominator - remainder,
        remainder,
        denominator,
    )


def resize_premultiplied(
    source: list[list[tuple[int, int, int, int]]],
    destination_size: int,
) -> list[list[tuple[int, int, int, int]]]:
    destination: list[list[tuple[int, int, int, int]]] = []

    for y in range(destination_size):
        y0, y1, wy0, wy1, denominator = axis_weights(
            y,
            destination_size,
        )
        denominator_squared = denominator * denominator
        row: list[tuple[int, int, int, int]] = []

        for x in range(destination_size):
            x0, x1, wx0, wx1, _ = axis_weights(
                x,
                destination_size,
            )
            samples = (
                (source[y0][x0], wx0 * wy0),
                (source[y0][x1], wx1 * wy0),
                (source[y1][x0], wx0 * wy1),
                (source[y1][x1], wx1 * wy1),
            )

            channels = []
            for channel in range(4):
                weighted = sum(
                    pixel[channel] * weight
                    for pixel, weight in samples
                )
                channels.append(
                    max(
                        0,
                        min(
                            255,
                            (
                                weighted
                                + denominator_squared // 2
                            )
                            // denominator_squared,
                        ),
                    )
                )

            alpha = channels[3]
            channels[0] = min(channels[0], alpha)
            channels[1] = min(channels[1], alpha)
            channels[2] = min(channels[2], alpha)
            row.append(tuple(channels))

        destination.append(row)

    return destination


def write_bmp32(
    pixels: list[list[tuple[int, int, int, int]]],
) -> bytes:
    height = len(pixels)
    width = len(pixels[0])
    row_stride = width * 4
    image_size = row_stride * height
    result = bytearray(54 + image_size)

    result[0:2] = b"BM"
    result[2:6] = len(result).to_bytes(4, "little")
    result[10:14] = (54).to_bytes(4, "little")
    result[14:18] = (40).to_bytes(4, "little")
    result[18:22] = width.to_bytes(4, "little", signed=True)
    result[22:26] = height.to_bytes(4, "little", signed=True)
    result[26:28] = (1).to_bytes(2, "little")
    result[28:30] = (32).to_bytes(2, "little")
    result[34:38] = image_size.to_bytes(4, "little")

    for y, row in enumerate(pixels):
        destination_y = height - 1 - y
        for x, pixel in enumerate(row):
            offset = 54 + destination_y * row_stride + x * 4
            result[offset : offset + 4] = bytes(pixel)

    return bytes(result)


def generated_asset(kind: str, size: int) -> bytes:
    source = parse_source_bmp(
        RESOURCE_DIR / f"classic_{kind}.bmp"
    )
    return write_bmp32(
        resize_premultiplied(source, size)
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--verify",
        action="store_true",
        help="verify committed HiDPI assets instead of rewriting them",
    )
    args = parser.parse_args()

    mismatches: list[str] = []

    for kind in KINDS:
        for suffix, size in TARGETS:
            path = (
                RESOURCE_DIR
                / f"classic_{kind}_{suffix}.bmp"
            )
            expected = generated_asset(kind, size)

            if args.verify:
                if not path.exists() or path.read_bytes() != expected:
                    mismatches.append(str(path.relative_to(ROOT)))
            else:
                path.write_bytes(expected)
                print(
                    f"generated {path.relative_to(ROOT)} "
                    f"({size}x{size}, {len(expected)} bytes)"
                )

    if mismatches:
        for path in mismatches:
            print(
                f"Classic HiDPI asset mismatch: {path}",
                file=sys.stderr,
            )
        return 1

    if args.verify:
        print("Classic HiDPI assets verified")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
