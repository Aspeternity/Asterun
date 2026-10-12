# Asterun batch A acceptance kit (U-4e): hold an exclusive handle on the test
# installation's VERSION file so the uninstall worker cannot delete it, exhausts
# its retries and calls Restart Manager - the same lock the CI regression uses
# ([System.IO.File]::Open(..., FileShare.None)). The launcher and the worker
# only check VERSION with GetFileAttributes, which this handle does not block,
# so the uninstall starts normally and only the delete of VERSION fails.
#
# Usage (a separate, non-elevated Windows PowerShell window; keep it open):
#   powershell -ExecutionPolicy Bypass -File Hold-FileLock.ps1 -Path "C:\Program Files\Asterun\VERSION"
# Release: press Enter in that window (closing the window also releases it).
# -HoldSeconds N holds for N seconds instead of waiting for Enter (used by CI).
param([Parameter(Mandatory = $true)][string]$Path, [int]$HoldSeconds = 0)

$ErrorActionPreference = 'Stop'
if ((Split-Path -Leaf $Path) -ne 'VERSION') { throw 'this helper only locks the installation VERSION file' }
if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "file not found: $Path" }
$handle = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::None)
try {
    Write-Output "LOCKED $Path (exclusive, no sharing) by PowerShell pid=$PID at $(Get-Date -Format o)"
    Write-Output 'Start the uninstall now. When the worker has reported its error and you have stopped Process Monitor, press Enter here to release.'
    if ($HoldSeconds -gt 0) { Start-Sleep -Seconds $HoldSeconds } else { [void](Read-Host 'Press Enter to release the lock') }
} finally {
    $handle.Close()
    Write-Output "RELEASED $Path at $(Get-Date -Format o)"
}
