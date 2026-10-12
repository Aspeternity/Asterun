# Uninstall launcher -> worker end-to-end flow (Windows only, run by CTest).
#
# What runs. A throw-away test installation (Asterun.exe, Update.exe,
# Uninstall.exe, dict/, third_party/ when present in the build, VERSION,
# README.md) with test user data under data\ is built in a new directory under
# %TEMP%. The LAUNCHER (Uninstall.exe without arguments) is started from that
# installation, exactly as a user starts it; no --perform, no test-only product
# entry point, no bypass. The test then presses the launcher's own
# "Uninstall and keep personal data" button. The button is located with UI
# Automation inside the launcher's own dialog window (process id, button id
# CommandButton_1001 and its text must all match) and pressed with its
# InvokePattern when it offers one, otherwise with the task dialog's documented
# TDM_CLICK_BUTTON message for that button id, posted to that dialog window
# only. No screen coordinates, no synthesized keystrokes, no window of any other
# process. The worker's result dialog is confirmed the same way (IDOK).
#
# The launcher must then create its random Asterun-Elevated.* copy under ITS
# temporary directory (the test gives it a private TEMP/TMP inside the test
# root), start that copy with ShellExecuteEx "runas", and exit. The test
# identifies the worker as the Uninstall.exe process whose parent is the
# launcher and whose image lies in that new directory, and records image path,
# command line, parent PID, creation time and token integrity / elevation type
# of both processes. Success is judged on the file system and on the worker's
# own completion dialog and exit code, never on a window closing or the launcher
# exiting:
#   - every program file of the test installation is gone, only data\ remains;
#   - every file under data\ is byte-identical (SHA-256) to before, no file
#     added or removed;
#   - the worker's completion dialog says the uninstall finished with user data
#     kept; after its OK button is pressed the worker exits with code 0.
#
# Evidence level. The test process must already be elevated (on the GitHub
# runner: integrity High, token elevation type 1 = no split token), so the
# launcher it starts runs elevated too and "runas" shows no consent dialog. This proves the launcher -> worker uninstall flow (secured copy,
# ShellExecuteEx, worker validation and removal). It does NOT prove the real
# split-token UAC path (a medium-integrity launcher and the consent prompt); that
# remains unverified. The recorded token summaries show which case ran.
#
# Safety. The worker removes the managed Everything service host under
# %ProgramFiles%\Aspeternity\Asterun\EverythingService and an Everything service
# it owns. The test refuses to run (exit 77) when either exists, so it never acts
# on a real installation's components. Without elevation it also exits 77 (the
# worker could otherwise raise a UAC prompt); CI treats a skipped test as a
# failure, so CI always runs it. Everything the test created is removed at the
# end. The worker's own reboot-time cleanup registration (MoveFileEx
# DELAY_UNTIL_REBOOT, product behaviour) is reported, not edited; the files it
# names are already deleted by the test.
#
# Exit codes: 0 pass, 1 fail, 77 skipped.

param([Parameter(Mandatory = $true)][string]$AppDir)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

if ($PSVersionTable.PSEdition -ne 'Core') {
    $env:PSModulePath = @(
        (Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'WindowsPowerShell\Modules'),
        (Join-Path $env:ProgramFiles 'WindowsPowerShell\Modules'),
        (Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\Modules')
    ) -join ';'
}

$script:failures = New-Object System.Collections.Generic.List[string]
function Check([bool]$condition, [string]$message) {
    if ($condition) { Write-Host "  ok   $message" }
    else { Write-Host "  FAIL $message"; $script:failures.Add($message) }
}
function Info([string]$message) { Write-Host "  info $message" }
# Non-ASCII UI strings are built from code points so this file stays ASCII.
function U([int[]]$codePoints) { return -join ($codePoints | ForEach-Object { [char]$_ }) }

$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host 'SKIPPED: the uninstall flow needs an elevated test process (the worker is started with runas)'
    exit 77
}
$serviceHostRoot = Join-Path $env:ProgramFiles 'Aspeternity\Asterun\EverythingService'
$everythingService = Get-Service -Name 'Everything' -ErrorAction SilentlyContinue
if ((Test-Path -LiteralPath $serviceHostRoot) -or $everythingService) {
    Write-Host "SKIPPED: an Everything service or $serviceHostRoot exists on this machine; the uninstall worker would act on it"
    exit 77
}

