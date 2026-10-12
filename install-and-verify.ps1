# media-player / install-and-verify.ps1
# Runs the privileged installer through a UAC prompt, then reports the result.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File install-and-verify.ps1
#
# A UAC prompt will appear - approve it. Everything else is automatic.
#
# This runs force-install.ps1, not install.ps1. force-install.ps1 is the path
# that actually works: it compiles the mod, installs the library, writes the
# full Windhawk registration and repairs userprofile.json. install.ps1 is kept
# only as history - it stopped writing the registration on purpose, because
# hand-writing Windhawk's bookkeeping is what broke recompilation in the first
# place.

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$installer = Join-Path $here 'force-install.ps1'
$log = Join-Path $here 'force-install-log.txt'

foreach ($file in @($installer)) {
    if (-not (Test-Path $file)) {
        throw "Missing file: $file"
    }
}

Remove-Item $log -Force -ErrorAction SilentlyContinue

Write-Host 'Requesting administrator rights (approve the UAC prompt)...' -ForegroundColor Cyan

# -Wait is required: without it the log would be read before it is written.
try {
    Start-Process powershell -Verb RunAs -Wait -ArgumentList @(
        '-NoProfile'
        '-ExecutionPolicy', 'Bypass'
        '-File', $installer
    )
} catch {
    Write-Host ''
    Write-Host 'The UAC prompt was cancelled, so nothing was changed.' -ForegroundColor Yellow
    Write-Host 'Run this script again when you are ready to approve it.' -ForegroundColor Yellow
    exit 1
}

if (-not (Test-Path $log)) {
    throw "The installer produced no log. Elevation was probably cancelled."
}

Write-Host ''
Get-Content $log | ForEach-Object { Write-Host $_ }

$text = Get-Content $log -Raw
if ($text -match 'FORCE INSTALL DONE') {
    Write-Host ''
    Write-Host 'Installer finished successfully.' -ForegroundColor Green
    exit 0
} else {
    Write-Host ''
    Write-Host 'Installer reported a problem - see the log above.' -ForegroundColor Red
    exit 1
}
