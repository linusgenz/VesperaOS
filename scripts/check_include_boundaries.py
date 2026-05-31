#!/usr/bin/env python3

from __future__ import annotations

import re
import sys
from pathlib import Path


PUBLIC_HEADER_ROOT = "include/vespera"
FILE_EXTENSIONS = {".h", ".hpp"}
INTERNAL_PREFIXES = ("kernel/", "arch/", "drivers/", "filesystem/")
ALLOWLIST_PATH = "scripts/include_boundary_allowlist.txt"

ANGLE_INCLUDE = re.compile(r"^\s*#include\s+<([^>]+)>")
QUOTE_INCLUDE = re.compile(r'^\s*#include\s+"([^"]+)"')


def load_allowlist(repo_root: Path) -> set[str]:
    allowlist: set[str] = set()
    path = repo_root / ALLOWLIST_PATH
    if not path.exists():
        return allowlist

    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        allowlist.add(line)
    return allowlist


def is_internal_include(include_path: str) -> bool:
    normalized = include_path.replace("\\", "/").lstrip("./")
    return any(normalized.startswith(prefix) for prefix in INTERNAL_PREFIXES)


def collect_violations(repo_root: Path, allowlist: set[str]) -> list[tuple[Path, int, str, str]]:
    violations: list[tuple[Path, int, str, str]] = []
    root = repo_root / PUBLIC_HEADER_ROOT

    if not root.exists():
        return violations

    for path in root.rglob("*"):
        if path.suffix not in FILE_EXTENSIONS or not path.is_file():
            continue

        rel = path.relative_to(repo_root).as_posix()
        if rel in allowlist:
            continue

        try:
            lines = path.read_text(encoding="utf-8").splitlines()
        except UnicodeDecodeError:
            continue

        for idx, line in enumerate(lines, start=1):
            angle = ANGLE_INCLUDE.match(line)
            quote = QUOTE_INCLUDE.match(line)
            include_path = None
            style = ""

            if angle:
                include_path = angle.group(1)
                style = "angle"
            elif quote:
                include_path = quote.group(1)
                style = "quote"

            if not include_path:
                continue

            if is_internal_include(include_path):
                violations.append((path.relative_to(repo_root), idx, include_path, style))

    return violations


def main() -> int:
    repo_root = Path(__file__).resolve().parent.parent
    allowlist = load_allowlist(repo_root)
    violations = collect_violations(repo_root, allowlist)

    if not violations:
        print("include-boundaries: OK (public headers do not include internal trees)")
        return 0

    print("include-boundaries: FAIL")
    print("Public headers under include/vespera must not include kernel/arch/drivers/filesystem headers.")
    for rel, line_no, include_path, style in violations:
        print(f"  - {rel}:{line_no}: {style} include of '{include_path}'")

    if allowlist:
        print("")
        print(f"Allowlist in use: {ALLOWLIST_PATH}")

    return 1


if __name__ == "__main__":
    sys.exit(main())