$AppDir = (Resolve-Path -LiteralPath $AppDir).Path
foreach ($name in 'Asterun.exe', 'Update.exe', 'Uninstall.exe') {
    if (-not (Test-Path -LiteralPath (Join-Path $AppDir $name))) { throw "$name not found in $AppDir" }
}

Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
namespace AsterunFlowTest {
public static class Native {
    delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr lParam);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetWindowTextW(IntPtr hwnd, StringBuilder text, int max);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassNameW(IntPtr hwnd, StringBuilder text, int max);
    [DllImport("user32.dll", SetLastError = true)] static extern bool PostMessageW(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
    // TDM_CLICK_BUTTON (WM_USER + 102): click the button with this id in this task dialog.
    public static bool ClickTaskDialogButton(IntPtr dialog, int buttonId) {
        return PostMessageW(dialog, 0x0400 + 102, new IntPtr(buttonId), IntPtr.Zero);
    }
    [DllImport("kernel32.dll", SetLastError = true)] static extern IntPtr OpenProcess(uint access, bool inherit, int processId);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
    [DllImport("advapi32.dll", SetLastError = true)] static extern bool OpenProcessToken(IntPtr process, uint access, out IntPtr token);
    [DllImport("advapi32.dll", SetLastError = true)] static extern bool GetTokenInformation(IntPtr token, int infoClass, IntPtr info, int length, out int returned);
    [DllImport("advapi32.dll")] static extern IntPtr GetSidSubAuthorityCount(IntPtr sid);
    [DllImport("advapi32.dll")] static extern IntPtr GetSidSubAuthority(IntPtr sid, uint index);

    // Visible top-level windows owned by one process: "hwnd|class|title".
    public static string[] TopLevelWindows(int processId) {
        List<string> found = new List<string>();
        EnumWindows(delegate (IntPtr hwnd, IntPtr lParam) {
            uint owner;
            GetWindowThreadProcessId(hwnd, out owner);
            if (owner == (uint)processId && IsWindowVisible(hwnd)) {
                StringBuilder title = new StringBuilder(512);
                StringBuilder cls = new StringBuilder(256);
                GetWindowTextW(hwnd, title, title.Capacity);
                GetClassNameW(hwnd, cls, cls.Capacity);
                found.Add(hwnd.ToInt64().ToString() + "|" + cls.ToString() + "|" + title.ToString());
            }
            return true;
        }, IntPtr.Zero);
        return found.ToArray();
    }

    // Token integrity level (S-1-16-<rid>) and TOKEN_ELEVATION_TYPE
    // (1 default / no split token, 2 full, 3 limited) of a process.
    public static string TokenSummary(int processId) {
        IntPtr process = OpenProcess(0x1000, false, processId);
        if (process == IntPtr.Zero) return "unavailable (OpenProcess error " + Marshal.GetLastWin32Error() + ")";
        try {
            IntPtr token;
            if (!OpenProcessToken(process, 0x0008, out token)) return "unavailable (OpenProcessToken error " + Marshal.GetLastWin32Error() + ")";
            try {
                int length;
                GetTokenInformation(token, 25, IntPtr.Zero, 0, out length);
                string integrity = "unknown";
                if (length > 0) {
                    IntPtr buffer = Marshal.AllocHGlobal(length);
                    try {
                        if (GetTokenInformation(token, 25, buffer, length, out length)) {
                            IntPtr sid = Marshal.ReadIntPtr(buffer);
                            byte count = Marshal.ReadByte(GetSidSubAuthorityCount(sid));
                            integrity = "S-1-16-" + Marshal.ReadInt32(GetSidSubAuthority(sid, (uint)(count - 1)));
                        }
                    } finally { Marshal.FreeHGlobal(buffer); }
                }
                string elevationType = "unknown";
                IntPtr value = Marshal.AllocHGlobal(4);
                try {
                    int returned;
                    if (GetTokenInformation(token, 18, value, 4, out returned)) elevationType = Marshal.ReadInt32(value).ToString();
                } finally { Marshal.FreeHGlobal(value); }
                return "integrity=" + integrity + " elevationType=" + elevationType;
            } finally { CloseHandle(token); }
        } finally { CloseHandle(process); }
    }
}
}
'@

$launcherTitles = @('Uninstall Asterun', (U 0x5378,0x8F7D,0x0020,0x0041,0x0073,0x0074,0x0065,0x0072,0x0075,0x006E))
$keepDataButtons = @('Uninstall and keep personal data', (U 0x5378,0x8F7D,0x5E76,0x4FDD,0x7559,0x4E2A,0x4EBA,0x6570,0x636E))
$okButtons = @('OK', (U 0x786E,0x5B9A))
$doneTexts = @('Asterun has been uninstalled', (U 0x5DF2,0x5378,0x8F7D,0x5B8C,0x6210))
$keptTexts = @('User data remains', (U 0x7528,0x6237,0x6570,0x636E,0x4ECD,0x4FDD,0x7559))

