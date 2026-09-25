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

    if ($migratedSettings.schemaVersion -ne 11) {
        throw "Packaged runtime did not migrate schema-2 settings to schema 11."
    }

    if ($migratedSettings.general.soundEnabled -ne $true) {
        throw "Settings migration must enable application sounds by default."
    }

    if ($migratedSettings.general.startupBehavior -ne "silent") {
        throw "Packaged runtime migration must preserve legacy showOnStartup=false as startupBehavior=silent."
    }

    foreach ($removedSetting in @(
        "showOnStartup",
        "hideAfterLaunch",
        "clearQueryOnShow",
        "hideOnFocusLost"
    )) {
        if ($migratedSettings.general.PSObject.Properties.Name -contains $removedSetting) {
            throw "Packaged runtime migration must not serialize removed general setting '$removedSetting'."
        }
    }

    foreach ($removedSetting in @(
        "wildcardMatching",
        "numericQuickLaunchOrder"
    )) {
        if ($migratedSettings.behavior.PSObject.Properties.Name -contains $removedSetting) {
            throw "Packaged runtime migration must not serialize removed behavior setting '$removedSetting'."
        }
    }

    if ($migratedSettings.windowPlacement.launcherMode -ne "top" -or
        $migratedSettings.windowPlacement.settingsMode -ne "center" -or
        $migratedSettings.windowPlacement.shortcutManagerMode -ne "center" -or
        $migratedSettings.windowPlacement.launcherLastValid -ne $false -or
        $migratedSettings.windowPlacement.settingsLastValid -ne $false -or
        $migratedSettings.windowPlacement.shortcutManagerLastValid -ne $false) {
        throw "Packaged runtime migration must default window placement to launcher=top/settings=center/shortcutManager=center with no remembered positions."
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

    $version = (Get-Content $versionPath -Raw).Trim()
    $expectedUpdateChannel = "stable"

    if ($migratedSettings.update.channel -ne $expectedUpdateChannel) {
        throw "Packaged runtime migration update channel '$($migratedSettings.update.channel)' does not match expected '$expectedUpdateChannel'."
    }

    $expectedHotkeyActions = @(
        "launcher.activate",
        "launcher.activateSecondary",
        "launcher.openSettings",
        "launcher.openShortcutManager",
        "launcher.exitApplication",
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

    $fileVersion = (Get-Item $exe).VersionInfo.FileVersion

    Write-Host "Portable runtime smoke passed:"
    Write-Host "  VERSION: $version"
    Write-Host "  FileVersion string: $fileVersion"
    Write-Host "  Process id: $($process.Id)"
    Write-Host "  Startup observation: $StartupSeconds seconds"
    Write-Host "  Runtime migration: schema 2 -> 10 with startup-behavior cleanup + current Hotkey Registry + default window placement + default-on Pinyin + default-off result icons + release-appropriate update defaults"
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
