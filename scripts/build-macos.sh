#!/usr/bin/env bash
# build-macos.sh — Build FlickImp daemon on macOS
# Run directly from any directory; locates project root relative to this script.
#
# Usage:
#   ./scripts/build-macos.sh           — configure + build
#   ./scripts/build-macos.sh --clean   — wipe build dir first, then configure + build
#
# Build dir: <project-root>/build-mac
# Requires:  Xcode Command Line Tools, cmake
#   xcode-select --install
#   brew install cmake          # if cmake not already present
#
# libcurl ships with macOS — no separate install needed.

set -euo pipefail
PROJ="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$PROJ/build-mac"

if [[ "${1:-}" == "--clean" ]]; then
    echo "--- Cleaning build directory ---"
    rm -rf "$BUILD"
fi

if [[ ! -d "$BUILD" ]]; then
    echo "--- Configuring (build-mac) ---"
    cmake -S "$PROJ" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release
    echo ""
fi

echo "--- Building ---"
cmake --build "$BUILD" --parallel "$(sysctl -n hw.logicalcpu)"

echo ""
echo "Done. Binary: $BUILD/flickimp"

# SN: 00001
