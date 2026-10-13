# Update.exe execution-chain integration test (Windows only, run by CTest).
#
# Exercises the Update.exe built in this tree end to end on throw-away
# directories: local archive SHA-256 verification, ValidateSource, file
# replacement with a separate backup directory, launch of the new
# Asterun.exe with --post-update-health-event, the health signal, and the
# success cleanup. Nothing outside the directories created here is touched:
# no existing installation, no product version rule, no network.
#
# This is NOT the client's online update discovery (manifest download,
# version comparison, asset download); that path is not exercised here.
#
# Modes:
#   Portable  - writable install under %TEMP% (--secure-reextract 0), plus two
#               negative cases: wrong SHA-256 and VERSION mismatch must leave
#               the install unchanged.
#   Protected - install directory created under %ProgramFiles% (admin-only
#               ACL), --secure-reextract 1: the updater must be elevated,
#               re-extracts the archive into a protected work root and
#               deletes the user-writable archive. Exits 77 (skipped) when the
#               test process is not elevated; CI treats a skipped test as a
#               failure, so CI always runs it.
#
# Exit codes: 0 pass, 1 fail, 77 skipped (Protected without elevation).
#
# -PlantDll (optional, used only by the external batch A acceptance kit): a
# DLL copied, for the successful Portable case only, as bcrypt.dll next to the
# Update.exe copy and as version.dll into the package, so that a Process
# Monitor capture can show where both executables resolve those static
# imports. CTest never passes it.

