#!/usr/bin/env bash
# BSP-002 §4.3 — delayed self-review reminder (agent Stop)
git diff --quiet && git diff --cached --quiet && exit 0
cat <<'EOF'
BSP-002 §4.3 — do not review this yet. Minimum 30 minutes; overnight if it touches
security-sensitive code, data handling, or public API surface.
Review in the GitHub PR diff view, not the editor. Checklist: BSP-002 §4.2.
EOF
