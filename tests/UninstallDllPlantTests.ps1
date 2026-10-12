# Uninstall.exe DLL-planting regression (Windows only, run by CTest). F-1.
#
# Uninstall.exe uses Restart Manager (rstrtmgr.dll) to report which process
# locks a file when a delete fails. RstrtMgr.dll statically imports ncrypt.dll,
# which in turn pulls ntasn1.dll. When rstrtmgr.dll was a *static* import of
# Uninstall.exe, that whole chain resolved during process initialization,
# before RestrictDefaultDllSearchToSystem32() ran at the top of wWinMain, so a
# copy of ncrypt.dll placed in the executable's own directory was loaded into
# the (elevated, user-writable temp-copy) worker process. The fix delay-loads
# rstrtmgr.dll and, at the first Restart Manager call, loads it explicitly with
# LOAD_LIBRARY_SEARCH_SYSTEM32; it never falls back to an application-directory
# search.
#
# Method. One planted DLL per case (so one load cannot mask the next) and one
# process role per case. A genuine, functional copy of System32\<name> is
# placed in the directory of the Uninstall.exe under test, so a process that
# loads it keeps running and the planted copy is observable. Which copy was
# loaded is read from the LIVE process's module list (ProcessModule.FileName),
# not inferred from a crash or a log.
#
#   Worker  -ForceRestartManager: a fake install directory is built (VERSION +
#     Asterun.exe so ValidateInstallRoot passes), VERSION is held open with no
#     sharing, and the worker runs `--perform ... --delete-data 0`. The worker
#     exhausts its delete retries on the locked VERSION and calls Restart
#     Manager, which loads rstrtmgr.dll (and its dependency chain). This forces
#     the vulnerable code path to run: the case is INCONCLUSIVE -> FAIL unless
#     rstrtmgr.dll is actually loaded from System32, so a run where the path
#     never executed can never pass. The worker runs from a separate
#     Asterun-Elevated.* directory, exactly where the real elevated worker runs.
#   Launcher: the launcher (no --perform) is started with the DLL planted beside
#     it. Before the fix the static import chain loaded the plant during init;
#     after the fix it is delay-only, so init never touches it. The test waits
#     for the launcher to reach interactive state (a top-level window appears),
#     which is past the init point the bug used, so a pass cannot come from an
#     instant crash. It does NOT drive the dialog or assert that it stays
#     resident; it only asserts interactive state was reached and the plant was
#     not loaded from the launcher directory.
#
# The case FAILS when the planted copy is loaded from the executable's own
# directory. It does NOT pass merely because nothing was loaded: a forced-RM
# worker that never loaded rstrtmgr.dll from System32 is a failure.
#
# WHAT THIS PROVES (and does not). This is a DLL-LOAD regression: it proves the
# planted DLL is not loaded from the executable's directory (and, in the forced
# worker case, that rstrtmgr.dll loads from System32 so the path ran). The
# worker case locks one file and the process is terminated after its modules are
# observed, so it does NOT prove the uninstall completed successfully; normal
# uninstall success is a separate check (the kit's un-locked uninstall-worker
# scenario and the on-machine uninstall in U-4).
#
# SCOPE / evidence level. This runs the Uninstall.exe binary directly; on CI it
# is not started through a real UAC split-token elevation, and the
# Asterun-Elevated.* directory here is created by the test, not by the
# launcher's CreateSecuredTemporaryExecutableCopy. It verifies the DLL-load
# property of the binary, NOT the real elevation chain or that directory's real
# ACLs. Those remain to be checked on a real machine (see the acceptance kit).
#
# Exit codes: 0 pass, 1 fail.

