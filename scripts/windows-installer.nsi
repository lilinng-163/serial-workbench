; SPDX-License-Identifier: Apache-2.0
Unicode true
Target amd64-unicode
RequestExecutionLevel user
SetCompressor /SOLID lzma

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!include "nsDialogs.nsh"
!include "Sections.nsh"

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
!define MUI_WELCOMEPAGE_TEXT "安装串口、网络和 SSH 调试工作台，并提供共享 MCP 接入。$\r$\n$\r$\n您可以选择安装目录和桌面快捷方式。软件已包含运行库，无需安装 Python 或 Qt。$\r$\n$\r$\n升级时会检测可见窗口、后台工作台和 MCP 占用，并提供保存设置后关闭并继续的选项。"
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
!define MUI_COMPONENTSPAGE_TEXT_TOP "已默认勾选检测到的 Agent 使用指南，可取消或手动增选。以后也可在设置 → AI / MCP → Agent 接入 → 配置 skill 中更新。"
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

Var MaintenanceLock

; Keep a delete-on-close lease until setup completes. New MCP clients cannot
; relaunch the runtime while files are being replaced; cancellation/crashes
; release the lease automatically. The helper runs outside the install tree.
!macro Maintenance PREFIX
Function ${PREFIX}ReleaseMaintenance
    ${If} $MaintenanceLock != ""
        System::Call 'kernel32::CloseHandle(p $MaintenanceLock)'
        StrCpy $MaintenanceLock ""
    ${EndIf}
FunctionEnd

Function ${PREFIX}VerifyClosed
    CreateDirectory "$INSTDIR"
    System::Call 'kernel32::CreateFileW(w "$INSTDIR\.workbench-maintenance", i 0xC0010000, i 0, p 0, i 4, i 0x04000000, p 0) p.r0'
    ${If} $0 == -1
        MessageBox MB_OK|MB_ICONSTOP "无法准备安装目录。请检查该目录的写入权限，并确认没有另一个安装程序正在运行。" /SD IDOK
        SetErrorLevel 4
        Quit
    ${EndIf}
    StrCpy $MaintenanceLock $0
    InitPluginsDir
    SetOutPath "$PLUGINSDIR"
    File /oname=SerialWorkbenchMaintenance.exe "${PAYLOAD_DIR}\SerialWorkbenchMaintenance.exe"

    check:
    ClearErrors
    ExecWait '"$PLUGINSDIR\SerialWorkbenchMaintenance.exe" --check "$INSTDIR"' $0
    ${If} ${Errors}
        StrCpy $0 4
    ${EndIf}
    ${If} $0 == 0
        Return
    ${EndIf}
    ${If} $0 == 2
        ClearErrors
        ${GetParameters} $1
        ${GetOptions} $1 "/CLOSEAPPLICATIONS" $2
        ${IfNot} ${Errors}
            Goto close
        ${EndIf}
        MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION|MB_DEFBUTTON2 "此目录的工作台或 MCP 仍在运行（可能在后台）。$\r$\n$\r$\n点“确定”将保存工作台设置、关闭连接，并结束此目录的 MCP 桥接后继续。安装完成后，请在 Agent 中重新连接 MCP。$\r$\n$\r$\n其他目录的工作台和 Agent 本身不会关闭。" /SD IDCANCEL IDOK close
        Goto cancelled
    ${EndIf}
    Goto retry

    close:
    ClearErrors
    ExecWait '"$PLUGINSDIR\SerialWorkbenchMaintenance.exe" --close "$INSTDIR"' $0
    ${If} ${Errors}
        StrCpy $0 4
    ${EndIf}
    ${If} $0 == 0
        Return
    ${EndIf}

    retry:
    MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "文件暂时无法更新，尚未替换安装文件。$\r$\n$\r$\n请关闭无响应的工作台/命令行、断开 Agent MCP，并检查安装目录写权限或其他程序的文件占用，再点“重试”。工作台不会被强制结束。$\r$\n$\r$\n检测代码：$0" /SD IDCANCEL IDRETRY check
    cancelled:
    Call ${PREFIX}ReleaseMaintenance
    SetErrorLevel 2
    Quit
FunctionEnd
!macroend
!insertmacro Maintenance ""
!insertmacro Maintenance "un."

Function .onInstSuccess
    Call ReleaseMaintenance
FunctionEnd
Function .onInstFailed
    Call ReleaseMaintenance
FunctionEnd
Function .onGUIEnd
    Call ReleaseMaintenance
FunctionEnd
Function un.onUninstFailed
    Call un.ReleaseMaintenance
FunctionEnd
Function un.onGUIEnd
    Call un.ReleaseMaintenance
FunctionEnd

