# Go-windows.ps1 — Full Windows release pipeline: build + package with sleep/hibernate inhibit
# Prevents system from sleeping or hibernating during long compile and packaging runs.
#
# Usage:
#   .\scripts\Go-windows.ps1              -- configure + build + package
#   .\scripts\Go-windows.ps1 -Clean       -- wipe build dir first, then build + package
#
# -Clean is forwarded to build-windows.ps1.
#
# The sleep inhibit is held inside a try/finally so it is always released —
# on normal exit, on build/package failure, and on Ctrl+C.

param(
    [switch]$Clean
)

$scripts = $PSScriptRoot

# ── Sleep/hibernate inhibitor ──────────────────────────────────────────────────
# SetThreadExecutionState signals Windows that the system is in use.
# ES_CONTINUOUS (0x80000000) makes the state persistent until explicitly cleared.
# ES_SYSTEM_REQUIRED (0x00000001) prevents idle sleep and hibernate.
# Clearing is done by calling with ES_CONTINUOUS alone (no ES_SYSTEM_REQUIRED).
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class SleepGuard {
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern uint SetThreadExecutionState(uint esFlags);
    public const uint ES_CONTINUOUS      = 0x80000000u;
    public const uint ES_SYSTEM_REQUIRED = 0x00000001u;
    public const uint INHIBIT            = ES_CONTINUOUS | ES_SYSTEM_REQUIRED;
    public const uint RELEASE            = ES_CONTINUOUS;
}
'@

function Enable-SleepInhibit {
    $result = [SleepGuard]::SetThreadExecutionState([SleepGuard]::INHIBIT)
    if ($result -eq 0) {
        Write-Host "[Go] WARNING: SetThreadExecutionState failed -- system may still sleep."
    } else {
        Write-Host "[Go] Sleep/hibernate inhibited for build duration."
    }
}

function Disable-SleepInhibit {
    [void][SleepGuard]::SetThreadExecutionState([SleepGuard]::RELEASE)
    Write-Host "[Go] Sleep/hibernate inhibit released."
}

# ── Pipeline ───────────────────────────────────────────────────────────────────
Enable-SleepInhibit
$pipelineExit = 0

try {
    # Phase 1: Build
    Write-Host ""
    Write-Host "[Go] === Phase 1: Build ==="

    $buildArgs = @{}
    if ($Clean) { $buildArgs["Clean"] = $true }

    & "$scripts\build-windows.ps1" @buildArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Host ""
        Write-Host "[Go] Build failed -- aborting pipeline."
        $pipelineExit = 1
        return
    }

    # Phase 2: Package
    Write-Host ""
    Write-Host "[Go] === Phase 2: Package ==="

    & "$scripts\package-windows.ps1"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[Go] Packaging failed."
        $pipelineExit = 1
        return
    }

    Write-Host ""
    Write-Host "[Go] Pipeline complete."

} finally {
    Disable-SleepInhibit
}

exit $pipelineExit

# SN: 00001
