# media-player / force-install.ps1
# Installs the mod into Windhawk without using the Windhawk UI, for the case
# where its bookkeeping is out of sync with the files on disk.
#
# Background: Windhawk records installed mod versions in userprofile.json. It
# had "taskbar-media-widget 1.2.0" recorded, while that build no longer existed,
# so it considered the mod installed and never rebuilt it - leaving the mod
# enabled with no library to load. This script repairs that state directly.
#
# Run elevated (install-and-verify.ps1 triggers the UAC prompt).
#
# -Logging turns Windhawk's log for this mod on while the build is installed,
# which is what makes the visualizer's own lines ("visualizer: capturing ...",
# "visualizer: loudest block so far ...") visible. It is off by default: the
# visualizer reports its peak once a second, and that is a lot of text to keep
# in a log nobody is reading.

param(
    [switch]$Logging
)

$ErrorActionPreference = 'Stop'

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$source = Join-Path $here 'taskbar-media-widget.wh.cpp'
$log = Join-Path $here 'force-install-log.txt'
$modId = 'taskbar-media-widget'
$modsRoot = 'C:\ProgramData\Windhawk\Engine\Mods'
$modsDir = Join-Path $modsRoot '64'
$profilePath = 'C:\ProgramData\Windhawk\userprofile.json'

Start-Transcript -Path $log -Force | Out-Null
function Say($m) { Write-Host $m }

