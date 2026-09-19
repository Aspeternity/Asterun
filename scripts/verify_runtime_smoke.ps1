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
    $versionPath = Join-Path $tempRoot "VERSION"
    $data = Join-Path $tempRoot "data"

    if (-not (Test-Path $exe)) {
        throw "Portable runtime smoke archive has no ALTRunNext.exe."
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

    $version = (Get-Content $versionPath -Raw).Trim()
    $fileVersion = (Get-Item $exe).VersionInfo.FileVersion

    Write-Host "Portable runtime smoke passed:"
    Write-Host "  VERSION: $version"
    Write-Host "  FileVersion string: $fileVersion"
    Write-Host "  Process id: $($process.Id)"
    Write-Host "  Startup observation: $StartupSeconds seconds"
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
