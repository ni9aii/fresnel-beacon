#!/usr/bin/env python3
"""Validate OTA-related entries in an ESP-IDF style partitions.csv."""
from __future__ import annotations

import csv
import sys
from pathlib import Path


def validate(path: Path) -> list[str]:
    errors: list[str] = []
    rows = []
    with path.open(newline="", encoding="utf-8") as fh:
        for raw in fh:
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            rows.append(next(csv.reader([line])))
    names = [r[0].strip() for r in rows if r]
    subtypes = [(r[0].strip(), r[2].strip() if len(r) > 2 else "") for r in rows if r]
    ota_data = [n for n, st in subtypes if st == "ota" or n == "otadata"]
    ota_apps = [n for n, st in subtypes if st.startswith("ota_")]
    if not ota_data and "otadata" not in names:
        errors.append("missing otadata partition (subtype ota)")
    if len(ota_apps) < 2:
        errors.append(
            f"need at least two OTA app slots (ota_0, ota_1, ...); found {ota_apps}"
        )
    # basic size parse
    for r in rows:
        if len(r) < 5:
            errors.append(f"row too short: {r}")
            continue
        size = r[4].strip()
        if size and not re_size_ok(size):
            errors.append(f"bad size {size!r} for {r[0]}")
    return errors


def re_size_ok(size: str) -> bool:
    import re
    return bool(re.fullmatch(r"0x[0-9A-Fa-f]+|\d+[KMG]?", size))


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        # auto-discover
        root = Path(__file__).resolve().parents[1]
        cands = list(root.rglob("*partition*.csv"))
        if not cands:
            print("no partition csv found", file=sys.stderr)
            return 2
        paths = cands
    else:
        paths = [Path(a) for a in argv[1:]]
    rc = 0
    for path in paths:
        errs = validate(path)
        if errs:
            rc = 1
            print(f"{path}:")
            for e in errs:
                print(f"  - {e}")
        else:
            print(f"{path}: OK")
    return rc


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