try {
    $isAdmin = (New-Object Security.Principal.WindowsPrincipal(
        [Security.Principal.WindowsIdentity]::GetCurrent())).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $isAdmin) { throw 'Must run elevated.' }

    $sourceText = Get-Content $source -Raw
    if ($sourceText -match 'SetWindowSubclass|SetParent\(') {
        throw 'Refusing: source still patches the taskbar window.'
    }
    $version = (($sourceText -split "`n" | Select-String '@version' |
        Select-Object -First 1).Line -replace '.*@version\s+', '').Trim()
    $compilerOptions = (($sourceText -split "`n" | Select-String '@compilerOptions' |
        Select-Object -First 1).Line -replace '.*@compilerOptions\s+', '').Trim() -split '\s+'
    Say "source version: $version"
    Say "link libraries: $($compilerOptions -join ' ')"

    # ------------------------------------------------------------- 1. stop
    Say ''
    Say '[1/5] stopping Windhawk'
    Stop-Service Windhawk -Force -ErrorAction SilentlyContinue
    Get-Process -Name windhawk, windhawk-x64-helper -ErrorAction SilentlyContinue |
        Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2

    # ---------------------------------------------------------- 2. compile
    Say '[2/5] compiling'
    $compiler = 'C:\Program Files\Windhawk\Compiler\bin\clang++.exe'
    $include = 'C:\Program Files\Windhawk\Compiler\include'
    $dllName = "${modId}_${version}_$((Get-Random -Minimum 100000 -Maximum 999999)).dll"
    $dllPath = Join-Path $modsDir $dllName

    # The widget is set in Inter, which Windows does not ship. The mod loads the
    # two weights from its own directory, so they are installed next to it. The
    # stale-file sweep below only removes "$modId*", so they are left alone.
    #
    # Explorer keeps the fonts mapped while the mod is loaded, so an unchanged
    # file is left as it is and a copy that does fail is only a warning: the
    # running widget keeps using the copy that is already there.
    foreach ($font in 'Inter-Regular.ttf', 'Inter-Medium.ttf') {
        $fontSource = Join-Path $here "fonts\$font"
        $fontTarget = Join-Path $modsDir $font
        if (-not (Test-Path $fontSource)) {
            Say "  WARNING: $fontSource missing, the widget will use a system font"
            continue
        }
        $unchanged = (Test-Path $fontTarget) -and
            ((Get-Item $fontTarget).Length -eq (Get-Item $fontSource).Length)
        if ($unchanged) {
            continue
        }
        try {
            Copy-Item $fontSource $fontTarget -Force -ErrorAction Stop
            Say "  installed $font"
        } catch {
            Say "  WARNING: cannot replace $font (in use by Explorer); the widget"
            Say "           keeps using the copy already installed"
        }
    }

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
    Say "  built $dllName ($((Get-Item $dllPath).Length) bytes)"

    # Remove other builds of this mod so nothing stale can be picked up.
    Get-ChildItem $modsRoot -Recurse -Filter "$modId*" -File -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -ne $dllPath } | ForEach-Object {
            Remove-Item $_.FullName -Force -ErrorAction SilentlyContinue
            Say "  removed stale $($_.Name)"
        }

    # ------------------------------------------------------- 3. registration
    Say '[3/5] writing the Windhawk registration'
    $regPath = "HKLM:\SOFTWARE\Windhawk\Engine\Mods\$modId"
    $settingsKey = Join-Path $regPath 'Settings'

    # The registration key is rebuilt from scratch below, and that used to take
    # the settings page with it: every reinstall silently reset the user's own
    # choices (the screen, the opacity). They are collected first and written back
    # afterwards, so a reinstall only adds what is new.
    $storedSettings = [ordered]@{}
    if (Test-Path $settingsKey) {
        $existingKey = Get-Item $settingsKey
        foreach ($name in $existingKey.GetValueNames()) {
            $storedSettings[$name] = @{
                Value = $existingKey.GetValue($name)
                Kind  = $existingKey.GetValueKind($name)
            }
        }
        Say "  keeping $($storedSettings.Count) stored settings"
    }

    # The stored settings are moved with the layout where the layout itself
    # changed: 2.8.0 made the widget 35 px narrower (the progress bar, the meter,
    # the time and the buttons were pulled towards the title and the artist), and
    # 2.8.2 took another 40 off it (the time moved on towards the visualizer and
    # the bar got 40 shorter with it). So a stored 466 or 431 is the width of an
    # earlier layout rather than a choice - an earlier install wrote it - and
    # leaving it would draw an empty strip inside the panel to the right of the
    # buttons, because the panel is drawn at the layout's own width. 2.8.8 went
    # the other way: the bar is 38 px longer again, so that the time has room for
    # "00:00:00 / 00:00:00", and the layout is 419 wide.
    $oldLayoutWidths = 466, 431, 391, 381
    $newLayoutWidth = 419
    if ($storedSettings.Contains('widgetWidth') -and
        $oldLayoutWidths -contains $storedSettings['widgetWidth'].Value) {
        $old = $storedSettings['widgetWidth'].Value
        $storedSettings['widgetWidth'].Value = $newLayoutWidth
        Say "  widgetWidth migrated $old -> $newLayoutWidth (the layout's width changed)"
    }

    # The default gap to the tray went from 8 to -2 in 2.8.6. A stored 8 is the
    # old default written back by an installer or by the settings page, not a
    # choice, so it follows the default.
    if ($storedSettings.Contains('rightMargin') -and
        $storedSettings['rightMargin'].Value -eq 8) {
        $storedSettings['rightMargin'].Value = -2
        Say '  rightMargin migrated 8 -> -2 (the default gap got smaller)'
    }

    if (Test-Path $regPath) { Remove-Item $regPath -Recurse -Force }
    New-Item -Path $regPath -Force | Out-Null

    New-ItemProperty $regPath -Name 'LibraryFileName' -Value $dllName -PropertyType String -Force | Out-Null
    New-ItemProperty $regPath -Name 'Disabled' -Value 0 -PropertyType DWord -Force | Out-Null
    New-ItemProperty $regPath -Name 'LoggingEnabled' -Value ([int]($Logging.IsPresent)) -PropertyType DWord -Force | Out-Null
    New-ItemProperty $regPath -Name 'DebugLoggingEnabled' -Value 0 -PropertyType DWord -Force | Out-Null
    New-ItemProperty $regPath -Name 'Include' -Value 'explorer.exe' -PropertyType String -Force | Out-Null
    New-ItemProperty $regPath -Name 'Exclude' -Value '' -PropertyType String -Force | Out-Null
    New-ItemProperty $regPath -Name 'IncludeCustom' -Value '' -PropertyType String -Force | Out-Null
    New-ItemProperty $regPath -Name 'ExcludeCustom' -Value '' -PropertyType String -Force | Out-Null
    New-ItemProperty $regPath -Name 'IncludeExcludeCustomOnly' -Value 0 -PropertyType DWord -Force | Out-Null
    New-ItemProperty $regPath -Name 'PatternsMatchCriticalSystemProcesses' -Value 0 -PropertyType DWord -Force | Out-Null
    New-ItemProperty $regPath -Name 'Architecture' -Value 'x86-64' -PropertyType String -Force | Out-Null
    New-ItemProperty $regPath -Name 'Version' -Value $version -PropertyType String -Force | Out-Null
    $now = [int][double]::Parse((Get-Date -UFormat %s))
    New-ItemProperty $regPath -Name 'SettingsChangeTime' -Value $now -PropertyType DWord -Force | Out-Null

    # The mod falls back to built-in defaults when nothing is saved, but write
    # the intended values anyway so the Windhawk settings page shows them.
    #
    # The user's own values first, then the intended defaults for whatever is
    # still missing: this machine is configured, and an installer that reset the
    # widget to another screen or another opacity would undo real choices.
    $settingsKey = Join-Path $regPath 'Settings'
    New-Item -Path $settingsKey -Force | Out-Null
    foreach ($name in $storedSettings.Keys) {
        New-ItemProperty $settingsKey -Name $name -Value $storedSettings[$name].Value `
            -PropertyType $storedSettings[$name].Kind -Force | Out-Null
    }
    $defaults = @{
        widgetWidth = 419; widgetHeight = 42
        rightMargin = -2
        fallbackRightOffset = 220; pollIntervalMs = 400; opacity = 255
        followTaskbarTheme = 1
        backgroundBlur = 1
        showProgressBar = 1; seekOnClick = 1; forceTopmostEveryTick = 1
        # The visualizer's own three, so the settings page can show them: it
        # edits the stored values, and a setting that was never stored is a
        # setting the page cannot offer.
        visualizer = 1; visualizerGain = 100; visualizerFps = 33
    }
    foreach ($n in $defaults.Keys) {
        if ($null -ne (Get-ItemProperty $settingsKey -Name $n -ErrorAction SilentlyContinue)) {
            continue
        }
        New-ItemProperty $settingsKey -Name $n -Value $defaults[$n] -PropertyType DWord -Force | Out-Null
        Say "  default $n = $($defaults[$n])"
    }

    # The screen is a dropdown in the settings page, and Windhawk stores such a
    # choice as a string. A DWORD there would leave the settings page unable to
    # show the value, so a number left by an older installer is rewritten as the
    # word for the same screen.
    $monitor = Get-ItemProperty $settingsKey -Name 'taskbarMonitor' -ErrorAction SilentlyContinue
    if ($null -eq $monitor) {
        New-ItemProperty $settingsKey -Name 'taskbarMonitor' -Value 'screen1' -PropertyType String -Force | Out-Null
        Say '  default taskbarMonitor = screen1'
    } elseif ($monitor.taskbarMonitor -isnot [string]) {
        Remove-ItemProperty -Path $settingsKey -Name 'taskbarMonitor' -Force
        New-ItemProperty $settingsKey -Name 'taskbarMonitor' -Value "screen$($monitor.taskbarMonitor)" -PropertyType String -Force | Out-Null
        Say "  taskbarMonitor rewritten as screen$($monitor.taskbarMonitor)"
    } else {
        Say "  keeping taskbarMonitor = $($monitor.taskbarMonitor)"
    }
    Say "  registered $modId $version -> $dllName"

    # Windhawk compiles a mod from the source kept in ModsSource and takes the
    # settings schema from it, so a stale copy there describes a mod that no
    # longer exists (it still said 1.3.0 while 2.3.0 was being installed). Keep
    # it in step with what was just compiled.
    $modsSourceDir = 'C:\ProgramData\Windhawk\ModsSource'
    $sourceTarget = Join-Path $modsSourceDir "$modId.wh.cpp"
    if (Test-Path $modsSourceDir) {
        if (Test-Path $sourceTarget) {
            $sourceVersion = ((Get-Content $sourceTarget -Raw) -split "`n" |
                Select-String '@version' | Select-Object -First 1).Line.Trim()
            Say "  replacing ModsSource copy ($sourceVersion)"
        }
        Copy-Item $source $sourceTarget -Force
        Say "  copied source to ModsSource, so the settings schema matches"
    } else {
        Say "  WARNING: $modsSourceDir not found, the settings schema stays stale"
    }

    # ------------------------------------------------------- 4. userprofile
    Say '[4/5] repairing userprofile.json'
    if (Test-Path $profilePath) {
        Copy-Item $profilePath "$profilePath.bak" -Force
        $profile = Get-Content $profilePath -Raw | ConvertFrom-Json
        if ($profile.mods.PSObject.Properties.Name -contains $modId) {
            $profile.mods.$modId.version = $version
        } else {
            $entry = [pscustomobject]@{ version = $version }
            $profile.mods | Add-Member -NotePropertyName $modId -NotePropertyValue $entry -Force
        }
        $profile | ConvertTo-Json -Depth 10 | Set-Content $profilePath -Encoding UTF8
        Say "  recorded version $version (backup: userprofile.json.bak)"
    }

    # ---------------------------------------------------------- 5. restart
    Say '[5/5] starting Windhawk'
    Start-Service Windhawk
    Start-Sleep -Seconds 5
    Say "  service: $((Get-Service Windhawk).Status)"

    # ------------------------------------------------------------- summary
    Say ''
    Say 'RESULT'
    Say "  library on disk : $(Test-Path $dllPath)"
    Say "  registered file : $((Get-ItemProperty $regPath).LibraryFileName)"
    Say "  disabled        : $((Get-ItemProperty $regPath).Disabled)"
    Say ''
    Say 'FORCE INSTALL DONE'
}
catch {
    Say ''
    Say "ERROR: $($_.Exception.Message)"
    try { Start-Service Windhawk -ErrorAction SilentlyContinue } catch {}
}
finally {
    Stop-Transcript | Out-Null
}
