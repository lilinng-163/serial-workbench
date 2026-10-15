@echo off
setlocal
chcp 65001 >nul
cd /d "%~dp0.."
if not exist ".venv\Scripts\python.exe" (
    echo Python development environment is missing: .venv\Scripts\python.exe
    pause
    exit /b 1
)
echo Starting internal SSH demo. Please wait...
".venv\Scripts\python.exe" "scripts\run-ssh-demo.py" --detached
if errorlevel 1 (
    echo.
    echo Startup failed. Log: build\ssh-new-device-demo.log
    pause
    exit /b 1
)
endlocal
