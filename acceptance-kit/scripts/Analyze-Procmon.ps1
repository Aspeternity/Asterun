# Asterun batch A acceptance kit: analyse a Process Monitor CSV export of one
# scenario and give a verdict. Normally called by Invoke-Scenario.ps1.
#
# Capture: all processes or a Process Name filter on Asterun.exe, Update.exe and
# Uninstall.exe, all operations, default columns (they include PID and Detail).
# The capture must start before the scenario: the analysis needs the
# "Process Start" event of every target process (its Detail holds the command
# line) and the "Load Image" event of its own .exe (the image path).
#
# 1. Target processes, identified by executable name, image path AND command line:
#   main               Asterun.exe under -AppDir, no service/health argument   (normal mode)
#   update-noargs      Update.exe under -AppDir, no --apply                   (hardened)
#   update-chain       Update.exe with --apply, image in an Asterun-Elevated.* directory (hardened)
#                      + Asterun.exe with --post-update-health-event          (normal mode)
#   service-command    Asterun.exe under -AppDir with --set-managed-everything-service
#                      or --repair-managed-everything-service                 (hardened)
#   uninstall-launcher Uninstall.exe under -AppDir, no --perform              (hardened)
#   uninstall-worker   Uninstall.exe with --perform, image in Asterun-Elevated.* under -TempDir (hardened)
#   A required target that is not in the capture, or that lacks its Process
#   Start or exe Load Image event, makes the result INVALID. Another process
#   of the same name (for example the ordinary Asterun.exe) never stands in.
#
# 2. DLL events of each target PID:
#   LOADED        Load Image                          - the DLL was mapped from that path
#   OPEN-EXECUTE  CreateFile SUCCESS with Execute in the desired access
#                                                     - opened for image mapping
#   SEARCH        result NAME NOT FOUND / PATH NOT FOUND in the directory of the
#                 target's own image (the first place the default search order
#                 looks; the loader probes there without execute access)
#                                                     - a lookup of a file that is not there
#   FILE-OP       anything else, including NAME NOT FOUND in other watched
#                 directories (existence checks of files the program itself
#                 manages, e.g. the updater checking the install directory),
#                 reads, copies, deletes      - reported; never a failure by itself
#   Locations (strict):
#     WATCHED        -AppDir, -TempDir and the image directory of the target
#                    (long and 8.3 forms)
#     SYSTEM32       %WINDIR%\System32 itself (and below)
#     WINDOWS-OTHER  elsewhere under %WINDIR% (SysWOW64, WinSxS, ...) - NOT System32
#     OTHER          anywhere else
#   Every claim of "from System32" in this script means SYSTEM32 only.
#
# 3. Planted DLLs (-PlantedPath): full paths of copies of the probe DLL, each
#   named after a static import of the process that runs from the same
#   directory. A plant is relevant to a target PID when it lies in the
#   directory of that PID's image (where the old loader looked first).
#   -Role control   (old build, expected to be vulnerable): the target LOADED a
#                   relevant planted path -> "CONTROL-VALID name=<dll>". No such
#                   load: CONTROL-INVALID (the capture or the plant did not work).
#   -Role candidate (build under test), per relevant plant:
#                   LOADED from a watched directory              -> FAIL
#                   LOADED the same name from SYSTEM32 and the name is
#                   CONTROL-VALID in a -ControlReport             -> verified
#                   otherwise                                     -> not verified
#   A target (all its matched PIDs together) is PASS only when at least one
#   plant is verified for one of its PIDs; without a relevant plant or a valid
#   control it is UNVERIFIED, however clean the capture looks. PIDs of the
#   target without a plant (for example the negative cases of update-chain)
#   are still checked by rule 4.
#
# 4. Other rules for candidate targets:
#   hardened    : any LOADED or OPEN-EXECUTE in WATCHED -> FAIL;
#                 SEARCH in WATCHED -> REVIEW (listed; a lookup is not a load).
#   normal mode : LOADED or OPEN-EXECUTE of a C/C++ runtime DLL in WATCHED -> FAIL;
#                 other WATCHED activity is listed as INFO (the main process
#                 does not call SetDefaultDllDirectories by design).
#   -FunctionalResult FAIL -> FAIL; NOTRUN -> at most UNVERIFIED.
#   -ProbeLog: lines the probe wrote during the scenario. Auxiliary only. A
#   probe line from a target PID with no matching LOADED event makes the
#   capture INVALID (the capture missed a load).
#
# 5. -ReportDll a,b: for every matched target PID, one line per name:
#      DLL-LOAD pid=<pid> name=<dll> class=<SYSTEM32|WINDOWS-OTHER|WATCHED|OTHER> path=<full Load Image path>
#   or DLL-LOAD pid=<pid> name=<dll> class=NOT-OBSERVED (no Load Image of that
#   name in the PID), or, when the target itself is missing,
#      DLL-LOAD name=<dll> class=CAPTURE-INVALID.
#   Each matched PID also gets TARGET-IMAGE-DIR pid=<pid> dir=<directory of its image>.
#
# 6. -RequireSystemLoad a,b: a matched target must have LOADED each name.
#   From SYSTEM32 -> satisfied; loaded only from elsewhere (not watched) ->
#   REVIEW (the path ran, but not from System32); never loaded -> INVALID (the
#   path the scenario was meant to cover did not run).
#
# 7. -Role observation (real, un-planted capture such as the on-machine UAC
#   run): no plant, no control, no PASS. The result is OBSERVED (exit 12) with
#   the -ReportDll lines as the observation; FAIL (20) if a target LOADED or
#   opened for execute any DLL from a watched directory; INVALID (30) if the
#   target is missing. -RequireSystemLoad applies as in 6.
#
# Overall: INVALID > FAIL > REVIEW > UNVERIFIED > PASS
#          (control: CONTROL-VALID 0, CONTROL-INVALID 31, INVALID 30).
# Exit codes: PASS 0, REVIEW 10, UNVERIFIED 11, OBSERVED 12, FAIL 20, INVALID 30, CONTROL-INVALID 31.
param(
    [Parameter(Mandatory = $true)][string]$Csv,
    [Parameter(Mandatory = $true)]
    [ValidateSet('main', 'update-noargs', 'update-chain', 'service-command', 'uninstall-launcher', 'uninstall-worker')]
    [string]$Scenario,
    [Parameter(Mandatory = $true)][string]$AppDir,
    [string]$TempDir = $env:TEMP,
    [ValidateSet('candidate', 'control', 'observation')][string]$Role = 'candidate',
    [string[]]$PlantedPath = @(),
    [string[]]$ControlReport = @(),
    [ValidateSet('PASS', 'FAIL', 'NOTRUN')][string]$FunctionalResult = 'NOTRUN',
    [string]$FunctionalDetail = '',
    [string]$ProbeLog = '',
    [int]$ProbeLogSkip = 0,
    # Leaf DLL names that at least one matched target PID must have LOADED from
    # System32. Proves the exercised code path actually ran: e.g. rstrtmgr.dll
    # for an uninstall scenario built to invoke Restart Manager. If a required
    # name was never loaded from System32 the result is INVALID (the path the
    # scenario was meant to cover did not execute), never PASS.
    [string[]]$RequireSystemLoad = @(),
    # Leaf DLL names whose Load Image paths are reported per matched PID (rule 5).
    [string[]]$ReportDll = @(),
    [Parameter(Mandatory = $true)][string]$Out
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
$lines = New-Object System.Collections.Generic.List[string]
function Emit([string]$text) { $lines.Add($text) }
function Finish([string]$verdict, [int]$code) {
    Emit "RESULT: $verdict"
    [System.IO.File]::WriteAllLines($Out, $lines, (New-Object System.Text.UTF8Encoding($false)))
    $lines | ForEach-Object { Write-Output $_ }
    exit $code
}

if (-not (Test-Path -LiteralPath $Csv)) { throw "CSV not found: $Csv" }
# Lists may also arrive as one string (powershell -File): split on ';'.
$PlantedPath = @($PlantedPath | ForEach-Object { $_ -split ';' } | Where-Object { $_ } | ForEach-Object { $_.Trim() })
$RequireSystemLoad = @($RequireSystemLoad | ForEach-Object { $_ -split ',' } | Where-Object { $_ } | ForEach-Object { $_.Trim().ToLowerInvariant() })
$ControlReport = @($ControlReport | ForEach-Object { $_ -split ';' } | Where-Object { $_ })
$ReportDll = @($ReportDll | ForEach-Object { $_ -split ',' } | Where-Object { $_ } | ForEach-Object { $_.Trim().ToLowerInvariant() })
Emit "scenario=$Scenario role=$Role csv=$Csv"
Emit "appDir=$AppDir tempDir=$TempDir planted=$($PlantedPath -join ';') functional=$FunctionalResult $FunctionalDetail"

# Stream the export and keep only rows of the three executables: a capture of
# all processes can be hundreds of MB, too much for Import-Csv.
$exeNames = 'asterun.exe', 'update.exe', 'uninstall.exe'
$reader = New-Object System.IO.StreamReader($Csv, $true)
$header = $reader.ReadLine()
$kept = New-Object System.Collections.Generic.List[string]
$total = 0
while ($null -ne ($row = $reader.ReadLine())) {
    ++$total
    foreach ($exe in $exeNames) {
        if ($row.IndexOf('"' + $exe + '"', [StringComparison]::OrdinalIgnoreCase) -ge 0) { $kept.Add($row); break }
    }
}
$reader.Close()
Emit "rows=$total rows-of-target-executables=$($kept.Count)"
if ($null -eq $header -or $total -eq 0) { Finish 'INVALID - the CSV has no events' 30 }
foreach ($column in 'Process Name', 'PID', 'Operation', 'Path', 'Result', 'Detail') {
    if ($header.IndexOf('"' + $column + '"') -lt 0) {
        Finish "INVALID - the CSV has no '$column' column (export with the default columns)" 30
    }
}
$events = @()
if ($kept.Count -gt 0) {
    $events = @(@($header) + $kept.ToArray() | ConvertFrom-Csv | Where-Object { $exeNames -contains $_.'Process Name'.ToLowerInvariant() })
}

# Directory forms: as given, long and 8.3 (Process Monitor reports long paths).
$pathApi = Add-Type -PassThru -Namespace AsterunKit -Name AnalyzePathForms -MemberDefinition @'
[System.Runtime.InteropServices.DllImport("kernel32.dll", CharSet = System.Runtime.InteropServices.CharSet.Unicode)]
public static extern uint GetLongPathNameW(string shortPath, System.Text.StringBuilder longPath, uint size);
[System.Runtime.InteropServices.DllImport("kernel32.dll", CharSet = System.Runtime.InteropServices.CharSet.Unicode)]
public static extern uint GetShortPathNameW(string longPath, System.Text.StringBuilder shortPath, uint size);
'@
function Get-Forms([string]$path) {
    $forms = @($path)
    if ($env:OS -eq 'Windows_NT') {
        $buffer = New-Object System.Text.StringBuilder 32768
        if ($pathApi::GetLongPathNameW($path, $buffer, 32768) -gt 0) { $forms += $buffer.ToString() }
        $buffer = New-Object System.Text.StringBuilder 32768
        if ($pathApi::GetShortPathNameW($path, $buffer, 32768) -gt 0) { $forms += $buffer.ToString() }
    }
    return @($forms | ForEach-Object { ($_.TrimEnd('\') + '\').ToLowerInvariant() } | Select-Object -Unique)
}
function Test-Under([string]$path, [string[]]$roots) {
    $lower = $path.ToLowerInvariant()
    foreach ($root in $roots) { if ($lower.StartsWith($root)) { return $true } }
    return $false
}
$appForms = @(Get-Forms $AppDir)
$tempForms = @(Get-Forms $TempDir)
$windows = ($env:WINDIR.TrimEnd('\') + '\').ToLowerInvariant()
$system32 = ($env:WINDIR.TrimEnd('\') + '\System32\').ToLowerInvariant()
function Get-Location([string]$path, [string[]]$watched) {
    if (Test-Under $path $watched) { return 'WATCHED' }
    $lower = $path.ToLowerInvariant()
    if ($lower.StartsWith($system32)) { return 'SYSTEM32' }
    if ($lower.StartsWith($windows)) { return 'WINDOWS-OTHER' }
    return 'OTHER'
}
Emit "watched=$((@($appForms + $tempForms) | Select-Object -Unique) -join ';') (+ each target's image directory)"

# Every PID of the three executables: command line and image path.
$processes = @{}
foreach ($event in $events) {
    $name = $event.'Process Name'.ToLowerInvariant()
    $key = "$name|$($event.PID)"
    if (-not $processes.ContainsKey($key)) {
        $processes[$key] = [pscustomobject]@{ Name = $name; Pid = [string]$event.PID; CommandLine = $null; Image = $null; Events = (New-Object System.Collections.Generic.List[object]) }
    }
    $entry = $processes[$key]
    $entry.Events.Add($event)
    if ($event.Operation -eq 'Process Start' -and $null -eq $entry.CommandLine) {
        $match = [regex]::Match($event.Detail, 'Command line:\s*(.*?)(,\s*Current directory:|$)')
        if ($match.Success) { $entry.CommandLine = $match.Groups[1].Value }
    }
    if ($event.Operation -eq 'Load Image' -and $null -eq $entry.Image -and
        $event.Path.ToLowerInvariant().EndsWith('\' + $name)) {
        $entry.Image = $event.Path
    }
}
Emit "processes of the target executables in the capture: $($processes.Count)"
foreach ($entry in $processes.Values) {
    $commandLine = if ($null -eq $entry.CommandLine) { '<no Process Start event>' } else { $entry.CommandLine }
    $image = if ($null -eq $entry.Image) { '<no Load Image of the exe>' } else { $entry.Image }
    Emit "  $($entry.Name) pid=$($entry.Pid) events=$($entry.Events.Count) image=$image cmd=$commandLine"
}

$serviceArgs = '--set-managed-everything-service', '--repair-managed-everything-service'
function Test-Arg([string]$commandLine, [string[]]$arguments) {
    foreach ($argument in $arguments) { if ($commandLine -like "*$argument*") { return $true } }
    return $false
}
$anywhere = @('')
$targets = @(
    switch ($Scenario) {
        'main'               { @{ Role = 'Asterun.exe main mode'; Name = 'asterun.exe'; ImageUnder = $appForms; Hardened = $false
                                  Match = { param($c) -not (Test-Arg $c ($serviceArgs + '--post-update-health-event')) } } }
        'update-noargs'      { @{ Role = 'Update.exe without --apply'; Name = 'update.exe'; ImageUnder = $appForms; Hardened = $true
                                  Match = { param($c) -not (Test-Arg $c @('--apply')) } } }
        'update-chain'       { @{ Role = 'Update.exe --apply worker'; Name = 'update.exe'; ImageUnder = $anywhere; Hardened = $true
                                  Match = { param($c) Test-Arg $c @('--apply') }; ImageLike = '*\asterun-elevated.*\update.exe' }
                               @{ Role = 'updated Asterun.exe with --post-update-health-event'; Name = 'asterun.exe'; ImageUnder = $anywhere; Hardened = $false
                                  Match = { param($c) Test-Arg $c @('--post-update-health-event') } } }
        'service-command'    { @{ Role = 'Asterun.exe service command'; Name = 'asterun.exe'; ImageUnder = $appForms; Hardened = $true
                                  Match = { param($c) Test-Arg $c $serviceArgs } } }
        'uninstall-launcher' { @{ Role = 'Uninstall.exe launcher'; Name = 'uninstall.exe'; ImageUnder = $appForms; Hardened = $true
                                  Match = { param($c) -not (Test-Arg $c @('--perform')) } } }
        'uninstall-worker'   { @{ Role = 'Uninstall.exe --perform worker'; Name = 'uninstall.exe'; ImageUnder = $tempForms; Hardened = $true
                                  Match = { param($c) Test-Arg $c @('--perform') }; ImageLike = '*\asterun-elevated.*\uninstall.exe' } }
    }
)

# Planted names with a valid control.
$controlValid = @{}
foreach ($report in $ControlReport) {
    if (-not $report) { continue }
    if (-not (Test-Path -LiteralPath $report)) { Emit "control report not found: $report"; continue }
    foreach ($line in [System.IO.File]::ReadAllLines($report)) {
        $match = [regex]::Match($line, '^CONTROL-VALID name=(\S+)')
        if ($match.Success) { $controlValid[$match.Groups[1].Value.ToLowerInvariant()] = $report }
    }
}
if ($Role -eq 'candidate') {
    Emit "planted names with a valid control: $((@($controlValid.Keys) | Sort-Object) -join ',')"
}

# Probe log lines written during the scenario (auxiliary evidence).
$probeLines = @()
if ($ProbeLog -and (Test-Path -LiteralPath $ProbeLog)) {
    $all = @([System.IO.File]::ReadAllLines($ProbeLog, [System.Text.Encoding]::Unicode))
    if ($all.Count -gt $ProbeLogSkip) { $probeLines = @($all[$ProbeLogSkip..($all.Count - 1)] | Where-Object { $_ }) }
}
Emit "probe log lines during the scenario: $($probeLines.Count) (auxiliary)"
foreach ($line in $probeLines) { Emit "  probe: $line" }

$crtPattern = '^(vcruntime.*|msvcp.*|api-ms-win-crt-.*|ucrtbase|concrt.*)\.dll$'
$rank = @{ 'PASS' = 0; 'UNVERIFIED' = 1; 'REVIEW' = 2; 'FAIL' = 3; 'INVALID' = 4 }
$overall = 'PASS'
function Raise([string]$verdict) { if ($rank[$verdict] -gt $rank[$script:overall]) { $script:overall = $verdict } }
$controlLoads = @{}
$allLoads = New-Object System.Collections.Generic.List[object]

foreach ($target in $targets) {
    $candidates = @($processes.Values | Where-Object { $_.Name -eq $target.Name })
    $matched = @($candidates | Where-Object {
        $_.Image -and $_.CommandLine -and (Test-Under $_.Image $target.ImageUnder) -and (& $target.Match $_.CommandLine) -and
        (-not $target.ContainsKey('ImageLike') -or $_.Image.ToLowerInvariant() -like $target.ImageLike)
    })
    Emit ""
    Emit "TARGET $($target.Role): matched pids=$((@($matched | ForEach-Object { $_.Pid })) -join ',')"
    if ($matched.Count -eq 0) {
        $incomplete = @($candidates | Where-Object { -not $_.Image -or -not $_.CommandLine })
        if ($incomplete.Count -gt 0) {
            Emit "  $($incomplete.Count) $($target.Name) process(es) lack the Process Start or exe Load Image event: start the capture before the scenario"
        }
        foreach ($name in $ReportDll) { Emit "DLL-LOAD name=$name class=CAPTURE-INVALID" }
        Emit '  verdict: INVALID (required target process not captured)'
        Raise 'INVALID'
        continue
    }
    $roleVerdict = 'PASS'; $roleReasons = @(); $roleVerified = @()
    foreach ($entry in $matched) {
        $imageDirForms = @(Get-Forms (Split-Path -Parent $entry.Image))
        $watched = @($appForms + $tempForms + $imageDirForms | Select-Object -Unique)
        $loaded = @(); $openExecute = @(); $search = @(); $fileOps = @()
        foreach ($event in $entry.Events) {
            $path = $event.Path
            if (-not $path -or -not $path.ToLowerInvariant().EndsWith('.dll')) { continue }
            $location = Get-Location $path $watched
            $access = [regex]::Match([string]$event.Detail, 'Desired Access: (.*?)(, Disposition:|, Options:|$)').Groups[1].Value
            $leafDir = (Split-Path -Parent $path).TrimEnd('\').ToLowerInvariant() + '\'
            $inImageDir = $imageDirForms -contains $leafDir
            $item = [pscustomobject]@{ Op = $event.Operation; Result = $event.Result; Path = $path; Leaf = (Split-Path -Leaf $path).ToLowerInvariant(); Location = $location; Access = $access }
            if ($event.Operation -eq 'Load Image') {
                $loaded += $item
                $allLoads.Add($item)
            }
            elseif (($event.Result -eq 'NAME NOT FOUND' -or $event.Result -eq 'PATH NOT FOUND') -and $inImageDir) { $search += $item }
            elseif ($event.Operation -eq 'CreateFile' -and $event.Result -eq 'SUCCESS' -and $event.Detail -match 'Desired Access:((?!Disposition:).)*Execute') { $openExecute += $item }
            else { $fileOps += $item }
        }
        $watchedLoaded = @($loaded | Where-Object { $_.Location -eq 'WATCHED' })
        $watchedOpen = @($openExecute | Where-Object { $_.Location -eq 'WATCHED' })
        $watchedSearch = @($search | Where-Object { $_.Location -eq 'WATCHED' })
        $watchedFileOps = @($fileOps | Where-Object { $_.Location -eq 'WATCHED' })
        Emit "  pid=$($entry.Pid) image=$($entry.Image)"
        Emit "TARGET-IMAGE-DIR pid=$($entry.Pid) dir=$($entry.Image.Substring(0, $entry.Image.LastIndexOf('\')))"
        Emit "    LOADED watched=$($watchedLoaded.Count) system32=$(@($loaded | Where-Object { $_.Location -eq 'SYSTEM32' }).Count) windows-other=$(@($loaded | Where-Object { $_.Location -eq 'WINDOWS-OTHER' }).Count) other=$(@($loaded | Where-Object { $_.Location -eq 'OTHER' }).Count); OPEN-EXECUTE watched=$($watchedOpen.Count); SEARCH watched=$($watchedSearch.Count); FILE-OP watched=$($watchedFileOps.Count)"
        foreach ($item in @($watchedLoaded + $watchedOpen)) { Emit "      $($item.Op) $($item.Result) $($item.Path) [$($item.Access)]" }
        foreach ($item in $watchedSearch) { Emit "      (search) $($item.Op) $($item.Result) $($item.Path) [$($item.Access)]" }
        foreach ($item in $watchedFileOps) { Emit "      (file-op) $($item.Op) $($item.Result) $($item.Path) [$($item.Access)]" }
        foreach ($item in @($loaded | Where-Object { $_.Location -eq 'OTHER' -or $_.Location -eq 'WINDOWS-OTHER' })) { Emit "      (info) LOADED from $($item.Location): $($item.Path)" }
        foreach ($name in $ReportDll) {
            $hits = @($loaded | Where-Object { $_.Leaf -eq $name })
            if ($hits.Count -eq 0) { Emit "DLL-LOAD pid=$($entry.Pid) name=$name class=NOT-OBSERVED" }
            foreach ($hit in $hits) { Emit "DLL-LOAD pid=$($entry.Pid) name=$name class=$($hit.Location) path=$($hit.Path)" }
        }

        # Probe lines of this PID must match a LOADED event, or the capture missed it.
        foreach ($line in $probeLines) {
            $match = [regex]::Match($line, '^pid=(\d+)\t.*\tdll=(.*)$')
            if ($match.Success -and $match.Groups[1].Value -eq $entry.Pid) {
                $dll = $match.Groups[2].Value.ToLowerInvariant()
                if (-not (@($loaded | Where-Object { $_.Path.ToLowerInvariant() -eq $dll }).Count)) {
                    Emit "    INVALID: the probe log shows $dll in pid $($entry.Pid) but the capture has no Load Image for it"
                    Raise 'INVALID'
                }
            }
        }

        # Plants next to this PID's image.
        $imageDir = (Split-Path -Parent $entry.Image).TrimEnd('\').ToLowerInvariant() + '\'
        $relevant = @($PlantedPath | Where-Object {
            $plantDir = @(Get-Forms (Split-Path -Parent $_))
            @($imageDirForms | Where-Object { $plantDir -contains $_ }).Count -gt 0 -or $plantDir -contains $imageDir
        })
        $verified = @()
        $plantFail = $false
        foreach ($plant in $relevant) {
            $name = (Split-Path -Leaf $plant).ToLowerInvariant()
            $fromWatched = @($watchedLoaded | Where-Object { $_.Leaf -eq $name }).Count -gt 0
            $fromSystem = @($loaded | Where-Object { $_.Leaf -eq $name -and $_.Location -eq 'SYSTEM32' }).Count -gt 0
            $fromWinOther = @($loaded | Where-Object { $_.Leaf -eq $name -and $_.Location -eq 'WINDOWS-OTHER' }).Count -gt 0
            Emit "    plant $plant : loaded from WATCHED=$fromWatched SYSTEM32=$fromSystem WINDOWS-OTHER=$fromWinOther"
            if ($Role -eq 'control') { if ($fromWatched) { $controlLoads[$name] = $entry.Pid }; continue }
            if ($fromWatched) { $plantFail = $true }
            elseif ($fromSystem -and $controlValid.ContainsKey($name)) { $verified += $name }
            elseif ($fromSystem) { Emit "    ($name resolved to System32 but has no CONTROL-VALID report)" }
            elseif ($fromWinOther) { Emit "    ($name resolved under %WINDIR% but NOT System32 - not counted as verified)" }
        }
        if ($relevant.Count -eq 0 -and $Role -ne 'observation') { Emit '    no plant next to this image' }
        if ($Role -eq 'control') { continue }
        if ($Role -eq 'observation') {
            if ($watchedLoaded.Count -gt 0 -or $watchedOpen.Count -gt 0) { $roleVerdict = 'FAIL'; $roleReasons += "pid $($entry.Pid): DLL loaded/opened for execution from a watched directory" }
            continue
        }

        $verdict = 'PASS'
        if ($target.Hardened) {
            if ($watchedLoaded.Count -gt 0 -or $watchedOpen.Count -gt 0) { $verdict = 'FAIL'; $roleReasons += "pid $($entry.Pid): DLL loaded/opened for execution from a watched directory" }
            elseif ($watchedSearch.Count -gt 0) { $verdict = 'REVIEW'; $roleReasons += "pid $($entry.Pid): DLL lookups (not loads) in a watched directory" }
        } else {
            $crt = @(@($watchedLoaded + $watchedOpen) | Where-Object { $_.Leaf -match $crtPattern })
            if ($crt.Count -gt 0) { $verdict = 'FAIL'; $roleReasons += "pid $($entry.Pid): C/C++ runtime DLL loaded from a watched directory" }
            elseif (@($watchedLoaded + $watchedOpen + $watchedSearch).Count -gt 0) {
                Emit '    INFO: other DLL activity in watched directories (main process is not hardened at run time by design)'
            }
        }
        if ($plantFail) { $verdict = 'FAIL'; $roleReasons += "pid $($entry.Pid): planted DLL loaded" }
        if ($verified.Count -gt 0) { Emit "    verified against control: $($verified -join ',')"; $roleVerified += $verified }
        Emit "    pid verdict before verification: $verdict"
        if ($rank[$verdict] -gt $rank[$roleVerdict]) { $roleVerdict = $verdict }
    }
    if ($Role -eq 'control') { continue }
    if ($Role -eq 'observation') {
        Emit "  observation: $(if ($roleVerdict -eq 'FAIL') { 'FAIL (' + ($roleReasons -join '; ') + ')' } else { 'no DLL loaded from a watched directory' })"
        if ($roleVerdict -eq 'FAIL') { Raise 'FAIL' }
        continue
    }
    if ($FunctionalResult -eq 'FAIL') { $roleVerdict = 'FAIL'; $roleReasons += 'functional check failed' }
    if ($rank[$roleVerdict] -lt $rank['UNVERIFIED']) {
        if ($roleVerified.Count -eq 0) { $roleVerdict = 'UNVERIFIED'; $roleReasons += 'no plant next to the target resolved to System32 with a valid control' }
        elseif ($FunctionalResult -eq 'NOTRUN') { $roleVerdict = 'UNVERIFIED'; $roleReasons += 'functional check not run' }
    }
    Emit "  verdict: $roleVerdict$(if ($roleReasons) { ' (' + ($roleReasons -join '; ') + ')' })"
    Raise $roleVerdict
}

if ($Role -eq 'control') {
    Emit ''
    if ($overall -eq 'INVALID') { Finish 'INVALID' 30 }
    if ($controlLoads.Count -eq 0) {
        Finish "CONTROL-INVALID - no planted DLL ($($PlantedPath -join ';')) was loaded by a target from its directory" 31
    }
    foreach ($name in $controlLoads.Keys) { Emit "CONTROL-VALID name=$name pid=$($controlLoads[$name])" }
    Finish 'CONTROL-VALID' 0
}

# The scenario must have exercised the path it was built for: each required DLL
# had to be loaded from System32 by a matched target. A missing one means the
# code path (e.g. Restart Manager) never ran, so a clean capture is not a pass.
if ($RequireSystemLoad.Count -gt 0) {
    Emit ''
    foreach ($name in $RequireSystemLoad) {
        $hits = @($allLoads | Where-Object { $_.Leaf -eq $name })
        $fromSys32 = @($hits | Where-Object { $_.Location -eq 'SYSTEM32' })
        if ($fromSys32.Count -gt 0) {
            Emit "required load of $name : SYSTEM32 ($(@($fromSys32 | ForEach-Object { $_.Path }) -join '; '))"
        } elseif ($hits.Count -gt 0) {
            Emit "required load of $name : loaded but NOT from System32 ($(@($hits | ForEach-Object { $_.Location + ' ' + $_.Path }) -join '; '))"
            Emit "    REVIEW: the path ran but $name did not come from System32"
            Raise 'REVIEW'
        } else {
            Emit "required load of $name : NOT-OBSERVED"
            Emit "    INVALID: $name was never loaded by a target - the exercised path did not run"
            Raise 'INVALID'
        }
    }
}
$codes = @{ 'PASS' = 0; 'REVIEW' = 10; 'UNVERIFIED' = 11; 'FAIL' = 20; 'INVALID' = 30 }
if ($Role -eq 'observation') {
    # An observation never claims PASS; with nothing worse it is OBSERVED.
    if ($overall -eq 'PASS' -or $overall -eq 'UNVERIFIED') { Finish 'OBSERVED' 12 }
}
Finish $overall $codes[$overall]
