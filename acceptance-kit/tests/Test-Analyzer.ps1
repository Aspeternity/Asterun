# Self-test of Analyze-Procmon.ps1 on small synthetic Process Monitor exports
# (tests\analyzer-fixtures). Each case states the verdict the rules require;
# the analyzer runs in a child process of the same PowerShell edition.
# Usage: powershell -ExecutionPolicy Bypass -File Test-Analyzer.ps1 -Out C:\Acceptance\logs\analyzer-selftest
param([Parameter(Mandatory = $true)][string]$Out)

$ErrorActionPreference = 'Stop'
$fixtures = Join-Path $PSScriptRoot 'analyzer-fixtures'
$analyzer = Join-Path (Split-Path -Parent $PSScriptRoot) 'scripts\Analyze-Procmon.ps1'
$shell = (Get-Process -Id $PID).Path
if (Test-Path -LiteralPath $Out) { throw "refusing to reuse $Out" }
New-Item -ItemType Directory -Path $Out | Out-Null
$app = 'C:\W\app'; $temp = 'C:\Users\u\AppData\Local\Temp'
$ctlMain = Join-Path $Out 'control-main.txt'
$ctlUpdate = Join-Path $fixtures 'control-update.txt'
$probe = Join-Path $fixtures 'probe.txt'
$plantApp = 'C:\W\app\version.dll'
$plantWorker = "$temp\Asterun-Elevated.kit-1\rstrtmgr.dll"
$plantChain = "$temp\asterun-updater-it-1\Asterun-Elevated.updater-it-portable\bcrypt.dll;$temp\asterun-updater-it-1\portable-install\version.dll"

