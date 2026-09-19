#!/usr/bin/env python3

from pathlib import Path
import os
import sys

ROOT = Path(__file__).resolve().parents[1]


def fail(message: str) -> None:
    print(f"release tag error: {message}", file=sys.stderr)
    raise SystemExit(1)


version = (ROOT / "VERSION").read_text(
    encoding="utf-8"
).strip()

tag = (
    sys.argv[1].strip()
    if len(sys.argv) > 1
    else os.environ.get("GITHUB_REF_NAME", "").strip()
)

if not tag:
    fail(
        "no tag was supplied and GITHUB_REF_NAME is empty"
    )

expected = f"v{version}"

if tag != expected:
    fail(
        f"tag {tag!r} does not match VERSION "
        f"{version!r}; expected {expected!r}"
    )

print(
    "release tag verified:",
    tag,
    "matches VERSION",
    version,
)
