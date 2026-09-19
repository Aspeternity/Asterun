#!/usr/bin/env python3

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]


def fail(message: str) -> None:
    print(f"version metadata error: {message}", file=sys.stderr)
    raise SystemExit(1)


version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
match = re.fullmatch(
    r"(\d+)\.(\d+)\.(\d+)(?:-(alpha|beta|rc)\.(\d+)(?:\.(\d+))?)?",
    version,
)
if not match:
    fail(f"VERSION is not supported SemVer: {version!r}")

major, minor, patch = map(int, match.group(1, 2, 3))
channel = match.group(4)
channel_number = int(match.group(5) or 0)
channel_patch = int(match.group(6) or 0)

if channel_patch and channel != "alpha":
    fail("only alpha prereleases currently support a hotfix component")

if channel == "alpha":
    if channel_patch or channel_number >= 3:
        revision = channel_number * 10 + channel_patch
    else:
        revision = channel_number
elif channel == "beta":
    revision = 99 + channel_number
elif channel == "rc":
    revision = 199 + channel_number
else:
    revision = 300

expected_numeric_csv = f"{major},{minor},{patch},{revision}"
expected_numeric_dot = f"{major}.{minor}.{patch}.{revision}"
base_version = f"{major}.{minor}.{patch}"

cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
project_match = re.search(
    r"project\(ALTRunNext\s+VERSION\s+([0-9.]+)\s+LANGUAGES",
    cmake,
)
if not project_match:
    fail("CMake project version declaration was not found")
if project_match.group(1) != base_version:
    fail(
        f"CMake project version {project_match.group(1)!r} "
        f"does not match VERSION base {base_version!r}"
    )

resources = (ROOT / "src/resources.rc").read_text(encoding="utf-8")
for field in ("FILEVERSION", "PRODUCTVERSION"):
    found = re.search(rf"^\s*{field}\s+([0-9,]+)\s*$", resources, re.MULTILINE)
    if not found:
        fail(f"{field} was not found in src/resources.rc")
    if found.group(1) != expected_numeric_csv:
        fail(
            f"{field}={found.group(1)!r}, expected {expected_numeric_csv!r}"
        )

for field in ("FileVersion", "ProductVersion"):
    found = re.search(
        rf'VALUE\s+"{field}",\s+"([^"]+)\\0"',
        resources,
    )
    if not found:
        fail(f"{field} string was not found in src/resources.rc")
    if found.group(1) != version:
        fail(f"{field}={found.group(1)!r}, expected {version!r}")

manifest = (ROOT / "src/app.manifest").read_text(encoding="utf-8")
identity = re.search(
    r'<assemblyIdentity\s+version="([^"]+)"',
    manifest,
)
if not identity:
    fail("assemblyIdentity version was not found in src/app.manifest")
if identity.group(1) != expected_numeric_dot:
    fail(
        f"manifest version {identity.group(1)!r}, "
        f"expected {expected_numeric_dot!r}"
    )

readme = (ROOT / "README.md").read_text(encoding="utf-8")
if f"## v{version} " not in readme:
    fail(f"README.md has no current version heading for v{version}")

changelog = (ROOT / "CHANGELOG.md").read_text(encoding="utf-8")
if f"## {version}" not in changelog:
    fail(f"CHANGELOG.md has no current version heading for {version}")

template = (ROOT / "src/core/Version.hpp.in").read_text(encoding="utf-8")
if "@ALTRUN_VERSION@" not in template:
    fail("Version.hpp.in no longer derives the runtime version from VERSION")

print(
    "version metadata verified:",
    version,
    "->",
    expected_numeric_dot,
)
