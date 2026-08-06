#!/usr/bin/env bash
# BSP-001 §4.1, §4.3 — block secret-bearing staged files even with git add -f
set -euo pipefail
pattern='(^|/)\.env($|\.)|\.(pem|key|p12|pfx|keystore)$|(^|/)id_rsa|(^|/)credentials\.json$|(^|/)secrets\.ya?ml$|(^|/)\.netrc$'
staged=$(git diff --cached --name-only --diff-filter=ACM)
blocked=$(printf '%s\n' "$staged" | grep -Ei "$pattern" | grep -v '\.env\.example$' || true)
if [[ -n "$blocked" ]]; then
  echo "BSP-001 §4.1 violation — secret-bearing files staged:" >&2
  printf '  %s\n' $blocked >&2
  echo "Move values to .env (gitignored) or the OS keychain. Commit .env.example with placeholders only." >&2
  exit 1
fi
