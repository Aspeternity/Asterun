#!/usr/bin/env python3
"""Validate Asterun ICOs and their actual Windows executable resources.

No third-party dependencies. ICO checks run on Linux and Windows; passing
--executables on Windows also verifies compiled PE RT_GROUP_ICON / RT_ICON.
"""

from __future__ import annotations

import argparse
import ctypes
import os
from pathlib import Path
import struct
import sys
import zlib

ROOT = Path(__file__).resolve().parents[1]
MAIN_SIZES = {16, 32, 48, 128, 256}
UTILITY_SIZES = {16, 20, 24, 32, 40, 48, 64, 128, 256}
SOURCES = {
    "app": ("asterun.ico", MAIN_SIZES),
    "tray": ("asterun_tray.ico", MAIN_SIZES),
    "update": ("asterun_update.ico", UTILITY_SIZES),
    "uninstall": ("asterun_uninstall.ico", UTILITY_SIZES),
}
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def verify_png(data: bytes, expected_size: int, label: str) -> None:
    require(data.startswith(PNG_SIGNATURE), f"{label}: missing PNG signature")
    position = 8
    idat = bytearray()
    image_header = None
    seen_end = False
    while position < len(data):
        require(position + 12 <= len(data), f"{label}: truncated PNG chunk")
        size = struct.unpack_from(">I", data, position)[0]
        end = position + size + 12
        require(end <= len(data), f"{label}: PNG chunk exceeds file")
        kind = data[position + 4 : position + 8]
        payload = data[position + 8 : end - 4]
        stored_crc = struct.unpack_from(">I", data, end - 4)[0]
        actual_crc = zlib.crc32(kind + payload)
        require(stored_crc == actual_crc, f"{label}: invalid {kind!r} CRC")
        if kind == b"IHDR":
            require(image_header is None and size == 13, f"{label}: invalid IHDR")
            image_header = struct.unpack(">IIBBBBB", payload)
        elif kind == b"IDAT":
            idat.extend(payload)
        elif kind == b"IEND":
            require(size == 0, f"{label}: invalid IEND")
            require(end == len(data), f"{label}: bytes after IEND")
            seen_end = True
        else:
            require(kind[0] & 0x20, f"{label}: unsupported critical PNG chunk")
        position = end
        if seen_end:
            break
    require(seen_end and image_header is not None and idat, f"{label}: incomplete PNG")
    width, height, depth, color, compression, filtering, interlace = image_header
    require((width, height) == (expected_size, expected_size), f"{label}: wrong PNG dimensions")
    require((depth, color, compression, filtering, interlace) == (8, 6, 0, 0, 0),
            f"{label}: expected non-interlaced RGBA PNG")
    try:
        decoder = zlib.decompressobj()
        pixels = decoder.decompress(bytes(idat)) + decoder.flush()
    except zlib.error as exc:
        raise ValueError(f"{label}: invalid compressed PNG pixels: {exc}") from exc
    expected_bytes = expected_size * (expected_size * 4 + 1)
    require(decoder.eof and not decoder.unused_data and not decoder.unconsumed_tail,
            f"{label}: incomplete or trailing zlib data")
    require(len(pixels) == expected_bytes, f"{label}: unexpected decompressed pixel bytes")
    row_bytes = expected_size * 4 + 1
    require(all(pixels[i] <= 4 for i in range(0, len(pixels), row_bytes)),
            f"{label}: invalid PNG scanline filter")


def parse_ico(data: bytes, label: str, expected_sizes: set[int]) -> list[tuple[int, bytes]]:
    require(len(data) >= 6, f"{label}: truncated ICO header")
    reserved, kind, count = struct.unpack_from("<HHH", data)
    require(reserved == 0 and kind == 1, f"{label}: invalid ICO header")
    require(len(data) >= 6 + count * 16, f"{label}: incomplete directory")
    frames = []
    sizes = set()
    for index in range(count):
        width, height, colors, zero, planes, depth, length, offset = struct.unpack_from(
            "<BBBBHHII", data, 6 + 16 * index
        )
        width = width or 256
        height = height or 256
        frame_label = f"{label} {width}x{height}"
        require(width == height and width not in sizes, f"{frame_label}: duplicate/non-square frame")
        require(planes == 1 and depth == 32 and colors == 0 and zero == 0,
                f"{frame_label}: invalid pixel format")
        require(offset >= 6 + 16 * count and length > 0 and offset + length <= len(data),
                f"{frame_label}: frame extends beyond ICO file")
        png = data[offset:offset + length]
        verify_png(png, width, frame_label)
        frames.append((width, png))
        sizes.add(width)
    require(sizes == expected_sizes, f"{label}: sizes {sorted(sizes)}, expected {sorted(expected_sizes)}")
    return frames


