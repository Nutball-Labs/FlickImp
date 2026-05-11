#!/usr/bin/env bash
# package-linux.sh — Produce RPM, DEB, and TGZ packages via CPack
# Assumes build-linux.sh has already been run successfully.
#
# Usage:
#   ./scripts/package-linux.sh
#
# Output: packages/ at project root
#   flickimp-X.Y.Z-1.x86_64.rpm
#   watching_X.Y.Z_amd64.deb
#   flickimp-X.Y.Z-Linux.tar.gz
#
# Requires: cmake, rpm-build (for RPM), dpkg-deb (for DEB)
#   sudo dnf install rpm-build

set -euo pipefail
PROJ="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$PROJ/build-linux"

echo "=== Packaging (RPM, DEB, TGZ) ==="
(cd "$BUILD" && cpack)

echo ""
echo "Packages:"
ls -lh "$PROJ/packages"/flickimp-*.rpm \
        "$PROJ/packages"/watching_*.deb \
        "$PROJ/packages"/flickimp-*-Linux.tar.gz 2>/dev/null \
    | awk '{print "  "$NF, "("$5")"}'

# SN: 00001
