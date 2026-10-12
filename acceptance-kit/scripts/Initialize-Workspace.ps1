# Asterun batch A acceptance kit: create a NEW workspace for one acceptance run.
#
# Inputs are only read (in Windows Sandbox they are read-only mapped folders):
#   -BaselineZip   v1.0.5 package (Asterun-x64.zip or Asterun-ARM64.zip of the release)
#   -CandidateZip  candidate package: the Asterun-<arch>.zip of the batch A run, or
#                  the GitHub artifact download that wraps it (unwrapped here)
#   -Tools         folder with Procmon64.exe (x64) / Procmon64a.exe (ARM64), or ProcessMonitor.zip
#   -Kit           the kit folder (probe DLL and test script are taken from it)
# Outputs:
#   <WorkRoot>\<RunName>  new workspace: packages are copied in and unpacked there
#   <Logs>\<RunName>\initialize.txt  hashes of every input and of the three executables
#
# Nothing is deleted. If <WorkRoot>\<RunName> or <Logs>\<RunName> already exists
# the script stops before writing anything (exit 2); choose another -RunName or
# remove the old folder yourself after checking it.
#
# Usage (elevated Windows PowerShell 5.1 inside the sandbox / VM):
#   powershell -ExecutionPolicy Bypass -File C:\Acceptance\kit\scripts\Initialize-Workspace.ps1 `
#     -BaselineZip C:\Acceptance\packages\Asterun-x64-v1.0.5.zip `
#     -CandidateZip C:\Acceptance\packages\Asterun-x64-candidate.zip `
#     -Tools C:\Acceptance\tools -Kit C:\Acceptance\kit -Logs C:\Acceptance\logs
param(
    [Parameter(Mandatory = $true)][string]$BaselineZip,
    [Parameter(Mandatory = $true)][string]$CandidateZip,
    [Parameter(Mandatory = $true)][string]$Tools,
    [Parameter(Mandatory = $true)][string]$Kit,
    [Parameter(Mandatory = $true)][string]$Logs,
    [string]$WorkRoot = 'C:\AsterunWork',
    [string]$RunName = ('run-' + (Get-Date -Format 'yyyyMMdd-HHmmss')),
    [ValidateSet('x64', 'ARM64')][string]$Arch = $(if ($env:PROCESSOR_ARCHITECTURE -eq 'ARM64') { 'ARM64' } else { 'x64' }),
    # Published SHA-256 of the v1.0.5 x64 release asset; pass '' for ARM64 (recorded only).
    [string]$BaselineSha256 = '2bf93316a54d84fa7721016d3b0bf48b6a6ddec92f58acde9c94d7401321bd23',
    [string]$CandidateSha256 = ''
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
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Get-Sha256([string]$path) {
    $stream = [System.IO.File]::OpenRead($path)
    try { return ([BitConverter]::ToString([System.Security.Cryptography.SHA256]::Create().ComputeHash($stream))).Replace('-', '').ToLowerInvariant() }
    finally { $stream.Dispose() }
}

$work = Join-Path $WorkRoot $RunName
$logDir = Join-Path $Logs $RunName
foreach ($existing in $work, $logDir) {
    if (Test-Path -LiteralPath $existing) {
        Write-Output "STOP: $existing already exists. Nothing was changed. Use a new -RunName."
        exit 2
    }
}
foreach ($item in $BaselineZip, $CandidateZip, $Tools, $Kit) {
    if (-not (Test-Path -LiteralPath $item)) { throw "input not found: $item" }
}
$probe = Join-Path $Kit ("probe-dll\out\{0}\asterun_dll_probe.dll" -f $(if ($Arch -eq 'ARM64') { 'arm64' } else { 'x64' }))
$updaterTest = Join-Path $Kit 'tests\UpdaterIntegrationTests.ps1'
foreach ($item in $probe, $updaterTest) { if (-not (Test-Path -LiteralPath $item)) { throw "kit file not found: $item" } }

New-Item -ItemType Directory -Path $work | Out-Null
New-Item -ItemType Directory -Path $logDir | Out-Null
foreach ($sub in 'packages', 'baseline', 'candidate', 'tools', 'probe', 'tests', 'scenarios') {
    New-Item -ItemType Directory -Path (Join-Path $work $sub) | Out-Null
}
$record = New-Object System.Collections.Generic.List[string]
$record.Add("time=$(Get-Date -Format o) run=$RunName arch=$Arch work=$work logs=$logDir")
$failures = 0

function Expand-Package([string]$zip, [string]$destination, [string]$label) {
    $copy = Join-Path $work "packages\$label.zip"
    Copy-Item -LiteralPath $zip -Destination $copy
    $record.Add("$label input=$zip sha256=$(Get-Sha256 $copy)")
    $archive = [System.IO.Compression.ZipFile]::OpenRead($copy)
    try {
        $names = @($archive.Entries | ForEach-Object { $_.FullName })
        $inner = @($names | Where-Object { $_ -like 'Asterun-*.zip' })
    } finally { $archive.Dispose() }
    if ($names.Count -eq 1 -and $inner.Count -eq 1) {
        # GitHub artifact download: a zip that contains the package zip.
        $unwrapped = Join-Path $work "packages\$label-wrapper"
        [System.IO.Compression.ZipFile]::ExtractToDirectory($copy, $unwrapped)
        $copy = Join-Path $unwrapped $inner[0]
        $record.Add("  unwrapped artifact -> $($inner[0]) sha256=$(Get-Sha256 $copy)")
    }
    [System.IO.Compression.ZipFile]::ExtractToDirectory($copy, $destination)
    return (Get-Sha256 $copy)
}

$baselineHash = Expand-Package $BaselineZip (Join-Path $work 'baseline') 'baseline'
if ($BaselineSha256) {
    $ok = $baselineHash -eq $BaselineSha256.ToLowerInvariant()
    $record.Add("  baseline expected sha256=$BaselineSha256 match=$ok")
    if (-not $ok) { $failures++ }
}
$candidateHash = Expand-Package $CandidateZip (Join-Path $work 'candidate') 'candidate'
if ($CandidateSha256) {
    $ok = $candidateHash -eq $CandidateSha256.ToLowerInvariant()
    $record.Add("  candidate expected sha256=$CandidateSha256 match=$ok")
    if (-not $ok) { $failures++ }
}
foreach ($label in 'baseline', 'candidate') {
    $dir = Join-Path $work $label
    $version = if (Test-Path -LiteralPath (Join-Path $dir 'VERSION')) { (Get-Content -LiteralPath (Join-Path $dir 'VERSION') -Raw).Trim() } else { '<missing>' }
    $record.Add("$label directory=$dir VERSION=$version")
    foreach ($exe in 'Asterun.exe', 'Update.exe', 'Uninstall.exe') {
        $path = Join-Path $dir $exe
        if (-not (Test-Path -LiteralPath $path)) { $record.Add("  $exe MISSING"); $failures++; continue }
        $record.Add("  $exe size=$((Get-Item -LiteralPath $path).Length) sha256=$(Get-Sha256 $path)")
    }
}

$procmonName = if ($Arch -eq 'ARM64') { 'Procmon64a.exe' } else { 'Procmon64.exe' }
$procmonSource = Join-Path $Tools $procmonName
$procmonTarget = Join-Path $work "tools\$procmonName"
if (Test-Path -LiteralPath $procmonSource) {
    Copy-Item -LiteralPath $procmonSource -Destination $procmonTarget
} elseif (Test-Path -LiteralPath (Join-Path $Tools 'ProcessMonitor.zip')) {
    $zip = Join-Path $Tools 'ProcessMonitor.zip'
    $record.Add("ProcessMonitor.zip sha256=$(Get-Sha256 $zip)")
    $archive = [System.IO.Compression.ZipFile]::OpenRead($zip)
    try {
        $entry = @($archive.Entries | Where-Object { $_.FullName -eq $procmonName })
        if ($entry.Count -ne 1) { throw "$procmonName not in $zip" }
        [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry[0], $procmonTarget)
    } finally { $archive.Dispose() }
} else { throw "neither $procmonName nor ProcessMonitor.zip in $Tools" }
$signature = Get-AuthenticodeSignature -LiteralPath $procmonTarget
$record.Add("$procmonName sha256=$(Get-Sha256 $procmonTarget) signature=$($signature.Status) signer=$($signature.SignerCertificate.Subject)")
if ($signature.Status -ne 'Valid') { $failures++ }

Copy-Item -LiteralPath $probe -Destination (Join-Path $work 'probe\asterun_dll_probe.dll')
$record.Add("probe=$probe sha256=$(Get-Sha256 $probe)")
Copy-Item -LiteralPath $updaterTest -Destination (Join-Path $work 'tests\UpdaterIntegrationTests.ps1')
$record.Add("updater test sha256=$(Get-Sha256 $updaterTest)")

$record.Add($(if ($failures) { "RESULT: FAIL ($failures problems above)" } else { 'RESULT: PASS' }))
$record.Add("WORKSPACE=$work")
[System.IO.File]::WriteAllLines((Join-Path $logDir 'initialize.txt'), $record, (New-Object System.Text.UTF8Encoding($false)))
$record | ForEach-Object { Write-Output $_ }
exit $(if ($failures) { 1 } else { 0 })
