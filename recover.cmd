@echo off
REM media-player / recover.cmd
REM
REM Un-freezes the taskbar WITHOUT needing a working taskbar, a working mouse,
REM or a working keyboard shortcut to the shell.
REM
REM How to run it when the desktop is frozen:
REM   1. Press Ctrl+Alt+Del  (this screen is drawn by Windows itself and works
REM      even when explorer.exe is hung).
REM   2. Choose Task Manager.
REM   3. In Task Manager: File -> Run new task  (Alt+F, then Down, then Enter).
REM   4. Tick "Create this task with administrative privileges".
REM   5. Paste this line into the Open box and press Enter:
REM
REM      "D:\projects\media-player\recover.cmd"
REM
REM What it does:
REM   * stops the Windhawk service, so it cannot re-inject mods;
REM   * kills the Windhawk processes and the hung explorer.exe;
REM   * restarts explorer.exe only, without any Windhawk mods loaded.
REM
REM The Windhawk service is left stopped on purpose. After your desktop is back,
REM run the commands printed at the end to remove the faulty mod and restore
REM Windhawk for your other mods.

setlocal

echo ============================================================
echo  Recovering: stopping Windhawk, restarting explorer.exe
echo ============================================================
echo.

net session >nul 2>&1
if errorlevel 1 (
    echo [!] Not running as Administrator.
    echo     In Task Manager, tick "Create this task with administrative privileges"
    echo     before pressing Enter.
    echo.
    pause
    exit /b 1
)

echo [1/4] Stopping the Windhawk service...
sc stop Windhawk >nul 2>&1
timeout /t 2 /nobreak >nul

echo [2/4] Killing Windhawk processes...
taskkill /F /IM windhawk.exe >nul 2>&1
taskkill /F /IM windhawk-x64-helper.exe >nul 2>&1
taskkill /F /IM VSCodium.exe >nul 2>&1
timeout /t 2 /nobreak >nul

echo [3/4] Restarting explorer.exe...
taskkill /F /IM explorer.exe >nul 2>&1
timeout /t 3 /nobreak >nul

echo [4/4] Starting the shell...
start "" explorer.exe
timeout /t 4 /nobreak >nul

echo.
echo Your taskbar and desktop should be back, with no Windhawk mods loaded.
echo.
echo ============================================================
echo  Make the fix permanent (run these as Administrator)
echo ============================================================
echo.
echo Open PowerShell as Administrator and run:
echo.
echo   Remove-Item 'HKLM:\SOFTWARE\Windhawk\Engine\Mods\taskbar-media-widget' -Recurse -Force
echo   sc.exe config Windhawk start= auto
echo   Start-Service Windhawk
echo.
echo The first line deletes the faulty mod from Windhawk's autostart list.
echo The other two bring Windhawk back for your remaining mods
echo (Taskbar multi-tray, Windows 11 Taskbar Styler).
echo.
pause
