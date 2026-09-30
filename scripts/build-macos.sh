#!/usr/bin/env bash
# build-macos.sh — Build the FlickImp daemon on macOS
# Run directly from any directory; locates project root relative to this script.
#
# Usage:
#   ./scripts/build-macos.sh            — configure (if needed) + build (universal: arm64 + x86_64)
#   ./scripts/build-macos.sh --clean    — wipe build dir first, then configure + build
#   ./scripts/build-macos.sh --native   — build for this Mac's architecture only (faster)
#
# Build dir: <project-root>/build-mac
#
# One-time setup:
#   xcode-select --install          # Apple clang + SDK (libcurl ships with macOS)
#   brew install cmake              # or the CMake.app installer from cmake.org
#   ./scripts/get-deps.sh           # vendored SQLite / httplib / json (run automatically if missing)

set -euo pipefail
PROJ="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$PROJ/build-mac"

# ── Args ──────────────────────────────────────────────────────────────────────
CLEAN=false
ARCHS="arm64;x86_64"
for arg in "$@"; do
    case "$arg" in
        --clean)  CLEAN=true ;;
        --native) ARCHS="$(uname -m)" ;;
        *) echo "Unknown argument: $arg" >&2; exit 1 ;;
    esac
done

# ── Version banner (BSD sed — macOS grep has no -P) ───────────────────────────
VHP="$PROJ/lib/version.hpp"
vfield() { sed -nE "s/^#define $1[[:space:]]+\"?([^\"[:space:]]*)\"?.*/\1/p" "$VHP" | head -1; }
VERSION="$(vfield VERSION_MAJOR).$(vfield VERSION_MINOR).$(vfield VERSION_PATCH)$(vfield VERSION_SUFFIX)"
INNER="  Building FlickImp version ${VERSION}  —  macOS (${ARCHS})  "
BORDER=$(printf '%*s' $(( ${#INNER} + 2 )) '' | tr ' ' '*')
echo "$BORDER"
echo "*${INNER}*"
echo "$BORDER"
echo ""

# ── Preflight ─────────────────────────────────────────────────────────────────
if ! xcode-select -p &>/dev/null; then
    echo "ERROR: Xcode Command Line Tools not installed. Run: xcode-select --install" >&2
    exit 1
fi
if ! command -v cmake &>/dev/null; then
    echo "ERROR: cmake not found. Run: brew install cmake" >&2
    exit 1
fi
if [[ ! -f "$PROJ/third_party/sqlite3/sqlite3.c" || ! -f "$PROJ/third_party/httplib.h" \
      || ! -f "$PROJ/third_party/json.hpp" ]]; then
    echo "--- third_party/ incomplete — fetching dependencies ---"
    "$PROJ/scripts/get-deps.sh"
    echo ""
fi

# ── Configure ─────────────────────────────────────────────────────────────────
if $CLEAN; then
    echo "--- Cleaning build directory ---"
    rm -rf "$BUILD"
fi

configure() {
    cmake -S "$PROJ" -B "$BUILD" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_OSX_ARCHITECTURES="$ARCHS"
}

if [[ ! -f "$BUILD/CMakeCache.txt" ]]; then
    echo "--- Configuring (build-mac) ---"
    configure
    echo ""
else
    CACHED_VER=$(sed -n 's/^FLICKIMP_VERSION:STRING=//p' "$BUILD/CMakeCache.txt")
    CACHED_ARCH=$(sed -n 's/^CMAKE_OSX_ARCHITECTURES:STRING=//p' "$BUILD/CMakeCache.txt")
    if [[ "$CACHED_VER" != "$VERSION" || "$CACHED_ARCH" != "$ARCHS" ]]; then
        echo "--- Config changed (v$CACHED_VER/$CACHED_ARCH → v$VERSION/$ARCHS), reconfiguring ---"
        configure
        echo ""
    fi
fi

# ── Build ─────────────────────────────────────────────────────────────────────
echo "--- Building ---"
cmake --build "$BUILD" --parallel "$(sysctl -n hw.logicalcpu)"

echo ""
echo "Done. Binary: $BUILD/flickimp"
echo "      Arch:   $(lipo -archs "$BUILD/flickimp" 2>/dev/null || echo unknown)"
echo "      Try:    $BUILD/flickimp --version"

# SN: 00005
