# uninstall.ps1 -- Remove FlickImp for the current Windows user
#
# Run by double-clicking uninstall.cmd, or:
#   powershell -ExecutionPolicy Bypass -File .\uninstall.ps1          (keeps config + database)
#   powershell -ExecutionPolicy Bypass -File .\uninstall.ps1 -Purge   (deletes them too)

param([switch]$Purge)

$ErrorActionPreference = "Stop"

$appDir   = Join-Path $env:LOCALAPPDATA "Programs\FlickImp"
$dataDir  = Join-Path $env:APPDATA "flickimp"
$exe      = Join-Path $appDir "flickimp.exe"
$taskName = "FlickImp"
$shortcut = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs\FlickImp.url"

Write-Host "--- Stopping FlickImp ---"
if (Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue) {
    Stop-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
    Unregister-ScheduledTask -TaskName $taskName -Confirm:$false
}
Get-Process flickimp -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -eq $exe } | Stop-Process -Force
Start-Sleep -Milliseconds 500

Remove-Item $shortcut -ErrorAction SilentlyContinue

# This script may be running from $appDir -- step out before deleting it
Set-Location $env:TEMP
Write-Host "--- Removing $appDir ---"
Remove-Item -Recurse -Force $appDir -ErrorAction SilentlyContinue

if ($Purge) {
    $ans = Read-Host "Delete your FlickImp database and config in $dataDir? This cannot be undone. [y/N]"
    if ($ans -match '^[Yy]') {
        Remove-Item -Recurse -Force $dataDir
        Write-Host "Config, database and log deleted."
    } else {
        Write-Host "Kept $dataDir"
    }
} else {
    Write-Host "Kept config and database in: $dataDir"
    Write-Host "(run uninstall.ps1 -Purge to delete them)"
}

Write-Host "FlickImp uninstalled."
exit 0

# SN: 00005
