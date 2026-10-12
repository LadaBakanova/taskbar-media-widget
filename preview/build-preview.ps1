# media-player / build-preview.ps1
# Builds preview.exe: the widget's own painting code, rendered off screen.
#
# Purpose: check the widget against design.pen pixel by pixel without installing
# anything into Windhawk and without administrator rights. The mod source is
# included by preview.cpp, so this builds exactly the code that ships.
#
# The two Inter weights are copied next to the executable, because that is where
# the mod looks for them.

$ErrorActionPreference = 'Stop'

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $here
$source = Join-Path $here 'preview.cpp'
$modSource = Join-Path $root 'taskbar-media-widget.wh.cpp'
$outDir = Join-Path $here 'build'
$compiler = 'C:\Program Files\Windhawk\Compiler\bin\clang++.exe'
$include = 'C:\Program Files\Windhawk\Compiler\include'

if (-not (Test-Path $compiler)) { throw "Compiler not found: $compiler" }
if (-not (Test-Path $modSource)) { throw "Mod source not found: $modSource" }

$compilerOptions = ((Get-Content $modSource -Raw) -split "`n" | Select-String '@compilerOptions' |
    Select-Object -First 1).Line -replace '.*@compilerOptions\s+', '' -split '\s+'

New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$exePath = Join-Path $outDir 'preview.exe'

$compilerArgs = @(
    '-x', 'c++', '-std=c++23', '-target', 'x86_64-w64-mingw32'
    '-DUNICODE', '-D_UNICODE'
    '-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00', '-D_WIN32_IE=0x0A00'
    '-DNTDDI_VERSION=0x0A000008'
    '-D__USE_MINGW_ANSI_STDIO=0', '-DWH_MOD', '-DWH_EDITING', '-municode'
    "-I$include", "-I$(Join-Path $include 'winrt')"
    '-Wno-pragma-pack', '-Wno-pragma-system-header-outside-header'
    '-O1', $source, '-o', $exePath
) + $compilerOptions + @('-l:libc++.a', '-l:libunwind.a', '-l:libc++abi.a')

Write-Host "compiling $source"
& $compiler @compilerArgs
if ($LASTEXITCODE -ne 0) { throw "Compilation failed with exit code $LASTEXITCODE" }

foreach ($font in 'Inter-Regular.ttf', 'Inter-Medium.ttf') {
    $from = Join-Path $root "fonts\$font"
    if (Test-Path $from) {
        Copy-Item $from $outDir -Force
    } else {
        Write-Host "WARNING: $from not found, the preview will fall back to a system font"
    }
}

Write-Host ''
Write-Host "BUILD OK  $exePath"
