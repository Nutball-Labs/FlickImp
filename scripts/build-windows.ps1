# build-windows.ps1 — Build FlickImp daemon on Windows
# Run from PowerShell; locates project root relative to this script.
#
# Usage:
#   .\scripts\build-windows.ps1           -- configure + build
#   .\scripts\build-windows.ps1 -Clean    -- wipe build dir and reconfigure
#
# Build dir: <project-root>\build-win
#
# Requires: CMake (on PATH), a C++17 compiler (MSVC via Visual Studio, or MinGW),
#           and vcpkg with the static curl triplet installed.
#
# One-time vcpkg setup (if you don't have it):
#   git clone https://github.com/microsoft/vcpkg C:\vcpkg
#   C:\vcpkg\bootstrap-vcpkg.bat
#   C:\vcpkg\vcpkg install curl:x64-windows-static
#
# Set VCPKG_ROOT to your vcpkg directory (or edit $vcpkgRoot below):
#   $env:VCPKG_ROOT = "C:\vcpkg"

param([switch]$Clean)

$ErrorActionPreference = "Stop"

$vcpkgRoot     = if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { "C:\vcpkg" }
$toolchainFile = "$vcpkgRoot\scripts\buildsystems\vcpkg.cmake"
$src           = Split-Path $PSScriptRoot -Parent
$build         = "$src\build-win"

if (-not (Test-Path $toolchainFile)) {
    Write-Host "ERROR: vcpkg not found at $vcpkgRoot"
    Write-Host "Set the VCPKG_ROOT environment variable or edit `$vcpkgRoot in this script."
    Write-Host "See the top of this script for setup instructions."
    exit 1
}

if ($Clean -and (Test-Path $build)) {
    Write-Host "--- Cleaning build directory ---"
    Remove-Item -Recurse -Force $build
}

if (-not (Test-Path $build)) {
    Write-Host "--- Configuring (build-win) ---"
    Write-Host "    vcpkg: $vcpkgRoot"
    cmake -S $src -B $build `
        -DCMAKE_BUILD_TYPE=Release `
        -DVCPKG_TARGET_TRIPLET=x64-windows-static `
        -DCMAKE_TOOLCHAIN_FILE=$toolchainFile
    if ($LASTEXITCODE -ne 0) { Write-Host "Configure failed"; exit 1 }
    Write-Host ""
}

Write-Host "--- Building ---"
cmake --build $build --config Release
if ($LASTEXITCODE -ne 0) { Write-Host "Build failed"; exit 1 }

# Copy web assets next to the binary so the daemon can find them in-place
$webSrc = "$src\web"
$webDst = "$build\Release\web"
if ((Test-Path $webSrc) -and -not (Test-Path $webDst)) {
    Copy-Item -Recurse $webSrc $webDst
}

Write-Host ""
Write-Host "Done. Binary: $build\Release\flickimp.exe"
Write-Host "      Web:    $build\Release\web\"

# SN: 00001
