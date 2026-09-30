#!/usr/bin/env bash
# Go-MacOS.sh — Full macOS release pipeline: build + package with sleep inhibit
# Prevents system from sleeping or suspending during long compile and packaging runs.
#
# Usage:
#   ./scripts/Go-MacOS.sh              -- configure + build + package
#   ./scripts/Go-MacOS.sh --clean      -- wipe build dir first, then build + package
#   ./scripts/Go-MacOS.sh --native     -- this Mac's architecture only (default: universal)
#
# --clean and --native are forwarded to build-macos.sh.
#
# caffeinate is held via a background PID and a trap so it is always released —
# on normal exit, on build/package failure, and on Ctrl+C.

set -euo pipefail
SCRIPTS="$(cd "$(dirname "$0")" && pwd)"

# ── Sleep inhibitor ────────────────────────────────────────────────────────────
# caffeinate -s prevents system sleep; -i prevents idle sleep.
# Run in background so the trap can kill it regardless of how the script exits.
caffeinate -s -i &
CAFF_PID=$!

cleanup() {
    if kill -0 "$CAFF_PID" 2>/dev/null; then
        kill "$CAFF_PID" 2>/dev/null || true
        echo ""
        echo "[Go] Sleep inhibit released."
    fi
}
trap cleanup EXIT

echo "[Go] Sleep/hibernate inhibited for build duration (caffeinate PID $CAFF_PID)."

# ── Args ───────────────────────────────────────────────────────────────────────
BUILD_ARGS=()
for arg in "$@"; do
    case "$arg" in
        --clean|--native) BUILD_ARGS+=("$arg") ;;
        *) echo "[Go] Unknown argument: $arg" >&2; exit 1 ;;
    esac
done

# ── Pipeline ───────────────────────────────────────────────────────────────────

# Phase 1: Build
echo ""
echo "[Go] === Phase 1: Build ==="

set +e
"$SCRIPTS/build-macos.sh" ${BUILD_ARGS[@]+"${BUILD_ARGS[@]}"}
BUILD_EXIT=$?
set -e

if [[ $BUILD_EXIT -ne 0 ]]; then
    echo ""
    echo "[Go] Build failed -- aborting pipeline."
    exit 1
fi

# Phase 2: Package
echo ""
echo "[Go] === Phase 2: Package ==="

set +e
"$SCRIPTS/package-macos.sh"
PKG_EXIT=$?
set -e

if [[ $PKG_EXIT -ne 0 ]]; then
    echo ""
    echo "[Go] Packaging failed."
    exit 1
fi

echo ""
echo "[Go] Pipeline complete."

# SN: 00005
