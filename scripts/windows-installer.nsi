; SPDX-License-Identifier: Apache-2.0
Unicode true
Target amd64-unicode
RequestExecutionLevel user
SetCompressor /SOLID lzma

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!include "nsDialogs.nsh"

!ifndef PRODUCT_NAME
!define PRODUCT_NAME "Serial Workbench"
!endif
!ifndef REGISTRY_KEY
!define REGISTRY_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\SerialWorkbench"
!endif

Name "${PRODUCT_NAME} ${VERSION}"
OutFile "${OUTPUT_EXE}"
InstallDir "$LOCALAPPDATA\Programs\SerialWorkbench"
InstallDirRegKey HKCU "${REGISTRY_KEY}" "InstallLocation"
BrandingText "${PRODUCT_NAME}"
ShowInstDetails show
ShowUninstDetails show
SetFont "Microsoft YaHei UI" 9
VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "${PRODUCT_NAME}"
VIAddVersionKey "FileDescription" "${PRODUCT_NAME} Setup"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "LegalCopyright" "Apache-2.0"

!define MUI_ICON "${BRAND_DIR}\lightning.ico"
!define MUI_UNICON "${BRAND_DIR}\lightning.ico"
!define MUI_ABORTWARNING
!define MUI_WELCOMEFINISHPAGE_BITMAP "${BRAND_DIR}\installer-sidebar.bmp"
!define MUI_WELCOMEPAGE_TITLE "安装 ${PRODUCT_NAME}"
!define MUI_WELCOMEPAGE_TEXT "安装串口、网络和 SSH 调试工作台，并提供共享 MCP 接入。$\r$\n$\r$\n您可以选择安装目录和桌面快捷方式。软件已包含运行库，无需安装 Python 或 Qt。$\r$\n$\r$\n继续之前，请关闭正在运行的工作台。"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "${BRAND_DIR}\installer-header.bmp"
!define MUI_HEADERIMAGE_RIGHT
!define MUI_DIRECTORYPAGE_TEXT_TOP "选择安装位置。请选择可写目录，例如用户目录或其他磁盘上的专用文件夹。"
!define MUI_FINISHPAGE_RUN "$INSTDIR\SerialWorkbench.exe"
!define MUI_FINISHPAGE_RUN_TEXT "启动 ${PRODUCT_NAME}"
!define MUI_FINISHPAGE_RUN_NOTCHECKED
!define MUI_UNCONFIRMPAGE_TEXT_TOP "卸载程序将移除 ${PRODUCT_NAME} 及其快捷方式。个人设置默认保留；您可在下一页选择删除。其他文件不会被删除。"
!define MUI_UNFINISHPAGE_TEXT "${PRODUCT_NAME} 已卸载。$\r$\n$\r$\n默认保留个人设置，便于以后重新安装。"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${PAYLOAD_DIR}\LICENSE"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
UninstPage custom un.SettingsPage un.SettingsLeave
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH
!insertmacro MUI_LANGUAGE "SimpChinese"

Var RemoveSettings
Var RemoveSettingsCheck

; A running EXE cannot be opened exclusively for write. Check all programs
; before installing/removing any file rather than silently killing processes.
!macro VerifyClosed PREFIX
Function ${PREFIX}VerifyClosed
    ${If} ${FileExists} "$INSTDIR\SerialWorkbench.exe"
        System::Call 'kernel32::CreateFileW(w "$INSTDIR\SerialWorkbench.exe", i 0xC0000000, i 0, p 0, i 3, i 0, p 0) p.r0'
        ${If} $0 == -1
            MessageBox MB_OK|MB_ICONSTOP "工作台正在运行或文件不可写。请关闭工作台并检查安装目录权限，然后重试。" /SD IDOK
            SetErrorLevel 2
            Quit
        ${EndIf}
        System::Call 'kernel32::CloseHandle(p r0)'
    ${EndIf}
    ${If} ${FileExists} "$INSTDIR\SerialWorkbenchMCP.exe"
        System::Call 'kernel32::CreateFileW(w "$INSTDIR\SerialWorkbenchMCP.exe", i 0xC0000000, i 0, p 0, i 3, i 0, p 0) p.r0'
        ${If} $0 == -1
            MessageBox MB_OK|MB_ICONSTOP "MCP 桥接程序正在运行或文件不可写。请先在 Agent 中断开 MCP，再重试。" /SD IDOK
            SetErrorLevel 2
            Quit
        ${EndIf}
        System::Call 'kernel32::CloseHandle(p r0)'
    ${EndIf}
    ${If} ${FileExists} "$INSTDIR\SerialWorkbenchSSHAskpass.exe"
        System::Call 'kernel32::CreateFileW(w "$INSTDIR\SerialWorkbenchSSHAskpass.exe", i 0xC0000000, i 0, p 0, i 3, i 0, p 0) p.r0'
        ${If} $0 == -1
            MessageBox MB_OK|MB_ICONSTOP "SSH 正在等待认证或文件不可写。请关闭 SSH 认证窗口和工作台，再重试。" /SD IDOK
            SetErrorLevel 2
            Quit
        ${EndIf}
        System::Call 'kernel32::CloseHandle(p r0)'
    ${EndIf}
