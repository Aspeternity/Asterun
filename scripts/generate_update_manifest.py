#!/usr/bin/env python3

import argparse
import json
import re
from pathlib import Path


def parse_checksums(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line:
            continue
        parts = line.split()
        if len(parts) != 2:
            raise SystemExit(f"Invalid checksum line: {raw!r}")
        digest, name = parts
        name = name.lstrip("*")
        if not re.fullmatch(r"[0-9a-fA-F]{64}", digest):
            raise SystemExit(f"Invalid SHA-256 for {name}")
        result[name] = digest.lower()
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--checksums", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    if not re.fullmatch(
        r"\d+\.\d+\.\d+(?:-(?:alpha|beta|rc)\.\d+(?:\.\d+)?)?",
        args.version,
    ):
        raise SystemExit(f"Unsupported version: {args.version}")

    if not re.fullmatch(r"[0-9a-fA-F]{40}", args.commit):
        raise SystemExit("Commit must be a 40-character Git SHA.")

    checksums = parse_checksums(args.checksums)

    required = {
        "x64": "ALTRunNext-x64.zip",
        "ARM64": "ALTRunNext-ARM64.zip",
    }

    for name in required.values():
        if name not in checksums:
            raise SystemExit(f"Missing checksum for {name}")

    manifest = {
        "schemaVersion": 1,
        "version": args.version,
        "commit": args.commit.lower(),
        "prerelease": "-" in args.version,
        "assets": {
            arch: {
                "name": name,
                "sha256": checksums[name],
            }
            for arch, name in required.items()
        },
    }

    args.output.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )

    loaded = json.loads(args.output.read_text(encoding="utf-8"))
    if loaded != manifest:
        raise SystemExit("Generated update manifest failed round-trip validation.")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
