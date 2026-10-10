#!/usr/bin/env python3
"""Guard CI test runs against false green results.

The registered test inventory comes from ``ctest --show-only=json-v1`` and the
execution record from ``ctest --output-junit``. This script fails when:

* no tests are registered, or a registered test has no label / an unknown label;
* a required label has no tests, or a forbidden label is present;
* (execution mode) the JUnit record does not contain every registered test
  exactly once, contains unknown tests, or reports anything other than a pass;
* (build-only mode) a registered test executable was not produced.

It writes the full inventory/execution list as a TSV file and, when running in
GitHub Actions, appends it to the job summary.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import xml.etree.ElementTree as ElementTree
from pathlib import Path

KNOWN_LABELS = {"portable", "windows"}


def fail(message: str) -> None:
    print(f"error: {message}", file=sys.stderr)
    sys.exit(1)


def load_inventory(path: Path) -> list[dict]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("kind") != "ctestInfo":
        fail(f"{path} is not a ctest --show-only=json-v1 document")
    inventory = []
    for test in data.get("tests", []):
        labels: list[str] = []
        for prop in test.get("properties", []):
            if prop.get("name") == "LABELS":
                labels = list(prop.get("value", []))
        inventory.append({
            "name": test["name"],
            "labels": labels,
            "command": test.get("command", []),
        })
    return inventory


def check_inventory(inventory: list[dict], required: list[str], forbidden: list[str]) -> None:
    if not inventory:
        fail("no tests are registered")
    names = [test["name"] for test in inventory]
    duplicates = sorted({name for name in names if names.count(name) > 1})
    if duplicates:
        fail(f"duplicate test names: {duplicates}")
    for test in inventory:
        if len(test["labels"]) != 1 or test["labels"][0] not in KNOWN_LABELS:
            fail(f"test {test['name']} must have exactly one label from "
                 f"{sorted(KNOWN_LABELS)}, has {test['labels']}")
    present = {test["labels"][0] for test in inventory}
    for label in required:
        if label not in present:
            fail(f"required label '{label}' has no registered tests")
    for label in forbidden:
        if label in present:
            fail(f"label '{label}' must not be registered on this platform")


def load_junit(path: Path) -> dict[str, dict]:
    root = ElementTree.parse(path).getroot()
    cases: dict[str, dict] = {}
    for case in root.iter("testcase"):
        name = case.get("name", "")
        if name in cases:
            fail(f"test {name} appears more than once in {path}")
        problems = [child.tag for child in case
                    if child.tag in {"failure", "error", "skipped"}]
        output = "\n".join((child.text or "") for child in case
                           if child.tag in {"system-out", "failure", "error"})
        cases[name] = {
            "status": case.get("status", ""),
            "time": case.get("time", ""),
            "problems": problems,
            "output": output,
        }
    return cases


def escape_property(value: str) -> str:
    """Escape a workflow-command property value (e.g. an annotation title)."""
    return (value.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")
            .replace(":", "%3A").replace(",", "%2C"))


def annotate_failure(name: str, output: str) -> None:
    """Report a failed test as a GitHub Actions error annotation.

    Annotations are visible on the pull request and through the checks API,
    so the failure reason is available without downloading logs.
    """
    if os.environ.get("GITHUB_ACTIONS") != "true":
        return
    tail = output.strip()[-3000:] or "(no test output captured)"
    escaped = tail.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")
    print(f"::error title={escape_property(f'CTest {name} failed')}::{escaped}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inventory", required=True, type=Path,
                        help="output of ctest --show-only=json-v1")
    parser.add_argument("--junit", type=Path,
                        help="output of ctest --output-junit (execution mode)")
    parser.add_argument("--build-only", action="store_true",
                        help="tests are compiled but intentionally not run")
    parser.add_argument("--require-label", action="append", default=[])
    parser.add_argument("--forbid-label", action="append", default=[])
    parser.add_argument("--list-output", required=True, type=Path)
    parser.add_argument("--title", default="CTest execution list")
    args = parser.parse_args()

    if bool(args.junit) == args.build_only:
        fail("pass exactly one of --junit or --build-only")

    inventory = load_inventory(args.inventory)
    check_inventory(inventory, args.require_label, args.forbid_label)

    rows = []
    errors = []
    if args.build_only:
        for test in inventory:
            command = test["command"]
            built = bool(command) and Path(command[0]).is_file()
            if not built:
                errors.append(f"{test['name']}: executable not built ({command[:1]})")
            rows.append((test["name"], test["labels"][0],
                         "built, NOT RUN" if built else "MISSING", ""))
    else:
        cases = load_junit(args.junit)
        registered = {test["name"] for test in inventory}
        unexpected = sorted(set(cases) - registered)
        if unexpected:
            errors.append(f"JUnit contains unregistered tests: {unexpected}")
        for test in inventory:
            case = cases.get(test["name"])
            if case is None:
                errors.append(f"{test['name']}: registered but not executed")
                rows.append((test["name"], test["labels"][0], "NOT EXECUTED", ""))
                continue
            if case["status"] != "run" or case["problems"]:
                errors.append(f"{test['name']}: status={case['status']} {case['problems']}")
                annotate_failure(test["name"], case["output"])
            rows.append((test["name"], test["labels"][0],
                         "passed" if case["status"] == "run" and not case["problems"]
                         else case["status"] or "failed",
                         case["time"]))

    counts: dict[str, int] = {}
    for _, label, _, _ in rows:
        counts[label] = counts.get(label, 0) + 1
    summary = ", ".join(f"{label}={count}" for label, count in sorted(counts.items()))
    mode = "build-only (not run)" if args.build_only else "executed"

    args.list_output.parent.mkdir(parents=True, exist_ok=True)
    with args.list_output.open("w", encoding="utf-8", newline="\n") as handle:
        handle.write("test\tlabel\tresult\tseconds\n")
        for row in rows:
            handle.write("\t".join(row) + "\n")

    step_summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if step_summary:
        with open(step_summary, "a", encoding="utf-8") as handle:
            handle.write(f"### {args.title}\n\n")
            handle.write(f"{len(rows)} tests, {mode}; labels: {summary}\n\n")
            handle.write("| Test | Label | Result | Seconds |\n|---|---|---|---|\n")
            for row in rows:
                handle.write("| " + " | ".join(row) + " |\n")
            handle.write("\n")

    results: dict[str, int] = {}
    for _, _, result, _ in rows:
        results[result] = results.get(result, 0) + 1
    result_summary = ", ".join(f"{result}={count}" for result, count in sorted(results.items()))
    if os.environ.get("GITHUB_ACTIONS") == "true":
        # The full list as a notice annotation: readable on the pull request
        # and through the checks API, independent of artifact downloads.
        listing = "%0A".join(f"{name} [{label}] {result}" for name, label, result, _ in rows)
        print(f"::notice title={escape_property(args.title)}::{len(rows)} tests, {mode}; labels: {summary}; "
              f"results: {result_summary}%0A{listing}")

    print(f"{args.title}: {len(rows)} tests, {mode}; labels: {summary}; results: {result_summary}")
    for row in rows:
        print("  " + "\t".join(row))
    if errors:
        for error in errors:
            print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
