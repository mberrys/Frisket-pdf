#!/usr/bin/env bash
# BSP-001 §4.1, §4.4 — agent Write/Edit guard (Claude Code PreToolUse / Cursor preToolUse)
set -euo pipefail
path=$(python -c "
import json, sys
raw = sys.stdin.read().strip()
if not raw:
    sys.exit(0)
try:
    d = json.loads(raw)
except json.JSONDecodeError:
    sys.exit(0)
ti = d.get('tool_input', {})
print(d.get('file_path') or d.get('path') or ti.get('file_path') or ti.get('path', ''))
" 2>/dev/null || true)
[[ -z "$path" ]] && exit 0
case "$path" in
  *.env|*.env.*|*.pem|*.key|*id_rsa*|*credentials.json)
    [[ "$path" == *.env.example ]] && exit 0
    echo "BLOCKED — BSP-001 §4.1: agent writes to secret-bearing files are prohibited. Ask the operator to set this by hand." >&2
    exit 2
    ;;
  *.gitleaks.toml)
    echo "BLOCKED — BSP-001 §4.4: allowlist edits need an inline justification comment and operator sign-off." >&2
    exit 2
    ;;
esac
exit 0