# Runs a kit script in Windows PowerShell 5.1, saves its complete output and
# reports exit code and output (head and tail) as a notice annotation.
# Exits with the script's exit code.
param(
    [Parameter(Mandatory = $true)][string]$Title,
    [Parameter(Mandatory = $true)][string]$Log,
    [Parameter(Mandatory = $true)][string]$Script,
    [string[]]$Arguments = @()
)
$powershell = "$env:WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe"
& $powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $Script @Arguments *> $Log
$code = $LASTEXITCODE
$lines = @(Get-Content -LiteralPath $Log -ErrorAction SilentlyContinue)
$shown = if ($lines.Count -le 120) { $lines } else { @($lines[0..39]) + @("... ($($lines.Count - 100) lines omitted) ...") + @($lines[($lines.Count - 60)..($lines.Count - 1)]) }
& (Join-Path $PSScriptRoot 'Write-Notice.ps1') -Title "$Title (exit $code)" -Lines $shown
exit $code
