# Asterun batch A acceptance kit: run the probe self-test and every Process
# Monitor scenario of one workspace in a fixed order (controls first), each in
# its own Windows PowerShell process, and write <Logs>\<run>\summary.txt.
#
# Expected: the probe self-test passes, every baseline control is
# CONTROL-VALID and every candidate scenario is PASS. Anything else is listed
# as a deviation and the script exits 1. The individual reports
# (<Logs>\<run>\*-report.txt) hold the evidence.
#
# Usage (elevated):
#   powershell -ExecutionPolicy Bypass -File C:\Acceptance\kit\scripts\Run-Acceptance.ps1 `
#     -Work C:\AsterunWork\run-YYYYMMDD-HHMMSS -Logs C:\Acceptance\logs
param(
    [Parameter(Mandatory = $true)][string]$Work,
    [Parameter(Mandatory = $true)][string]$Logs,
    [string[]]$Only = @()
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
$Work = (Resolve-Path -LiteralPath $Work).Path
$logDir = Join-Path $Logs (Split-Path -Leaf $Work)
if (-not (Test-Path -LiteralPath $logDir)) { throw "log folder of this run not found: $logDir" }
$powershell = "$env:WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe"
$kit = Split-Path -Parent $PSScriptRoot

$steps = @(
    @{ Name = 'probe-self-test'; Expected = 0 },
    @{ Name = 'control-main'; Build = 'baseline'; Scenario = 'main'; Expected = 0 },
    @{ Name = 'control-update-noargs'; Build = 'baseline'; Scenario = 'update-noargs'; Expected = 0 },
    @{ Name = 'control-uninstall-launcher'; Build = 'baseline'; Scenario = 'uninstall-launcher'; Expected = 0 },
    @{ Name = 'candidate-main'; Build = 'candidate'; Scenario = 'main'; Expected = 0 },
    @{ Name = 'candidate-update-noargs'; Build = 'candidate'; Scenario = 'update-noargs'; Expected = 0 },
    @{ Name = 'candidate-update-chain'; Build = 'candidate'; Scenario = 'update-chain'; Expected = 0 },
    @{ Name = 'candidate-service-command'; Build = 'candidate'; Scenario = 'service-command'; Expected = 0 },
    @{ Name = 'candidate-uninstall-launcher'; Build = 'candidate'; Scenario = 'uninstall-launcher'; Expected = 0 },
    @{ Name = 'candidate-uninstall-worker'; Build = 'candidate'; Scenario = 'uninstall-worker'; Expected = 0 }
)
$names = @{ 0 = 'PASS / CONTROL-VALID'; 1 = 'ERROR'; 10 = 'REVIEW'; 11 = 'UNVERIFIED'; 20 = 'FAIL'; 30 = 'INVALID'; 31 = 'CONTROL-INVALID' }
$summary = New-Object System.Collections.Generic.List[string]
$summary.Add("time=$(Get-Date -Format o) work=$Work")
$deviations = 0
foreach ($step in $steps) {
    if ($Only.Count -and $Only -notcontains $step.Name) { continue }
    $output = Join-Path $logDir "$($step.Name).console.txt"
    if ($step.Name -eq 'probe-self-test') {
        & $powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $kit 'probe-dll\Test-Probe.ps1') `
            -Dll (Join-Path $Work 'probe\asterun_dll_probe.dll') *> $output
    } else {
        & $powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'Invoke-Scenario.ps1') `
            -Work $Work -Logs $Logs -Scenario $step.Scenario -Build $step.Build *> $output
    }
    $code = $LASTEXITCODE
    $result = if ($names.ContainsKey($code)) { $names[$code] } else { "exit $code" }
    $ok = $code -eq $step.Expected
    if (-not $ok) { $deviations++ }
    $line = '{0,-30} {1,-22} {2}' -f $step.Name, $result, $(if ($ok) { 'as expected' } else { 'DEVIATION' })
    $summary.Add($line)
    Write-Output $line
}
$summary.Add($(if ($deviations) { "RESULT: $deviations deviation(s)" } else { 'RESULT: all steps as expected' }))
[System.IO.File]::WriteAllLines((Join-Path $logDir 'summary.txt'), $summary, (New-Object System.Text.UTF8Encoding($false)))
Write-Output $summary[-1]
exit $(if ($deviations) { 1 } else { 0 })
