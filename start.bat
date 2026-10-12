@echo off
cd /d "%~dp0"
if not exist ".venv\Scripts\python.exe" (
    echo Run setup.bat first.
    pause
    exit /b 1
)
start "Serial Workbench" ".venv\Scripts\pythonw.exe" "run.py" %*
