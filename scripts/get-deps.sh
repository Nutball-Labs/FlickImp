#!/usr/bin/env bash
# get-deps.sh — Download vendored third-party headers into third_party/
#
# Run once before your first build:
#   ./scripts/get-deps.sh
#
# Downloads:
#   third_party/sqlite3/sqlite3.c + sqlite3.h  — SQLite amalgamation 3.47.2
#   third_party/httplib.h                      — cpp-httplib v0.18.1
#   third_party/json.hpp                       — nlohmann/json v3.11.3

set -euo pipefail
PROJ="$(cd "$(dirname "$0")/.." && pwd)"
T="$PROJ/third_party"

mkdir -p "$T/sqlite3"

echo "--- Downloading SQLite amalgamation ---"
TMP=$(mktemp -d)
curl -fsSL "https://www.sqlite.org/2024/sqlite-amalgamation-3470200.zip" -o "$TMP/sqlite.zip"
unzip -q "$TMP/sqlite.zip" -d "$TMP"
cp "$TMP"/sqlite-amalgamation-3470200/sqlite3.{c,h} "$T/sqlite3/"
rm -rf "$TMP"
echo "    OK: third_party/sqlite3/sqlite3.{c,h}"

echo "--- Downloading cpp-httplib v0.18.1 ---"
curl -fsSL "https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.18.1/httplib.h" \
    -o "$T/httplib.h"
echo "    OK: third_party/httplib.h"

echo "--- Downloading nlohmann/json v3.11.3 ---"
curl -fsSL "https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp" \
    -o "$T/json.hpp"
echo "    OK: third_party/json.hpp"

echo ""
echo "Done. Run ./scripts/build-linux.sh to build."

# SN: 00001
