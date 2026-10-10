#!/usr/bin/env python3
"""Cross-check the execution lists written by verify_ctest_run.py.

Every test that the reference list (the Linux core run) records as a passed
"portable" test must appear with the same label in each target list, with the
result that list is expected to have ("passed" for executed Windows runs,
"built, NOT RUN" for build-only runs). This catches portable tests that are
compiled or registered on Linux but silently dropped from a Windows job.
"""

from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path


def read_list(path: Path) -> dict[str, tuple[str, str]]:
    with path.open(encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        rows = {row["test"]: (row["label"], row["result"]) for row in reader}
    if not rows:
        raise SystemExit(f"error: {path} lists no tests")
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", required=True, type=Path)
    parser.add_argument("--target", action="append", nargs=2, default=[],
                        metavar=("LIST", "EXPECTED_RESULT"), required=True)
    args = parser.parse_args()

    reference = read_list(args.reference)
    portable = sorted(name for name, (label, result) in reference.items()
                      if label == "portable" and result == "passed")
    if len(portable) != len(reference):
        print(f"error: reference {args.reference} must contain only passed "
              f"portable tests", file=sys.stderr)
        return 1

    errors = []
    for list_path, expected in args.target:
        target = read_list(Path(list_path))
        for name in portable:
            entry = target.get(name)
            if entry != ("portable", expected):
                errors.append(f"{list_path}: {name} is {entry}, expected ('portable', '{expected}')")
        target_portable = sum(1 for label, _ in target.values() if label == "portable")
        if target_portable != len(portable):
            errors.append(f"{list_path}: {target_portable} portable tests, "
                          f"reference has {len(portable)}")
        windows = sum(1 for label, _ in target.values() if label == "windows")
        if expected == "passed" and any(result != "passed" for _, result in target.values()):
            errors.append(f"{list_path}: not every test passed")
        print(f"{list_path}: {target_portable} portable (reference {len(portable)}), "
              f"{windows} windows, expected result '{expected}'")

    for error in errors:
        print(f"error: {error}", file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
