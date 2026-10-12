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
    # After the F-1 fix rstrtmgr.dll is delay-loaded, so a clean launcher/worker
    # never loads it and the planted rstrtmgr.dll is not loaded either: the
    # honest result is UNVERIFIED (nothing resolved from System32 to verify).
    # The forced-RM regression below is where rstrtmgr.dll is actually loaded.
    @{ Name = 'candidate-uninstall-launcher'; Build = 'candidate'; Scenario = 'uninstall-launcher'; Expected = 11 },
    @{ Name = 'candidate-uninstall-worker'; Build = 'candidate'; Scenario = 'uninstall-worker'; Expected = 11 },
    # --- F-1 regression: Uninstall.exe must not load ncrypt.dll from its own
    # directory via the Restart Manager chain. One name per scenario so one
    # failing load cannot mask another (ncrypt first; ntasn1 recorded below).
    # Baseline controls (v1.0.5) reproduce the load before the fix: CONTROL-VALID.
    @{ Name = 'control-uninstall-launcher-ncrypt'; Build = 'baseline'; Scenario = 'uninstall-launcher'; Extra = 'ncrypt.dll'; Tag = 'ncrypt'; NoDefault = $true; Expected = 0 },
    @{ Name = 'control-uninstall-worker-ncrypt'; Build = 'baseline'; Scenario = 'uninstall-worker'; Extra = 'ncrypt.dll'; Tag = 'ncrypt'; NoDefault = $true; Expected = 0 },
    # The decisive regression: the worker is forced to run Restart Manager (a
    # locked VERSION), rstrtmgr.dll MUST load from System32 (proves the path
    # ran), and the planted ncrypt.dll MUST NOT load from the worker directory.
    # A load of the plant is FAIL; a capture where Restart Manager never ran is
    # INVALID; the fixed build is PASS.
    @{ Name = 'candidate-uninstall-rm-worker-ncrypt'; Build = 'candidate'; Scenario = 'uninstall-worker'; Extra = 'ncrypt.dll'; Tag = 'rm-ncrypt'; ForceRm = $true; NoDefault = $true; Expected = 0 },
    # Supporting: the launcher and a clean worker never reach Restart Manager
    # after the fix, so the plant is not loaded and the result is UNVERIFIED
    # (honest: the path that would load it did not run here - the forced-RM
    # scenario above is where it runs). A load of the plant would still be FAIL.
    @{ Name = 'candidate-uninstall-launcher-ncrypt'; Build = 'candidate'; Scenario = 'uninstall-launcher'; Extra = 'ncrypt.dll'; Tag = 'ncrypt'; NoDefault = $true; Expected = 11 },
    @{ Name = 'candidate-uninstall-worker-ncrypt'; Build = 'candidate'; Scenario = 'uninstall-worker'; Extra = 'ncrypt.dll'; Tag = 'ncrypt'; NoDefault = $true; Expected = 11 },
    # ntasn1.dll (ncrypt's own delay import) recorded separately; it may never
    # load even on baseline, so it is recorded, not a pass gate.
    @{ Name = 'investigate-uninstall-worker-ntasn1'; Build = 'candidate'; Scenario = 'uninstall-worker'; Extra = 'ntasn1.dll'; Tag = 'ntasn1'; ForceRm = $true; NoDefault = $true; Expected = 'record' },
    # Main-process runtime ncrypt.dll load is a separate, documented design
    # boundary (the launcher UI hosts third-party shell extensions); recorded,
    # not fixed in this change.
    @{ Name = 'investigate-main-ncrypt'; Build = 'candidate'; Scenario = 'main'; Extra = 'ncrypt.dll'; Tag = 'ncrypt'; NoDefault = $true; Expected = 'record' }
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
        $extra = @()
        if ($step.ContainsKey('Extra')) { $extra = @('-ExtraPlant', $step.Extra, '-Tag', $step.Tag) }
        if ($step.ContainsKey('ForceRm')) { $extra += '-ForceRestartManager' }
        if ($step.ContainsKey('NoDefault')) { $extra += '-NoDefaultPlant' }
        & $powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'Invoke-Scenario.ps1') `
            -Work $Work -Logs $Logs -Scenario $step.Scenario -Build $step.Build @extra *> $output
    }
    $code = $LASTEXITCODE
    $result = if ($names.ContainsKey($code)) { $names[$code] } else { "exit $code" }
    if ($step.Expected -is [string]) {  # 'record': investigation only
        $state = 'recorded (investigation)'
    } else {
        $ok = $code -eq $step.Expected
        if (-not $ok) { $deviations++ }
        $state = if ($ok) { 'as expected' } else { 'DEVIATION' }
    }
    $line = '{0,-40} {1,-22} {2}' -f $step.Name, $result, $state
    $summary.Add($line)
    Write-Output $line
}
$summary.Add($(if ($deviations) { "RESULT: $deviations deviation(s)" } else { 'RESULT: all steps as expected' }))
[System.IO.File]::WriteAllLines((Join-Path $logDir 'summary.txt'), $summary, (New-Object System.Text.UTF8Encoding($false)))
Write-Output $summary[-1]
exit $(if ($deviations) { 1 } else { 0 })