FunctionEnd
!macroend
!insertmacro VerifyClosed ""
!insertmacro VerifyClosed "un."

Function .onInit
    SetShellVarContext current
    SetRegView 64
FunctionEnd

Section "工作台与 MCP（必需）" Application
    SectionIn RO
    ReadRegStr $1 HKCU "${REGISTRY_KEY}" "InstallLocation"
    ${If} $1 != ""
    ${AndIf} $1 != $INSTDIR
        MessageBox MB_OK|MB_ICONSTOP "工作台已安装在：$\r$\n$1$\r$\n$\r$\n请在原目录升级，或先卸载后再选择新目录。个人设置可保留。" /SD IDOK
        SetErrorLevel 3
        Quit
    ${EndIf}
    ${If} ${FileExists} "$INSTDIR\SerialWorkbench.exe"
    ${AndIfNot} ${FileExists} "$INSTDIR\installation.json"
        MessageBox MB_OK|MB_ICONSTOP "该目录已包含其他分发版本，请选择新的安装目录，避免覆盖现有文件。" /SD IDOK
        SetErrorLevel 3
        Quit
    ${EndIf}
    Call VerifyClosed
    SetOutPath "$INSTDIR"
    GetTempFileName $0 "$INSTDIR"
    ${If} $0 == ""
        MessageBox MB_OK|MB_ICONSTOP "无法写入安装目录，请选择可写位置。" /SD IDOK
        SetErrorLevel 4
        Quit
    ${EndIf}
    Delete "$0"
    SetOverwrite on
    ClearErrors
    File /r "${PAYLOAD_DIR}\*"
    ${If} ${Errors}
        MessageBox MB_OK|MB_ICONSTOP "文件安装未完成，请检查目录权限与剩余空间后重试。" /SD IDOK
        SetErrorLevel 4
        Quit
    ${EndIf}
    ClearErrors
    FileOpen $0 "$INSTDIR\installation.json" w
    FileWrite $0 '{"distribution":"installed","version":"${VERSION}"}$\r$\n'
    FileClose $0
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    ${If} ${Errors}
        MessageBox MB_OK|MB_ICONSTOP "无法创建安装标记或卸载程序，请检查目录权限后重试。" /SD IDOK
        SetErrorLevel 4
        Quit
    ${EndIf}
    CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
    CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk" "$INSTDIR\SerialWorkbench.exe" "" "$INSTDIR\SerialWorkbench.exe" 0
    CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\卸载.lnk" "$INSTDIR\Uninstall.exe" "" "$INSTDIR\Uninstall.exe" 0
    ClearErrors
    WriteRegStr HKCU "${REGISTRY_KEY}" "DisplayName" "${PRODUCT_NAME}"
    WriteRegStr HKCU "${REGISTRY_KEY}" "DisplayVersion" "${VERSION}"
    WriteRegStr HKCU "${REGISTRY_KEY}" "Publisher" "lilinng"
    WriteRegStr HKCU "${REGISTRY_KEY}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKCU "${REGISTRY_KEY}" "DisplayIcon" "$INSTDIR\SerialWorkbench.exe,0"
    WriteRegStr HKCU "${REGISTRY_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegStr HKCU "${REGISTRY_KEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
    WriteRegStr HKCU "${REGISTRY_KEY}" "URLInfoAbout" "https://github.com/lilinng-163/serial-workbench"
    WriteRegDWORD HKCU "${REGISTRY_KEY}" "EstimatedSize" ${SIZE_KB}
    WriteRegDWORD HKCU "${REGISTRY_KEY}" "NoModify" 1
    WriteRegDWORD HKCU "${REGISTRY_KEY}" "NoRepair" 1
    ${If} ${Errors}
        MessageBox MB_OK|MB_ICONSTOP "无法注册 Windows 卸载入口。请检查当前账户的注册表权限；可运行安装目录中的 Uninstall.exe 清理。" /SD IDOK
        SetErrorLevel 5
        Quit
    ${EndIf}
SectionEnd

!macro InstallAgentSkill AGENT
    InitPluginsDir
    ClearErrors
    ExecWait '"$INSTDIR\SerialWorkbench.exe" --install-skill ${AGENT} --skill-report "$PLUGINSDIR\skill-${AGENT}.json"' $0
    ${If} ${Errors}
        StrCpy $0 4
    ${EndIf}
    ${If} $0 != 0
        DetailPrint "${AGENT} skill 安装失败；主程序安装已完成。"
        MessageBox MB_OK|MB_ICONEXCLAMATION "${AGENT} skill 未安装完成。请检查用户目录写入权限，稍后可重新运行安装器选择此组件。已有 skill 文件会先备份。" /SD IDOK
    ${EndIf}
!macroend

!ifdef WORKBENCH_TEST_SKILL_CODEX
Section "Codex 使用指南 skill（当前用户 .codex/skills）" SkillCodex
!else
Section /o "Codex 使用指南 skill（当前用户 .codex/skills）" SkillCodex
!endif
    !insertmacro InstallAgentSkill codex
SectionEnd
Section /o "Claude Code 使用指南 skill（当前用户 .claude/skills）" SkillClaude
    !insertmacro InstallAgentSkill claude-code
SectionEnd
Section /o "OpenCode 使用指南 skill（当前用户 .config/opencode/skills）" SkillOpenCode
    !insertmacro InstallAgentSkill opencode
SectionEnd
Section /o "Pi Agent 使用指南 skill（当前用户 .pi/agent/skills）" SkillPi
    !insertmacro InstallAgentSkill pi
SectionEnd

Section "创建桌面快捷方式" Desktop
    CreateShortCut "$DESKTOP\${PRODUCT_NAME}.lnk" "$INSTDIR\SerialWorkbench.exe" "" "$INSTDIR\SerialWorkbench.exe" 0
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
!insertmacro MUI_DESCRIPTION_TEXT ${Application} "安装软件、运行库、共享 MCP 桥、开始菜单与卸载程序。"
!insertmacro MUI_DESCRIPTION_TEXT ${Desktop} "在当前用户桌面创建浅绿色闪电图标的快捷方式。"
!insertmacro MUI_DESCRIPTION_TEXT ${SkillCodex} "可选。安装软件/MCP/脚本使用指南；遵循 CODEX_HOME，变更文件先备份，不修改 MCP 地址或其他配置。卸载软件时保留 skill。"
!insertmacro MUI_DESCRIPTION_TEXT ${SkillClaude} "可选。遵循 CLAUDE_CONFIG_DIR，保留已有 skill 备份；安装后重载 Agent。"
!insertmacro MUI_DESCRIPTION_TEXT ${SkillOpenCode} "可选。遵循 OPENCODE_CONFIG_DIR 或 XDG_CONFIG_HOME，安装后重载 Agent。"
!insertmacro MUI_DESCRIPTION_TEXT ${SkillPi} "可选。遵循 PI_CODING_AGENT_DIR，安装后可使用 /reload。"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Function un.onInit
    SetShellVarContext current
    SetRegView 64
    StrCpy $RemoveSettings 0
    ${IfNot} ${FileExists} "$INSTDIR\installation.json"
        MessageBox MB_OK|MB_ICONSTOP "未找到安装标记，停止卸载。" /SD IDOK
        SetErrorLevel 3
        Quit
    ${EndIf}
FunctionEnd

Function un.SettingsPage
    !insertmacro MUI_HEADER_TEXT "个人设置" "选择是否保留工作台设置。"
    nsDialogs::Create 1018
    Pop $0
    ${NSD_CreateLabel} 0 0 100% 36u "默认保留连接参数、页面布局、外观设置和加密保存的 SSH 密码，以便重新安装后继续使用。"
    Pop $0
    ${NSD_CreateCheckbox} 0 48u 100% 18u "同时删除当前用户的个人设置和保存的 SSH 密码"
    Pop $RemoveSettingsCheck
    nsDialogs::Show
FunctionEnd

Function un.SettingsLeave
    ${NSD_GetState} $RemoveSettingsCheck $RemoveSettings
FunctionEnd

Section "Uninstall"
    Call un.VerifyClosed
    ; Generated exact file list; remove empty directories only. Do not use
    ; recursive deletion on an installation path chosen by the user.
    !include "${REMOVE_MANIFEST}"
    Delete "$INSTDIR\installation.json"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir "$INSTDIR"
    Delete "$DESKTOP\${PRODUCT_NAME}.lnk"
    Delete "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk"
    Delete "$SMPROGRAMS\${PRODUCT_NAME}\卸载.lnk"
    RMDir "$SMPROGRAMS\${PRODUCT_NAME}"
    DeleteRegKey HKCU "${REGISTRY_KEY}"
    ${If} $RemoveSettings == ${BST_CHECKED}
        Delete "$LOCALAPPDATA\SerialWorkbench\settings.json"
        Delete "$LOCALAPPDATA\SerialWorkbench\ssh\*.json"
        Delete "$LOCALAPPDATA\SerialWorkbench\ssh\*.lock"
        RMDir "$LOCALAPPDATA\SerialWorkbench\ssh"
        RMDir "$LOCALAPPDATA\SerialWorkbench"
    ${EndIf}
SectionEnd
