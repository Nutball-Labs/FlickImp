# package-windows.ps1 — Produce ZIP and MSI packages for Windows via CPack and WiX
# Assumes build-windows.ps1 has already been run successfully.
#
# Usage:
#   .\scripts\package-windows.ps1
#
# Output: packages\ at project root
#   flickimp-X.Y.Z-win64.zip
#   flickimp-X.Y.Z-win64.msi   (only if WiX 6 is installed)
#
# Optional — MSI installer via WiX 6:
#   dotnet tool install --global wix
#   https://wixtoolset.org/releases/

$ErrorActionPreference = "Stop"

$src   = Split-Path $PSScriptRoot -Parent
$build = "$src\build-win"
$wix   = "$env:USERPROFILE\.dotnet\tools\wix.exe"

$null = New-Item -ItemType Directory -Force "$src\packages"

if (-not (Test-Path $build)) {
    Write-Host "ERROR: build-win not found — run .\scripts\build-windows.ps1 first."
    exit 1
}

# ---------------------------------------------------------------------------
# CPack ZIP
# ---------------------------------------------------------------------------
Write-Host "=== Package ZIP (CPack) ==="
Push-Location $build
cpack -C Release
$rc = $LASTEXITCODE
Pop-Location
if ($rc -ne 0) { Write-Host "CPack failed"; exit 1 }

# ---------------------------------------------------------------------------
# WiX MSI (optional)
# ---------------------------------------------------------------------------
if (Test-Path $wix) {
    Write-Host ""
    Write-Host "=== Package MSI (WiX 6) ==="
    cmake --build $build --config Release --target msi
    if ($LASTEXITCODE -ne 0) { Write-Host "MSI build failed"; exit 1 }
} else {
    Write-Host ""
    Write-Host "NOTE: wix.exe not found — MSI skipped."
    Write-Host "      Install: dotnet tool install --global wix"
}

Write-Host ""
Write-Host "Packages:"
Get-ChildItem "$src\packages\flickimp-*-win64.*" -ErrorAction SilentlyContinue |
    Where-Object { $_.Extension -ne ".wixpdb" } |
    Select-Object Name, @{N="Size";E={"{0:N0} KB" -f ($_.Length/1KB)}}

# SN: 00001
