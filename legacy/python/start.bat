REM SPDX-License-Identifier: Apache-2.0

@echo off
cd /d "%~dp0"
if not exist ".venv\Scripts\pythonw.exe" (
    echo Run setup.bat in this folder first.
    pause
    exit /b 1
)
start "Serial Workbench Python" ".venv\Scripts\pythonw.exe" "run.py" %*
