# media-player / verify-build.ps1
# Compiles the mod into .\build\ WITHOUT touching Windhawk.
#
# Purpose: a fast, safe check that the source still builds after an edit.
# Nothing here needs administrator rights and nothing is written outside this
# folder, so a broken edit can never stop the taskbar from starting.
#
# To actually install what you build, use force-install.ps1 (elevated).
#
# If this fails with "cannot open output file ...\build\taskbar-media-widget.dll:
# Permission denied", the DLL from an earlier build is in the way: the folder was
# created by an elevated install and may hold a file the running mod has mapped.
# That is NOT a reason to think there is nothing to build - it cost three
# iterations once. Build into another folder instead:
#
#   .\verify-build.ps1 -OutDir build-verify

param(
    [string]$OutDir = 'build'
)

$ErrorActionPreference = 'Stop'

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$source = Join-Path $here 'taskbar-media-widget.wh.cpp'
$outDir = Join-Path $here $OutDir
$compiler = 'C:\Program Files\Windhawk\Compiler\bin\clang++.exe'
$include = 'C:\Program Files\Windhawk\Compiler\include'

if (-not (Test-Path $compiler)) { throw "Compiler not found: $compiler" }
if (-not (Test-Path $source)) { throw "Source not found: $source" }

$sourceText = Get-Content $source -Raw

# Safety rail carried over from the taskbar-freeze incident: the mod must never
# attach to the taskbar window. Refuse to build if that ever creeps back in.
if ($sourceText -match 'SetWindowSubclass|SetParent\(') {
    throw 'Refusing: source patches the taskbar window (SetWindowSubclass / SetParent).'
}

$version = (($sourceText -split "`n" | Select-String '@version' |
    Select-Object -First 1).Line -replace '.*@version\s+', '').Trim()
$compilerOptions = (($sourceText -split "`n" | Select-String '@compilerOptions' |
    Select-Object -First 1).Line -replace '.*@compilerOptions\s+', '').Trim() -split '\s+'

New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$dllPath = Join-Path $outDir 'taskbar-media-widget.dll'

Write-Host "source   : $source"
Write-Host "version  : $version"
Write-Host "libs     : $($compilerOptions -join ' ')"
Write-Host 'compiling...'

$compilerArgs = @(
    '-x', 'c++', '-std=c++23', '-target', 'x86_64-w64-mingw32', '-shared'
    '-DUNICODE', '-D_UNICODE'
    '-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00', '-D_WIN32_IE=0x0A00'
    '-DNTDDI_VERSION=0x0A000008'
    '-D__USE_MINGW_ANSI_STDIO=0', '-DWH_MOD', '-DWH_EDITING'
    "-I$include", "-I$(Join-Path $include 'winrt')"
    '-Wno-pragma-pack', '-Wno-pragma-system-header-outside-header'
    '-O1', $source, '-o', $dllPath
) + $compilerOptions

& $compiler @compilerArgs
if ($LASTEXITCODE -ne 0) { throw "Compilation failed with exit code $LASTEXITCODE" }

$size = (Get-Item $dllPath).Length
Write-Host ''
Write-Host "BUILD OK  version $version  $size bytes"
Write-Host "  $dllPath"
Write-Host ''
Write-Host 'Windhawk was not touched. To install this build, run force-install.ps1 elevated.'
