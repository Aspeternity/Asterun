# Tests the test-only helper. Never starts Update.exe or handles any window.
param([Parameter(Mandatory = $true)][string]$Helper)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
# CI starts Windows PowerShell from PowerShell 7. Use the matching module
# directories so Get-FileHash can load Microsoft.PowerShell.Utility.
if ($PSVersionTable.PSEdition -ne 'Core') {
    $env:PSModulePath = @(
        (Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'WindowsPowerShell\Modules'),
        (Join-Path $env:ProgramFiles 'WindowsPowerShell\Modules'),
        (Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\Modules')
    ) -join ';'
}
Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
public static class HelperCommandLine {
    [DllImport("shell32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr CommandLineToArgvW(string command, out int count);
    [DllImport("kernel32.dll")]
    private static extern IntPtr LocalFree(IntPtr value);
    public static string[] Parse(string command) {
        int count;
        IntPtr argv = CommandLineToArgvW(command, out count);
        if (argv == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error());
        try {
            var result = new string[count];
            for (int i = 0; i < count; ++i)
                result[i] = Marshal.PtrToStringUni(Marshal.ReadIntPtr(argv, i * IntPtr.Size));
            return result;
        } finally { LocalFree(argv); }
    }
}
'@
$Helper = (Resolve-Path -LiteralPath $Helper).Path
$root = Join-Path ([IO.Path]::GetTempPath()) ('asterun-helper-test-' + [guid]::NewGuid().ToString('N'))
if (Test-Path -LiteralPath $root) { throw 'Refusing to reuse a test directory' }
[void][IO.Directory]::CreateDirectory($root)
$processes = New-Object System.Collections.Generic.List[System.Diagnostics.Process]

function Require([bool]$value, [string]$message) {
    if (-not $value) { throw $message }
    Write-Host "PASS: $message"
}
function New-Case([string]$name, [string]$mode) {
    # Same leaf case name in two different roots catches shared-name records.
    $caseRoot = Join-Path $root "$name\same-case"
    $install = Join-Path $caseRoot 'install'
    $records = Join-Path $caseRoot 'records'
    [void][IO.Directory]::CreateDirectory($install)
    [void][IO.Directory]::CreateDirectory($records)
    $exe = Join-Path $install 'Asterun.exe'
    Copy-Item -LiteralPath $Helper -Destination $exe
    [IO.File]::WriteAllText((Join-Path $records 'mode'), $mode)
    return @{ Exe = $exe; Records = $records; Hash = (Get-FileHash -LiteralPath $exe).Hash }
}
function Start-Helper($case) {
    $p = Start-Process -FilePath $case.Exe -PassThru
    $null = $p.Handle
    $processes.Add($p)
    return $p
}
function Check-Record($case, $process) {
    $file = Join-Path $case.Records 'launches.txt'
    Require (Test-Path -LiteralPath $file) 'own record exists'
    $lines = @([IO.File]::ReadAllLines($file))
    Require ($lines.Count -eq 1) 'exactly one start recorded'
    Require ($lines[0].StartsWith("pid=$($process.Id) ")) 'record identifies the launched process'
    Require ($lines[0].Contains('integrity=S-1-16-')) 'integrity recorded'
    Require ($lines[0].Contains('elevationType=')) 'elevation type recorded'
    $command = ($lines[0] -split ' cmdline=', 2)[1]
    # Shell execution may change the executable's path spelling or quoting.
    # PID and the case-owned record identify the process; parse argv to check
    # the actual no-argument contract, including rejection of an empty argument.
    $arguments = @([HelperCommandLine]::Parse($command))
    Write-Host "Recorded command: $command; argument count: $($arguments.Count)"
    Require ($arguments.Count -eq 1) 'no application arguments'
    Require ([IO.Path]::GetFileName($arguments[0]) -ieq 'Asterun.exe') 'command names the test executable'
    Require ((Get-FileHash -LiteralPath $case.Exe).Hash -eq $case.Hash) 'helper executable unchanged'
}

try {
    Require (@([HelperCommandLine]::Parse('"C:\test directory\Asterun.exe" ')).Count -eq 1) 'parser accepts a quoted executable with trailing whitespace'
    Require (@([HelperCommandLine]::Parse('"C:\test directory\Asterun.exe" ""')).Count -eq 2) 'parser detects an empty application argument'
    Require (@([HelperCommandLine]::Parse('"C:\test directory\Asterun.exe" --unexpected')).Count -eq 2) 'parser detects an application argument'
    $zero = New-Case 'first' 'exit0'
    $three = New-Case 'second' 'exit3'
    $a = Start-Helper $zero
    $b = Start-Helper $three
    Require ($a.WaitForExit(10000)) 'exit0 terminates'
    Require ($b.WaitForExit(10000)) 'exit3 terminates'
    Require ($a.ExitCode -eq 0) 'exit0 uses code 0'
    Require ($b.ExitCode -eq 3) 'exit3 uses code 3'
    Check-Record $zero $a
    Check-Record $three $b

    $stay = New-Case 'third' 'stay'
    $c = Start-Helper $stay
    Require (-not $c.WaitForExit(5500)) 'stay survives the five-second observation window'
    Check-Record $stay $c
    $c.Kill()
    Require ($c.WaitForExit(5000)) 'test-owned stay process stopped'
    Write-Host 'Helper isolation and exit modes: PASS. Update.exe recovery was NOT tested.'
} finally {
    foreach ($p in $processes) {
        if (-not $p.HasExited) { $p.Kill(); [void]$p.WaitForExit(5000) }
        $p.Dispose()
    }
    Remove-Item -LiteralPath $root -Recurse -Force
}
