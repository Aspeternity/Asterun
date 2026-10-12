# Asterun batch A acceptance kit (U-4d): record ownership, ACL and writability
# of the EXACT directory the uninstall worker ran from, as confirmed by the
# Process Monitor analysis (Analyze-Procmon.ps1 ... -Role observation), never by
# guessing the newest Asterun-Elevated.* directory.
#
#   -Report      the analyzer report of U-4c or U-4e. Its line
#                "TARGET-IMAGE-DIR pid=<pid> dir=<dir>" names the directory of
#                the confirmed worker image.
#   -WorkerPid   only needed when the report lists more than one worker PID.
#   -WriteTest   also try to create (and then delete) one file in that
#                directory. Only a write test run from a NON-elevated process
#                counts for the "a standard user can write here" conclusion;
#                the output states the account, integrity level and elevation.
#
# Run it twice: once (elevated or not) without -WriteTest for owner/ACL, and
# once from a NON-elevated Windows PowerShell with -WriteTest.
#
# Exit codes: 0 recorded; 13 write test ran elevated (not valid for the
# non-elevated conclusion); 30 no confirmed worker in the report; 31 several
# workers and no -Pid; 32 the directory no longer exists (nothing else is tested).
param(
    [Parameter(Mandatory = $true)][string]$Report,
    [Parameter(Mandatory = $true)][string]$Out,
    [string]$WorkerPid = '',
    [switch]$WriteTest
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
$lines = New-Object System.Collections.Generic.List[string]
function Emit([string]$text) { $lines.Add($text); Write-Output $text }
function Done([int]$code) {
    [System.IO.File]::WriteAllLines($Out, $lines, (New-Object System.Text.UTF8Encoding($false)))
    exit $code
}

Emit "time=$(Get-Date -Format o) report=$Report"
if (-not (Test-Path -LiteralPath $Report)) { throw "report not found: $Report" }
$targets = @()
foreach ($line in [System.IO.File]::ReadAllLines($Report)) {
    $m = [regex]::Match($line, '^TARGET-IMAGE-DIR pid=(\d+) dir=(.+)$')
    if ($m.Success) { $targets += [pscustomobject]@{ Pid = $m.Groups[1].Value; Dir = $m.Groups[2].Value.Trim() } }
}
if ($WorkerPid) { $targets = @($targets | Where-Object { $_.Pid -eq $WorkerPid }) }
$dirs = @($targets | ForEach-Object { $_.Dir } | Select-Object -Unique)
if ($dirs.Count -eq 0) {
    Emit 'RESULT: NO-TARGET - the report has no confirmed worker (TARGET-IMAGE-DIR). Nothing was tested.'
    Done 30
}
if ($dirs.Count -gt 1) {
    Emit "RESULT: AMBIGUOUS - several worker directories: $($targets | ForEach-Object { "pid=$($_.Pid) dir=$($_.Dir)" } | Out-String). Re-run with -WorkerPid."
    Done 31
}
$dir = $dirs[0]
Emit "worker pid=$(@($targets | ForEach-Object { $_.Pid }) -join ',') directory=$dir"
if ((Split-Path -Leaf $dir) -notlike 'Asterun-Elevated.*') {
    Emit 'RESULT: UNEXPECTED - the confirmed worker directory is not an Asterun-Elevated.* directory; recorded as is, nothing tested.'
    Done 30
}
if (-not (Test-Path -LiteralPath $dir -PathType Container)) {
    Emit 'RESULT: DIRECTORY-GONE - this exact directory no longer exists (for example after a reboot). Ownership/ACL/writability could not be recorded; no other directory was tested.'
    Done 32
}

# Who is running this, and is the token elevated?
Add-Type -Namespace AsterunKit -Name TokenInfo -MemberDefinition @'
[System.Runtime.InteropServices.DllImport("advapi32.dll", SetLastError = true)]
public static extern bool GetTokenInformation(System.IntPtr token, int infoClass, out int info, int length, out int returned);
'@
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$elevated = 0; $returned = 0
[void][AsterunKit.TokenInfo]::GetTokenInformation($identity.Token, 20, [ref]$elevated, 4, [ref]$returned)  # TokenElevation
$integrity = ((whoami /groups) | Select-String -Pattern 'Mandatory Label\\(\w+) Mandatory Level' | ForEach-Object { $_.Matches[0].Groups[1].Value }) -join ','
Emit "account=$($identity.Name) sid=$($identity.User.Value) elevated=$([bool]$elevated) integrity=$integrity"

$acl = Get-Acl -LiteralPath $dir
Emit "Owner=$($acl.Owner)"
Emit "Group=$($acl.Group)"
Emit "Sddl=$($acl.Sddl)"
Emit 'Note: the owner recorded by the file system is not necessarily the creator; judge ownership and creation separately.'
Emit 'Access:'
foreach ($ace in $acl.Access) {
    Emit "  $($ace.IdentityReference) | $($ace.AccessControlType) | $($ace.FileSystemRights) | inherited=$($ace.IsInherited) | inheritance=$($ace.InheritanceFlags) | propagation=$($ace.PropagationFlags)"
}
Emit 'icacls:'
foreach ($line in @(& "$env:WINDIR\System32\icacls.exe" $dir 2>&1)) { Emit "  $line" }

$code = 0
if ($WriteTest) {
    $probe = Join-Path $dir ("u4-writetest-" + [guid]::NewGuid().ToString('N') + '.txt')
    try {
        [System.IO.File]::WriteAllText($probe, 'asterun acceptance write test')
        Emit "write test: SUCCEEDED (created $probe)"
    } catch {
        Emit "write test: FAILED ($($_.Exception.Message))"
    } finally {
        if (Test-Path -LiteralPath $probe) { Remove-Item -LiteralPath $probe -Force; Emit 'write test file removed' }
    }
    if ($elevated) {
        Emit 'write test validity: NOT VALID for the standard-user conclusion - this process is elevated. Re-run with -WriteTest from a non-elevated Windows PowerShell.'
        $code = 13
    } else {
        Emit "write test validity: VALID for account $($identity.Name) (not elevated, integrity $integrity)"
    }
}
Emit 'RESULT: RECORDED'
Done $code
