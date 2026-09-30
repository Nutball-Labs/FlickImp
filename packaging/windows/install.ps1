# install.ps1 -- Install FlickImp for the current Windows user
#
# Run by double-clicking install.cmd in the unzipped folder, or:
#   powershell -ExecutionPolicy Bypass -File .\install.ps1
#
# Installs (no administrator rights needed):
#   %LOCALAPPDATA%\Programs\FlickImp\      flickimp.exe + web\
#   %APPDATA%\flickimp\                    fi_config.json + flickimp.db + flickimp.log (kept on upgrade)
#   Task Scheduler task "FlickImp"         starts FlickImp hidden at logon
#   Start Menu shortcut "FlickImp"         opens the web UI
#
# Re-running install upgrades in place; your config and database are kept.

$ErrorActionPreference = "Stop"

$src      = $PSScriptRoot
$appDir   = Join-Path $env:LOCALAPPDATA "Programs\FlickImp"
$dataDir  = Join-Path $env:APPDATA "flickimp"
$config   = Join-Path $dataDir "fi_config.json"
$logFile  = Join-Path $dataDir "flickimp.log"
$exe      = Join-Path $appDir "flickimp.exe"
$taskName = "FlickImp"
$me       = "$env:USERDOMAIN\$env:USERNAME"
$shortcut = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs\FlickImp.url"

if (-not ((Test-Path "$src\flickimp.exe") -and (Test-Path "$src\web"))) {
    Write-Host "ERROR: run install from the unzipped FlickImp folder (flickimp.exe + web\ not found)."
    exit 1
}

Write-Host "Installing FlickImp for $me"
Write-Host ""

# -- Stop a running instance --------------------------------------------------
if (Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue) {
    Stop-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
}
Get-Process flickimp -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -eq $exe } |
    ForEach-Object { Write-Host "--- Stopping running FlickImp (PID $($_.Id)) ---"; $_ | Stop-Process -Force }
Start-Sleep -Milliseconds 500

# -- Copy program files -------------------------------------------------------
Write-Host "--- Copying files to $appDir ---"
New-Item -ItemType Directory -Force $appDir, $dataDir | Out-Null
if (Test-Path "$appDir\web") { Remove-Item -Recurse -Force "$appDir\web" }
Copy-Item "$src\flickimp.exe" $appDir -Force
Copy-Item "$src\web" "$appDir\web" -Recurse -Force
foreach ($f in "uninstall.ps1", "uninstall.cmd", "README-Windows.txt") {
    if (Test-Path "$src\$f") { Copy-Item "$src\$f" $appDir -Force }
}
# Strip the "downloaded from the internet" mark so SmartScreen doesn't block the exe
Get-ChildItem $appDir -Recurse -File | Unblock-File

# -- Config (first install only) ----------------------------------------------
$port = 8647
if (-not (Test-Path $config)) {
    Write-Host ""
    Write-Host "--- First-time setup ---"
    Write-Host "FlickImp looks up shows and movies via TMDB. Get a free API Read Access"
    Write-Host "Token at https://www.themoviedb.org/settings/api (the long 'eyJ...' one)."
    Write-Host ""
    $token = Read-Host "TMDB API Read Access Token (Enter to skip, edit later)"
    $p = Read-Host "Web UI port [8647]"
    if ($p -match '^\d+$') { $port = [int]$p }
    $cfg = [ordered]@{
        port              = $port
        fi_web_root       = ""
        fi_db_path        = ""
        tmdb_api_key      = ""
        tmdb_bearer_token = $token.Trim()
    }
    # UTF-8 without BOM (Windows PowerShell 5.1's -Encoding UTF8 adds one)
    [IO.File]::WriteAllText($config, ($cfg | ConvertTo-Json), (New-Object Text.UTF8Encoding $false))
    Write-Host "Wrote $config"
} else {
    Write-Host "--- Keeping existing config: $config ---"
    $m = Select-String -Path $config -Pattern '"port"\s*:\s*(\d+)' | Select-Object -First 1
    if ($m) { $port = [int]$m.Matches[0].Groups[1].Value }
}
$url = "http://localhost:$port"

# -- Autostart at logon (Task Scheduler) --------------------------------------
# flickimp.exe is a console program; "conhost --headless" runs it with no
# visible window. Output goes to the log via --log.
Write-Host "--- Registering logon task '$taskName' ---"
$action    = New-ScheduledTaskAction -Execute "$env:SystemRoot\System32\conhost.exe" `
                 -Argument "--headless `"$exe`" --log `"$logFile`"" -WorkingDirectory $appDir
$trigger   = New-ScheduledTaskTrigger -AtLogOn -User $me
$principal = New-ScheduledTaskPrincipal -UserId $me -LogonType Interactive -RunLevel Limited
$settings  = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries `
                 -ExecutionTimeLimit ([TimeSpan]::Zero) -StartWhenAvailable `
                 -RestartCount 3 -RestartInterval (New-TimeSpan -Minutes 1) -MultipleInstances IgnoreNew
Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Principal $principal `
    -Settings $settings -Description "FlickImp watchlist web service ($url)" -Force | Out-Null
Start-ScheduledTask -TaskName $taskName

# -- Start Menu shortcut ------------------------------------------------------
"[InternetShortcut]`r`nURL=$url`r`n" | Set-Content -Path $shortcut -Encoding ASCII

# -- Verify -------------------------------------------------------------------
Write-Host ""
Write-Host -NoNewline "Waiting for FlickImp to answer on $url "
$ok = $false
for ($i = 0; $i -lt 10; $i++) {
    try {
        Invoke-WebRequest "$url/api/about" -UseBasicParsing -TimeoutSec 2 | Out-Null
        $ok = $true
        break
    } catch {
        Write-Host -NoNewline "."
        Start-Sleep -Seconds 1
    }
}
Write-Host ""

if (-not $ok) {
    Write-Host "WARNING: FlickImp did not respond within 10 seconds. Last log lines:"
    if (Test-Path $logFile) { Get-Content $logFile -Tail 20 }
    exit 1
}

Write-Host "-- OK"
Write-Host ""
Write-Host "FlickImp is running and will start automatically when you log in."
Write-Host "  Web UI:    $url   (Start Menu -> FlickImp)"
Write-Host "  Config:    $config"
Write-Host "  Log:       $logFile"
Write-Host "  CLI:       & `"$exe`" --check"
Write-Host "  Uninstall: $appDir\uninstall.cmd"
Write-Host ""
Write-Host "If Windows Firewall asks about flickimp.exe: 'Cancel' keeps it local to this PC;"
Write-Host "'Allow' lets phones/other PCs on your network use it."
$ans = Read-Host "Open the web UI now? [Y/n]"
if ($ans -notmatch '^[Nn]') { Start-Process $url }
exit 0

# SN: 00005
