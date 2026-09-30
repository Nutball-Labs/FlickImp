#!/usr/bin/env bash
# package-macos.sh — Produce macOS TGZ and ZIP packages via CPack
# Assumes build-macos.sh has already been run successfully.
#
# Usage:
#   ./scripts/package-macos.sh
#
# Output: packages/ at project root
#   flickimp-X.Y.Z-macOS.tar.gz
#   flickimp-X.Y.Z-macOS.zip
#
# Each archive holds flickimp, web/, install.sh, uninstall.sh and README-macOS.txt.
# The end user unpacks it and runs ./install.sh (per-user, launchd autostart).
#
# Requires: cmake (for cpack)

set -euo pipefail
PROJ="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$PROJ/build-mac"

# ── Version banner ────────────────────────────────────────────────────────────
VHP="$PROJ/lib/version.hpp"
vfield() { sed -nE "s/^#define $1[[:space:]]+\"?([^\"[:space:]]*)\"?.*/\1/p" "$VHP" | head -1; }
VERSION="$(vfield VERSION_MAJOR).$(vfield VERSION_MINOR).$(vfield VERSION_PATCH)$(vfield VERSION_SUFFIX)"
INNER="  Packaging FlickImp version ${VERSION}  —  macOS  "
BORDER=$(printf '%*s' $(( ${#INNER} + 2 )) '' | tr ' ' '*')
echo "$BORDER"
echo "*${INNER}*"
echo "$BORDER"
echo ""

if [[ ! -f "$BUILD/CMakeCache.txt" ]]; then
    echo "ERROR: build-mac not found — run ./scripts/build-macos.sh first." >&2
    exit 1
fi

CACHED_VER=$(sed -n 's/^FLICKIMP_VERSION:STRING=//p' "$BUILD/CMakeCache.txt")
if [[ "$CACHED_VER" != "$VERSION" ]]; then
    echo "ERROR: build-mac is v$CACHED_VER but version.hpp is v$VERSION — rebuild first." >&2
    exit 1
fi

mkdir -p "$PROJ/packages"
echo "=== Packaging TGZ + ZIP (CPack) ==="
(cd "$BUILD" && cpack)

echo ""
echo "Packages:"
ls -lh "$PROJ/packages"/flickimp-"$VERSION"-macOS.* 2>/dev/null \
    | awk '{print "  "$NF, "("$5")"}'

echo ""
echo "To publish, upload to the GitHub release (after it exists):"
echo "  gh release upload v$VERSION packages/flickimp-$VERSION-macOS.* --repo Nutball-Labs/FlickImp --clobber"

# SN: 00005
