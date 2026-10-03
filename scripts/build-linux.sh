#!/usr/bin/env bash
# build-linux.sh — Full compile for FlickImp on Linux (Alma/RHEL/Fedora)
# Run directly from any directory; locates project root relative to this script.
#
# Usage:
#   ./scripts/build-linux.sh           — confirm version, configure (if needed) + build
#   ./scripts/build-linux.sh --clean   — wipe build dir first, then configure + build
#   ./scripts/build-linux.sh -y        — skip the version prompt (also skipped
#                                        automatically when stdin isn't a terminal)
#
# At the version prompt: Enter builds the version shown, a new version string
# (MAJOR.MINOR.PATCH plus optional suffix, e.g. 1.6.1 or 1.7.0-rc1) is written
# to lib/version.hpp first, and q quits without building.
#
# Build dir: <project-root>/build-linux
# Requires:  cmake, g++ (C++17), libcurl
#   sudo dnf install cmake gcc-c++ libcurl-devel
#
# Optional (for flickimp-config Qt configurator):
#   sudo dnf install qt6-qtbase-devel

set -euo pipefail
PROJ="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$PROJ/build-linux"

CLEAN=0
ASK=1
for arg in "$@"; do
    case "$arg" in
        --clean)    CLEAN=1 ;;
        -y|--yes)   ASK=0 ;;
        *) echo "Unknown option: $arg" >&2; exit 1 ;;
    esac
done
[[ -t 0 ]] || ASK=0

# ── Version banner ────────────────────────────────────────────────────────────
VHP="$PROJ/lib/version.hpp"

read_version() {
    MAJOR=$(grep -m1 '#define VERSION_MAJOR'  "$VHP" | awk '{print $3}' | tr -d '\r')
    MINOR=$(grep -m1 '#define VERSION_MINOR'  "$VHP" | awk '{print $3}' | tr -d '\r')
    PATCH=$(grep -m1 '#define VERSION_PATCH'  "$VHP" | awk '{print $3}' | tr -d '\r')
    SUFFIX=$(grep -m1 '#define VERSION_SUFFIX' "$VHP" | grep -oP '(?<=")[^"]*' | tr -d '\r' || true)
    VERSION="${MAJOR}.${MINOR}.${PATCH}${SUFFIX}"
}

banner() {
    local inner="  Building FlickImp version ${VERSION}  "
    local border
    border=$(printf '%*s' $(( ${#inner} + 2 )) | tr ' ' '*')
    echo "$border"
    echo "*${inner}*"
    echo "$border"
    echo ""
}

# Write MAJOR.MINOR.PATCH[SUFFIX] into version.hpp, keeping its layout
write_version() {
    local maj=$1 min=$2 pat=$3 suf=$4
    sed -i -E \
        -e "s/^(#define VERSION_MAJOR[[:space:]]+).*/\1${maj}/" \
        -e "s/^(#define VERSION_MINOR[[:space:]]+).*/\1${min}/" \
        -e "s/^(#define VERSION_PATCH[[:space:]]+).*/\1${pat}/" \
        -e "s/^(#define VERSION_SUFFIX[[:space:]]+).*/\1\"${suf}\"/" \
        "$VHP"
}

read_version
banner

if (( ASK )); then
    while true; do
        read -r -p "Build ${VERSION}? [Enter = yes, new version, q = quit]: " answer
        answer="${answer//[[:space:]]/}"
        if [[ -z "$answer" ]]; then
            break
        elif [[ "$answer" == [qQ] ]]; then
            echo "Build cancelled."
            exit 0
        elif [[ "$answer" =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)([-+.~][A-Za-z0-9.+~-]*)?$ ]]; then
            write_version "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}" \
                          "${BASH_REMATCH[3]}" "${BASH_REMATCH[4]}"
            read_version
            echo ""
            echo "lib/version.hpp updated."
            banner
            break
        else
            echo "  Not a version: use MAJOR.MINOR.PATCH with an optional suffix, e.g. 1.6.1 or 1.7.0-rc1"
        fi
    done
fi

if (( CLEAN )); then
    echo "--- Cleaning build directory ---"
    rm -rf "$BUILD"
fi

if [[ ! -d "$BUILD" ]]; then
    echo "--- Configuring (build-linux) ---"
    cmake -S "$PROJ" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
    echo ""
elif [[ -f "$BUILD/CMakeCache.txt" ]]; then
    CACHED=$(grep -s 'FLICKIMP_VERSION:STRING=' "$BUILD/CMakeCache.txt" | cut -d= -f2 | tr -d '\r' || true)
    if [[ "$CACHED" != "$VERSION" ]]; then
        echo "--- Version changed ($CACHED → $VERSION), reconfiguring ---"
        cmake -S "$PROJ" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
        echo ""
    fi
fi

echo "--- Building ---"
cmake --build "$BUILD" --parallel "$(nproc)"

echo ""
echo "Done. Binaries in $BUILD/"

# SN: 00006
