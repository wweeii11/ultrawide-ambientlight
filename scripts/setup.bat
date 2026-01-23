@echo off

setlocal
set "PS_SCRIPT=%~dp0sign_and_install.ps1"

:: 1. Check for administrative permissions
net session >nul 2>&1
if %errorLevel% == 0 (
    goto :RunScript
) else (
    goto :RequestElevation
)

:RequestElevation
    echo Requesting administrative privileges...
    :: This command re-runs the current batch file as Admin. 
    :: It passes the 'ELEVATED' argument to skip the loop.
    powershell -Command "Start-Process -FilePath '%~f0' -ArgumentList 'ELEVATED' -Verb RunAs"
    exit /b

:RunScript
    @echo ambientlight can run without setup.
    @echo Install is optional to workaround some video players that do not normally allow overlay.
    @echo:
    @echo This script will
    @echo - Create and sign a self-signed certificate to ambientlight_a.exe 
    @echo - Copy to "Program Files\ultrawide-ambientlight"
    @echo - Create shortcut in Start Menu
    @echo:
    @echo Are you sure you want to continue? (Y/N)
    set /p choice=
    if /I "%choice%" NEQ "Y" goto :eof

    echo Running with Administrator privileges...
    :: Set working directory to script location
    cd /d "%~dp0"

    :: 2. Call PowerShell with the Bypass flag
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%PS_SCRIPT%"
    
    echo.
    echo Installation Process Finished.
    pause

:eof