def get_resource_reader(exe_path: Path):
    require(os.name == "nt", "EXE resource verification must run on Windows")
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    ptr = ctypes.c_void_p
    kernel.LoadLibraryExW.argtypes = [ctypes.c_wchar_p, ptr, ctypes.c_uint32]
    kernel.LoadLibraryExW.restype = ptr
    kernel.FindResourceW.argtypes = [ptr, ptr, ptr]
    kernel.FindResourceW.restype = ptr
    kernel.SizeofResource.argtypes = [ptr, ptr]
    kernel.SizeofResource.restype = ctypes.c_uint32
    kernel.LoadResource.argtypes = [ptr, ptr]
    kernel.LoadResource.restype = ptr
    kernel.LockResource.argtypes = [ptr]
    kernel.LockResource.restype = ptr
    kernel.FreeLibrary.argtypes = [ptr]
    kernel.FreeLibrary.restype = ctypes.c_int
    module = kernel.LoadLibraryExW(str(exe_path.resolve()), None, 0x22)
    require(bool(module), f"cannot load {exe_path} as resource-only image")

    def read(resource_type: int, resource_id: int) -> bytes:
        resource = kernel.FindResourceW(module, ptr(resource_id), ptr(resource_type))
        require(bool(resource), f"{exe_path.name}: resource {resource_type}/{resource_id} missing")
        length = kernel.SizeofResource(module, resource)
        loaded = kernel.LoadResource(module, resource)
        address = kernel.LockResource(loaded)
        require(length > 0 and bool(address), f"{exe_path.name}: invalid resource data")
        return ctypes.string_at(address, length)

    return read, lambda: kernel.FreeLibrary(module)


def verify_exe(exe_path: Path, groups: list[tuple[int, list[tuple[int, bytes]], str]],
               notification_ico: bytes | None = None) -> None:
    require(exe_path.is_file(), f"missing executable: {exe_path}")
    read, close = get_resource_reader(exe_path)
    try:
        for group_id, source_frames, label in groups:
            group = read(14, group_id)  # RT_GROUP_ICON
            require(len(group) >= 6, f"{label}: invalid resource group")
            reserved, kind, count = struct.unpack_from("<HHH", group)
            require((reserved, kind, count) == (0, 1, len(source_frames)),
                    f"{label}: group frame count/type mismatch")
            require(len(group) == 6 + count * 14, f"{label}: incorrect group length")
            embedded = {}
            for i in range(count):
                width, height, colors, zero, planes, depth, length, icon_id = struct.unpack_from(
                    "<BBBBHHIH", group, 6 + i * 14
                )
                pixels = read(3, icon_id)  # RT_ICON
                size = width or 256
                require(size == (height or 256) and length == len(pixels),
                        f"{label}: incorrect RT_ICON size")
                require(planes == 1 and depth == 32 and zero == 0 and colors == 0,
                        f"{label}: invalid RT_GROUP_ICON frame format")
                require(size not in embedded, f"{label}: duplicate RT_ICON size")
                embedded[size] = pixels
            expected = dict(source_frames)
            require(embedded == expected, f"{label}: EXE icon bytes differ from source ICO")
            print(f"PASS {exe_path.name} RT_GROUP_ICON {group_id}: {sorted(embedded)}")
        if notification_ico is not None:
            require(read(10, 103) == notification_ico,
                    f"{exe_path.name}: notification icon RCDATA is not the Asterun ICO")
            print(f"PASS {exe_path.name} notification RCDATA 103")
    finally:
        close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executables", type=Path, help="Directory with built Asterun.exe, Update.exe and Uninstall.exe")
    args = parser.parse_args()
    frames = {}
    paths = {}
    for name, (filename, sizes) in SOURCES.items():
        path = ROOT / "src" / "resources" / filename
        data = path.read_bytes()
        paths[name] = data
        frames[name] = parse_ico(data, filename, sizes)
        print(f"PASS {filename}: {sorted(sizes)} (PNG CRC + zlib + RGBA)")
    require(paths["app"] == paths["tray"], "app and tray must use identical approved Asterun icon")
    if args.executables:
        directory = args.executables
        verify_exe(directory / "Asterun.exe", [(101, frames["app"], "app"), (102, frames["tray"], "tray")], paths["app"])
        verify_exe(directory / "Update.exe", [(1, frames["update"], "update")])
        verify_exe(directory / "Uninstall.exe", [(1, frames["uninstall"], "uninstall")])
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, struct.error) as exc:
        print(f"Icon resource validation failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
