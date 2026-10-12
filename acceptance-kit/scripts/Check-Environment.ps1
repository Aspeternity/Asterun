# Asterun batch A acceptance kit: record the test environment and decide
# whether it counts as "clean" (no Visual C++ runtime installed).
# Usage (inside the test VM / Windows Sandbox):
#   powershell -ExecutionPolicy Bypass -File Check-Environment.ps1 -Log C:\Acceptance\logs\environment.txt
param([Parameter(Mandatory = $true)][string]$Log)

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
New-Item -ItemType Directory -Force -Path (Split-Path $Log) | Out-Null

$os = Get-CimInstance Win32_OperatingSystem
$build = (Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion')
$lines = @()
$lines += "time=$(Get-Date -Format o)"
$lines += "os=$($os.Caption) version=$($os.Version) displayVersion=$($build.DisplayVersion) ubr=$($build.UBR) arch=$env:PROCESSOR_ARCHITECTURE"
$lines += "user=$env:USERNAME temp=$env:TEMP public=$env:PUBLIC"
$integrity = (whoami /groups | Select-String -Pattern 'Mandatory Label\\(\w+) Mandatory Level' | ForEach-Object { $_.Matches[0].Groups[1].Value }) -join ','
$admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
$lines += "integrity-level=$integrity running-elevated=$admin"
if ($admin) {
    $lines += 'NOTE: this shell runs elevated (typical in Windows Sandbox). UAC prompts may not appear here; run the elevation steps also in a VM with a standard (split-token) administrator account.'
}
$longTemp = (Get-Item -LiteralPath $env:TEMP).FullName
$lines += "temp-long-path=$longTemp"

$runtimeFiles = 'vcruntime140.dll', 'vcruntime140_1.dll', 'msvcp140.dll', 'msvcp140_atomic_wait.dll'
$found = @()
foreach ($dir in @("$env:WINDIR\System32", "$env:WINDIR\SysWOW64")) {
    foreach ($file in $runtimeFiles) {
        $path = Join-Path $dir $file
        if (Test-Path $path) { $found += $path }
    }
}
$runtimeKeys = 'HKLM:\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64',
               'HKLM:\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\arm64',
               'HKLM:\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\x86'
foreach ($key in $runtimeKeys) {
    if (Test-Path $key) {
        $value = Get-ItemProperty $key
        $found += "$key Installed=$($value.Installed) Version=$($value.Version)"
    }
}

if ($found.Count -eq 0) {
    $lines += 'vc-runtime=ABSENT  => environment is CLEAN for the runtime test'
} else {
    $lines += 'vc-runtime=PRESENT => NOT clean; do not use this environment for the runtime test'
    $lines += $found | ForEach-Object { "  $_" }
}

$procmon = Get-Process -Name Procmon*, Procmon64* -ErrorAction SilentlyContinue
$lines += "procmon-running=$([bool]$procmon)"
$lines | Tee-Object -FilePath $Log
