param(
    [string]$InstallRoot = (Get-Location).Path
)

$ErrorActionPreference = "Stop"

function Resolve-ServiceExecutable {
    param([string]$PathName)

    if ([string]::IsNullOrWhiteSpace($PathName)) {
        return $null
    }

    $trimmed = $PathName.Trim()
    if ($trimmed.StartsWith('"')) {
        $end = $trimmed.IndexOf('"', 1)
        if ($end -gt 1) {
            return $trimmed.Substring(1, $end - 1)
        }
        return $null
    }

    $match = [regex]::Match($trimmed, '^(.*?\.exe)(?:\s|$)', 'IgnoreCase')
    if ($match.Success) {
        return $match.Groups[1].Value
    }

    return $null
}

function Get-AuthenticodeSummary {
    param([Parameter(Mandatory = $true)][string]$Path)

    $signature = Get-AuthenticodeSignature -FilePath $Path
    $subject = if ($signature.SignerCertificate) {
        $signature.SignerCertificate.Subject
    } else {
        ""
    }

    [pscustomobject]@{
        Path = $Path
        Status = [string]$signature.Status
        Subject = $subject
        IsVoidtools = (
            $signature.Status -eq [System.Management.Automation.SignatureStatus]::Valid -and
            $subject -match '(?i)(?:^|,\s*)O=voidtools(?: PTY LTD)?(?:,|$)'
        )
    }
}

$install = [System.IO.Path]::GetFullPath($InstallRoot)
$managedRoot = Join-Path $install "data\tools\Everything"
$protectedRoot = Join-Path $env:ProgramFiles "Aspeternity\Asterun\EverythingService"

$failures = [System.Collections.Generic.List[string]]::new()

Write-Host "Asterun managed Everything validation"
Write-Host "  Install root:   $install"
Write-Host "  Managed root:   $managedRoot"
Write-Host "  Protected root: $protectedRoot"
Write-Host ""

$portable = @(
    Get-ChildItem -Path $managedRoot -Recurse -File -Filter "Everything.exe" -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Directory.Name -match '^\d+(?:\.\d+)+-(?:x64|ARM64)$'
        }
)

if ($portable.Count -eq 0) {
    $failures.Add("No ALTRun-managed portable Everything.exe was found under data\tools\Everything.")
} else {
    Write-Host "Portable managed candidates:"
    foreach ($item in $portable) {
        $sig = Get-AuthenticodeSummary -Path $item.FullName
        Write-Host "  $($item.FullName)"
        Write-Host "    Signature: $($sig.Status)"
        Write-Host "    Signer:    $($sig.Subject)"
        if (-not $sig.IsVoidtools) {
            $failures.Add("Portable Everything signature/publisher validation failed: $($item.FullName)")
        }
    }
    Write-Host ""
}

$service = Get-CimInstance Win32_Service -Filter "Name='Everything'" -ErrorAction SilentlyContinue
if (-not $service) {
    $failures.Add("The Windows service named 'Everything' is not installed.")
} else {
    $serviceExe = Resolve-ServiceExecutable -PathName $service.PathName

    Write-Host "Everything service:"
    Write-Host "  State:      $($service.State)"
    Write-Host "  Start mode: $($service.StartMode)"
    Write-Host "  PathName:   $($service.PathName)"

    if ([string]::IsNullOrWhiteSpace($serviceExe)) {
        $failures.Add("Could not parse the Everything service executable path.")
    } else {
        $serviceExe = [System.IO.Path]::GetFullPath($serviceExe)
        $protectedFull = [System.IO.Path]::GetFullPath($protectedRoot).TrimEnd('\') + '\'

        if (-not $serviceExe.StartsWith(
                $protectedFull,
                [System.StringComparison]::OrdinalIgnoreCase)) {
            $failures.Add("Everything service is not hosted under the protected Asterun Program Files root: $serviceExe")
        }

        if (-not (Test-Path -LiteralPath $serviceExe -PathType Leaf)) {
            $failures.Add("Everything service executable does not exist: $serviceExe")
        } else {
            $serviceSig = Get-AuthenticodeSummary -Path $serviceExe
            Write-Host "  Signature:  $($serviceSig.Status)"
            Write-Host "  Signer:     $($serviceSig.Subject)"

            if (-not $serviceSig.IsVoidtools) {
                $failures.Add("Protected Everything service host signature/publisher validation failed: $serviceExe")
            }

            $versionDirectory = Split-Path -Leaf (Split-Path -Parent $serviceExe)
            $matchingPortable = @(
                $portable | Where-Object { $_.Directory.Name -eq $versionDirectory }
            )

            if ($matchingPortable.Count -ne 1) {
                $failures.Add("Expected exactly one portable source matching service version directory '$versionDirectory'; found $($matchingPortable.Count).")
            } else {
                $sourceHash = (Get-FileHash -LiteralPath $matchingPortable[0].FullName -Algorithm SHA256).Hash
                $serviceHash = (Get-FileHash -LiteralPath $serviceExe -Algorithm SHA256).Hash

                Write-Host "  Source SHA-256:  $sourceHash"
                Write-Host "  Service SHA-256: $serviceHash"

                if ($sourceHash -ne $serviceHash) {
                    $failures.Add("Portable source and protected service host SHA-256 hashes differ.")
                }
            }
        }
    }
}

Write-Host ""
if ($failures.Count -eq 0) {
    Write-Host "PASS: managed Everything source, publisher, protected service path and copied bytes are consistent."
    exit 0
}

Write-Host "FAIL:"
foreach ($failure in $failures) {
    Write-Host "  - $failure"
}
exit 1
