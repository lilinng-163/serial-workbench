@echo off
cd /d "%~dp0"
if not exist "dist\SerialWorkbenchNative\SerialWorkbench.exe" (
    echo Build the native application with build-native.ps1 first.
    pause
    exit /b 1
)
start "Serial Workbench" "dist\SerialWorkbenchNative\SerialWorkbench.exe" %*