function Get-Windows([int]$processId) {
    return @([AsterunFlowTest.Native]::TopLevelWindows($processId) | ForEach-Object {
        $parts = $_.Split([char]'|', 3)
        [pscustomobject]@{ Hwnd = [IntPtr][long]$parts[0]; Class = $parts[1]; Title = $parts[2] }
    })
}
function Wait-Window([int]$processId, [string[]]$titles, [int]$timeoutSeconds) {
    $deadline = (Get-Date).AddSeconds($timeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $match = @(Get-Windows $processId | Where-Object { $titles -contains $_.Title })
        if ($match.Count -gt 0) { return $match[0] }
        Start-Sleep -Milliseconds 200
    }
    return $null
}
function Get-UiaElements([IntPtr]$hwnd) {
    $root = [System.Windows.Automation.AutomationElement]::FromHandle($hwnd)
    return @($root.FindAll([System.Windows.Automation.TreeScope]::Subtree, [System.Windows.Automation.Condition]::TrueCondition))
}
function Format-Element($element) {
    $c = $element.Current
    return "$($c.ControlType.ProgrammaticName) name='$($c.Name)' automationId='$($c.AutomationId)' class='$($c.ClassName)' pid=$($c.ProcessId)"
}
function Get-DialogText([IntPtr]$hwnd) {
    return ((Get-UiaElements $hwnd | ForEach-Object { $_.Current.Name }) -join "`n")
}
# Press one button inside one task dialog window of one process. The element
# must match the process id, the task dialog button id (AutomationId
# CommandButton_<id>) and one of the expected texts. Task dialog buttons are
# exposed as CCPushButton panes; use InvokePattern when offered, else post
# TDM_CLICK_BUTTON for that id to that dialog window.
function Invoke-Button([IntPtr]$hwnd, [int]$processId, [int]$buttonId, [string[]]$names) {
    $automationId = "CommandButton_$buttonId"
    $buttons = @(Get-UiaElements $hwnd | Where-Object {
        $_.Current.AutomationId -eq $automationId -and
        $names -contains $_.Current.Name -and $_.Current.ProcessId -eq $processId })
    if ($buttons.Count -ne 1) { throw "expected exactly one button $automationId named '$($names -join "' / '")' in window $hwnd of pid $processId, found $($buttons.Count)" }
    $button = $buttons[0]
    $description = Format-Element $button
    $invoke = $null
    if ($button.TryGetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern, [ref]$invoke)) {
        try { $invoke.Invoke() }
        catch [System.Windows.Automation.ElementNotAvailableException] { $description += ' [dialog closed during Invoke]' }
        return "$description via InvokePattern"
    }
    if (-not [AsterunFlowTest.Native]::ClickTaskDialogButton($hwnd, $buttonId)) { throw "TDM_CLICK_BUTTON $buttonId could not be posted to window $hwnd" }
    return "$description via TDM_CLICK_BUTTON($buttonId) to that dialog window (no InvokePattern on this element)"
}
function Get-Sha256([string]$path) { return (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
function Get-TreeSnapshot([string]$dir) {
    $snapshot = @{}
    $prefix = $dir.TrimEnd('\').Length + 1
    foreach ($item in @(Get-ChildItem -LiteralPath $dir -Recurse -Force)) {
        $relative = $item.FullName.Substring($prefix)
        if ($item.PSIsContainer) { $snapshot[$relative] = '<dir>' } else { $snapshot[$relative] = Get-Sha256 $item.FullName }
    }
    return $snapshot
}
function Compare-Snapshot([hashtable]$before, [hashtable]$after) {
    $differences = @()
    foreach ($key in $before.Keys) {
        if (-not $after.ContainsKey($key)) { $differences += "missing: $key" }
        elseif ($after[$key] -ne $before[$key]) { $differences += "changed: $key" }
    }
    foreach ($key in $after.Keys) { if (-not $before.ContainsKey($key)) { $differences += "added: $key" } }
    return @($differences)
}
function Get-UninstallProcesses {
    return @(Get-CimInstance -ClassName Win32_Process -Filter "Name = 'Uninstall.exe'")
}
function Under([string]$path, [string]$dir) {
    if (-not $path) { return $false }
    return $path.ToLowerInvariant().StartsWith(($dir.TrimEnd('\') + '\').ToLowerInvariant())
}

$id = [guid]::NewGuid().ToString('N')
$root = Join-Path $env:TEMP "asterun-uninstall-flow-$id"
if (Test-Path -LiteralPath $root) { throw "refusing to reuse existing directory $root" }
$install = Join-Path $root 'install'
$launcherTemp = Join-Path $root 'tmp'
$launcher = $null
$worker = $null
$workerInfo = $null

try {
    New-Item -ItemType Directory -Path $root | Out-Null
    # %TEMP% may be a short (8.3) path; use the long form everywhere so paths
    # reported by the file system, the processes and this test compare equal.
    $root = @(Get-ChildItem -LiteralPath (Split-Path -Parent $root) -Directory -Filter (Split-Path -Leaf $root))[0].FullName
    $install = Join-Path $root 'install'
    $launcherTemp = Join-Path $root 'tmp'
    New-Item -ItemType Directory -Path $install, $launcherTemp | Out-Null

    # Test installation laid out like the portable package.
    foreach ($name in 'Asterun.exe', 'Update.exe', 'Uninstall.exe') { Copy-Item -LiteralPath (Join-Path $AppDir $name) -Destination $install }
    foreach ($name in 'dict', 'third_party') {
        $source = Join-Path $AppDir $name
        if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination $install -Recurse }
    }
    $repoVersion = Join-Path $PSScriptRoot '..\VERSION'
    $version = if (Test-Path -LiteralPath $repoVersion) { (Get-Content -LiteralPath $repoVersion -Raw).Trim() } else { '0.0.0' }
    Set-Content -LiteralPath (Join-Path $install 'VERSION') -Value $version -NoNewline
    $readme = Join-Path $PSScriptRoot '..\README.md'
    if (Test-Path -LiteralPath $readme) { Copy-Item -LiteralPath $readme -Destination $install } else { Set-Content -LiteralPath (Join-Path $install 'README.md') -Value 'Asterun' }

    # Test user data (kept by "Uninstall and keep personal data").
    $data = Join-Path $install 'data'
    New-Item -ItemType Directory -Path $data, (Join-Path $data 'backups') | Out-Null
    $configDir = Join-Path $PSScriptRoot '..\config'
    foreach ($pair in @(@('settings.example.json', 'settings.json'), @('commands.example.json', 'commands.json'), @('usage.example.json', 'usage.json'))) {
        $source = Join-Path $configDir $pair[0]
        $target = Join-Path $data $pair[1]
        if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination $target } else { Set-Content -LiteralPath $target -Value "{`"test`":`"$id`"}" }
    }
    Set-Content -LiteralPath (Join-Path $data 'backups\settings.backup.json') -Value "{`"backup`":`"$id`"}"
    [System.IO.File]::WriteAllText((Join-Path $data (U 0x4E2D,0x6587,0x7B14,0x8BB0,0x002E,0x0074,0x0078,0x0074)), "user note $id", (New-Object System.Text.UTF8Encoding($false)))

    $installedUninstallHash = Get-Sha256 (Join-Path $install 'Uninstall.exe')
    $programEntries = @(Get-ChildItem -LiteralPath $install -Force | Where-Object { $_.Name -ne 'data' } | ForEach-Object { $_.Name })
    $dataBefore = Get-TreeSnapshot $data
    Info "test installation $install; program entries: $($programEntries -join ', '); data entries: $($dataBefore.Keys.Count)"
    $lua = (Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System' -ErrorAction SilentlyContinue)
    $luaValue = if ($lua -and ($lua.PSObject.Properties.Name -contains 'EnableLUA')) { $lua.EnableLUA } else { '<absent>' }
    Info "machine EnableLUA=$luaValue; test process token: $([AsterunFlowTest.Native]::TokenSummary($PID))"
    $before = @(Get-ChildItem -LiteralPath $launcherTemp -Force)
    Check ($before.Count -eq 0) 'launcher TEMP is empty before the launcher runs'

    # 1. Start the launcher from the test installation, as a user would.
    $info = New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName = Join-Path $install 'Uninstall.exe'
    $info.UseShellExecute = $false
    $info.WorkingDirectory = $install
    $info.EnvironmentVariables['TEMP'] = $launcherTemp
    $info.EnvironmentVariables['TMP'] = $launcherTemp
    $launcher = [System.Diagnostics.Process]::Start($info)
    $null = $launcher.Handle
    Info "launcher pid=$($launcher.Id) image=$($info.FileName) (no arguments) token: $([AsterunFlowTest.Native]::TokenSummary($launcher.Id))"

    # 2. Its own dialog, its own button.
    $dialog = Wait-Window $launcher.Id $launcherTitles 30
    if (-not $dialog) { throw 'the launcher showed no uninstall dialog within 30 s' }
    Info "launcher dialog hwnd=$($dialog.Hwnd) class=$($dialog.Class) title='$($dialog.Title)'"
    Check (@(Get-ChildItem -LiteralPath $launcherTemp -Force).Count -eq 0) 'no worker directory exists before the button is pressed'
    $pressed = Get-Date
    $how = Invoke-Button $dialog.Hwnd $launcher.Id 1001 $keepDataButtons
    Info "pressed: $how"

    # 3. The worker the launcher started.
    $deadline = (Get-Date).AddSeconds(60)
    while (-not $workerInfo -and (Get-Date) -lt $deadline) {
        $workerInfo = @(Get-UninstallProcesses | Where-Object { $_.ParentProcessId -eq $launcher.Id -and (Under $_.ExecutablePath $launcherTemp) }) | Select-Object -First 1
        if (-not $workerInfo) { Start-Sleep -Milliseconds 200 }
    }
    if (-not $workerInfo) { throw 'no worker process (parent = launcher, image under the launcher TEMP) appeared within 60 s' }
    $worker = [System.Diagnostics.Process]::GetProcessById([int]$workerInfo.ProcessId)
    $null = $worker.Handle
    $workerImage = [string]$workerInfo.ExecutablePath
    $workerDir = $workerImage.Substring(0, $workerImage.LastIndexOf('\'))
    $workerCmd = [string]$workerInfo.CommandLine
    Info "worker pid=$($workerInfo.ProcessId) parent=$($workerInfo.ParentProcessId) created=$($workerInfo.CreationDate.ToString('o'))"
    Info "worker image=$workerImage"
    Info "worker command line=$workerCmd"
    Info "worker token: $([AsterunFlowTest.Native]::TokenSummary([int]$workerInfo.ProcessId))"
    Check ($workerInfo.ParentProcessId -eq $launcher.Id) "worker's parent is the launcher (pid $($launcher.Id))"
    $dirLeaf = $workerDir.Substring($workerDir.LastIndexOf('\') + 1)
    Check ($dirLeaf -like 'Asterun-Elevated.*') "worker runs from an Asterun-Elevated.* directory ($dirLeaf)"
    Check ($workerDir.Substring(0, $workerDir.LastIndexOf('\')).TrimEnd('\') -ieq $launcherTemp.TrimEnd('\')) 'that directory is directly under the launcher TEMP (created by the launcher, not by the test)'
    $createdDirs = @(Get-ChildItem -LiteralPath $launcherTemp -Force)
    Check ($createdDirs.Count -eq 1 -and (Get-Item -LiteralPath $workerDir).CreationTime -ge $pressed.AddSeconds(-2)) "exactly one new directory appeared under the launcher TEMP after the button was pressed ($($createdDirs.Count))"
    Check ((Get-Sha256 $workerImage) -eq $installedUninstallHash) 'worker image is a byte-identical copy of the installed Uninstall.exe (hash taken before the launcher ran)'
    Check ($workerCmd -match '(^|\s)--perform(\s|$)') 'worker command line has --perform'
    Check ($workerCmd -match "--parent-pid $($launcher.Id)(\s|$)") 'worker command line names the launcher as --parent-pid'
    Check ($workerCmd.Contains("--install `"$install`"")) 'worker command line names the test installation as --install'
    Check ($workerCmd -match '--delete-data 0(\s|$)') 'worker command line keeps data (--delete-data 0)'

    # 4. The launcher hands over and exits.
    Check ($launcher.WaitForExit(30000)) 'launcher exited after starting the worker'
    if ($launcher.HasExited) { Check ($launcher.ExitCode -eq 0) "launcher exit code 0 (got $($launcher.ExitCode))" }

    # 5. The worker's own result dialog, then the file system.
    $done = Wait-Window $worker.Id @('Asterun') 180
    if (-not $done) { throw 'the worker showed no result dialog within 180 s' }
    $text = Get-DialogText $done.Hwnd
    Info "worker dialog hwnd=$($done.Hwnd) title='$($done.Title)' text: $($text -replace "`r?`n", ' | ')"
    Check (@($doneTexts | Where-Object { $text.Contains($_) }).Count -gt 0) 'worker reports that the uninstall finished'
    Check (@($keptTexts | Where-Object { $text.Contains($_) }).Count -gt 0) 'worker reports that user data was kept'

    $remaining = @(Get-ChildItem -LiteralPath $install -Force | ForEach-Object { $_.Name })
    Check ($remaining.Count -eq 1 -and $remaining[0] -eq 'data') "only data\ remains in the installation (found: $($remaining -join ', '))"
    foreach ($entry in $programEntries) { Check (-not (Test-Path -LiteralPath (Join-Path $install $entry))) "program entry removed: $entry" }
    $differences = @(Compare-Snapshot $dataBefore (Get-TreeSnapshot $data))
    Check ($differences.Count -eq 0) "data\ unchanged: $($dataBefore.Keys.Count) entries, SHA-256 identical ($($differences -join '; '))"

    $how = Invoke-Button $done.Hwnd $worker.Id 1 $okButtons
    Info "pressed: $how"
    Check ($worker.WaitForExit(30000)) 'worker exited after its result dialog was confirmed'
    if ($worker.HasExited) { Check ($worker.ExitCode -eq 0) "worker exit code 0 (got $($worker.ExitCode))" }
    $differences = @(Compare-Snapshot $dataBefore (Get-TreeSnapshot $data))
    Check ($differences.Count -eq 0) "data\ still unchanged after the worker exited ($($differences -join '; '))"

    $pending = (Get-ItemProperty -LiteralPath 'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager' -ErrorAction SilentlyContinue)
    $pendingEntries = if ($pending -and ($pending.PSObject.Properties.Name -contains 'PendingFileRenameOperations')) { @($pending.PendingFileRenameOperations | Where-Object { $_ -and $_.ToLowerInvariant().Contains($dirLeaf.ToLowerInvariant()) }) } else { @() }
    Info "worker scheduled its own copy for deletion at reboot (product cleanup): $($pendingEntries.Count) PendingFileRenameOperations entries name $dirLeaf; not edited, the files are removed below"
} catch {
    Write-Host "  FAIL exception: $($_.Exception.Message)"
    $script:failures.Add($_.Exception.Message)
} finally {
    if ($script:failures.Count -gt 0) {
        Write-Host '  diagnostics:'
        foreach ($p in @($launcher, $worker)) {
            if (-not $p) { continue }
            $alive = -not $p.HasExited
            Write-Host "    pid=$($p.Id) alive=$alive$(if (-not $alive) { " exit=$($p.ExitCode)" })"
            if ($alive) {
                foreach ($w in @(Get-Windows $p.Id)) {
                    Write-Host "      window hwnd=$($w.Hwnd) class=$($w.Class) title='$($w.Title)'"
                    try { foreach ($e in @(Get-UiaElements $w.Hwnd)) { Write-Host "        $(Format-Element $e)" } } catch { Write-Host "        UI Automation failed: $($_.Exception.Message)" }
                }
            }
        }
        foreach ($p in @(Get-UninstallProcesses)) { Write-Host "    Uninstall.exe pid=$($p.ProcessId) parent=$($p.ParentProcessId) image=$($p.ExecutablePath) cmd=$($p.CommandLine)" }
        if (Test-Path -LiteralPath $root) { Get-ChildItem -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue | ForEach-Object { Write-Host "    $($_.FullName.Substring($root.Length))" } }
    }
    # Stop only processes this test started or whose image lies in its own root.
    if ($launcher -and -not $launcher.HasExited) { Stop-Process -Id $launcher.Id -Force -ErrorAction SilentlyContinue }
    foreach ($p in @(Get-UninstallProcesses | Where-Object { Under $_.ExecutablePath $root })) { Stop-Process -Id $p.ProcessId -Force -ErrorAction SilentlyContinue }
    Start-Sleep -Milliseconds 500
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath $root) { Write-Host "  FAIL cleanup: $root could not be removed"; $script:failures.Add('cleanup') }
    else { Write-Host "  info removed the test root $root" }
}

if ($script:failures.Count -gt 0) {
    Write-Host "Uninstall launcher flow test FAILED ($($script:failures.Count) checks)"
    exit 1
}
Write-Host 'Uninstall launcher flow test passed: launcher -> worker uninstall flow (no real split-token UAC prompt on this machine; see token summaries above)'
exit 0
