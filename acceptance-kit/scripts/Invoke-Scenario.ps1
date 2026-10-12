# Asterun batch A acceptance kit: run ONE scenario under Process Monitor and
# analyse it (Analyze-Procmon.ps1).
#
# Every scenario runs on a fresh copy inside the workspace created by
# Initialize-Workspace.ps1 (<Work>\scenarios\...), plants the probe DLL under
# the name of a static import of the target, captures with Process Monitor,
# runs a functional check, exports the capture to CSV and analyses it.
#
#   scenario            planted DLL (static import)   functional check
#   main                version.dll   next to Asterun.exe    still running after -WaitSeconds
#   update-noargs       bcrypt.dll    next to Update.exe     exits with code 2 (no arguments)
#   service-command     version.dll   next to Asterun.exe    --set-managed-everything-service disabled exits 0
#   uninstall-launcher  rstrtmgr.dll  next to Uninstall.exe  still running (data choice dialog) after -WaitSeconds
#   uninstall-worker    rstrtmgr.dll  next to the worker copy in %TEMP%\Asterun-Elevated.kit-*
#                                     --perform --delete-data 0 on a disposable install copy: program
#                                     files removed, data\ kept
#   update-chain        bcrypt.dll next to the Update.exe copy, version.dll in the package
#                                     UpdaterIntegrationTests.ps1 -Mode Portable exits 0 (candidate only)
#
# -Build baseline runs the v1.0.5 package as the positive control (expected
# CONTROL-VALID: it loads the planted DLL from its own directory, then usually
# fails to start because the probe exports nothing; that failure is expected).
# -Build candidate runs the batch A package; its report counts a planted name
# as verified only with a CONTROL-VALID report of the same name in the logs
# folder of this run, so run the controls first (Run-Acceptance.ps1 does).
#
# Must run elevated: Process Monitor loads a driver, and the service command
# and the uninstall worker run elevated in real use too.
param(
    [Parameter(Mandatory = $true)][string]$Work,
    [Parameter(Mandatory = $true)][string]$Logs,
    [Parameter(Mandatory = $true)]
    [ValidateSet('main', 'update-noargs', 'update-chain', 'service-command', 'uninstall-launcher', 'uninstall-worker')]
    [string]$Scenario,
    [Parameter(Mandatory = $true)][ValidateSet('baseline', 'candidate')][string]$Build,
    [int]$WaitSeconds = 10
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
if ($PSVersionTable.PSEdition -ne 'Core') {
    $env:PSModulePath = @(
        (Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'WindowsPowerShell\Modules'),
        (Join-Path $env:ProgramFiles 'WindowsPowerShell\Modules'),
        (Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\Modules')
    ) -join ';'
}
$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'run this script elevated' }
if ($Build -eq 'baseline' -and $Scenario -notin 'main', 'update-noargs', 'uninstall-launcher') {
    throw "the baseline control exists for main, update-noargs and uninstall-launcher only"
}

$Work = (Resolve-Path -LiteralPath $Work).Path
$runName = Split-Path -Leaf $Work
$logDir = Join-Path $Logs $runName
if (-not (Test-Path -LiteralPath $logDir)) { throw "log folder of this run not found: $logDir (run Initialize-Workspace.ps1 first)" }
$package = Join-Path $Work $Build
$probe = Join-Path $Work 'probe\asterun_dll_probe.dll'
$procmon = @(Get-ChildItem -LiteralPath (Join-Path $Work 'tools') -Filter 'Procmon64*.exe')[0].FullName
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$role = if ($Build -eq 'baseline') { 'control' } else { 'candidate' }
$label = "$role-$Scenario-$stamp"
$scenarioDir = Join-Path $Work "scenarios\$label"
if (Test-Path -LiteralPath $scenarioDir) { throw "refusing to reuse $scenarioDir" }
New-Item -ItemType Directory -Path $scenarioDir | Out-Null
$pml = Join-Path $logDir "$label.pml"
$csv = Join-Path $logDir "$label.csv"
$report = Join-Path $logDir "$label-report.txt"
$transcript = New-Object System.Collections.Generic.List[string]
# Write-Host, not Write-Output: Note is also called inside functions that return values.
function Note([string]$text) { $transcript.Add($text); Write-Host $text }

# Child processes inherit this error mode: no loader error dialog in the
# baseline control (missing exports of the probe), no crash dialog.
Add-Type -Namespace AsterunKit -Name ErrorMode -MemberDefinition @'
[System.Runtime.InteropServices.DllImport("kernel32.dll")]
public static extern uint SetErrorMode(uint mode);
'@
[void][AsterunKit.ErrorMode]::SetErrorMode(0x8003)

# Use the long form of the temporary directory (it can be an 8.3 path such as
# C:\Users\RUNNER~1\...): Process Monitor reports long paths, and the
# directories created below are gone before the analysis runs.
Add-Type -Namespace AsterunKit -Name LongPath -MemberDefinition @'
[System.Runtime.InteropServices.DllImport("kernel32.dll", CharSet = System.Runtime.InteropServices.CharSet.Unicode)]
public static extern uint GetLongPathNameW(string shortPath, System.Text.StringBuilder longPath, uint size);
'@
$buffer = New-Object System.Text.StringBuilder 32768
if ([AsterunKit.LongPath]::GetLongPathNameW($env:TEMP, $buffer, 32768) -gt 0) { $env:TEMP = $buffer.ToString(); $env:TMP = $env:TEMP }

function Start-Child([string]$file, [string]$arguments) {
    $info = New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName = $file
    $info.Arguments = $arguments
    $info.UseShellExecute = $false
    $info.WorkingDirectory = Split-Path -Parent $file
    Note "start: `"$file`" $arguments"
    return [System.Diagnostics.Process]::Start($info)
}
function Copy-Package([string]$destination) {
    New-Item -ItemType Directory -Path $destination | Out-Null
    Get-ChildItem -LiteralPath $package -Force | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $destination -Recurse }
}
function Plant([string]$directory, [string]$name) {
    $path = Join-Path $directory $name
    Copy-Item -LiteralPath $probe -Destination $path
    Note "planted probe as $path"
    $script:planted += $path
}
function Stop-Under([string[]]$roots) {
    foreach ($process in @(Get-CimInstance Win32_Process)) {
        if (-not $process.ExecutablePath) { continue }
        foreach ($root in $roots) {
            if ($process.ExecutablePath.StartsWith($root.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
                Note "stopping pid=$($process.ProcessId) $($process.ExecutablePath)"
                Stop-Process -Id $process.ProcessId -Force -ErrorAction SilentlyContinue
            }
        }
    }
}
function Get-ExitedPid {
    $parent = Start-Process -FilePath "$env:WINDIR\System32\cmd.exe" -ArgumentList '/c', 'exit 0' -WindowStyle Hidden -PassThru
    $parent.WaitForExit()
    return $parent.Id
}

# Prepare the scenario directories and planted DLLs.
$app = Join-Path $scenarioDir 'app'
$cleanupRoots = @($scenarioDir)
$planted = @()
$workerDir = $null
switch ($Scenario) {
    { $_ -in 'main', 'service-command' } { Copy-Package $app; Plant $app 'version.dll' }
    'update-noargs'      { Copy-Package $app; Plant $app 'bcrypt.dll' }
    'uninstall-launcher' { Copy-Package $app; Plant $app 'rstrtmgr.dll' }
    'uninstall-worker' {
        Copy-Package $app
        New-Item -ItemType Directory -Path (Join-Path $app 'data') -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $app 'data\kit-marker.txt') -Value $label
        $workerDir = Join-Path $env:TEMP "Asterun-Elevated.kit-$stamp"
        if (Test-Path -LiteralPath $workerDir) { throw "refusing to reuse $workerDir" }
        New-Item -ItemType Directory -Path $workerDir | Out-Null
        Copy-Item -LiteralPath (Join-Path $package 'Uninstall.exe') -Destination $workerDir
        Plant $workerDir 'rstrtmgr.dll'
        $cleanupRoots += $workerDir
    }
    # The test creates its own directories; it prints where it planted the probe.
    'update-chain' { Copy-Package $app }
}

$probeLog = Join-Path $env:PUBLIC 'AsterunDllProbe\probe-log.txt'
$probeSkip = if (Test-Path -LiteralPath $probeLog) { @([System.IO.File]::ReadAllLines($probeLog, [System.Text.Encoding]::Unicode)).Count } else { 0 }

# Capture.
if (@(Get-Process -Name 'Procmon*' -ErrorAction SilentlyContinue).Count -gt 0) { throw 'Process Monitor is already running; close it first' }
Note "procmon: $procmon -> $pml"
Start-Process -FilePath $procmon -ArgumentList '/AcceptEula', '/Quiet', '/Minimized', '/BackingFile', "`"$pml`"" | Out-Null
$ready = Start-Process -FilePath $procmon -ArgumentList '/AcceptEula', '/WaitForIdle' -PassThru -Wait
Start-Sleep -Seconds 2

$functional = 'FAIL'
$functionalDetail = ''
try {
    switch ($Scenario) {
        'main' {
            $process = Start-Child (Join-Path $app 'Asterun.exe') ''
            $exited = $process.WaitForExit($WaitSeconds * 1000)
            $functionalDetail = if ($exited) { "exited early with code $($process.ExitCode)" } else { "running after $WaitSeconds s" }
            if (-not $exited) { $functional = 'PASS' }
        }
        'update-noargs' {
            $process = Start-Child (Join-Path $app 'Update.exe') ''
            if ($process.WaitForExit(30000)) {
                $functionalDetail = "exit code $($process.ExitCode) (expected 2)"
                if ($process.ExitCode -eq 2) { $functional = 'PASS' }
            } else { $functionalDetail = 'did not exit within 30 s' }
        }
        'service-command' {
            $process = Start-Child (Join-Path $app 'Asterun.exe') '--set-managed-everything-service disabled'
            if ($process.WaitForExit(120000)) {
                $functionalDetail = "exit code $($process.ExitCode) (expected 0)"
                if ($process.ExitCode -eq 0) { $functional = 'PASS' }
            } else { $functionalDetail = 'did not exit within 120 s' }
        }
        'uninstall-launcher' {
            $process = Start-Child (Join-Path $app 'Uninstall.exe') ''
            $exited = $process.WaitForExit($WaitSeconds * 1000)
            $process.Refresh()
            $functionalDetail = if ($exited) { "exited early with code $($process.ExitCode)" } else { "running after $WaitSeconds s, window title '$($process.MainWindowTitle)'" }
            if (-not $exited) { $functional = 'PASS' }
        }
        'uninstall-worker' {
            $process = Start-Child (Join-Path $workerDir 'Uninstall.exe') "--perform --parent-pid $(Get-ExitedPid) --install `"$app`" --delete-data 0"
            $deadline = (Get-Date).AddSeconds(120)
            while ((Get-Date) -lt $deadline -and (Test-Path -LiteralPath (Join-Path $app 'Asterun.exe')) -and -not $process.HasExited) { Start-Sleep -Milliseconds 500 }
            Start-Sleep -Seconds 3
            $removed = -not (Test-Path -LiteralPath (Join-Path $app 'Asterun.exe')) -and -not (Test-Path -LiteralPath (Join-Path $app 'Update.exe'))
            $kept = Test-Path -LiteralPath (Join-Path $app 'data\kit-marker.txt')
            $state = if ($process.HasExited) { "exited with code $($process.ExitCode)" } else { 'still running (completion message)' }
            $functionalDetail = "program files removed=$removed data kept=$kept worker $state"
            if ($removed -and $kept -and (-not $process.HasExited -or $process.ExitCode -eq 0)) { $functional = 'PASS' }
        }
        'update-chain' {
            $test = Join-Path $Work 'tests\UpdaterIntegrationTests.ps1'
            $output = & "$env:WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $test `
                -AppDir $app -Mode Portable -PlantDll $probe -VersionFile (Join-Path $app 'VERSION') 2>&1
            $code = $LASTEXITCODE
            $output | ForEach-Object { Note "  | $_" }
            foreach ($line in $output) {
                $match = [regex]::Match([string]$line, 'planted: (\S.*?\.dll)')
                if ($match.Success) { $planted += $match.Groups[1].Value }
            }
            $functionalDetail = "UpdaterIntegrationTests.ps1 exit code $code (expected 0)"
            if ($code -eq 0) { $functional = 'PASS' }
        }
    }
} finally {
    Start-Sleep -Seconds 2
    Stop-Under $cleanupRoots
    Note 'procmon: terminate'
    Start-Process -FilePath $procmon -ArgumentList '/AcceptEula', '/Terminate' -Wait
}
Note "functional: $functional ($functionalDetail)"
if ($Build -eq 'baseline') { $functional = 'NOTRUN'; Note 'functional result not used for the baseline control' }

Note "procmon: export $csv"
Start-Process -FilePath $procmon -ArgumentList '/AcceptEula', '/Quiet', '/Minimized', '/OpenLog', "`"$pml`"", '/SaveAs', "`"$csv`"" -Wait
if (-not (Test-Path -LiteralPath $csv)) { throw "export failed: $csv" }
Note "csv size=$((Get-Item -LiteralPath $csv).Length)"

$controls = @(Get-ChildItem -LiteralPath $logDir -Filter 'control-*-report.txt' | ForEach-Object { $_.FullName })
$arguments = @{
    Csv = $csv; Scenario = $Scenario; AppDir = $app; TempDir = $env:TEMP; Role = $role
    PlantedPath = $planted; ControlReport = $controls; FunctionalResult = $functional
    FunctionalDetail = $functionalDetail; ProbeLog = $probeLog; ProbeLogSkip = $probeSkip; Out = $report
}
& (Join-Path $PSScriptRoot 'Analyze-Procmon.ps1') @arguments | Out-Null
$code = $LASTEXITCODE
[System.IO.File]::AppendAllText($report, "`r`n--- run transcript ---`r`n" + ($transcript -join "`r`n") + "`r`n")
Get-Content -LiteralPath $report | ForEach-Object { Write-Output $_ }

# Remove what this scenario created outside the logs folder; the scenario
# directory itself stays in the workspace for inspection.
if ($workerDir -and (Test-Path -LiteralPath $workerDir)) { Remove-Item -LiteralPath $workerDir -Recurse -Force -ErrorAction SilentlyContinue }
exit $code
