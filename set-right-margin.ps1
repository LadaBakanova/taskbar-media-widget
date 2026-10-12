# media-player / set-right-margin.ps1
#
# Moves the taskbar widget along the taskbar by changing "rightMargin" - the
# stored setting that places the widget's right edge relative to the system tray
# - and makes the running mod pick the new value up.
#
#   powershell -ExecutionPolicy Bypass -File .\set-right-margin.ps1 -Value -12
#
# How the number works (see PositionWidget in the mod source):
#
#   widget.x = trayWindow.left - rightMargin - widgetWidth
#
# so every step of 1 in rightMargin moves the widget 1 px to the LEFT. That is
# why a 20 px shift to the right is rightMargin = 8 - 20 = -12. A negative value
# is allowed and means the widget's right edge sits inside the tray window by
# |rightMargin| px; the theme insets the tray pill itself (FrostyGlass asks for
# 12 px), so -12 leaves the widget flush with the pill and covers nothing.
#
# The setting lives in HKLM, so the script elevates itself. The mod reads its
# settings when it loads: the value is written first and the Windhawk service is
# restarted only if the running mod does not notice the change on its own.
#
# The elevated copy is waited for. A helper that is merely started and left
# behind is killed together with the shell that started it, and then nothing
# happens at all - the symptom is a script that "ran fine" and changed nothing.
#
# ASCII only on purpose: Windows PowerShell 5.1 reads .ps1 files with Cyrillic as
# ANSI and mangles them (see CONTEXT.md).

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [int]$Value,

    # Print what would happen and stop before touching anything.
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'

$modId = 'taskbar-media-widget'
$regPath = "HKLM:\SOFTWARE\Windhawk\Engine\Mods\$modId"
$settingsKey = Join-Path $regPath 'Settings'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$log = Join-Path $here 'set-right-margin-log.txt'

# Written before anything else, so even a helper that is killed early leaves a
# trace behind.
Set-Content -Path $log -Value "set-right-margin: $(Get-Date -Format o)" -Encoding UTF8

function Say($message) {
    Write-Host $message
    Add-Content -Path $log -Value $message -Encoding UTF8
}

# The widget window of the mod, or $null when it is not there yet.
function Get-WidgetRect {
    $hwnd = [Native]::FindWindow('WindhawkTaskbarMediaWidget', $null)
    if ($hwnd -eq [IntPtr]::Zero) { return $null }
    $rect = New-Object Native+RECT
    [void][Native]::GetWindowRect($hwnd, [ref]$rect)
    return $rect
}

# Waits until the widget's left edge equals $expectedX, so the script can tell
# "the mod picked the value up" from "nothing happened yet".
function Wait-WidgetX([int]$expectedX, [int]$seconds) {
    for ($i = 0; $i -lt ($seconds * 2); $i++) {
        $rect = Get-WidgetRect
        if ($null -ne $rect -and $rect.left -eq $expectedX) { return $rect }
        Start-Sleep -Milliseconds 500
    }
    return (Get-WidgetRect)
}

try {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public class Native {
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindow(string className, string windowName);
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int left, top, right, bottom; }
}
'@

    $before = (Get-ItemProperty $settingsKey -Name 'rightMargin').rightMargin
    $beforeRect = Get-WidgetRect
    $beforeX = if ($null -ne $beforeRect) { $beforeRect.left } else { $null }
    $delta = $before - $Value
    $expectedX = if ($null -ne $beforeX) { $beforeX + $delta } else { $null }
    Say "rightMargin : $before -> $Value"
    Say "widget.x    : before $beforeX, expected $expectedX (delta $delta px)"

    if ($DryRun) {
        Say 'dry run: nothing was written'
        exit 0
    }

    # ---------------------------------------------------------------- elevate --
    $isAdmin = (New-Object Security.Principal.WindowsPrincipal(
        [Security.Principal.WindowsIdentity]::GetCurrent())).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $isAdmin) {
        Say 'the setting lives in HKLM, asking for elevation'
        Write-Host 'Waiting for the UAC prompt...'
        $arguments = "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Value $Value"
        $helper = Start-Process powershell -Verb RunAs -ArgumentList $arguments -PassThru
        # Without this wait the elevated copy is killed the moment this script
        # exits, and the setting is never written.
        $helper.WaitForExit()
        Say "elevated helper exited with code $($helper.ExitCode)"
        exit 0
    }

    # ------------------------------------------------------------------ write --
    Set-ItemProperty -Path $settingsKey -Name 'rightMargin' -Value $Value -Type DWord
    $stamp = [int][double]::Parse((Get-Date -UFormat %s))
    Set-ItemProperty -Path $regPath -Name 'SettingsChangeTime' -Value $stamp -Type DWord
    Say "stored rightMargin = $((Get-ItemProperty $settingsKey -Name 'rightMargin').rightMargin)"

    $rect = Wait-WidgetX -expectedX $expectedX -seconds 4
    if ($null -ne $rect -and $rect.left -eq $expectedX) {
        Say "the mod picked the setting up on its own: widget.x = $($rect.left)"
    } else {
        $seen = if ($null -ne $rect) { $rect.left } else { 'no window' }
        Say "no live reload (widget.x = $seen), restarting the Windhawk service"
        Stop-Service Windhawk -Force
        Start-Sleep -Seconds 2
        Start-Service Windhawk
        # The mod is injected again, waits kStartupDelayMs (1.5 s) for its window
        # and needs a moment for the taskbar and the tray to show up.
        $rect = Wait-WidgetX -expectedX $expectedX -seconds 20
        $seen = if ($null -ne $rect) { $rect.left } else { 'no window' }
        Say "after the restart: widget.x = $seen"
    }

    if ($null -ne $rect -and $rect.left -eq $expectedX) {
        Say "RESULT: widget at $($rect.left),$($rect.top)-$($rect.right),$($rect.bottom)"
        Say 'SET RIGHT MARGIN DONE'
    } else {
        $seen = if ($null -ne $rect) { $rect.left } else { 'no window' }
        Say "RESULT: expected widget.x = $expectedX, got $seen"
        Say 'SET RIGHT MARGIN NOT CONFIRMED'
    }
}
catch {
    Say "ERROR: $($_.Exception.Message)"
    exit 1
}
