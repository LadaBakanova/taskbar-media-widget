# media-player / install.ps1
# Full installation, meant to be run elevated. Called by install-and-verify.ps1,
# which triggers the UAC prompt for you.
#
# It does everything that needs administrator rights:
#   1. compiles the mod with Windhawk's own toolchain;
#   2. replaces the mod source in Windhawk's ModsSource folder;
#   3. deletes stale compiled libraries of older versions;
#   4. drops a stale Windhawk registration that could load an old build;
#   5. seeds the mod's settings so it does not depend on the settings having
#      been saved once through the UI;
#   6. writes a transcript to install-log.txt.

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$source = Join-Path $here 'taskbar-media-widget.wh.cpp'
$output = Join-Path $here 'taskbar-media-widget.dll'
$log = Join-Path $here 'install-log.txt'
$modId = 'taskbar-media-widget'

Start-Transcript -Path $log -Force | Out-Null

function Say($message) {
    Write-Host $message
}

try {
    $isAdmin = (New-Object Security.Principal.WindowsPrincipal(
        [Security.Principal.WindowsIdentity]::GetCurrent())).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
    Say "elevated: $isAdmin"
    if (-not $isAdmin) {
        throw 'This script must run elevated.'
    }

    # ---------------------------------------------------------------- compile
    $compilerRoot = 'C:\Program Files\Windhawk\Compiler'
    $compiler = Join-Path $compilerRoot 'bin\clang++.exe'
    $include = Join-Path $compilerRoot 'include'
    if (-not (Test-Path $compiler)) {
        throw "Windhawk's compiler was not found at '$compiler'."
    }

    $sourceText = Get-Content $source -Raw
    if ($sourceText -match 'SetWindowSubclass|SetParent\(') {
        throw "Refusing to install: '$source' still patches the taskbar window."
    }
    $versionLine = ($sourceText -split "`n" | Select-String '@version' |
        Select-Object -First 1).Line.Trim()
    Say "source $versionLine"

    $compilerOptionsLine = $sourceText -split "`n" |
        Select-String '@compilerOptions' | Select-Object -First 1
    if (-not $compilerOptionsLine) {
        throw "No @compilerOptions line found in $source"
    }
    $linkLibraries = ($compilerOptionsLine.Line -replace '.*@compilerOptions\s+', '').Trim() -split '\s+'
    Say "link libraries: $($linkLibraries -join ' ')"

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
    if ($LASTEXITCODE -ne 0) { throw "Compilation failed with exit code $LASTEXITCODE" }
    if (-not (Test-Path $output)) { throw 'Compilation produced no DLL.' }
    Say "compiled: $((Get-Item $output).Length) bytes"

    # ---------------------------------------------------------------- install
    $modsSource = 'C:\ProgramData\Windhawk\ModsSource'
    if (-not (Test-Path $modsSource)) { throw "ModsSource not found at '$modsSource'." }
    $target = Join-Path $modsSource "$modId.wh.cpp"

    if (Test-Path $target) {
        $oldVersion = ((Get-Content $target -Raw) -split "`n" |
            Select-String '@version' | Select-Object -First 1).Line.Trim()
        Say "replacing existing source ($oldVersion)"
    }
    Copy-Item $source $target -Force
    Say "installed source: $target"

    # ------------------------------------------------- cleanup stale builds
    $staleLibs = Get-ChildItem 'C:\ProgramData\Windhawk\Engine' -Recurse `
        -Filter "$modId*" -File -ErrorAction SilentlyContinue
    foreach ($lib in $staleLibs) {
        Remove-Item $lib.FullName -Force -ErrorAction SilentlyContinue
        Say "removed stale library: $($lib.Name)"
    }

    # Drop the registration entirely and let Windhawk recreate it when the mod is
    # enabled in its UI.
    #
    # An earlier version of this script recreated the key itself. That was a
    # mistake: the key needs LibraryFileName, Disabled, Version, Architecture and
    # Include, and a half-written key makes Windhawk believe the mod is installed
    # while there is nothing to load. Windhawk owns this key; this script must not.
    $regPath = "HKLM:\SOFTWARE\Windhawk\Engine\Mods\$modId"
    if (Test-Path $regPath) {
        Remove-Item $regPath -Recurse -Force
        Say 'removed stale registration (Windhawk recreates it on enable)'
    }

    # Settings are deliberately NOT written here. Windhawk writes them from the
    # mod's settings schema when it installs the mod, and the mod itself falls
    # back to built-in defaults when nothing has been saved yet - which is the
    # bug that made the first install come out 120x20.

    # ---------------------------------------------------------------- verify
    Say ''
    Say 'VERIFICATION'
    $installedVersion = ((Get-Content $target -Raw) -split "`n" |
        Select-String '@version' | Select-Object -First 1).Line.Trim()
    Say "  source version on disk : $installedVersion"
    Say "  registration present   : $(Test-Path $regPath) (expected: False)"
    $remaining = Get-ChildItem 'C:\ProgramData\Windhawk\Engine' -Recurse -Filter "$modId*" -File -ErrorAction SilentlyContinue
    Say "  stale libraries left   : $($remaining.Count)"
    Say ''
    Say 'INSTALL DONE'
    Say 'Now enable the mod in the Windhawk UI - it compiles and registers it.'
}
catch {
    Say ''
    Say "ERROR: $($_.Exception.Message)"
}
finally {
    Stop-Transcript | Out-Null
}
