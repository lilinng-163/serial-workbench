@echo off
setlocal
cd /d "%~dp0"
rem Build with Windows runtime DLLs, independent of third-party tools on PATH.
set "PATH=%SystemRoot%\System32;%SystemRoot%;%CD%\.venv\Scripts"
".venv\Scripts\python.exe" -m pip install -r requirements-dev.txt
if errorlevel 1 exit /b 1
".venv\Scripts\python.exe" -m PyInstaller --clean --noconfirm SerialWorkbench.spec
if errorlevel 1 exit /b 1
".venv\Scripts\python.exe" -m PyInstaller --clean --noconfirm --onefile --console --name SerialWorkbenchMCP --distpath dist\SerialWorkbench --copy-metadata mcp mcp_bridge.py
if errorlevel 1 exit /b 1
echo Output: dist\SerialWorkbench\SerialWorkbench.exe

