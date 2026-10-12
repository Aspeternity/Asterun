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
$plantRmWorker = "$temp\Asterun-Elevated.kit-rm\ncrypt.dll"
$ctlNcrypt = Join-Path $Out 'control-ncrypt.txt'
Set-Content -LiteralPath $ctlNcrypt -Value 'CONTROL-VALID name=ncrypt.dll pid=1'

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
    @('svc-search', 10, 'hardened PID: loader probe (NAME NOT FOUND in its image directory): REVIEW, not FAIL',
      @('-Scenario', 'service-command', '-PlantedPath', $plantApp, '-ControlReport', $ctlMain, '-FunctionalResult', 'PASS')),
    @('svc-probe-noexec', 10, 'hardened PID: lookup in its own image directory without execute access (loader probe): REVIEW',
      @('-Scenario', 'service-command', '-PlantedPath', $plantApp, '-ControlReport', $ctlMain, '-FunctionalResult', 'PASS')),
    @('svc-filecheck', 0, 'hardened PID: missing DLL checked in another watched directory (file check by the program, not a loader probe): PASS',
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
      @('-Scenario', 'update-chain', '-PlantedPath', $plantChain, '-ControlReport', "$ctlMain;$ctlUpdate", '-FunctionalResult', 'PASS')),
    # F-1 regression (fixed build): Restart Manager ran (rstrtmgr.dll from
    # System32), planted ncrypt.dll not loaded, ncrypt.dll from System32: PASS.
    @('rm-worker-fixed', 0, 'F-1 fixed: RM ran from System32, planted ncrypt not loaded: PASS',
      @('-Scenario', 'uninstall-worker', '-PlantedPath', $plantRmWorker, '-ControlReport', $ctlNcrypt, '-RequireSystemLoad', 'rstrtmgr.dll', '-FunctionalResult', 'PASS')),
    # Vulnerable build: planted ncrypt.dll loaded from the worker directory: FAIL.
    @('rm-worker-vuln', 20, 'F-1 vulnerable: planted ncrypt loaded from the worker directory: FAIL',
      @('-Scenario', 'uninstall-worker', '-PlantedPath', $plantRmWorker, '-ControlReport', $ctlNcrypt, '-RequireSystemLoad', 'rstrtmgr.dll', '-FunctionalResult', 'PASS')),
    # Restart Manager never ran (no rstrtmgr load): the path under test did not
    # execute, so a clean capture is INVALID, not a pass.
    @('rm-worker-norm', 30, 'required System32 load of rstrtmgr.dll missing (RM path did not run): INVALID',
      @('-Scenario', 'uninstall-worker', '-PlantedPath', $plantRmWorker, '-ControlReport', $ctlNcrypt, '-RequireSystemLoad', 'rstrtmgr.dll', '-FunctionalResult', 'PASS')),
    # Strict System32: a load from elsewhere under %WINDIR% is not "from System32".
    @('rm-worker-winother', 10, 'planted name resolved under %WINDIR% but not System32: not verified; required rstrtmgr load not from System32: REVIEW',
      @('-Scenario', 'uninstall-worker', '-PlantedPath', $plantRmWorker, '-ControlReport', $ctlNcrypt, '-RequireSystemLoad', 'rstrtmgr.dll', '-FunctionalResult', 'PASS'),
      @('required load of rstrtmgr.dll : loaded but NOT from System32', 'SYSTEM32=False WINDOWS-OTHER=True')),
    # Observation role (real un-planted capture): per-DLL paths, never PASS.
    @('obs-system32', 12, 'observation: rstrtmgr/ncrypt Load Image paths reported as SYSTEM32, worker image dir reported: OBSERVED',
      @('-Scenario', 'uninstall-worker', '-Role', 'observation', '-ReportDll', 'rstrtmgr.dll,ncrypt.dll'),
      @('DLL-LOAD pid=700 name=rstrtmgr.dll class=SYSTEM32 path=C:\Windows\System32\rstrtmgr.dll', 'DLL-LOAD pid=700 name=ncrypt.dll class=SYSTEM32 path=C:\Windows\System32\ncrypt.dll', 'TARGET-IMAGE-DIR pid=700 dir=C:\Users\u\AppData\Local\Temp\Asterun-Elevated.kit-rm', 'RESULT: OBSERVED'),
      'rm-worker-fixed'),
    @('obs-not-observed', 12, 'observation: normal uninstall without RM - rstrtmgr/ncrypt NOT-OBSERVED, not a pass or a failure: OBSERVED',
      @('-Scenario', 'uninstall-worker', '-Role', 'observation', '-ReportDll', 'rstrtmgr.dll,ncrypt.dll'),
      @('DLL-LOAD pid=700 name=rstrtmgr.dll class=NOT-OBSERVED', 'DLL-LOAD pid=700 name=ncrypt.dll class=NOT-OBSERVED', 'RESULT: OBSERVED'),
      'rm-worker-norm'),
    @('obs-winother', 12, 'observation: loads under %WINDIR% but outside System32 are classed WINDOWS-OTHER, not SYSTEM32: OBSERVED',
      @('-Scenario', 'uninstall-worker', '-Role', 'observation', '-ReportDll', 'rstrtmgr.dll,ncrypt.dll'),
      @('DLL-LOAD pid=700 name=rstrtmgr.dll class=WINDOWS-OTHER path=C:\Windows\SysWOW64\rstrtmgr.dll', 'DLL-LOAD pid=700 name=ncrypt.dll class=WINDOWS-OTHER path=C:\Windows\WinSxS\amd64_ncrypt_x\ncrypt.dll', 'RESULT: OBSERVED'),
      'rm-worker-winother'),
    @('obs-watched', 20, 'observation: ncrypt loaded from the worker directory: FAIL',
      @('-Scenario', 'uninstall-worker', '-Role', 'observation', '-ReportDll', 'rstrtmgr.dll,ncrypt.dll'),
      @('DLL-LOAD pid=700 name=ncrypt.dll class=WATCHED path=C:\Users\u\AppData\Local\Temp\Asterun-Elevated.kit-rm\ncrypt.dll', 'RESULT: FAIL'),
      'rm-worker-vuln'),
    @('obs-invalid', 30, 'observation: no uninstall worker in the capture: CAPTURE-INVALID / INVALID',
      @('-Scenario', 'uninstall-worker', '-Role', 'observation', '-ReportDll', 'rstrtmgr.dll,ncrypt.dll'),
      @('DLL-LOAD name=rstrtmgr.dll class=CAPTURE-INVALID', 'DLL-LOAD name=ncrypt.dll class=CAPTURE-INVALID', 'RESULT: INVALID'),
      'svc-ok'),
    @('obs-forced-norun', 30, 'observation of a forced-RM run where rstrtmgr never loaded: path did not run: INVALID',
      @('-Scenario', 'uninstall-worker', '-Role', 'observation', '-ReportDll', 'rstrtmgr.dll,ncrypt.dll', '-RequireSystemLoad', 'rstrtmgr.dll'),
      @('required load of rstrtmgr.dll : NOT-OBSERVED', 'RESULT: INVALID'),
      'rm-worker-norm')
)

