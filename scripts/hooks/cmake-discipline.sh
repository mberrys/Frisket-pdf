#!/usr/bin/env bash
# BSP-005 §4.2, §4.4 — CMake discipline on staged files
set -euo pipefail
fail=0
for f in $(git diff --cached --name-only --diff-filter=ACM | grep -E 'CMakeLists\.txt$|\.cmake$' || true); do
  if grep -nE '^\s*file\s*\(\s*GLOB' "$f"; then
    echo "BSP-005 §4.2 — file(GLOB) does not re-run on file addition. List sources explicitly. ($f)" >&2
    fail=1
  fi
  if grep -nE '^\s*(include_directories|link_directories|add_definitions)\s*\(' "$f"; then
    echo "BSP-005 §4.2 — directory-scoped command leaks config. Use target_*. ($f)" >&2
    fail=1
  fi
done
check_vcpkg_baseline() {
  local file="$1"
  local key="$2"
  local baseline
  baseline=$(grep -oE "\"${key}\"[[:space:]]*:[[:space:]]*\"[a-f0-9]*\"" "$file" | grep -oE '[a-f0-9]{40}' | head -1 || true)
  if [[ ${#baseline} -ne 40 ]]; then
    echo "BSP-005 §4.4 — ${key} in ${file} must be a full 40-char commit SHA (found ${#baseline} chars)." >&2
    return 1
  fi
  return 0
}
if git diff --cached --name-only | grep -q '^vcpkg\.json$'; then
  if grep -q '"builtin-baseline"' vcpkg.json; then
    check_vcpkg_baseline vcpkg.json builtin-baseline || fail=1
  fi
fi
if git diff --cached --name-only | grep -q '^vcpkg-configuration\.json$'; then
  if grep -q '"baseline"' vcpkg-configuration.json; then
    check_vcpkg_baseline vcpkg-configuration.json baseline || fail=1
  fi
fi
exit $fail