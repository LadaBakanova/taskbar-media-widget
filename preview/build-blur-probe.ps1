# media-player / build-blur-probe.ps1
# Builds blur-probe.exe: five background variants of the widget's panel shown on
# the live desktop side by side, photographed at the end of the run.
#
# Purpose: prove (or disprove) that a top-level acrylic backdrop window gives the
# layered widget a real blurred background, without installing anything into
# Windhawk and without administrator rights.

$ErrorActionPreference = 'Stop'

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$source = Join-Path $here 'blur-probe.cpp'
$outDir = Join-Path $here 'build'
$compiler = 'C:\Program Files\Windhawk\Compiler\bin\clang++.exe'
$include = 'C:\Program Files\Windhawk\Compiler\include'

if (-not (Test-Path $compiler)) { throw "Compiler not found: $compiler" }
if (-not (Test-Path $source)) { throw "Source not found: $source" }

New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$exePath = Join-Path $outDir 'blur-probe.exe'

$compilerArgs = @(
    '-x', 'c++', '-std=c++23', '-target', 'x86_64-w64-mingw32'
    '-DUNICODE', '-D_UNICODE'
    '-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00', '-D_WIN32_IE=0x0A00'
    '-DNTDDI_VERSION=0x0A000008'
    '-D__USE_MINGW_ANSI_STDIO=0'
    "-I$include"
    '-Wno-pragma-pack', '-Wno-pragma-system-header-outside-header'
    '-O1', $source, '-o', $exePath
    '-municode'
    '-l:libc++.a', '-l:libunwind.a', '-l:libc++abi.a'
    '-lgdiplus', '-ldwmapi', '-lgdi32', '-luser32', '-lole32'
)

Write-Host "compiling $source"
& $compiler @compilerArgs
if ($LASTEXITCODE -ne 0) { throw "Compilation failed with exit code $LASTEXITCODE" }

Write-Host ''
Write-Host "BUILD OK  $exePath"
