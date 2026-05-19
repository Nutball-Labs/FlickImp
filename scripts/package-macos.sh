#!/usr/bin/env bash
# package-macos.sh — Produce macOS TGZ and ZIP archives via CPack
# Assumes build-macos.sh has already been run successfully.
#
# Usage:
#   ./scripts/package-macos.sh
#
# Output: packages/ at project root
#   flickimp-X.Y.Z-macOS.tar.gz
#   flickimp-X.Y.Z-macOS.zip
#
# Requires: cmake (for cpack)

set -euo pipefail
PROJ="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$PROJ/build-mac"

echo "=== Packaging TGZ + ZIP (CPack) ==="
(cd "$BUILD" && cpack)

echo ""
echo "Packages:"
ls -lh "$PROJ/packages"/flickimp-*macOS* 2>/dev/null \
    | awk '{print "  "$NF, "("$5")"}'

# SN: 00001
