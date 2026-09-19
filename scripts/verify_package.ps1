param(
    [Parameter(Mandatory = $true)]
    [string]$Archive
)

$ErrorActionPreference = "Stop"

$archivePath = (Resolve-Path $Archive).Path
$verify = Join-Path (Get-Location) "verify-package"

if (Test-Path $verify) {
    Remove-Item $verify -Recurse -Force
}

Expand-Archive -Path $archivePath -DestinationPath $verify -Force

$required = @(
    "ALTRunNext.exe",
    "VERSION",
    "README.md",
    "CONFIG_SCHEMA.md",
    "DESKTOP_VALIDATION.md",
    "V0.5_RC_VALIDATION.md",
    "EVERYTHING_COMPATIBILITY.md",
    "dict",
    "third_party/cpp-pinyin-LICENSE.txt"
)

foreach ($entry in $required) {
    if (-not (Test-Path (Join-Path $verify $entry))) {
        throw "Release package is missing required entry: $entry"
    }
}

$allowedTopLevel = @(
    "ALTRunNext.exe",
    "VERSION",
    "README.md",
    "CONFIG_SCHEMA.md",
    "DESKTOP_VALIDATION.md",
    "V0.5_RC_VALIDATION.md",
    "EVERYTHING_COMPATIBILITY.md",
    "commands.example.json",
    "commands.example.tsv",
    "settings.example.ini",
    "settings.example.json",
    "usage.example.json",
    "dict",
    "third_party"
)

$actualTopLevel = @(
    Get-ChildItem $verify -Force |
        ForEach-Object { $_.Name }
)

$unexpectedTopLevel = @(
    $actualTopLevel |
        Where-Object { $_ -notin $allowedTopLevel }
)

if ($unexpectedTopLevel.Count -ne 0) {
    $unexpectedTopLevel | ForEach-Object {
        Write-Host "Unexpected top-level package entry: $_"
    }
    throw "Release package contains unexpected top-level entries."
}

$missingTopLevel = @(
    $allowedTopLevel |
        Where-Object { $_ -notin $actualTopLevel }
)

if ($missingTopLevel.Count -ne 0) {
    $missingTopLevel | ForEach-Object {
        Write-Host "Missing top-level package entry: $_"
    }
    throw "Release package top-level allowlist is incomplete."
}

$unexpectedDlls = @(
    Get-ChildItem $verify -Recurse -File -Filter "*.dll" -ErrorAction SilentlyContinue
)

if ($unexpectedDlls.Count -ne 0) {
    $unexpectedDlls | ForEach-Object { Write-Host $_.FullName }
    throw "Release package contains unexpected runtime DLLs."
}

$expectedVersion = (Get-Content VERSION -Raw).Trim()
$packagedVersion = (Get-Content (Join-Path $verify "VERSION") -Raw).Trim()

if ($packagedVersion -ne $expectedVersion) {
    throw "Packaged VERSION '$packagedVersion' does not match '$expectedVersion'."
}

$semver = [regex]::Match(
    $expectedVersion,
    '^(\d+)\.(\d+)\.(\d+)(?:-(alpha|beta|rc)\.(\d+)(?:\.(\d+))?)?

if (-not $semver.Success) {
    throw "Unsupported VERSION format: '$expectedVersion'."
}

$major = [int]$semver.Groups[1].Value
$minor = [int]$semver.Groups[2].Value
$patch = [int]$semver.Groups[3].Value
$channel = $semver.Groups[4].Value

$channelNumber = if ($semver.Groups[5].Success) {
    [int]$semver.Groups[5].Value
} else {
    0
}

$channelPatch = if ($semver.Groups[6].Success) {
    [int]$semver.Groups[6].Value
} else {
    0
}

if ($channelPatch -ne 0 -and $channel -ne "alpha") {
    throw "Only alpha prereleases currently support a hotfix component."
}

if ($channel -eq "alpha") {
    if ($channelPatch -ne 0 -or $channelNumber -ge 3) {
        $revision = $channelNumber * 10 + $channelPatch
    } else {
        $revision = $channelNumber
    }
} elseif ($channel -eq "beta") {
    $revision = 99 + $channelNumber
} elseif ($channel -eq "rc") {
    $revision = 199 + $channelNumber
} else {
    $revision = 300
}

$expectedWindowsVersion =
    "{0}.{1}.{2}.{3}" -f $major, $minor, $patch, $revision

$versionInfo =
    (Get-Item (Join-Path $verify "ALTRunNext.exe")).VersionInfo

$fileVersion =
    "{0}.{1}.{2}.{3}" -f
        $versionInfo.FileMajorPart,
        $versionInfo.FileMinorPart,
        $versionInfo.FileBuildPart,
        $versionInfo.FilePrivatePart

$productVersion =
    "{0}.{1}.{2}.{3}" -f
        $versionInfo.ProductMajorPart,
        $versionInfo.ProductMinorPart,
        $versionInfo.ProductBuildPart,
        $versionInfo.ProductPrivatePart

if ($fileVersion -ne $expectedWindowsVersion) {
    throw "EXE fixed FileVersion '$fileVersion' does not match '$expectedWindowsVersion'."
}

if ($productVersion -ne $expectedWindowsVersion) {
    throw "EXE fixed ProductVersion '$productVersion' does not match '$expectedWindowsVersion'."
}

Write-Host "Package contract verified:"
Write-Host "  Archive: $Archive"
Write-Host "  VERSION: $expectedVersion"
Write-Host "  Windows version: $expectedWindowsVersion"
Write-Host "  Top-level package entries: exact allowlist verified"

)

if (-not $semver.Success) {
    throw "Unsupported VERSION format: '$expectedVersion'."
}

$major = [int]$semver.Groups[1].Value
$minor = [int]$semver.Groups[2].Value
$patch = [int]$semver.Groups[3].Value
$channel = $semver.Groups[4].Value

$channelNumber = if ($semver.Groups[5].Success) {
    [int]$semver.Groups[5].Value
} else {
    0
}

$revision = switch ($channel) {
    "alpha" { $channelNumber }
    "beta"  { 99 + $channelNumber }
    "rc"    { 199 + $channelNumber }
    default { 300 }
}

$expectedWindowsVersion =
    "{0}.{1}.{2}.{3}" -f $major, $minor, $patch, $revision

$versionInfo =
    (Get-Item (Join-Path $verify "ALTRunNext.exe")).VersionInfo

$fileVersion =
    "{0}.{1}.{2}.{3}" -f
        $versionInfo.FileMajorPart,
        $versionInfo.FileMinorPart,
        $versionInfo.FileBuildPart,
        $versionInfo.FilePrivatePart

$productVersion =
    "{0}.{1}.{2}.{3}" -f
        $versionInfo.ProductMajorPart,
        $versionInfo.ProductMinorPart,
        $versionInfo.ProductBuildPart,
        $versionInfo.ProductPrivatePart

if ($fileVersion -ne $expectedWindowsVersion) {
    throw "EXE fixed FileVersion '$fileVersion' does not match '$expectedWindowsVersion'."
}

if ($productVersion -ne $expectedWindowsVersion) {
    throw "EXE fixed ProductVersion '$productVersion' does not match '$expectedWindowsVersion'."
}

Write-Host "Package contract verified:"
Write-Host "  Archive: $Archive"
Write-Host "  VERSION: $expectedVersion"
Write-Host "  Windows version: $expectedWindowsVersion"
Write-Host "  Top-level package entries: exact allowlist verified"
