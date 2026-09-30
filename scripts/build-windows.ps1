# build-windows.ps1 -- Build the FlickImp daemon on Windows (x64, MSVC)
# Run from PowerShell; locates project root relative to this script.
#
# Usage:
#   .\scripts\build-windows.ps1           -- configure (if needed) + build
#   .\scripts\build-windows.ps1 -Clean    -- wipe build dir, reconfigure + build
#
# Build dir: <project-root>\build-win   Binary: build-win\Release\flickimp.exe
#
# One-time setup (see README "Building on Windows"):
#   1. Visual Studio 2022 Build Tools (or VS 2022) with "Desktop development with C++"
#        winget install Microsoft.VisualStudio.2022.BuildTools
#        (then in the VS Installer tick "Desktop development with C++")
#   2. CMake 3.21+ and Git
#        winget install Kitware.CMake Git.Git
#   3. vcpkg
#        git clone https://github.com/microsoft/vcpkg C:\vcpkg
#        C:\vcpkg\bootstrap-vcpkg.bat -disableMetrics
#        [Environment]::SetEnvironmentVariable("VCPKG_ROOT", "C:\vcpkg", "User")
#
# libcurl is declared in vcpkg.json (manifest mode) -- vcpkg builds it
# automatically on first configure (takes a few minutes, cached afterwards).
# If scripts are blocked:  Set-ExecutionPolicy -Scope CurrentUser RemoteSigned

param([switch]$Clean)

$ErrorActionPreference = "Stop"

$src   = Split-Path $PSScriptRoot -Parent
$build = "$src\build-win"

# -- Version banner -----------------------------------------------------------
$vhp = Get-Content "$src\lib\version.hpp" -Raw
function Get-VField($name) {
    if ($vhp -match "#define\s+$name\s+`"?([^`"\s]*)`"?") { return $Matches[1] } else { return "" }
}
$version = "{0}.{1}.{2}{3}" -f (Get-VField VERSION_MAJOR), (Get-VField VERSION_MINOR),
                               (Get-VField VERSION_PATCH), (Get-VField VERSION_SUFFIX)
$inner  = "  Building FlickImp version $version  --  Windows x64  "
$border = "*" * ($inner.Length + 2)
Write-Host $border
Write-Host "*$inner*"
Write-Host $border
Write-Host ""

# -- Preflight ----------------------------------------------------------------
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "ERROR: cmake not found on PATH.  winget install Kitware.CMake  (then open a new terminal)"
    exit 1
}

$vcpkgRoot = if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { "C:\vcpkg" }
$toolchain = "$vcpkgRoot\scripts\buildsystems\vcpkg.cmake"
if (-not (Test-Path $toolchain)) {
    Write-Host "ERROR: vcpkg not found at $vcpkgRoot"
    Write-Host "Install it (see top of this script) or set VCPKG_ROOT to your vcpkg directory."
    exit 1
}

$tp = "$src\third_party"
if (-not ((Test-Path "$tp\sqlite3\sqlite3.c") -and (Test-Path "$tp\httplib.h") -and (Test-Path "$tp\json.hpp"))) {
    Write-Host "--- third_party\ incomplete -- fetching dependencies ---"
    & "$PSScriptRoot\get-deps.ps1"
    Write-Host ""
}

# -- Configure ----------------------------------------------------------------
if ($Clean -and (Test-Path $build)) {
    Write-Host "--- Cleaning build directory ---"
    Remove-Item -Recurse -Force $build
}

function Invoke-Configure {
    Write-Host "    vcpkg: $vcpkgRoot  (first run builds static curl -- be patient)"
    cmake -S $src -B $build -A x64 `
        -DVCPKG_TARGET_TRIPLET=x64-windows-static `
        -DCMAKE_TOOLCHAIN_FILE="$toolchain"
    if ($LASTEXITCODE -ne 0) { Write-Host "Configure failed"; exit 1 }
    Write-Host ""
}

$cache = "$build\CMakeCache.txt"
if (-not (Test-Path $cache)) {
    Write-Host "--- Configuring (build-win) ---"
    Invoke-Configure
} else {
    $cached = (Select-String -Path $cache -Pattern '^FLICKIMP_VERSION:STRING=(.*)$').Matches |
              ForEach-Object { $_.Groups[1].Value } | Select-Object -First 1
    if ($cached -ne $version) {
        Write-Host "--- Version changed ($cached -> $version), reconfiguring ---"
        Invoke-Configure
    }
}

# -- Build --------------------------------------------------------------------
Write-Host "--- Building ---"
cmake --build $build --config Release --parallel
if ($LASTEXITCODE -ne 0) { Write-Host "Build failed"; exit 1 }

# web\ is copied next to the binary by a POST_BUILD step in CMakeLists.txt
Write-Host ""
Write-Host "Done. Binary: $build\Release\flickimp.exe"
Write-Host "      Try:    $build\Release\flickimp.exe --version"
exit 0

# SN: 00005
