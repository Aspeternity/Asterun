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
    "Asterun.exe",
    "Update.exe",
    "Uninstall.exe",
    "VERSION",
    "README.md",
    "dict",
    "third_party/cpp-pinyin-LICENSE.txt",
    "third_party/miniz-LICENSE.txt"
)

foreach ($entry in $required) {
    if (-not (Test-Path (Join-Path $verify $entry))) {
        throw "Release package is missing required entry: $entry"
    }
}

$allowedTopLevel = @(
    "Asterun.exe",
    "Update.exe",
    "Uninstall.exe",
    "VERSION",
    "README.md",
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

$iconAssets = @(
    "src/resources/asterun.ico",
    "src/resources/asterun_tray.ico"
)

foreach ($asset in $iconAssets) {
    if (-not (Test-Path $asset -PathType Leaf)) {
        throw "Missing Asterun icon asset: $asset"
    }
}

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class AsterunIconProbe
{
    [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
    public static extern uint ExtractIconEx(
        string file,
        int index,
        IntPtr[] large,
        IntPtr[] small,
        uint count);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool DestroyIcon(IntPtr icon);
}
"@

function Test-EmbeddedExecutableIcon {
    param([Parameter(Mandatory = $true)][string]$Path)

    $large = New-Object IntPtr[] 1
    $small = New-Object IntPtr[] 1
    $count = [AsterunIconProbe]::ExtractIconEx(
        $Path,
        0,
        $large,
        $small,
        1)

    $present =
        $count -ge 1 -and
        ($large[0] -ne [IntPtr]::Zero -or
         $small[0] -ne [IntPtr]::Zero)

    foreach ($handle in @($large[0], $small[0])) {
        if ($handle -ne [IntPtr]::Zero) {
            [void][AsterunIconProbe]::DestroyIcon($handle)
        }
    }

    return $present
}

foreach ($exe in @("Asterun.exe", "Update.exe", "Uninstall.exe")) {
    $path = Join-Path $verify $exe

    if (-not (Test-EmbeddedExecutableIcon -Path $path)) {
        throw "Packaged executable has no embedded icon resource: $exe"
    }
}

$expectedVersion = (Get-Content VERSION -Raw).Trim()
$packagedVersion = (Get-Content (Join-Path $verify "VERSION") -Raw).Trim()

if ($packagedVersion -ne $expectedVersion) {
    throw "Packaged VERSION '$packagedVersion' does not match '$expectedVersion'."
}

$semver = [regex]::Match(
    $expectedVersion,
    '^(\d+)\.(\d+)\.(\d+)(?:-(alpha|beta|rc)\.(\d+)(?:\.(\d+))?)?$'
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

$channelPatch = if ($semver.Groups[6].Success) {
    [int]$semver.Groups[6].Value
} else {
    0
}

if ($channelPatch -ne 0 -and $channel -ne "alpha") {
    throw "Only alpha prereleases currently support a hotfix component."
}

if ($channel -eq "alpha") {
    if ($channelNumber -ge 4) {
        # Keep historical alpha.3 revisions stable (ending at 70), then
        # reserve 100 revision slots per new alpha phase so Windows
        # FileVersion remains monotonic across phase transitions.
        if ($channelPatch -eq 0) {
            throw "Alpha phase 4+ requires a hotfix component, e.g. alpha.4.1."
        }
        if ($channelPatch -ge 100) {
            throw "Alpha phase 4+ hotfix component must stay below 100."
        }
        $revision =
            70 +
            (($channelNumber - 4) * 100) +
            $channelPatch
    } elseif ($channelPatch -ne 0 -or $channelNumber -ge 3) {
        $revision = $channelNumber * 10 + $channelPatch
    } else {
        $revision = $channelNumber
    }
} elseif ($channel -eq "beta") {
    $revision = 10000 + $channelNumber
} elseif ($channel -eq "rc") {
    $revision = 20000 + $channelNumber
} else {
    $revision = 30000
}

$expectedWindowsVersion =
    "{0}.{1}.{2}.{3}" -f $major, $minor, $patch, $revision

$versionInfo =
    (Get-Item (Join-Path $verify "Asterun.exe")).VersionInfo

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
