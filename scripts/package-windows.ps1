# package-windows.ps1 -- Produce the Windows ZIP package via CPack
# Assumes build-windows.ps1 has already been run successfully.
#
# Usage:
#   .\scripts\package-windows.ps1
#
# Output: packages\ at project root
#   flickimp-X.Y.Z-win64.zip
#
# The zip holds flickimp.exe, web\, install.cmd/.ps1, uninstall.cmd/.ps1 and
# README-Windows.txt. The end user unzips it and double-clicks install.cmd
# (per-user install, Task Scheduler autostart at logon -- no admin needed).

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
$inner  = "  Packaging FlickImp version $version  --  Windows x64  "
$border = "*" * ($inner.Length + 2)
Write-Host $border
Write-Host "*$inner*"
Write-Host $border
Write-Host ""

$cache = "$build\CMakeCache.txt"
if (-not (Test-Path $cache)) {
    Write-Host "ERROR: build-win not found -- run .\scripts\build-windows.ps1 first."
    exit 1
}
$cached = (Select-String -Path $cache -Pattern '^FLICKIMP_VERSION:STRING=(.*)$').Matches |
          ForEach-Object { $_.Groups[1].Value } | Select-Object -First 1
if ($cached -ne $version) {
    Write-Host "ERROR: build-win is v$cached but version.hpp is v$version -- rebuild first."
    exit 1
}
if (-not (Test-Path "$build\Release\flickimp.exe")) {
    Write-Host "ERROR: build-win\Release\flickimp.exe missing -- run .\scripts\build-windows.ps1 first."
    exit 1
}

$null = New-Item -ItemType Directory -Force "$src\packages"

# Use the cpack next to cmake -- Chocolatey ships an unrelated cpack.exe that
# can shadow it on PATH.
$cpack = Join-Path (Split-Path (Get-Command cmake).Source) "cpack.exe"

Write-Host "=== Packaging ZIP (CPack) ==="
Push-Location $build
try {
    & $cpack -C Release -G ZIP
    $rc = $LASTEXITCODE
} finally {
    Pop-Location
}
if ($rc -ne 0) { Write-Host "CPack failed"; exit 1 }

Write-Host ""
Write-Host "Packages:"
Get-ChildItem "$src\packages\flickimp-$version-win64.zip" -ErrorAction SilentlyContinue |
    Select-Object Name, @{N="Size";E={"{0:N0} KB" -f ($_.Length/1KB)}} | Format-Table -AutoSize

Write-Host "To publish, upload to the GitHub release (after it exists):"
Write-Host "  gh release upload v$version packages\flickimp-$version-win64.zip --repo Nutball-Labs/FlickImp --clobber"
exit 0

# SN: 00005
