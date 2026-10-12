# Self-test of asterun_dll_probe.dll before using it: loads it into this
# PowerShell process by full path and checks that it wrote its log line.
# Usage: powershell -ExecutionPolicy Bypass -File Test-Probe.ps1 -Dll C:\Acceptance\probe\x64\asterun_dll_probe.dll
param([Parameter(Mandatory = $true)][string]$Dll)

$ErrorActionPreference = 'Stop'
if ($PSVersionTable.PSEdition -ne 'Core') {
    # Started from PowerShell 7, Windows PowerShell inherits PowerShell 7 module
    # folders first and cannot auto-load its own modules (CimCmdlets, Utility).
    $env:PSModulePath = @(
        (Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'WindowsPowerShell\Modules'),
        (Join-Path $env:ProgramFiles 'WindowsPowerShell\Modules'),
        (Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\Modules')
    ) -join ';'
}
$log = Join-Path $env:PUBLIC 'AsterunDllProbe\probe-log.txt'
$before = if (Test-Path $log) { (Get-Content -Encoding Unicode $log).Count } else { 0 }

Add-Type -Namespace AsterunKit -Name Native -MemberDefinition @'
[System.Runtime.InteropServices.DllImport("kernel32.dll", CharSet = System.Runtime.InteropServices.CharSet.Unicode, SetLastError = true)]
public static extern System.IntPtr LoadLibraryW(string path);
'@
$handle = [AsterunKit.Native]::LoadLibraryW((Resolve-Path $Dll).Path)
if ($handle -eq [IntPtr]::Zero) { throw "LoadLibraryW failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())" }

$after = if (Test-Path $log) { (Get-Content -Encoding Unicode $log).Count } else { 0 }
if ($after -le $before) { throw "probe did not write to $log" }
"probe OK: $((Get-Content -Encoding Unicode $log)[-1])"
