REM SPDX-License-Identifier: Apache-2.0

@echo off
cd /d "%~dp0"
if not exist ".venv\Scripts\python.exe" python -m venv .venv
if errorlevel 1 goto :failure
".venv\Scripts\python.exe" -m pip install -r requirements.txt
if errorlevel 1 goto :failure
echo Ready. Run start.bat to launch Serial Workbench.
pause
exit /b 0
:failure
echo Setup failed. Check the error above.
pause
exit /b 1
