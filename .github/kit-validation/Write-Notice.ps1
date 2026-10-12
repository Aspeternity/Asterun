# Writes one GitHub Actions notice annotation from a title and lines of text.
param([Parameter(Mandatory = $true)][string]$Title, [string[]]$Lines = @(), [string]$File = '')
if ($File) { $Lines += @(Get-Content -LiteralPath $File) }
function Escape([string]$text, [bool]$property) {
    $text = $text.Replace('%', '%25').Replace("`r", '').Replace("`n", '%0A')
    if ($property) { $text = $text.Replace(':', '%3A').Replace(',', '%2C') }
    return $text
}
$body = (@($Lines | ForEach-Object { Escape ([string]$_) $false })) -join '%0A'
if ($body.Length -gt 60000) { $body = $body.Substring(0, 60000) + '%0A...(truncated)' }
Write-Output "::notice title=$(Escape $Title $true)::$body"
