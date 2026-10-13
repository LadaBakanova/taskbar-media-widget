# media-player / run-probe.ps1
# Builds and runs the media session probe. Read-only diagnostic: it never sends
# a command to your player, and it needs no administrator rights.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File run-probe.ps1
#   powershell -ExecutionPolicy Bypass -File run-probe.ps1 -Watch
#
# Plain mode   : analyses the currently playing track and prints a VERDICT.
# -Watch mode  : prints every media session once a second for 30 s, which shows
#                whether a session appears at all while music is playing.
#
# TWO LESSONS BAKED INTO THIS SCRIPT - do not "simplify" them away:
#
# 1. The MinGW runtime is linked statically on purpose. Linking it dynamically
#    makes the .exe import libc++.whl / libunwind.whl, which do not exist on a
#    normal system, and the program then fails to start with 0xC0000135.
# 2. The executable is launched directly and its streams are captured by
#    PowerShell. Start-Process with redirected output goes through pipes, which
#    some environments block, and it also hides the exit code when the process
#    cannot start at all.

[CmdletBinding()]
param(
    [switch]$Watch
)

$ErrorActionPreference = 'Stop'

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$source = Join-Path $here 'probe-media.cpp'
$exe = Join-Path $here 'probe-media.exe'
$log = Join-Path $here $(if ($Watch) { 'probe-watch.txt' } else { 'probe-result.txt' })

$compiler = 'C:\Program Files\Windhawk\Compiler\bin\clang++.exe'
$include = 'C:\Program Files\Windhawk\Compiler\include'

if (-not (Test-Path $compiler)) {
    throw "Windhawk's compiler was not found at '$compiler'."
}

Write-Host '=== Building the probe ===' -ForegroundColor Cyan
& $compiler -x c++ -std=c++23 -target x86_64-w64-mingw32 -municode -O1 `
    "-I$include" "-I$(Join-Path $include 'winrt')" `
    $source -o $exe `
    -lwindowsapp -lruntimeobject -lole32 -loleaut32 -luser32 -lshlwapi `
    -l:libc++.a -l:libunwind.a -l:libc++abi.a

if ($LASTEXITCODE -ne 0) {
    throw "Compilation failed with exit code $LASTEXITCODE"
}

if ($Watch) {
    Write-Host ''
    Write-Host '=== Watching media sessions for 30 seconds ===' -ForegroundColor Cyan
    Write-Host 'START PLAYBACK IN YANDEX MUSIC NOW - a track must be audible.' -ForegroundColor Yellow
} else {
    Write-Host ''
    Write-Host '=== Running the probe (about 20 seconds) ===' -ForegroundColor Cyan
    Write-Host 'A track must already be PLAYING in Yandex Music.' -ForegroundColor Yellow
}
Write-Host ''

$launchFailed = $false
try {
    if ($Watch) {
        $output = & $exe --watch 2>&1
    } else {
        $output = & $exe 2>&1
    }
} catch {
    $launchFailed = $true
    $output = @("The probe could not be started: $($_.Exception.Message)")
}

$exitCode = $LASTEXITCODE
$output | ForEach-Object { Write-Host $_ }

if ($launchFailed -or $exitCode -eq -1073741515) {
    Write-Host ''
    Write-Host '[!] The probe did not run.' -ForegroundColor Red
    if ($exitCode -eq -1073741515) {
        Write-Host '    Exit code -1073741515 is 0xC0000135: a runtime DLL is missing.' -ForegroundColor Red
        Write-Host '    The MinGW runtime must be linked statically. Check the compiler' -ForegroundColor Red
        Write-Host '    flags in this script: -l:libc++.a -l:libunwind.a -l:libc++abi.a' -ForegroundColor Red
    }
    exit 1
}

$output | Set-Content -Path $log -Encoding UTF8

Write-Host ''
Write-Host "Saved to: $log" -ForegroundColor Green
Write-Host 'Send me the VERDICT block.' -ForegroundColor Green
