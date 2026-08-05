#!/usr/bin/env bash
# BSP-002 §3.3, §3.4, §3.2; BSP-006 §3.4, §5.1 — pre-push policy
set -euo pipefail

protected="${IVORY_PROTECTED_BRANCHES:-^refs/heads/(main|master|stable|release/.*)$}"

while read -r local_ref local_sha remote_ref remote_sha; do
  [[ -z "$remote_ref" ]] && continue

  # 1.10 — release tag gate
  if [[ "$remote_ref" =~ ^refs/tags/v(.+)$ ]]; then
    ver="${BASH_REMATCH[1]}"
    tagtype=$(git cat-file -t "$local_sha" 2>/dev/null || echo "")
    [[ "$tagtype" == "tag" ]] || {
      echo "BSP-006 §3.4 — lightweight tag rejected; use git tag -s -a." >&2
      exit 1
    }
    git tag -v "v$ver" >/dev/null 2>&1 || {
      echo "BSP-006 §3.4 — tag v$ver is not signed." >&2
      exit 1
    }
    declared=""
    if [[ -f CMakeLists.txt ]]; then
      declared=$(grep -oPm1 'project\s*\(.*VERSION\s+\K[0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt 2>/dev/null || true)
    elif [[ -f package.json ]]; then
      declared=$(grep -oPm1 '"version"\s*:\s*"\K[0-9]+\.[0-9]+\.[0-9]+' package.json 2>/dev/null || true)
    fi
    if [[ -n "$declared" && "$declared" != "${ver%%-*}" ]]; then
      echo "BSP-006 §3.4 — tag v$ver ≠ declared VERSION $declared." >&2
      exit 1
    fi
    if [[ -f CHANGELOG.md ]]; then
      grep -qE "^## \[${ver}\] - [0-9]{4}-[0-9]{2}-[0-9]{2}" CHANGELOG.md \
        || {
          echo "BSP-006 §5.1 — no dated CHANGELOG heading for $ver. Promote [Unreleased]." >&2
          exit 1
        }
    fi
    continue
  fi

  # 1.7 — direct push and force-push guard
  if [[ "$remote_ref" =~ $protected ]]; then
    echo "BSP-002 §3.3 — direct push to $remote_ref refused. Open a PR from a feature branch." >&2
    exit 1
  fi

  [[ "$local_ref" == "0000000000000000000000000000000000000000" ]] && continue

  range="${remote_sha}..${local_sha}"
  [[ "$remote_sha" == "0000000000000000000000000000000000000000" ]] && range="$local_sha"

  # 1.8 — commit signature check
  if [[ -n "$range" ]]; then
    unsigned=$(git log --format='%H %G?' "$range" 2>/dev/null | awk '$2 !~ /^[GU]$/ {print $1}' || true)
    if [[ -n "$unsigned" ]]; then
      echo "BSP-002 §3.4 — unsigned commits in push:" >&2
      printf '  %s\n' $unsigned >&2
      echo "Configure signing: git config commit.gpgsign true; git config gpg.format ssh" >&2
      exit 1
    fi
  fi
done

# 1.9 — branch lifetime warning (non-blocking)
branch=$(git rev-parse --abbrev-ref HEAD)
case "$branch" in
  feature/*) limit=$((3 * 86400)) ;;
  bugfix/*) limit=$((2 * 86400)) ;;
  hotfix/*) limit=$((4 * 3600)) ;;
  *) limit=0 ;;
esac
if (( limit > 0 )); then
  base=$(git merge-base HEAD origin/master 2>/dev/null \
    || git merge-base HEAD origin/main 2>/dev/null \
    || git merge-base HEAD origin/stable 2>/dev/null \
    || true)
  if [[ -n "${base:-}" ]]; then
    first=$(git log --format=%ct "$base..HEAD" 2>/dev/null | tail -1 || true)
    if [[ -n "${first:-}" ]]; then
      age=$(( $(date +%s) - first ))
      if (( age > limit )); then
        echo "BSP-002 §3.2 — $branch is $((age / 3600))h old, past its target. Consider splitting." >&2
      fi
    fi
  fi
fi
