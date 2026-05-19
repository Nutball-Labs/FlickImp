# get-deps.ps1 — Download vendored third-party headers for Windows
# Run once before your first build:
#   .\scripts\get-deps.ps1
#
# Downloads:
#   third_party\sqlite3\sqlite3.c + sqlite3.h  — SQLite amalgamation 3.47.2
#   third_party\httplib.h                      — cpp-httplib v0.18.1
#   third_party\json.hpp                       — nlohmann/json v3.11.3
#
# libcurl is supplied by vcpkg — see build-windows.ps1 for vcpkg setup.

$ErrorActionPreference = "Stop"

$src = Split-Path $PSScriptRoot -Parent
$t   = "$src\third_party"

New-Item -ItemType Directory -Force "$t\sqlite3" | Out-Null

# ---------------------------------------------------------------------------
# SQLite amalgamation
# ---------------------------------------------------------------------------
Write-Host "--- Downloading SQLite amalgamation ---"
$tmp = Join-Path ([System.IO.Path]::GetTempPath()) ([System.Guid]::NewGuid().ToString())
New-Item -ItemType Directory $tmp | Out-Null
Invoke-WebRequest "https://www.sqlite.org/2024/sqlite-amalgamation-3470200.zip" `
    -OutFile "$tmp\sqlite.zip" -UseBasicParsing
Expand-Archive "$tmp\sqlite.zip" -DestinationPath $tmp
Copy-Item "$tmp\sqlite-amalgamation-3470200\sqlite3.c" "$t\sqlite3\"
Copy-Item "$tmp\sqlite-amalgamation-3470200\sqlite3.h" "$t\sqlite3\"
Remove-Item -Recurse -Force $tmp
Write-Host "    OK: third_party\sqlite3\sqlite3.{c,h}"

# ---------------------------------------------------------------------------
# cpp-httplib
# ---------------------------------------------------------------------------
Write-Host "--- Downloading cpp-httplib v0.18.1 ---"
Invoke-WebRequest "https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.18.1/httplib.h" `
    -OutFile "$t\httplib.h" -UseBasicParsing
Write-Host "    OK: third_party\httplib.h"

# ---------------------------------------------------------------------------
# nlohmann/json
# ---------------------------------------------------------------------------
Write-Host "--- Downloading nlohmann/json v3.11.3 ---"
Invoke-WebRequest "https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp" `
    -OutFile "$t\json.hpp" -UseBasicParsing
Write-Host "    OK: third_party\json.hpp"

Write-Host ""
Write-Host "Done."
Write-Host "Next: set up vcpkg + curl (see build-windows.ps1), then run .\scripts\build-windows.ps1"

# SN: 00001