Function .onInit
    SetShellVarContext current
    SetRegView 64
    Call DetectSkillAgents
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
        MessageBox MB_OK|MB_ICONEXCLAMATION "${AGENT} skill 未安装完成。请检查用户目录写入权限，稍后可在设置 → AI / MCP → 配置 skill 中重试。已有 skill 文件会先备份。" /SD IDOK
    ${EndIf}
!macroend

Section /o "Codex 使用指南 skill（当前用户 .codex/skills）" SkillCodex
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

Section /o "Hermes 使用指南 skill（当前用户）" SkillHermes
    !insertmacro InstallAgentSkill hermes
SectionEnd
Section /o "OpenClaw 使用指南 skill（当前用户）" SkillOpenClaw
    !insertmacro InstallAgentSkill openclaw
SectionEnd
Section /o "Grok Build 使用指南 skill（当前用户）" SkillGrok
    !insertmacro InstallAgentSkill grok
SectionEnd
Section /o "Gemini CLI 使用指南 skill（当前用户）" SkillGemini
    !insertmacro InstallAgentSkill gemini
SectionEnd

Section /o "Cursor 使用指南 skill（当前用户）" SkillCursor
    !insertmacro InstallAgentSkill cursor
SectionEnd
Section /o "Copilot 使用指南 skill（当前用户）" SkillCopilot
    !insertmacro InstallAgentSkill copilot
SectionEnd
Section /o "Windsurf 使用指南 skill（当前用户）" SkillWindsurf
    !insertmacro InstallAgentSkill windsurf
SectionEnd
Section /o "Devin 使用指南 skill（当前用户）" SkillDevin
    !insertmacro InstallAgentSkill devin
SectionEnd
Section /o "Cline 使用指南 skill（当前用户）" SkillCline
    !insertmacro InstallAgentSkill cline
SectionEnd
Section /o "Kilo 使用指南 skill（当前用户）" SkillKilo
    !insertmacro InstallAgentSkill kilo
SectionEnd
Section /o "Kiro 使用指南 skill（当前用户）" SkillKiro
    !insertmacro InstallAgentSkill kiro
SectionEnd

Section /o "Antigravity 使用指南 skill（当前用户）" SkillAntigravity
    !insertmacro InstallAgentSkill antigravity
