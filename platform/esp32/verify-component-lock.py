#!/usr/bin/env python3
"""Validate ESP-IDF managed-component markers against dependencies.lock."""

from __future__ import annotations

import argparse
import re
from pathlib import Path

COMPONENT_RE = re.compile(r"^  ([^ ].*):$")
HASH_RE = re.compile(r"^    component_hash: ([0-9a-f]{64})$")


def expected_hashes(lock_path: Path) -> dict[str, str]:
    expected: dict[str, str] = {}
    current: str | None = None
    for line in lock_path.read_text(encoding="utf-8").splitlines():
        component = COMPONENT_RE.match(line)
        if component:
            current = component.group(1)
            continue
        digest = HASH_RE.match(line)
        if digest and current and current != "idf":
            expected[current.replace("/", "__")] = digest.group(1)
    if not expected:
        raise SystemExit(f"No managed component hashes found in {lock_path}")
    return expected


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--lock", type=Path, required=True)
    parser.add_argument("--components", type=Path, required=True)
    args = parser.parse_args()

    expected = expected_hashes(args.lock)
    actual_dirs = {
        path.name
        for path in args.components.iterdir()
        if path.is_dir()
    }
    if actual_dirs != set(expected):
        raise SystemExit(
            "Managed component set differs from lock: "
            f"expected={sorted(expected)} actual={sorted(actual_dirs)}"
        )

    for name, wanted in sorted(expected.items()):
        marker = args.components / name / ".component_hash"
        found = marker.read_text(encoding="ascii").strip()
        if found != wanted:
            raise SystemExit(
                f"Managed component hash mismatch for {name}: "
                f"expected={wanted} found={found}"
            )
        print(f"validated component: {name} {found}")


if __name__ == "__main__":
    main()
