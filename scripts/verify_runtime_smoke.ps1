param(
    [Parameter(Mandatory = $true)]
    [string]$Archive,
    [int]$StartupSeconds = 4
)

$ErrorActionPreference = "Stop"

if ($env:OS -ne "Windows_NT") {
    throw "Portable runtime smoke must run on Windows."
}

$archivePath = (Resolve-Path $Archive).Path
$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) (
    "ALTRunNext-runtime-smoke-" + [guid]::NewGuid().ToString("N")
)

New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

$process = $null

try {
    Expand-Archive -Path $archivePath -DestinationPath $tempRoot -Force

    $exe = Join-Path $tempRoot "ALTRunNext.exe"
    $updater = Join-Path $tempRoot "Update.exe"
    $uninstaller = Join-Path $tempRoot "Uninstall.exe"
    $legacyUpdaterBridge = Join-Path $tempRoot "ALTRunNext.Updater.exe"
    $versionPath = Join-Path $tempRoot "VERSION"
    $data = Join-Path $tempRoot "data"

    if (-not (Test-Path $exe)) {
        throw "Portable runtime smoke archive has no ALTRunNext.exe."
    }
    if (-not (Test-Path $updater)) {
        throw "Portable runtime smoke archive has no Update.exe."
    }
    if (-not (Test-Path $uninstaller)) {
        throw "Portable runtime smoke archive has no Uninstall.exe."
    }
    if (-not (Test-Path $legacyUpdaterBridge)) {
        throw "Portable runtime smoke archive has no alpha.9 compatibility updater bridge."
    }

    New-Item -ItemType Directory -Force -Path $data | Out-Null

    # Use an intentionally unlikely hotkey and disable providers so the smoke
    # test exercises portable startup/message-loop/data initialization without
    # depending on the hosted runner's desktop apps or Alt+Space ownership.
    $settings = @'
{
  "schemaVersion": 2,
  "general": {
    "startWithWindows": false,
    "showOnStartup": false,
    "hideAfterLaunch": true,
    "clearQueryOnShow": true,
    "hideOnFocusLost": true,
    "showTrayIcon": false,
    "popupMonitor": "cursor"
  },
  "hotkey": {
    "modifiers": ["ctrl", "shift"],
    "key": "f24",
    "auxiliary": {
      "enabled": false,
      "modifiers": [],
      "key": "pause"
    }
  },
  "behavior": {
    "wildcardMatching": false,
    "numericQuickLaunch": false,
    "numericQuickLaunchOrder": "one-to-zero",
    "executeSingleResultImmediately": false
  },
  "appearance": {
    "launcher": "classic",
    "language": "en-US"
  },
  "providers": {
    "windows.startmenu": false,
    "windows.packaged": false,
    "windows.apppaths": false,
    "windows.path": false
  }
}
'@

    Set-Content -Path (Join-Path $data "settings.json") -Value $settings -Encoding utf8 -NoNewline

    # The schema-2 fixture intentionally migrates through the current schema.
    # Seed a recent runtime-only update check timestamp so this startup smoke
    # never depends on GitHub/network availability while still verifying that
    # migration defaults autoCheck to true.
    $updateRoot = Join-Path $data "update"
    New-Item -ItemType Directory -Force -Path $updateRoot | Out-Null
    $nowUnix = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
    $updateState = @{
        schemaVersion = 1
        lastCheckUnix = $nowUnix
    } | ConvertTo-Json
    Set-Content -Path (Join-Path $updateRoot "update-state.json") -Value $updateState -Encoding utf8 -NoNewline

    $process = Start-Process -FilePath $exe -WorkingDirectory $tempRoot -PassThru

    Start-Sleep -Seconds $StartupSeconds
    $process.Refresh()

    if ($process.HasExited) {
        throw "ALTRunNext exited during portable runtime startup smoke with code $($process.ExitCode)."
    }

    $probeResidue = @(
        Get-ChildItem -Path $data -Filter ".altrun-write-test-*.tmp" -File -ErrorAction SilentlyContinue
    )

    if ($probeResidue.Count -ne 0) {
        $probeResidue | ForEach-Object { Write-Host $_.FullName }
        throw "Startup writeability probe left temporary files behind."
    }

    $settingsPath = Join-Path $data "settings.json"
    if (-not (Test-Path $settingsPath)) {
        throw "Portable runtime smoke did not preserve/create settings.json."
    }

    $migratedSettings =
        Get-Content $settingsPath -Raw |
        ConvertFrom-Json

    if ($migratedSettings.schemaVersion -ne 7) {
        throw "Packaged runtime did not migrate schema-2 settings to schema 7."
    }

    if ($migratedSettings.behavior.pinyinSearch -ne $true) {
        throw "Packaged runtime migration must default Pinyin search to enabled."
    }

    if ($migratedSettings.appearance.showResultIcons -ne $false) {
        throw "Packaged runtime migration must default search-result icons to disabled."
    }

    if ($migratedSettings.update.autoCheck -ne $true) {
        throw "Packaged runtime migration must default automatic update checks to enabled."
    }

    if ($migratedSettings.update.channel -ne "development") {
        throw "Packaged prerelease runtime migration must default to the Development update channel."
    }

    $expectedHotkeyActions = @(
        "launcher.activate",
        "launcher.activateSecondary",
        "launcher.openSettings",
        "result.navigateCurrentFileManager",
        "result.copySelectedTarget"
    )

    $actualHotkeyActions = @(
        $migratedSettings.hotkeys.bindings.PSObject.Properties.Name
    )

    foreach ($actionId in $expectedHotkeyActions) {
        if ($actionId -notin $actualHotkeyActions) {
            throw "Packaged runtime migration is missing Hotkey Registry action '$actionId'."
        }
    }

    if ($migratedSettings.providers.'everything.filesystem' -ne $false) {
        throw "Packaged runtime migration must keep Everything disabled when legacy settings did not opt in."
    }

    $version = (Get-Content $versionPath -Raw).Trim()
    $fileVersion = (Get-Item $exe).VersionInfo.FileVersion

    Write-Host "Portable runtime smoke passed:"
    Write-Host "  VERSION: $version"
    Write-Host "  FileVersion string: $fileVersion"
    Write-Host "  Process id: $($process.Id)"
    Write-Host "  Startup observation: $StartupSeconds seconds"
    Write-Host "  Runtime migration: schema 2 -> 7 with frozen Hotkey Registry + default-on Pinyin + default-off result icons + Development update defaults"
}
finally {
    if ($null -ne $process) {
        try {
            $process.Refresh()
            if (-not $process.HasExited) {
                Stop-Process -Id $process.Id -Force
                Wait-Process -Id $process.Id -ErrorAction SilentlyContinue
            }
        }
        catch {
            Write-Warning "Unable to stop runtime-smoke process cleanly: $($_.Exception.Message)"
        }
    }

    if (Test-Path $tempRoot) {
        Remove-Item $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