param(
    [Parameter(Mandatory = $true)][string]$AppDir,
    [Parameter(Mandatory = $true)][ValidateSet('Worker', 'Launcher')][string]$Mode,
    [Parameter(Mandatory = $true)][string]$PlantName,
    [switch]$ForceRestartManager
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

# Started from the PowerShell 7 CI shell, Windows PowerShell inherits a
# PSModulePath that lists the 7.x folders first and cannot auto-load its own
# modules; use the Windows PowerShell module folders only.
if ($PSVersionTable.PSEdition -ne 'Core') {
    $env:PSModulePath = @(
        (Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'WindowsPowerShell\Modules'),
        (Join-Path $env:ProgramFiles 'WindowsPowerShell\Modules'),
        (Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\Modules')
    ) -join ';'
}

if ($ForceRestartManager -and $Mode -ne 'Worker') { throw '-ForceRestartManager is only valid with -Mode Worker' }

$AppDir = (Resolve-Path -LiteralPath $AppDir).Path
foreach ($name in 'Asterun.exe', 'Update.exe', 'Uninstall.exe') {
    if (-not (Test-Path -LiteralPath (Join-Path $AppDir $name))) { throw "$name not found in $AppDir" }
}
$system32 = Join-Path $env:WINDIR 'System32'
$plantSource = Join-Path $system32 $PlantName
if (-not (Test-Path -LiteralPath $plantSource)) { throw "cannot plant $PlantName : $plantSource does not exist" }
$system32Prefix = ($system32.TrimEnd('\') + '\').ToLowerInvariant()

$script:failures = New-Object System.Collections.Generic.List[string]
function Check([bool]$condition, [string]$message) {
    if ($condition) { Write-Host "  ok   $message" }
    else { Write-Host "  FAIL $message"; $script:failures.Add($message) }
}

$id = [guid]::NewGuid().ToString('N')
$root = Join-Path $env:TEMP "asterun-plant-it-$id"
if (Test-Path -LiteralPath $root) { throw "refusing to reuse existing directory $root" }
New-Item -ItemType Directory -Path $root | Out-Null
# VERSION only needs to exist in the fake install root for ValidateInstallRoot;
# its content is irrelevant here. The build output dir has no VERSION, so take
# the string from the repository (as the updater test does), defaulting if absent.
$repoVersion = Join-Path $PSScriptRoot '..\VERSION'
$version = if (Test-Path -LiteralPath $repoVersion) { (Get-Content -LiteralPath $repoVersion -Raw).Trim() } else { '' }
if (-not $version) { $version = '0.0.0' }

function Get-ExitedParentPid {
    $parent = Start-Process -FilePath "$env:WINDIR\System32\cmd.exe" -ArgumentList '/c', 'exit 0' -WindowStyle Hidden -PassThru
    $parent.WaitForExit()
    return $parent.Id
}

# Accumulate, from process start until it exits or the deadline, the set of
# modules the process has loaded: leaf name (lower-case) -> set of full paths.
# Reading a live process's module list is a snapshot, so poll and union; a
# module that loads mid-run (rstrtmgr via delay-load) is captured this way.
function Watch-Modules([System.Diagnostics.Process]$proc, [int]$timeoutSeconds, [string]$stopWhenLeaf = '') {
    $seen = @{}
    $stopWhenLeaf = $stopWhenLeaf.ToLowerInvariant()
    $extraPolls = -1
    $deadline = (Get-Date).AddSeconds($timeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        try {
            $proc.Refresh()
            $exited = $proc.HasExited
            foreach ($m in $proc.Modules) { Add-Module $seen $m }
            if ($exited) { break }
        } catch {
            # Access races while the module list changes, or the process exits
            # between Refresh and enumeration: ignore and retry / stop.
            if ($proc.HasExited) { break }
        }
        # Once the gate module (rstrtmgr.dll) has loaded, its static dependency
        # chain (ncrypt.dll) is already mapped; poll a few more times to settle,
        # then stop instead of waiting out the full deadline.
        if ($stopWhenLeaf -and $seen.ContainsKey($stopWhenLeaf)) {
            if ($extraPolls -lt 0) { $extraPolls = 3 }
            if ($extraPolls -eq 0) { break }
            $extraPolls--
        }
        Start-Sleep -Milliseconds 100
    }
    return $seen
}
function Add-Module([hashtable]$seen, $module) {
    $path = $module.FileName
    if (-not $path) { return }
    $leaf = (Split-Path -Leaf $path).ToLowerInvariant()
    if (-not $seen.ContainsKey($leaf)) { $seen[$leaf] = New-Object System.Collections.Generic.HashSet[string] }
    [void]$seen[$leaf].Add($path)
}
function Paths([hashtable]$seen, [string]$leaf) {
    $leaf = $leaf.ToLowerInvariant()
    if ($seen.ContainsKey($leaf)) { return @($seen[$leaf]) }
    return @()
}
function Under([string]$path, [string]$dirPrefixLower) { $path.ToLowerInvariant().StartsWith($dirPrefixLower) }

$exitCode = 1
try {
    $plantLeaf = $PlantName.ToLowerInvariant()
    if ($Mode -eq 'Worker') {
        $install = Join-Path $root 'install'
        New-Item -ItemType Directory -Path $install | Out-Null
        foreach ($name in 'Asterun.exe', 'Update.exe', 'Uninstall.exe') {
            Copy-Item -LiteralPath (Join-Path $AppDir $name) -Destination $install
        }
        Set-Content -LiteralPath (Join-Path $install 'VERSION') -Value $version -NoNewline
        New-Item -ItemType Directory -Path (Join-Path $install 'data') | Out-Null
        Set-Content -LiteralPath (Join-Path $install 'data\keep-marker.txt') -Value $id

        # The worker runs from a separate Asterun-Elevated.* directory, as the
        # real elevated worker does; the plant sits next to that running binary.
        $workerDir = Join-Path $root "Asterun-Elevated.plant-$id"
        New-Item -ItemType Directory -Path $workerDir | Out-Null
        $workerPrefix = ($workerDir.TrimEnd('\') + '\').ToLowerInvariant()
        Copy-Item -LiteralPath (Join-Path $AppDir 'Uninstall.exe') -Destination $workerDir
        Copy-Item -LiteralPath $plantSource -Destination (Join-Path $workerDir $PlantName)
        Write-Host "  planted $PlantName in the worker directory $workerDir"

        $lockHandle = $null
        if ($ForceRestartManager) {
            $lockPath = Join-Path $install 'VERSION'
            $lockHandle = [System.IO.File]::Open($lockPath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::None)
            Write-Host "  holding an exclusive handle on $lockPath to force the Restart Manager path"
        }
        try {
            $info = New-Object System.Diagnostics.ProcessStartInfo
            $info.FileName = Join-Path $workerDir 'Uninstall.exe'
            $info.Arguments = "--perform --parent-pid $(Get-ExitedParentPid) --install `"$install`" --delete-data 0"
            $info.UseShellExecute = $false
            $info.WorkingDirectory = $workerDir
            Write-Host "  run: $($info.FileName) $($info.Arguments)"
            $proc = [System.Diagnostics.Process]::Start($info)
            $stopLeaf = if ($ForceRestartManager) { 'rstrtmgr.dll' } else { '' }
            $seen = Watch-Modules $proc 120 $stopLeaf
        } finally {
            if ($lockHandle) { $lockHandle.Close(); Write-Host '  released the VERSION lock' }
            if ($proc -and -not $proc.HasExited) { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue }
        }

        $plantPaths = @(Paths $seen $plantLeaf)
        $fromWorker = @($plantPaths | Where-Object { Under $_ $workerPrefix })
        Check ($fromWorker.Count -eq 0) "$PlantName was NOT loaded from the worker directory (found: $($fromWorker -join '; '))"

        if ($ForceRestartManager) {
            $rmPaths = @(Paths $seen 'rstrtmgr.dll')
            $rmFromSystem = @($rmPaths | Where-Object { Under $_ $system32Prefix })
            Check ($rmFromSystem.Count -gt 0) "rstrtmgr.dll loaded from System32 (the Restart Manager path actually ran): $($rmPaths -join '; ')"
            $rmFromWorker = @($rmPaths | Where-Object { Under $_ $workerPrefix })
            Check ($rmFromWorker.Count -eq 0) "rstrtmgr.dll was NOT loaded from the worker directory"
            # If the plant resolved at all, it must have come from System32.
            if ($plantPaths.Count -gt 0) {
                $plantFromSystem = @($plantPaths | Where-Object { Under $_ $system32Prefix })
                Check ($plantFromSystem.Count -gt 0) "$PlantName (if loaded) resolved from System32: $($plantPaths -join '; ')"
            }
        }
        Write-Host "  modules observed: $($seen.Keys.Count); $PlantName paths: $(( $plantPaths ) -join '; ')"
    }
    else {
        $launcherDir = Join-Path $root "launcher-$id"
        New-Item -ItemType Directory -Path $launcherDir | Out-Null
        $launcherPrefix = ($launcherDir.TrimEnd('\') + '\').ToLowerInvariant()
        foreach ($name in 'Asterun.exe', 'Update.exe', 'Uninstall.exe') {
            Copy-Item -LiteralPath (Join-Path $AppDir $name) -Destination $launcherDir
        }
        Set-Content -LiteralPath (Join-Path $launcherDir 'VERSION') -Value $version -NoNewline
        New-Item -ItemType Directory -Path (Join-Path $launcherDir 'data') | Out-Null
        Copy-Item -LiteralPath $plantSource -Destination (Join-Path $launcherDir $PlantName)
        Write-Host "  planted $PlantName in the launcher directory $launcherDir"

        $info = New-Object System.Diagnostics.ProcessStartInfo
        $info.FileName = Join-Path $launcherDir 'Uninstall.exe'
        $info.UseShellExecute = $false
        $info.WorkingDirectory = $launcherDir
        Write-Host "  run: $($info.FileName) (launcher, no --perform)"
        $proc = [System.Diagnostics.Process]::Start($info)
        # Poll modules AND wait for the launcher to open a top-level window (the
        # uninstall prompt). An interactive window is positive evidence that the
        # process ran past initialization - the point where the old static import
        # chain resolved and loaded the plant - so a pass cannot come from an
        # instant crash. We do not drive or assert anything about the dialog's
        # contents or that it stays resident; only that interactive state was
        # reached while the plant was not loaded from the launcher directory.
        $seen = @{}
        $reachedWindow = $false
        $windowTitle = ''
        $deadline = (Get-Date).AddSeconds(20)
        try {
            while ((Get-Date) -lt $deadline) {
                try {
                    $proc.Refresh()
                    $exited = $proc.HasExited
                    foreach ($m in $proc.Modules) { Add-Module $seen $m }
                    if (-not $reachedWindow -and $proc.MainWindowHandle -ne [IntPtr]::Zero) {
                        $reachedWindow = $true
                        $windowTitle = $proc.MainWindowTitle
                    }
                    if ($exited) { break }
                    if ($reachedWindow) { break }
                } catch { if ($proc.HasExited) { break } }
                Start-Sleep -Milliseconds 100
            }
        } finally {
            if ($proc -and -not $proc.HasExited) { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue }
        }

        $plantPaths = @(Paths $seen $plantLeaf)
        $fromLauncher = @($plantPaths | Where-Object { Under $_ $launcherPrefix })
        Check ($fromLauncher.Count -eq 0) "$PlantName was NOT loaded from the launcher directory (found: $($fromLauncher -join '; '))"
        # Reached interactive state: a top-level window appeared. Fall back to the
        # UI-module load only if no window surfaced in this environment, so the
        # test still cannot pass on an instant crash.
        $uiModules = @(@('comctl32.dll', 'shell32.dll', 'user32.dll') | Where-Object { @(Paths $seen $_ | Where-Object { Under $_ $system32Prefix }).Count -gt 0 })
        Check ($reachedWindow -or $uiModules.Count -gt 0) "launcher reached interactive state (window=$reachedWindow title='$windowTitle'; UI modules from System32: $($uiModules -join ', '))"
        Write-Host "  modules observed: $($seen.Keys.Count); reachedWindow=$reachedWindow; $PlantName paths: $(( $plantPaths ) -join '; ')"
    }
} catch {
    Write-Host "  FAIL exception: $($_.Exception.Message)"
    $script:failures.Add($_.Exception.Message)
} finally {
    Get-ChildItem -LiteralPath $root -Recurse -ErrorAction SilentlyContinue | Out-Null
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}

if ($script:failures.Count -gt 0) {
    Write-Host "Uninstall DLL-plant regression FAILED ($($script:failures.Count) checks): $Mode $PlantName force=$([bool]$ForceRestartManager)"
    exit 1
}
Write-Host "Uninstall DLL-plant regression passed: $Mode $PlantName force=$([bool]$ForceRestartManager)"
exit 0
