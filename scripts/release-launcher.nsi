; SPDX-License-Identifier: Apache-2.0

Unicode true
Target amd64-unicode
RequestExecutionLevel user
SilentInstall silent
AutoCloseWindow true
SetCompressor /SOLID lzma

!include "FileFunc.nsh"

Name "Serial Workbench ${VERSION}"
OutFile "${OUTPUT_EXE}"
InstallDir "$LOCALAPPDATA\SerialWorkbench\runtime\${VERSION}-${PAYLOAD_ID}"
VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "Serial Workbench"
VIAddVersionKey "FileDescription" "Serial Workbench single-file launcher"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "LegalCopyright" "Apache-2.0"

Var Parameters
Var ExitCode

Section
    IfFileExists "$INSTDIR\.complete" 0 unpack
    IfFileExists "$INSTDIR\SerialWorkbench.exe" 0 unpack
    IfFileExists "$INSTDIR\SerialWorkbenchMCP.exe" 0 unpack
    IfFileExists "$INSTDIR\SerialWorkbenchSSHAskpass.exe" 0 unpack
    IfFileExists "$INSTDIR\platforms\qwindows.dll" launch unpack

unpack:
    SetOutPath "$INSTDIR"
    ClearErrors
    File /r "${PAYLOAD_DIR}\*"
    IfErrors failed

    FileOpen $0 "$INSTDIR\.complete" w
    IfErrors failed
    FileWrite $0 "${PAYLOAD_ID}$\r$\n"
    FileClose $0

launch:
    SetOutPath "$INSTDIR"
    ${GetParameters} $Parameters
    ClearErrors
    ExecWait '"$INSTDIR\SerialWorkbench.exe" --settings "$LOCALAPPDATA\SerialWorkbench\settings.json" $Parameters' $ExitCode
    IfErrors failed
    SetErrorLevel $ExitCode
    Quit

failed:
    MessageBox MB_OK|MB_ICONSTOP "Serial Workbench could not extract or start. Check the available disk space and directory permissions."
    SetErrorLevel 1
    Quit
SectionEnd
