#!/usr/bin/env python3
"""Validate the StackChan 16 MiB partition layout used by this firmware."""

from __future__ import annotations

import csv
import sys
from pathlib import Path

FLASH_SIZE = 0x1000000
SECTOR_SIZE = 0x1000


def parse_size(value: str) -> int:
    value = value.strip().upper()
    if value.endswith("M"):
        return int(value[:-1], 0) * 1024 * 1024
    return int(value, 0)


def load_partitions(path: Path) -> list[dict[str, int | str]]:
    partitions: list[dict[str, int | str]] = []
    previous_end = 0
    with path.open(encoding="utf-8", newline="") as source:
        for row in csv.reader(source, skipinitialspace=True):
            if not row or row[0].strip().startswith("#"):
                continue
            if len(row) < 5:
                raise ValueError(f"invalid partition row: {row}")

            name, _kind, _subtype, offset_text, size_text = (
                cell.strip() for cell in row[:5]
            )
            size = parse_size(size_text)
            offset = (
                parse_size(offset_text)
                if offset_text
                else (previous_end + SECTOR_SIZE - 1) // SECTOR_SIZE * SECTOR_SIZE
            )
            partitions.append({"name": name, "offset": offset, "size": size})
            previous_end = offset + size
    return partitions


def validate(path: Path) -> None:
    partitions = load_partitions(path)
    errors: list[str] = []
    by_name = {str(item["name"]): item for item in partitions}

    for item in partitions:
        name = str(item["name"])
        offset = int(item["offset"])
        size = int(item["size"])
        if offset % SECTOR_SIZE or size % SECTOR_SIZE:
            errors.append(f"{name} is not 4 KiB aligned")
        if offset + size > FLASH_SIZE:
            errors.append(f"{name} ends past 16 MiB flash")

    ordered = sorted(partitions, key=lambda item: int(item["offset"]))
    for left, right in zip(ordered, ordered[1:]):
        if int(left["offset"]) + int(left["size"]) > int(right["offset"]):
            errors.append(f"{left['name']} overlaps {right['name']}")

    expected = {
        "assets": (0xA00000, 0x600000),
        "coredump": (0x10000, 0x10000),
        "ota_0": (0x20000, 0x4F0000),
        "ota_1": (0x510000, 0x4F0000),
    }
    for name, pair in expected.items():
        item = by_name.get(name)
        actual = (int(item["offset"]), int(item["size"])) if item else None
        if actual != pair:
            errors.append(f"{name} expected offset/size {pair}, got {actual}")

    if errors:
        raise ValueError("; ".join(errors))


if __name__ == "__main__":
    try:
        validate(Path(sys.argv[1]))
    except (IndexError, OSError, ValueError) as exc:
        print(f"partition layout invalid: {exc}", file=sys.stderr)
        raise SystemExit(1) from exc
