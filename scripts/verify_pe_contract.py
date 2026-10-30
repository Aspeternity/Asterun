#!/usr/bin/env python3
"""Verify the DLL import and load-flag contract of the packaged executables.

For Asterun.exe, Update.exe and Uninstall.exe in the given directory, check:

* the file exists and is a PE32+ image for the expected architecture;
* every static import and every delay-load import is a Windows system DLL on
  that executable's allow-list, and none is a C/C++ runtime DLL
  (VCRUNTIME*, MSVCP*, ucrtbase, api-ms-win-crt-*): the runtime must be linked
  statically;
* the load configuration directory exists and its DependentLoadFlags field is
  exactly 0x800 (LOAD_LIBRARY_SEARCH_SYSTEM32, set by /DEPENDENTLOADFLAG).

DependentLoadFlags governs how the loader resolves the static imports listed
here (Windows 10 1607 and later). It does not cover DLLs loaded at run time;
those are handled separately by SetDefaultDllDirectories in the processes
that need it. A missing file, an unreadable or malformed image, a missing
directory or field, or any parsing problem is reported as a failure: nothing
is assumed to pass.

Only the Python standard library is used, so the check runs unchanged on the
Windows runners and on Linux against downloaded release archives.
"""

from __future__ import annotations

import argparse
import os
import struct
import sys
from pathlib import Path

MACHINES = {"x64": 0x8664, "ARM64": 0xAA64}
LOAD_LIBRARY_SEARCH_SYSTEM32 = 0x800

# Allowed system DLL imports per executable: the import libraries each target
# links in CMakeLists.txt plus KERNEL32. Lower case.
ALLOWED_IMPORTS = {
    "Asterun.exe": {
        "kernel32.dll", "shell32.dll", "ole32.dll", "oleaut32.dll", "comctl32.dll",
        "winmm.dll", "comdlg32.dll", "advapi32.dll", "user32.dll", "gdi32.dll",
        "imm32.dll", "msimg32.dll", "dwmapi.dll", "uxtheme.dll", "version.dll",
        "msi.dll", "winhttp.dll", "bcrypt.dll", "wintrust.dll", "crypt32.dll",
        # MSIX package manifests are read with XmlLite (System32, like the
        # rest; DependentLoadFlags keeps it from resolving anywhere else).
        "xmllite.dll",
        # The static MSVC STL imports WaitOnAddress/WakeByAddress* (std::atomic
        # wait) through this API set; with the DLL runtime they came through
        # MSVCP140_ATOMIC_WAIT.dll. API sets are resolved by the loader's API
        # set schema to system modules, never through a directory search.
        "api-ms-win-core-synch-l1-2-0.dll",
    },
    "Update.exe": {
        "kernel32.dll", "advapi32.dll", "bcrypt.dll", "ole32.dll", "oleaut32.dll",
        "shell32.dll", "user32.dll",
    },
    "Uninstall.exe": {
        "kernel32.dll", "advapi32.dll", "bcrypt.dll", "shell32.dll", "shlwapi.dll",
        "ole32.dll", "oleaut32.dll", "rstrtmgr.dll", "comctl32.dll", "winmm.dll",
        "user32.dll",
    },
}

# DLLs that must appear only as a delay import, never a static import, with the
# reason. rstrtmgr.dll statically pulls ncrypt.dll -> ntasn1.dll during process
# initialization, before SetDefaultDllDirectories runs, so a copy planted in the
# executable's own directory is loaded (finding F-1). Delay-loading it moves
# that resolution to the first Rm* call, after the search path is restricted.
# Being on the allow-list above does not satisfy this: the move to a delay
# import must not let the DLL slip past the static-import contract.
REQUIRED_DELAY_ONLY = {
    "Uninstall.exe": {"rstrtmgr.dll"},
}


class PeError(Exception):
    pass


def is_runtime_dll(name: str) -> bool:
    lower = name.lower()
    return (lower.startswith("vcruntime") or lower.startswith("msvcp")
            or lower.startswith("api-ms-win-crt-") or lower == "ucrtbase.dll"
            or lower.startswith("concrt") or lower.startswith("vccorlib"))


