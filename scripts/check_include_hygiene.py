#!/usr/bin/env python3

from __future__ import annotations

import os
import re
import sys
from pathlib import Path


ROOT_DIRS = ("arch", "kernel", "drivers", "filesystem")
FILE_EXTENSIONS = {".h", ".hpp", ".c", ".cc", ".cpp"}
DEEP_RELATIVE_INCLUDE = re.compile(r'^\s*#include\s+"(?:\.\./){2,}[^"]+"')


def _is_jetbrains_terminal() -> bool:
    """JetBrains setzt TERMINAL_EMULATOR oder JETBRAINS_REMOTE_DEV."""
    return (
            "TERMINAL_EMULATOR" in os.environ
            or "JETBRAINS_REMOTE_DEV" in os.environ
            or os.environ.get("TERMINAL_EMULATOR", "").startswith("JetBrains")
    )


def _supports_osc8() -> bool:
    """Kitty, WezTerm, iTerm2, Ghostty etc. setzen TERM oder TERM_PROGRAM."""
    term = os.environ.get("TERM", "")
    term_program = os.environ.get("TERM_PROGRAM", "")
    colorterm = os.environ.get("COLORTERM", "")
    return any([
        term == "xterm-kitty",
        term_program in ("iTerm.app", "WezTerm", "ghostty"),
        colorterm in ("truecolor", "24bit"),
        "KITTY_WINDOW_ID" in os.environ,
        "WEZTERM_EXECUTABLE" in os.environ,
        ])


def format_link(abs_path: Path, line_no: int, display: str) -> str:
    uri = f"file://{abs_path}:{line_no}"

    if _is_jetbrains_terminal():
        return f"{uri}"

    if _supports_osc8():
        ESC = "\033"
        return f"{ESC}]8;;{uri}{ESC}\\{display}{ESC}]8;;{ESC}\\"

    return display


def collect_violations(repo_root: Path) -> list[tuple[Path, int, str]]:
    violations: list[tuple[Path, int, str]] = []

    for root_name in ROOT_DIRS:
        root = repo_root / root_name
        if not root.exists():
            continue

        for path in root.rglob("*"):
            if path.suffix not in FILE_EXTENSIONS or not path.is_file():
                continue

            try:
                lines = path.read_text(encoding="utf-8").splitlines()
            except UnicodeDecodeError:
                continue

            for idx, line in enumerate(lines, start=1):
                if DEEP_RELATIVE_INCLUDE.match(line):
                    rel = path.relative_to(repo_root)
                    violations.append((rel, idx, line.strip()))

    return violations


def main() -> int:
    repo_root = Path(__file__).resolve().parent.parent
    violations = collect_violations(repo_root)

    if not violations:
        print("include-hygiene: OK (no deep relative includes in arch/kernel/drivers/filesystem)")
        return 0

    print("include-hygiene: FAIL")
    print('Found deep relative includes (pattern: #include "../../...") in kernel-side code:')

    for rel, line_no, line in violations:
        abs_path = repo_root / rel
        display = f"{rel}:{line_no}"
        link = format_link(abs_path, line_no, display)
        print(f"  - {link}: {line}")

    print("")
    print("Use repo-rooted angle includes instead (for example <kernel/...>, <arch/...>, <drivers/...>).")
    return 1


if __name__ == "__main__":
    sys.exit(main())