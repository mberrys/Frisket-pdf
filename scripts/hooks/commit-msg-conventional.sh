#!/usr/bin/env bash
# BSP-002 §3.4 — Conventional Commits v1.0.0
set -euo pipefail
msg=$(head -n1 "$1")
[[ "$msg" =~ ^(Merge|Revert|fixup!|squash!) ]] && exit 0
re='^(feat|fix|docs|style|refactor|perf|test|build|ci|chore|revert)(\([a-z0-9._/-]+\))?!?: .{1,72}$'
if ! [[ "$msg" =~ $re ]]; then
  cat >&2 <<'EOF'
BSP-002 §3.4 — commit message must follow Conventional Commits v1.0.0:
  <type>[optional scope]: <description>
Types: feat fix docs style refactor perf test build ci chore revert
Breaking change: append ! after type/scope, or add a BREAKING CHANGE: footer.
EOF
  exit 1
fi