class PeImage:
    def __init__(self, path: Path):
        self.path = path
        try:
            self.data = path.read_bytes()
        except OSError as error:
            raise PeError(f"cannot read: {error}") from error
        self._parse_headers()

    def _u16(self, offset: int) -> int:
        return self._unpack("<H", offset)

    def _u32(self, offset: int) -> int:
        return self._unpack("<I", offset)

    def _unpack(self, fmt: str, offset: int) -> int:
        size = struct.calcsize(fmt)
        if offset < 0 or offset + size > len(self.data):
            raise PeError(f"read past end of file at 0x{offset:x}")
        return struct.unpack_from(fmt, self.data, offset)[0]

    def _parse_headers(self) -> None:
        if self.data[:2] != b"MZ":
            raise PeError("missing MZ header")
        pe = self._u32(0x3C)
        if self.data[pe:pe + 4] != b"PE\0\0":
            raise PeError("missing PE signature")
        coff = pe + 4
        self.machine = self._u16(coff)
        sections = self._u16(coff + 2)
        optional_size = self._u16(coff + 16)
        optional = coff + 20
        if self._u16(optional) != 0x20B:
            raise PeError("not a PE32+ image")
        directory_count = self._u32(optional + 108)
        self.directories = []
        for index in range(min(directory_count, 16)):
            entry = optional + 112 + index * 8
            self.directories.append((self._u32(entry), self._u32(entry + 4)))
        self.sections = []
        table = optional + optional_size
        for index in range(sections):
            header = table + index * 40
            virtual_size = self._u32(header + 8)
            virtual_address = self._u32(header + 12)
            raw_size = self._u32(header + 16)
            raw_offset = self._u32(header + 20)
            self.sections.append((virtual_address, max(virtual_size, raw_size), raw_offset, raw_size))

    def directory(self, index: int) -> tuple[int, int]:
        if index >= len(self.directories):
            return (0, 0)
        return self.directories[index]

    def offset(self, rva: int) -> int:
        for virtual_address, size, raw_offset, raw_size in self.sections:
            if virtual_address <= rva < virtual_address + size:
                delta = rva - virtual_address
                if delta >= raw_size:
                    raise PeError(f"RVA 0x{rva:x} is not backed by file data")
                return raw_offset + delta
        raise PeError(f"RVA 0x{rva:x} is outside every section")

    def string(self, rva: int) -> str:
        start = self.offset(rva)
        end = self.data.find(b"\0", start)
        if end < 0:
            raise PeError(f"unterminated string at RVA 0x{rva:x}")
        return self.data[start:end].decode("ascii", errors="strict")

    def imports(self) -> list[str]:
        rva, size = self.directory(1)
        if rva == 0 or size == 0:
            raise PeError("no import directory")
        names = []
        cursor = self.offset(rva)
        while True:
            name_rva = self._u32(cursor + 12)
            first_thunk = self._u32(cursor + 16)
            if name_rva == 0 and first_thunk == 0:
                break
            names.append(self.string(name_rva))
            cursor += 20
        if not names:
            raise PeError("import directory lists no DLLs")
        return names

    def delay_imports(self) -> list[str]:
        rva, size = self.directory(13)
        if rva == 0 or size == 0:
            return []
        names = []
        cursor = self.offset(rva)
        while True:
            attributes = self._u32(cursor)
            name_rva = self._u32(cursor + 4)
            if name_rva == 0:
                break
            if attributes & 1 == 0:
                raise PeError("delay-load descriptor uses VA form, not supported")
            names.append(self.string(name_rva))
            cursor += 32
        return names

    def dependent_load_flags(self) -> int:
        rva, size = self.directory(10)
        if rva == 0 or size == 0:
            raise PeError("no load configuration directory")
        start = self.offset(rva)
        structure_size = self._u32(start)
        # IMAGE_LOAD_CONFIG_DIRECTORY64: DependentLoadFlags is the WORD at 0x4E.
        if structure_size < 0x50:
            raise PeError(f"load configuration too small for DependentLoadFlags ({structure_size} bytes)")
        return self._u16(start + 0x4E)


def check(path: Path, arch: str) -> tuple[list[str], dict]:
    errors: list[str] = []
    info: dict = {"file": path.name}
    if not path.is_file():
        return [f"{path}: file not found"], info
    try:
        image = PeImage(path)
        info["machine"] = f"0x{image.machine:04x}"
        if image.machine != MACHINES[arch]:
            errors.append(f"{path.name}: machine 0x{image.machine:04x}, expected 0x{MACHINES[arch]:04x} ({arch})")
        imports = image.imports()
        delay = image.delay_imports()
        flags = image.dependent_load_flags()
    except PeError as error:
        return errors + [f"{path.name}: {error}"], info
    info.update(imports=imports, delay=delay, flags=f"0x{flags:x}")
    allowed = ALLOWED_IMPORTS[path.name]
    for kind, names in (("import", imports), ("delay import", delay)):
        for name in names:
            if is_runtime_dll(name):
                errors.append(f"{path.name}: {kind} {name} is a C/C++ runtime DLL (runtime must be static)")
            elif name.lower() not in allowed:
                errors.append(f"{path.name}: {kind} {name} is not on the system DLL allow-list")
    delay_only = REQUIRED_DELAY_ONLY.get(path.name, set())
    static_lower = {name.lower() for name in imports}
    delay_lower = {name.lower() for name in delay}
    for name in sorted(delay_only):
        if name in static_lower:
            errors.append(
                f"{path.name}: {name} is a static import; it must be delay-loaded "
                f"so its dependency chain resolves after the DLL search path is restricted")
        if name not in delay_lower:
            errors.append(
                f"{path.name}: {name} must be present as a delay import (delay-loaded), but was not found")
    if flags != LOAD_LIBRARY_SEARCH_SYSTEM32:
        errors.append(f"{path.name}: DependentLoadFlags 0x{flags:x}, expected 0x{LOAD_LIBRARY_SEARCH_SYSTEM32:x}")
    return errors, info


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", required=True, type=Path)
    parser.add_argument("--arch", required=True, choices=sorted(MACHINES))
    args = parser.parse_args()

    all_errors: list[str] = []
    lines = []
    for name in ALLOWED_IMPORTS:
        errors, info = check(args.directory / name, args.arch)
        all_errors += errors
        lines.append(
            f"{name}\tmachine={info.get('machine', '?')}\tDependentLoadFlags={info.get('flags', '?')}\t"
            f"imports={','.join(info.get('imports', [])) or '?'}\tdelay={','.join(info.get('delay', [])) or '-'}")

    title = f"PE import/load-flag contract ({args.arch})"
    print(title)
    for line in lines:
        print("  " + line)
    status = "FAILED" if all_errors else "passed"
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as handle:
            handle.write(f"### {title}: {status}\n\n```\n" + "\n".join(lines) + "\n```\n\n")
    if os.environ.get("GITHUB_ACTIONS") == "true":
        body = "%0A".join(line.replace("%", "%25") for line in lines)
        safe_title = title.replace(",", "%2C").replace(":", "%3A")
        print(f"::notice title={safe_title}::{status}%0A{body}")
    for error in all_errors:
        print(f"error: {error}", file=sys.stderr)
    return 1 if all_errors else 0


if __name__ == "__main__":
    sys.exit(main())