param(
    [Parameter(Mandatory = $true)][string]$AppDir,
    [Parameter(Mandatory = $true)][ValidateSet('Portable', 'Protected')][string]$Mode,
    [string]$PlantDll = '',
    # VERSION of the package under test; defaults to the repository VERSION.
    [string]$VersionFile = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

# Started from PowerShell 7 (as the CI shell does), Windows PowerShell inherits
# a PSModulePath that lists the PowerShell 7 module folders first, and then
# cannot auto-load its own Microsoft.PowerShell.Utility (Get-FileHash) or
# CimCmdlets. Use the Windows PowerShell module folders only.
if ($PSVersionTable.PSEdition -ne 'Core') {
    $env:PSModulePath = @(
        (Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'WindowsPowerShell\Modules'),
        (Join-Path $env:ProgramFiles 'WindowsPowerShell\Modules'),
        (Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\Modules')
    ) -join ';'
}
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
if ($PlantDll) {
    if ($Mode -ne 'Portable') { throw '-PlantDll is only supported with -Mode Portable' }
    $PlantDll = (Resolve-Path -LiteralPath $PlantDll).Path
}

$script:failures = New-Object System.Collections.Generic.List[string]
function Check([bool]$condition, [string]$message) {
    if ($condition) { Write-Output "  ok   $message" }
    else { Write-Output "  FAIL $message"; $script:failures.Add($message) }
}

# Test-only Win32 bridge: close only MessageBox windows owned by the updater
# process, so failure-path integration runs never depend on human clicks.
Add-Type -Namespace AsterunUpdaterIt -Name DialogControl -MemberDefinition @'
[System.Runtime.InteropServices.DllImport("user32.dll", CharSet = System.Runtime.InteropServices.CharSet.Unicode)]
public static extern System.IntPtr FindWindow(string cls, string title);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern uint GetWindowThreadProcessId(System.IntPtr window, out uint pid);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool PostMessage(System.IntPtr window, uint msg, System.IntPtr wParam, System.IntPtr lParam);
'@

function Test-Elevated {
    $principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if ($Mode -eq 'Protected' -and -not (Test-Elevated)) {
    Write-Output 'SKIPPED: the protected-install case needs an elevated test process'
    exit 77
}

$AppDir = (Resolve-Path -LiteralPath $AppDir).Path
foreach ($name in 'Asterun.exe', 'Update.exe', 'Uninstall.exe') {
    if (-not (Test-Path -LiteralPath (Join-Path $AppDir $name))) { throw "$name not found in $AppDir" }
}
if (-not $VersionFile) { $VersionFile = Join-Path $PSScriptRoot '..\VERSION' }
$version = (Get-Content -LiteralPath $VersionFile -Raw).Trim()
if (-not $version) { throw 'VERSION is empty' }

$id = [guid]::NewGuid().ToString('N')
$root = Join-Path $env:TEMP "asterun-updater-it-$id"
if (Test-Path -LiteralPath $root) { throw "refusing to reuse existing directory $root" }
New-Item -ItemType Directory -Path $root | Out-Null
Set-Content -LiteralPath (Join-Path $root '.asterun-updater-it') -Value $id
$createdDirectories = New-Object System.Collections.Generic.List[string]
$createdDirectories.Add($root)

function New-OwnedDirectory([string]$path) {
    if (Test-Path -LiteralPath $path) { throw "refusing to reuse existing directory $path" }
    New-Item -ItemType Directory -Path $path | Out-Null
    Set-Content -LiteralPath (Join-Path $path '.asterun-updater-it') -Value $id
    $createdDirectories.Add($path)
}

function Copy-Product([string]$destination, [string]$readme, [string]$extraName) {
    New-Item -ItemType Directory -Force -Path $destination | Out-Null
    foreach ($name in 'Asterun.exe', 'Update.exe', 'Uninstall.exe') {
        Copy-Item -LiteralPath (Join-Path $AppDir $name) -Destination $destination
    }
    foreach ($folder in 'dict', 'third_party') {
        $source = Join-Path $AppDir $folder
        if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination $destination -Recurse }
    }
    Set-Content -LiteralPath (Join-Path $destination 'VERSION') -Value $version -NoNewline
    Set-Content -LiteralPath (Join-Path $destination 'README.md') -Value $readme -NoNewline
    Set-Content -LiteralPath (Join-Path $destination $extraName) -Value $readme -NoNewline
}

function Stop-InstanceFrom([string]$install) {
    $prefix = (Join-Path $install 'Asterun.exe').ToLowerInvariant()
    Get-CimInstance Win32_Process -Filter "Name = 'Asterun.exe'" | Where-Object {
        $_.ExecutablePath -and $_.ExecutablePath.ToLowerInvariant() -eq $prefix
    } | ForEach-Object {
        Write-Output "  stopping Asterun.exe pid=$($_.ProcessId) cmd=$($_.CommandLine)"
        Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue
    }
}

function Get-ExitedParentPid {
    $parent = Start-Process -FilePath "$env:WINDIR\System32\cmd.exe" -ArgumentList '/c', 'exit 0' -WindowStyle Hidden -PassThru
    $parent.WaitForExit()
    return $parent.Id
}

function Invoke-Updater([hashtable]$case) {
    $worker = Join-Path $root "Asterun-Elevated.updater-it-$($case.Name)"
    New-Item -ItemType Directory -Path $worker | Out-Null
    $updater = Join-Path $worker 'Update.exe'
    Copy-Item -LiteralPath (Join-Path $AppDir 'Update.exe') -Destination $updater
    if ($case.ContainsKey('Plant')) {
        Copy-Item -LiteralPath $PlantDll -Destination (Join-Path $worker 'bcrypt.dll')
        Write-Host "  planted: $(Join-Path $worker 'bcrypt.dll')"
    }

    $arguments = @(
        '--apply',
        '--parent-pid', (Get-ExitedParentPid),
        '--source', "`"$($case.Source)`"",
        '--archive', "`"$($case.Archive)`"",
        '--sha256', $case.Sha256,
        '--secure-reextract', $case.Secure,
        '--install', "`"$($case.Install)`"",
        '--backup', "`"$($case.Backup)`"",
        '--version', "`"$($case.Version)`"",
        '--health-event', "`"$($case.HealthEvent)`""
    )
    Write-Host "  run: $updater $($arguments -join ' ')"  # not Write-Output: this function returns the exit code
    $process = Start-Process -FilePath $updater -ArgumentList $arguments -PassThru
    $null = $process.Handle  # keep the handle so ExitCode is available after exit
    if ($case.ContainsKey('ExpectFailureDialog')) {
        $case.DialogObserved = $false
        $ticks = 0
        while (-not $process.WaitForExit(200)) {
            $dialog = [AsterunUpdaterIt.DialogControl]::FindWindow('#32770', 'Asterun')
            if ($dialog -ne [IntPtr]::Zero) {
                [uint32]$dialogPid = 0
                [void][AsterunUpdaterIt.DialogControl]::GetWindowThreadProcessId($dialog, [ref]$dialogPid)
                if ($dialogPid -eq [uint32]$process.Id) {
                    $case.DialogObserved = $true
                    # WM_COMMAND / IDOK; no window outside this worker is touched.
                    [void][AsterunUpdaterIt.DialogControl]::PostMessage(
                        $dialog, 0x0111, [IntPtr]1, [IntPtr]::Zero)
                }
            }
            $ticks += 1
            if ($ticks -ge 900) {
                Stop-Process -Id $process.Id -Force
                throw "Update.exe failure dialog did not complete ($($case.Name))"
            }
        }
    } elseif (-not $process.WaitForExit(180000)) {
        Stop-Process -Id $process.Id -Force
        throw "Update.exe did not exit within 180 s ($($case.Name))"
    }
    return $process.ExitCode
}

# Build the package archive with explicit '/'-separated entry names, as the
# release packaging (Compress-Archive in PowerShell 7) produces them. Windows
# PowerShell 5.1 runs on .NET Framework with the legacy ZipFile behaviour, so
# its Compress-Archive and ZipFile.CreateFromDirectory write '\' separators,
# which the updater's archive validation rejects as unsafe member names.
function New-PackageArchive([string]$source, [string]$archive) {
    $zip = [System.IO.Compression.ZipFile]::Open($archive, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        # Same path normalisation as the enumerated children (8.3 vs long form).
        $prefix = (Get-Item -LiteralPath $source).FullName.TrimEnd('\').Length + 1
        foreach ($file in Get-ChildItem -LiteralPath $source -Recurse -File -Force) {
            $name = $file.FullName.Substring($prefix).Replace('\', '/')
            [void][System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $file.FullName, $name)
        }
    } finally { $zip.Dispose() }
    $zip = [System.IO.Compression.ZipFile]::OpenRead($archive)
    try {
        $bad = @($zip.Entries | Where-Object { $_.FullName.Contains('\') })
        if ($bad.Count -gt 0) { throw "archive entry with '\': $($bad[0].FullName)" }
    } finally { $zip.Dispose() }
}

function New-Case([string]$name, [string]$installBase, [bool]$plant = $false) {
    # Every case uses the same argument construction; the successful case
    # proves it is accepted, so an exit code 2 in a negative case comes from
    # the gate under test, not from a malformed command line.
    $case = @{ Name = $name; Secure = '0' }
    $case.Source = Join-Path $root "$name-source"
    $case.Install = Join-Path $installBase "$name-install"
    $case.Backup = Join-Path $root "$name-backup"
    $case.Archive = Join-Path $root "$name-package.zip"
    $case.Version = $version
    $case.HealthEvent = "Local\Aspeternity.Asterun.UpdateHealth.it$id$name"
    $case.OldReadme = "updater-it old $id"
    $case.NewReadme = "updater-it new $id"

    Copy-Product $case.Source $case.NewReadme 'new-only.txt'
    if ($plant) {
        $case.Plant = $true
        Copy-Item -LiteralPath $PlantDll -Destination (Join-Path $case.Source 'version.dll')
        Write-Host "  planted: $(Join-Path $case.Install 'version.dll') (arrives with the package)"
    }
    if ($case.Install.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) {
        Copy-Product $case.Install $case.OldReadme 'old-only.txt'
    } else {
        New-OwnedDirectory $case.Install
        Copy-Product $case.Install $case.OldReadme 'old-only.txt'
    }
    New-PackageArchive $case.Source $case.Archive
    $case.Sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $case.Archive).Hash.ToLowerInvariant()
    return $case
}

# Facts the updater checks before it changes anything, printed when a case
# fails so a CI log shows which precondition did not hold.
function Write-Preconditions([hashtable]$case) {
    if (-not ('AsterunUpdaterIt.Token' -as [type])) {
        Add-Type -Namespace AsterunUpdaterIt -Name Token -MemberDefinition @'
[System.Runtime.InteropServices.DllImport("advapi32.dll", SetLastError = true)]
public static extern bool GetTokenInformation(System.IntPtr token, int infoClass, out int info, int length, out int returned);
'@
    }
    $elevated = 0; $type = 0; $returned = 0
    $token = [Security.Principal.WindowsIdentity]::GetCurrent().Token
    [void][AsterunUpdaterIt.Token]::GetTokenInformation($token, 20, [ref]$elevated, 4, [ref]$returned)  # TokenElevation
    [void][AsterunUpdaterIt.Token]::GetTokenInformation($token, 18, [ref]$type, 4, [ref]$returned)      # TokenElevationType
    Write-Output "  diag: test process TokenIsElevated=$elevated TokenElevationType=$type"
    foreach ($path in $case.Archive, $case.Install, $case.Source) {
        $current = $path
        while ($current) {
            $item = Get-Item -LiteralPath $current -Force -ErrorAction SilentlyContinue
            if ($item) {
                $reparse = [bool]($item.Attributes -band [IO.FileAttributes]::ReparsePoint)
                if ($reparse -or $current -eq $path) { Write-Output "  diag: $current attributes=$($item.Attributes) reparse=$reparse" }
            } else { Write-Output "  diag: $current missing" }
            $parent = Split-Path -Parent $current
            if ($parent -eq $current) { break }
            $current = $parent
        }
    }
}

function Assert-Unchanged([hashtable]$case, [int]$exitCode, [int]$expected, [bool]$expectRestart = $false) {
    Check ($exitCode -eq $expected) "$($case.Name): exit code $exitCode (expected $expected)"
    $readme = Get-Content -LiteralPath (Join-Path $case.Install 'README.md') -Raw
    Check ($readme -eq $case.OldReadme) "$($case.Name): install README.md unchanged"
    Check (-not (Test-Path -LiteralPath (Join-Path $case.Install 'new-only.txt'))) "$($case.Name): no file from the package was copied"
    Check (Test-Path -LiteralPath $case.Source) "$($case.Name): staged source kept"
    Check (Test-Path -LiteralPath $case.Archive) "$($case.Name): archive kept"
    $running = @(Get-CimInstance Win32_Process -Filter "Name = 'Asterun.exe'" | Where-Object {
        $_.ExecutablePath -and $_.ExecutablePath -ieq (Join-Path $case.Install 'Asterun.exe') })
    Check ($running.Count -eq [int]$expectRestart) "$($case.Name): previous Asterun launch expectation ($expectRestart)"
    if ($case.ContainsKey('ExpectFailureDialog')) {
        Check $case.DialogObserved "$($case.Name): updater displayed a failure message"
    }
}

function Assert-Updated([hashtable]$case, [int]$exitCode) {
    Check ($exitCode -eq 0) "$($case.Name): exit code $exitCode (expected 0: files replaced, health signal received)"
    if ($exitCode -ne 0) { Write-Preconditions $case }
    $readme = Get-Content -LiteralPath (Join-Path $case.Install 'README.md') -Raw
    Check ($readme -eq $case.NewReadme) "$($case.Name): install README.md replaced by the package version"
    Check (Test-Path -LiteralPath (Join-Path $case.Install 'new-only.txt')) "$($case.Name): new package file copied"
    Check ((Get-FileHash -LiteralPath (Join-Path $case.Install 'Asterun.exe')).Hash -eq
           (Get-FileHash -LiteralPath (Join-Path $AppDir 'Asterun.exe')).Hash) "$($case.Name): Asterun.exe matches the package"
    if ($case.ContainsKey('PreservedBackup')) {
        Check ((Test-Path -LiteralPath $case.PreservedBackup) -and
            ((Get-Content -LiteralPath $case.PreservedBackup -Raw) -eq 'previous recovery')) "$($case.Name): unrelated pre-existing recovery kept"
    } else {
        Check (-not (Test-Path -LiteralPath $case.Backup)) "$($case.Name): backup directory removed after success"
    }
    Check (-not (Test-Path -LiteralPath $case.Source)) "$($case.Name): staged source removed after success"
    Check (-not (Test-Path -LiteralPath $case.Archive)) "$($case.Name): archive removed after success"
    $work = @(Get-ChildItem -LiteralPath $case.Install -Force -Directory -Filter '.asterun-update-work.*')
    Check ($work.Count -eq 0) "$($case.Name): no protected work root left in the install"
    $started = @(Get-CimInstance Win32_Process -Filter "Name = 'Asterun.exe'" | Where-Object {
        $_.ExecutablePath -and $_.ExecutablePath -ieq (Join-Path $case.Install 'Asterun.exe') -and
        $_.CommandLine -like "*--post-update-health-event*$($case.HealthEvent)*" })
    Check ($started.Count -ge 1) "$($case.Name): new Asterun.exe running from the install with --post-update-health-event"
}

$exitCodeToReturn = 0
try {
    Write-Output "Updater integration test, mode=$Mode, version=$version, appDir=$AppDir, root=$root"
    if ($Mode -eq 'Portable') {
        $case = New-Case 'portable' $root ([bool]$PlantDll)
        $code = Invoke-Updater $case
        Assert-Updated $case $code
        Stop-InstanceFrom $case.Install

        $case = New-Case 'existingbackup' $root
        New-Item -ItemType Directory -Path $case.Backup | Out-Null
        $prior = Join-Path $case.Backup 'recovery-errors.txt'
        Set-Content -LiteralPath $prior -Value 'previous recovery' -NoNewline
        $case.ExpectFailureDialog = $true
        Assert-Unchanged $case (Invoke-Updater $case) 3 $true
        Stop-InstanceFrom $case.Install
        Check ((Test-Path -LiteralPath $prior) -and
            ((Get-Content -LiteralPath $prior -Raw) -eq 'previous recovery')) 'existingbackup: prior recovery must remain'

        $case = New-Case 'badsha' $root
        $case.Sha256 = ('0' * 64)
        $case.ExpectFailureDialog = $true
        Assert-Unchanged $case (Invoke-Updater $case) 2 $true
        Stop-InstanceFrom $case.Install

        $case = New-Case 'badversion' $root
        $case.Version = "$version-mismatch"
        $case.ExpectFailureDialog = $true
        Assert-Unchanged $case (Invoke-Updater $case) 2 $true
        Stop-InstanceFrom $case.Install

        # Fail after the first file replacement. A complete rollback must
        # restore the old product AND restart it; no new package files remain.
        $case = New-Case 'midapply' $root
        Set-Content -LiteralPath (Join-Path $case.Source 'zz-conflict.txt') -Value 'new' -NoNewline
        New-Item -ItemType Directory -Path (Join-Path $case.Install 'zz-conflict.txt') | Out-Null
        Remove-Item -LiteralPath $case.Archive
        New-PackageArchive $case.Source $case.Archive
        $case.Sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $case.Archive).Hash.ToLowerInvariant()
        $case.ExpectFailureDialog = $true
        Assert-Unchanged $case (Invoke-Updater $case) 3 $true
        Stop-InstanceFrom $case.Install
    } else {
        $programFiles = $env:ProgramFiles
        $base = Join-Path $programFiles "AsterunUpdaterIT-$id"
        New-OwnedDirectory $base
        $case = New-Case 'protected' $base
        $case.Secure = '1'
        # Secure re-extract uses its own protected work root, not --backup.
        # A successful update must not delete unrelated old recovery there.
        New-Item -ItemType Directory -Path $case.Backup | Out-Null
        $case.PreservedBackup = Join-Path $case.Backup 'recovery-errors.txt'
        Set-Content -LiteralPath $case.PreservedBackup -Value 'previous recovery' -NoNewline
        $code = Invoke-Updater $case
        Assert-Updated $case $code
        Stop-InstanceFrom $case.Install

        # The previous EXE cannot be launched after rollback. The updater must
        # report this and retain protected recovery files instead of deleting
        # its temporary protected work root before checking launch success.
        $case = New-Case 'restartfailed' $base
        $case.Secure = '1'
        $case.ExpectFailureDialog = $true
        Set-Content -LiteralPath (Join-Path $case.Install 'Asterun.exe') -Value 'invalid-old-app' -NoNewline
        Set-Content -LiteralPath (Join-Path $case.Source 'zz-conflict.txt') -Value 'new' -NoNewline
        New-Item -ItemType Directory -Path (Join-Path $case.Install 'zz-conflict.txt') | Out-Null
        Remove-Item -LiteralPath $case.Archive
        New-PackageArchive $case.Source $case.Archive
        $case.Sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $case.Archive).Hash.ToLowerInvariant()
        $code = Invoke-Updater $case
        Check ($code -eq 7) 'restartfailed: correctly reports failed old-version relaunch'
        Check $case.DialogObserved 'restartfailed: failure message displayed'
        Check ((Get-Content -LiteralPath (Join-Path $case.Install 'Asterun.exe') -Raw) -eq 'invalid-old-app') 'restartfailed: rollback restored original bytes'
        $protected = @(Get-ChildItem -LiteralPath $case.Install -Force -Directory -Filter '.asterun-update-work.*')
        Check ($protected.Count -eq 1) 'restartfailed: protected recovery root retained'
        if ($protected.Count -eq 1) {
            $saved = Join-Path $protected[0].FullName 'backup/Asterun.exe'
            Check ((Test-Path -LiteralPath $saved) -and
                ((Get-Content -LiteralPath $saved -Raw) -eq 'invalid-old-app')) 'restartfailed: backed-up executable retained'
        }
    }
} catch {
    Write-Output "  FAIL exception: $($_.Exception.Message)"
    $script:failures.Add($_.Exception.Message)
} finally {
    foreach ($process in @(Get-CimInstance Win32_Process -Filter "Name = 'Asterun.exe'")) {
        if (-not $process.ExecutablePath) { continue }
        foreach ($dir in $createdDirectories) {
            if ($process.ExecutablePath.StartsWith($dir, [StringComparison]::OrdinalIgnoreCase)) {
                Stop-Process -Id $process.ProcessId -Force -ErrorAction SilentlyContinue
            }
        }
    }
    Start-Sleep -Milliseconds 500
    # Remove only directories this run created, identified by their marker.
    for ($i = $createdDirectories.Count - 1; $i -ge 0; --$i) {
        $dir = $createdDirectories[$i]
        $marker = Join-Path $dir '.asterun-updater-it'
        if ((Test-Path -LiteralPath $marker) -and ((Get-Content -LiteralPath $marker -Raw).Trim() -eq $id)) {
            Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}

if ($script:failures.Count -gt 0) {
    Write-Output "Updater integration test FAILED ($($script:failures.Count) checks)"
    exit 1
}
Write-Output "Updater integration test passed ($Mode)"
exit 0
