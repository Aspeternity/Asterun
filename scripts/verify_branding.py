#!/usr/bin/env python3
"""Reject stale ALTRun Next product identity in active Asterun sources."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

HISTORICAL_FILES = {
    Path("scripts/verify_branding.py"),
    Path("CHANGELOG.md"),
    Path("docs/DESKTOP_VALIDATION.md"),
    Path("docs/CODE_REVIEW_FIXES_BETA3.md"),
    Path("docs/EVERYTHING_BETA_VALIDATION.md"),
    Path("docs/CLASSIC_UI_SPEC.md"),
}

TEXT_SUFFIXES = {
    ".cpp", ".hpp", ".h", ".in", ".rc", ".manifest",
    ".md", ".yml", ".yaml", ".py", ".ps1", ".sh",
    ".json", ".ini", ".tsv", ".txt",
}

FORBIDDEN = (
    "ALTRun Next",
    "ALTRunNext",
    "Aspeternity/ALTRunNext",
    "Aspeternity.ALTRunNext",
    ".altrun-",
    "altrun-development-release",
    "ALTRUN_VERSION",
    "IDI_ALTRUN",
    "IDW_ALTRUN",
)


def is_historical(path: Path) -> bool:
    rel = path.relative_to(ROOT)
    if rel in HISTORICAL_FILES:
        return True
    if rel.parent == Path("docs") and re.fullmatch(r"V.*_VALIDATION\.md", rel.name):
        return True
    return False


def main() -> int:
    failures: list[str] = []

    for path in ROOT.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in TEXT_SUFFIXES:
            continue
        if any(part in {".git", "build", "build-smoke", "build-compat"} for part in path.parts):
            continue
        if is_historical(path):
            continue

        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue

        for token in FORBIDDEN:
            if token not in text:
                continue
            for number, line in enumerate(text.splitlines(), start=1):
                if token in line:
                    failures.append(
                        f"{path.relative_to(ROOT)}:{number}: stale brand token {token!r}: {line.strip()}"
                    )

    if failures:
        print("Asterun branding verification failed:")
        for failure in failures:
            print(f"  {failure}")
        return 1

    print("Asterun branding verification passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
