#!/usr/bin/env python3
"""Run a bash hook script portably (Windows Git Bash, macOS, Linux)."""
from __future__ import annotations

import os
import shutil
import subprocess
import sys


def find_bash() -> str:
    preferred = [
        os.environ.get("BASH"),
        r"C:\Program Files\Git\bin\bash.exe",
        r"C:\Program Files\Git\usr\bin\bash.exe",
    ]
    for candidate in preferred:
        if candidate and os.path.isfile(candidate):
            return candidate
    discovered = shutil.which("bash")
    if discovered and "WindowsApps" not in discovered:
        return discovered
    raise SystemExit("bash not found — install Git for Windows or set BASH to bash.exe")


def main() -> int:
    if len(sys.argv) < 2:
        raise SystemExit("usage: run-bash.py <script.sh> [args...]")
    bash = find_bash()
    return subprocess.call([bash, *sys.argv[1:]])


if __name__ == "__main__":
    raise SystemExit(main())
