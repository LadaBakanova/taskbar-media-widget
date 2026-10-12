# media-player / build-and-install.ps1
# Compiles the Taskbar media widget Windhawk mod with Windhawk's own bundled
# MinGW toolchain and installs the source into Windhawk's ModsSource folder.
#
# Run from an ELEVATED PowerShell (writing to C:\ProgramData\Windhawk needs it):
#   powershell -ExecutionPolicy Bypass -File build-and-install.ps1
#
# Add -BuildOnly to compile without installing.
#
# IMPORTANT: the link libraries are read from the mod's own @compilerOptions
# line, because that is exactly what Windhawk uses. Keeping a second hardcoded
# copy here once produced a build that succeeded locally and failed inside
# Windhawk with "undefined symbol: SetWindowSubclass" (missing -lcomctl32).

[CmdletBinding()]
param(
    [switch]$BuildOnly
)

$ErrorActionPreference = 'Stop'

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$source = Join-Path $here 'taskbar-media-widget.wh.cpp'
$output = Join-Path $here 'taskbar-media-widget.dll'
$modId = 'taskbar-media-widget'

$compilerRoot = 'C:\Program Files\Windhawk\Compiler'
$compiler = Join-Path $compilerRoot 'bin\clang++.exe'
$include = Join-Path $compilerRoot 'include'

if (-not (Test-Path $compiler)) {
    throw "Windhawk's compiler was not found at '$compiler'. Is Windhawk installed?"
}

# Refuse to install anything that still contains the mechanism which froze the
# taskbar in version 1.0.0. Cheap guard against shipping the wrong file.
$sourceText = Get-Content $source -Raw
if ($sourceText -match 'SetWindowSubclass|SetParent\(') {
    throw "Refusing to install: '$source' still patches the taskbar window. That froze the taskbar before."
}
$versionLine = ($sourceText -split "`n" | Select-String '@version' | Select-Object -First 1).Line.Trim()
Write-Host "Source version: $versionLine" -ForegroundColor Cyan

Write-Host '=== Building Taskbar media widget ===' -ForegroundColor Cyan

$compilerOptionsLine = $sourceText -split "`n" | Select-String '@compilerOptions' | Select-Object -First 1
if (-not $compilerOptionsLine) {
    throw "No @compilerOptions line found in $source"
}
$linkLibraries = ($compilerOptionsLine.Line -replace '.*@compilerOptions\s+', '').Trim() -split '\s+'
Write-Host "Link libraries: $($linkLibraries -join ' ')"

# These flags mirror Windhawk's own compile_flags.txt plus the mod's
# @compilerOptions line.
$arguments = @(
    '-x', 'c++'
    '-std=c++23'
    '-target', 'x86_64-w64-mingw32'
    '-shared'
    '-DUNICODE', '-D_UNICODE'
    '-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00', '-D_WIN32_IE=0x0A00'
    '-DNTDDI_VERSION=0x0A000008'
    '-D__USE_MINGW_ANSI_STDIO=0'
    '-DWH_MOD', '-DWH_EDITING'
    "-I$include"
    "-I$(Join-Path $include 'winrt')"
    '-Wno-pragma-pack'
    '-Wno-pragma-system-header-outside-header'
    '-O1'
    $source
    '-o', $output
) + $linkLibraries

Remove-Item $output -Force -ErrorAction SilentlyContinue
& $compiler @arguments
if ($LASTEXITCODE -ne 0) {
    throw "Compilation failed with exit code $LASTEXITCODE"
}
if (-not (Test-Path $output)) {
    throw 'Compilation reported success but produced no DLL.'
}

$dll = Get-Item $output
Write-Host "Compiled OK: $($dll.FullName) ($($dll.Length) bytes)" -ForegroundColor Green

if ($BuildOnly) {
    exit 0
}

# ---------------------------------------------------------------------------
# Install into Windhawk
# ---------------------------------------------------------------------------

$modsSource = 'C:\ProgramData\Windhawk\ModsSource'
if (-not (Test-Path $modsSource)) {
    throw "Windhawk's ModsSource folder was not found at '$modsSource'."
}

$target = Join-Path $modsSource "$modId.wh.cpp"

# Show what is about to be replaced, so it is obvious if an old version was
# still sitting there.
if (Test-Path $target) {
    $oldVersion = ((Get-Content $target -Raw) -split "`n" | Select-String '@version' | Select-Object -First 1).Line.Trim()
    Write-Host "Replacing existing source ($oldVersion)" -ForegroundColor Yellow
}

Copy-Item $source $target -Force

$installedVersion = ((Get-Content $target -Raw) -split "`n" | Select-String '@version' | Select-Object -First 1).Line.Trim()
Write-Host "Installed: $target" -ForegroundColor Green
Write-Host "Installed version: $installedVersion" -ForegroundColor Green

# Remove compiled libraries of the unsafe 1.0.0 build so it can never be loaded
# by accident.
$oldLibs = Get-ChildItem 'C:\ProgramData\Windhawk\Engine' -Recurse `
    -Filter "$modId*" -File -ErrorAction SilentlyContinue
foreach ($lib in $oldLibs) {
    if ($lib.Name -notmatch '_1\.2\.0_') {
        Remove-Item $lib.FullName -Force -ErrorAction SilentlyContinue
        Write-Host "Removed stale compiled library: $($lib.Name)" -ForegroundColor Yellow
    }
}

# Make sure no registered entry can load an old build.
$regPath = "HKLM:\SOFTWARE\Windhawk\Engine\Mods\$modId"
if (Test-Path $regPath) {
    Remove-Item $regPath -Recurse -Force
    Write-Host 'Removed previous Windhawk registration for this mod.' -ForegroundColor Yellow
}

Write-Host ''
Write-Host '=== NEXT STEPS IN THE WINDHAWK UI ===' -ForegroundColor Cyan
Write-Host ''
Write-Host 'The stale 1.0.0 source has been replaced with 1.2.0, so compiling in'
Write-Host 'the UI now produces the safe build.'
Write-Host ''
Write-Host '  1. Open Windhawk.'
Write-Host "  2. Find the mod 'Taskbar media widget' in the Mods list."
Write-Host '  3. Press COMPILE, then turn the mod ON.'
Write-Host '  4. Start playback and check the taskbar.'
Write-Host ''
Write-Host 'Your other mods (Taskbar multi-tray, Windows 11 Taskbar Styler) are'
Write-Host 'currently DISABLED. Leave them off for the first test, then re-enable.'
Write-Host ''
Write-Host 'If the taskbar freezes: run recover.cmd as Administrator.' -ForegroundColor Red