SectionEnd
Section /o "Kimi 使用指南 skill（当前用户）" SkillKimi
    !insertmacro InstallAgentSkill kimi
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
!insertmacro MUI_DESCRIPTION_TEXT ${SkillHermes} "遵循 HERMES_HOME，安装软件/MCP/脚本使用指南，已有内容先备份。"
!insertmacro MUI_DESCRIPTION_TEXT ${SkillOpenClaw} "遵循 OPENCLAW_STATE_DIR，安装到 managed skills，已有内容先备份。"
!insertmacro MUI_DESCRIPTION_TEXT ${SkillGrok} "安装到当前用户 .grok/skills，已有内容先备份。"
!insertmacro MUI_DESCRIPTION_TEXT ${SkillGemini} "安装到当前用户 .gemini/skills，已有内容先备份。"
!insertmacro MUI_DESCRIPTION_TEXT ${SkillCursor} "可选。安装软件、MCP、脚本和 3D 使用指南到 Cursor 的用户 skill 目录；原内容先备份，安装后重载 Agent。"
!insertmacro MUI_DESCRIPTION_TEXT ${SkillCopilot} "可选。安装软件、MCP、脚本和 3D 使用指南到 Copilot 的用户 skill 目录；原内容先备份，安装后重载 Agent。"
!insertmacro MUI_DESCRIPTION_TEXT ${SkillWindsurf} "可选。安装软件、MCP、脚本和 3D 使用指南到 Windsurf 的用户 skill 目录；原内容先备份，安装后重载 Agent。"
!insertmacro MUI_DESCRIPTION_TEXT ${SkillDevin} "可选。安装软件、MCP、脚本和 3D 使用指南到 Devin 的用户 skill 目录；原内容先备份，安装后重载 Agent。"
!insertmacro MUI_DESCRIPTION_TEXT ${SkillCline} "可选。安装软件、MCP、脚本和 3D 使用指南到 Cline 的用户 skill 目录；原内容先备份，安装后重载 Agent。"
!insertmacro MUI_DESCRIPTION_TEXT ${SkillKilo} "可选。安装软件、MCP、脚本和 3D 使用指南到 Kilo 的用户 skill 目录；原内容先备份，安装后重载 Agent。"
!insertmacro MUI_DESCRIPTION_TEXT ${SkillKiro} "可选。安装软件、MCP、脚本和 3D 使用指南到 Kiro 的用户 skill 目录；原内容先备份，安装后重载 Agent。"
!insertmacro MUI_DESCRIPTION_TEXT ${SkillAntigravity} "可选。安装软件、MCP、脚本和 3D 使用指南到 Antigravity 的用户 skill 目录；原内容先备份，安装后重载 Agent。"
!insertmacro MUI_DESCRIPTION_TEXT ${SkillKimi} "可选。安装软件、MCP、脚本和 3D 使用指南到 Kimi Code 的用户 skill 目录；原内容先备份，安装后重载 Agent。"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; Reuse the native Agent detector before displaying the component page.
; These temporary files do not launch the workbench or write user configuration.
Function DetectSkillAgents
    InitPluginsDir
    SetOutPath "$PLUGINSDIR\skill-detection"
    File "${PAYLOAD_DIR}\*.dll"
    File /oname=SerialWorkbench.exe "${PAYLOAD_DIR}\SerialWorkbench.exe"
    SetOutPath "$PLUGINSDIR\skill-detection\platforms"
    File "${PAYLOAD_DIR}\platforms\qwindows.dll"
    ClearErrors
    ExecWait '"$PLUGINSDIR\skill-detection\SerialWorkbench.exe" --detect-skill-agents --skill-report "$PLUGINSDIR\agents.ini"' $0
    ${If} ${Errors}
        Return
    ${EndIf}
    ${If} $0 != 0
        Return
    ${EndIf}
    ReadINIStr $1 "$PLUGINSDIR\agents.ini" "agents" "codex"
    ${If} $1 == "1"
        !insertmacro SelectSection ${SkillCodex}
    ${EndIf}
    ReadINIStr $1 "$PLUGINSDIR\agents.ini" "agents" "claude-code"
    ${If} $1 == "1"
        !insertmacro SelectSection ${SkillClaude}
    ${EndIf}
    ReadINIStr $1 "$PLUGINSDIR\agents.ini" "agents" "opencode"
    ${If} $1 == "1"
        !insertmacro SelectSection ${SkillOpenCode}
    ${EndIf}
    ReadINIStr $1 "$PLUGINSDIR\agents.ini" "agents" "pi"
    ${If} $1 == "1"
        !insertmacro SelectSection ${SkillPi}
    ${EndIf}
    ReadINIStr $1 "$PLUGINSDIR\agents.ini" "agents" "hermes"
    ${If} $1 == "1"
        !insertmacro SelectSection ${SkillHermes}
    ${EndIf}
    ReadINIStr $1 "$PLUGINSDIR\agents.ini" "agents" "openclaw"
    ${If} $1 == "1"
        !insertmacro SelectSection ${SkillOpenClaw}
    ${EndIf}
    ReadINIStr $1 "$PLUGINSDIR\agents.ini" "agents" "grok"
    ${If} $1 == "1"
        !insertmacro SelectSection ${SkillGrok}
    ${EndIf}
    ReadINIStr $1 "$PLUGINSDIR\agents.ini" "agents" "gemini"
    ${If} $1 == "1"
        !insertmacro SelectSection ${SkillGemini}
    ${EndIf}
    ReadINIStr $1 "$PLUGINSDIR\agents.ini" "agents" "cursor"
    ${If} $1 == "1"
        !insertmacro SelectSection ${SkillCursor}
    ${EndIf}
    ReadINIStr $1 "$PLUGINSDIR\agents.ini" "agents" "copilot"
    ${If} $1 == "1"
        !insertmacro SelectSection ${SkillCopilot}
    ${EndIf}
    ReadINIStr $1 "$PLUGINSDIR\agents.ini" "agents" "windsurf"
    ${If} $1 == "1"
        !insertmacro SelectSection ${SkillWindsurf}
    ${EndIf}
    ReadINIStr $1 "$PLUGINSDIR\agents.ini" "agents" "devin"
    ${If} $1 == "1"
        !insertmacro SelectSection ${SkillDevin}
    ${EndIf}
    ReadINIStr $1 "$PLUGINSDIR\agents.ini" "agents" "cline"
    ${If} $1 == "1"
        !insertmacro SelectSection ${SkillCline}
    ${EndIf}
    ReadINIStr $1 "$PLUGINSDIR\agents.ini" "agents" "kilo"
    ${If} $1 == "1"
        !insertmacro SelectSection ${SkillKilo}
    ${EndIf}
    ReadINIStr $1 "$PLUGINSDIR\agents.ini" "agents" "kiro"
    ${If} $1 == "1"
        !insertmacro SelectSection ${SkillKiro}
    ${EndIf}
    ReadINIStr $1 "$PLUGINSDIR\agents.ini" "agents" "antigravity"
    ${If} $1 == "1"
        !insertmacro SelectSection ${SkillAntigravity}
    ${EndIf}
    ReadINIStr $1 "$PLUGINSDIR\agents.ini" "agents" "kimi"
    ${If} $1 == "1"
        !insertmacro SelectSection ${SkillKimi}
    ${EndIf}
FunctionEnd

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
    Call un.ReleaseMaintenance
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
