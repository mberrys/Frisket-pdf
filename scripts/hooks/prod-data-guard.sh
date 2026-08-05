#!/usr/bin/env bash
# BSP-003 §4.1 — advisory guard for real personal data in fixtures/seeds
set -euo pipefail
suspect=$(git diff --cached --name-only --diff-filter=ACM \
  | grep -Ei '\.(csv|sql|json|ndjson)$' \
  | grep -Ei 'prod|dump|export|seed|fixture|backup' || true)
for f in $suspect; do
  if head -c 8192 "$f" | grep -qEi '[a-z0-9._%+-]+@[a-z0-9.-]+\.[a-z]{2,}|\b(ssn|date_of_birth|dob|phone_number)\b'; then
    echo "BSP-003 §4.1 — $f may contain real personal data." >&2
    echo "Confirm it is synthetic or anonymized, not just stripped of names." >&2
    echo "Override with: PROD_DATA_ACK=1 git commit ..." >&2
    [[ "${PROD_DATA_ACK:-0}" == "1" ]] || exit 1
  fi
done
