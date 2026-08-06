#!/usr/bin/env bash
# BSP-005 §4.4 — clang-tidy on staged C/C++ files when compile_commands.json exists
set -euo pipefail
CLANG_TIDY="${CLANG_TIDY:-clang-tidy-18}"
compile_db=""
for candidate in build build/debug build/Release; do
  if [[ -f "${candidate}/compile_commands.json" ]]; then
    compile_db="$candidate"
    break
  fi
done
if [[ -z "$compile_db" ]]; then
  echo "BSP-005 — clang-tidy skipped: no compile_commands.json under build/." >&2
  echo "Configure with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON to enable this gate." >&2
  exit 0
fi
if ! command -v "${CLANG_TIDY}" >/dev/null 2>&1; then
  echo "BSP-005 — clang-tidy skipped: ${CLANG_TIDY} not on PATH." >&2
  exit 0
fi
status=0
while IFS= read -r file; do
  [[ -z "$file" ]] && continue
  "${CLANG_TIDY}" -p "$compile_db" --quiet "$file" || status=1
done < <(git diff --cached --name-only --diff-filter=ACM | grep -E '\.(c|cc|cpp|h|hpp)$' || true)
exit $status