$results = New-Object System.Collections.Generic.List[string]
$bad = 0
$index = 0
foreach ($case in $cases) {
    ++$index
    $report = if ($case[0] -eq 'control-main') { $ctlMain } else { Join-Path $Out ('{0:D2}-{1}.txt' -f $index, $case[0]) }
    $arguments = @('-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File', $analyzer,
        '-Csv', (Join-Path $fixtures "$(if ($case.Count -ge 6) { $case[5] } else { $case[0] }).csv"), '-AppDir', $app, '-TempDir', $temp, '-Out', $report) + $case[3]
    & $shell @arguments > "$report.console"
    $code = $LASTEXITCODE
    $ok = $code -eq $case[1]
    # Optional: literal lines the report must contain (paths, classes, result).
    $missing = @()
    if ($case.Count -ge 5) {
        $text = if (Test-Path -LiteralPath $report) { [System.IO.File]::ReadAllText($report) } else { '' }
        foreach ($needle in @($case[4])) { if (-not $text.Contains($needle)) { $missing += $needle } }
        if ($missing.Count -gt 0) { $ok = $false }
    }
    if (-not $ok) { $bad++ }
    $results.Add(('{0} {1,-22} expected={2,-3} got={3,-3} {4}' -f $(if ($ok) { 'ok  ' } else { 'FAIL' }), $case[0], $case[1], $code, $case[2]))
    foreach ($needle in $missing) { $results.Add("       missing in report: $needle") }
}
$results.Add("RESULT: $(if ($bad) { "$bad of $($cases.Count) cases deviate" } else { "all $($cases.Count) cases as expected" })")
[System.IO.File]::WriteAllLines((Join-Path $Out 'summary.txt'), $results)
$results | ForEach-Object { Write-Output $_ }
exit $(if ($bad) { 1 } else { 0 })
