#!/usr/bin/env bash
# push-gh.sh — Push current branch to GitHub via SSH (no tokens required)
#
# Usage: ./scripts/push-gh.sh [--dry-run]

set -euo pipefail

PROJ="$(cd "$(dirname "$0")/.." && pwd)"
REMOTE="git@github.com:Nutball-Labs/FlickImp.git"
BRANCH="$(git -C "$PROJ" rev-parse --abbrev-ref HEAD)"
DRY_RUN=false

[[ "${1:-}" == "--dry-run" ]] && DRY_RUN=true

echo "FlickImp — push to GitHub"
echo "  Remote : $REMOTE"
echo "  Branch : $BRANCH"
echo ""

if $DRY_RUN; then
    echo "[dry-run] Would run: git push $REMOTE $BRANCH"
    exit 0
fi

git -C "$PROJ" push "$REMOTE" "$BRANCH"

echo ""
echo "Done. https://github.com/Nutball-Labs/FlickImp/tree/${BRANCH}"

# SN: 00001
