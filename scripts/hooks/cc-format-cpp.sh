#!/usr/bin/env bash
# BSP-005 §7.2 — format C/C++ after agent file edits
set -euo pipefail
path=$(python -c "import json,sys; d=json.load(sys.stdin); ti=d.get('tool_input',{}); print(d.get('file_path') or d.get('path') or ti.get('file_path') or ti.get('path',''))")
[[ "$path" =~ \.(c|cc|cpp|h|hpp)$ ]] || exit 0
command -v clang-format >/dev/null 2>&1 || exit 0
clang-format -i "$path"
