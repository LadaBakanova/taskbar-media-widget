param(
  [Parameter(Mandatory=$true)][string]$Url,
  [string]$Grep = "",
  [int]$Context = 3,
  [int]$Max = 200
)

$ErrorActionPreference = 'Stop'
try {
  $r = Invoke-WebRequest -Uri $Url -UseBasicParsing -TimeoutSec 40 -Headers @{
    'User-Agent' = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Safari/537.36'
    'Accept-Language' = 'en-US,en;q=0.9'
  }
  $html = $r.Content
} catch {
  Write-Output "FETCH-ERR: $($_.Exception.Message)"
  exit 1
}

# strip script/style
$html = [regex]::Replace($html, '(?is)<script.*?</script>', ' ')
$html = [regex]::Replace($html, '(?is)<style.*?</style>', ' ')
$html = [regex]::Replace($html, '(?is)<noscript.*?</noscript>', ' ')
$html = [regex]::Replace($html, '(?is)<svg.*?</svg>', ' ')
# block tags -> newline
$html = [regex]::Replace($html, '(?i)<(br|/p|/div|/li|/tr|/h[1-6]|/pre|/td|/th)[^>]*>', "`n")
$html = [regex]::Replace($html, '(?s)<[^>]+>', ' ')
# entities
$html = $html -replace '&nbsp;', ' ' -replace '&lt;', '<' -replace '&gt;', '>' -replace '&amp;', '&' -replace '&quot;', '"' -replace '&#39;', "'"
$html = [regex]::Replace($html, '&#\d+;', ' ')

$lines = $html -split "`n" | ForEach-Object { ($_ -replace '[ \t]+', ' ').Trim() } | Where-Object { $_ -ne '' }

if ($Grep -ne '') {
  $pattern = $Grep
  $hits = for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match $pattern) { $i }
  }
  $shown = @{}
  $count = 0
  foreach ($h in $hits) {
    if ($count -ge $Max) { break }
    $s = [Math]::Max(0, $h - $Context)
    $e = [Math]::Min($lines.Count - 1, $h + $Context)
    for ($j = $s; $j -le $e; $j++) {
      if (-not $shown.ContainsKey($j)) { $shown[$j] = $true }
    }
    $count++
  }
  $keys = $shown.Keys | Sort-Object
  foreach ($k in $keys) { Write-Output $lines[$k] }
} else {
  $lines | Select-Object -First 400 | ForEach-Object { Write-Output $_ }
}