# name, expected exit code, what the case shows, arguments
$cases = @(
    @('control-main', 0, 'old build loads planted version.dll from its directory: CONTROL-VALID',
      @('-Scenario', 'main', '-Role', 'control', '-PlantedPath', $plantApp)),
    @('control-bad', 31, 'control without a load from the watched directory: CONTROL-INVALID',
      @('-Scenario', 'main', '-Role', 'control', '-PlantedPath', $plantApp)),
    @('cand-main', 0, 'planted name resolved to System32, valid control, functional PASS: PASS',
      @('-Scenario', 'main', '-PlantedPath', $plantApp, '-ControlReport', $ctlMain, '-FunctionalResult', 'PASS')),
    @('cand-main', 11, 'same capture without a control report: UNVERIFIED',
      @('-Scenario', 'main', '-PlantedPath', $plantApp, '-FunctionalResult', 'PASS')),
    @('cand-main', 11, 'same capture, functional check not run: UNVERIFIED',
      @('-Scenario', 'main', '-PlantedPath', $plantApp, '-ControlReport', $ctlMain)),
    @('cand-main', 20, 'same capture, functional check failed: FAIL',
      @('-Scenario', 'main', '-PlantedPath', $plantApp, '-ControlReport', $ctlMain, '-FunctionalResult', 'FAIL')),
    @('cand-main-crt', 20, 'main mode loads vcruntime140.dll from the app directory: FAIL',
      @('-Scenario', 'main', '-PlantedPath', $plantApp, '-ControlReport', $ctlMain, '-FunctionalResult', 'PASS')),
    @('svc-only-plain', 30, 'only an ordinary Asterun.exe, no service-command process: INVALID',
      @('-Scenario', 'service-command', '-PlantedPath', $plantApp, '-ControlReport', $ctlMain, '-FunctionalResult', 'PASS')),
    @('svc-ok', 0, 'service-command PID judged alone; a load by the ordinary PID is ignored: PASS',
      @('-Scenario', 'service-command', '-PlantedPath', $plantApp, '-ControlReport', $ctlMain, '-FunctionalResult', 'PASS')),
    @('svc-search', 10, 'hardened PID looked up a DLL in the app directory (NAME NOT FOUND): REVIEW, not FAIL',
      @('-Scenario', 'service-command', '-PlantedPath', $plantApp, '-ControlReport', $ctlMain, '-FunctionalResult', 'PASS')),
    @('svc-open', 20, 'hardened PID opened a DLL in the app directory for execute: FAIL',
      @('-Scenario', 'service-command', '-PlantedPath', $plantApp, '-ControlReport', $ctlMain, '-FunctionalResult', 'PASS')),
    @('svc-nostart', 30, 'target without its Process Start event (no command line): INVALID',
      @('-Scenario', 'service-command', '-PlantedPath', $plantApp, '-ControlReport', $ctlMain, '-FunctionalResult', 'PASS')),
    @('svc-ok', 30, 'probe log shows a load the capture does not have: INVALID',
      @('-Scenario', 'service-command', '-PlantedPath', $plantApp, '-ControlReport', $ctlMain, '-FunctionalResult', 'PASS', '-ProbeLog', $probe)),
    @('worker', 11, 'uninstall worker resolved rstrtmgr.dll to System32 but no control for that name: UNVERIFIED',
      @('-Scenario', 'uninstall-worker', '-PlantedPath', $plantWorker, '-FunctionalResult', 'PASS')),
    @('chain', 0, 'update chain: both targets present, both names verified: PASS',
      @('-Scenario', 'update-chain', '-PlantedPath', $plantChain, '-ControlReport', "$ctlMain;$ctlUpdate", '-FunctionalResult', 'PASS')),
    @('chain-neg', 0, 'update chain with an extra Update.exe PID without a plant (negative case): still PASS',
      @('-Scenario', 'update-chain', '-PlantedPath', $plantChain, '-ControlReport', "$ctlMain;$ctlUpdate", '-FunctionalResult', 'PASS')),
    @('chain-neg-load', 20, 'update chain where the unplanted Update.exe PID loads a DLL from its directory: FAIL',
      @('-Scenario', 'update-chain', '-PlantedPath', $plantChain, '-ControlReport', "$ctlMain;$ctlUpdate", '-FunctionalResult', 'PASS')),
    @('cand-main', 11, 'plant not next to the target image (wrong directory): UNVERIFIED',
      @('-Scenario', 'main', '-PlantedPath', 'C:\W\elsewhere\version.dll', '-ControlReport', $ctlMain, '-FunctionalResult', 'PASS')),
    @('chain-missing-asterun', 30, 'update chain without the restarted Asterun.exe: INVALID',
      @('-Scenario', 'update-chain', '-PlantedPath', $plantChain, '-ControlReport', "$ctlMain;$ctlUpdate", '-FunctionalResult', 'PASS'))
)

$results = New-Object System.Collections.Generic.List[string]
$bad = 0
$index = 0
foreach ($case in $cases) {
    ++$index
    $report = if ($case[0] -eq 'control-main') { $ctlMain } else { Join-Path $Out ('{0:D2}-{1}.txt' -f $index, $case[0]) }
    $arguments = @('-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File', $analyzer,
        '-Csv', (Join-Path $fixtures "$($case[0]).csv"), '-AppDir', $app, '-TempDir', $temp, '-Out', $report) + $case[3]
    & $shell @arguments > "$report.console"
    $code = $LASTEXITCODE
    $ok = $code -eq $case[1]
    if (-not $ok) { $bad++ }
    $results.Add(('{0} {1,-22} expected={2,-3} got={3,-3} {4}' -f $(if ($ok) { 'ok  ' } else { 'FAIL' }), $case[0], $case[1], $code, $case[2]))
}
$results.Add("RESULT: $(if ($bad) { "$bad of $($cases.Count) cases deviate" } else { "all $($cases.Count) cases as expected" })")
[System.IO.File]::WriteAllLines((Join-Path $Out 'summary.txt'), $results)
$results | ForEach-Object { Write-Output $_ }
exit $(if ($bad) { 1 } else { 0 })
