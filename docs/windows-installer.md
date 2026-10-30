# Windows 安装与卸载

安装包：`SerialWorkbench-<版本>-windows-x64-setup.exe`，Windows x64。

双击后按安装向导选择目录及桌面快捷方式。默认安装到当前用户的 `%LOCALAPPDATA%/Programs/SerialWorkbench`；可选其他可写目录。安装后程序和原生 MCP 桥直接位于所选目录，不再需要每次从单文件缓存启动。

Windows「已安装的应用」、开始菜单和安装目录均提供卸载入口。卸载界面可选择删除当前用户的设置，默认保留。开发版安装器会检测目标安装目录的可见窗口、后台实例、MCP 桥接（包括 CLI 的 `mcp` 模式），经确认保存设置后关闭并继续。升级期间阻止新的后台实例和桥接启动；完成、取消或安装进程退出后自动解除。请在完成后重新连接 Agent。

安装版设置存放在 `%LOCALAPPDATA%/SerialWorkbench/settings.json`；便携版继续使用程序目录的配置。`--settings` 可指定独立配置文件。安装版通过 `installation.json` 识别分发方式，该标记不包含用户名和绝对路径。

同一 Windows 用户启动新版安装包时，自动从卸载注册项读取原安装目录，安装向导直接使用该路径并覆盖升级，无需重新选择。若需要更换目录，先卸载并保留设置，再安装到新目录。安装后使用 AI/MCP 的一键更新重新同步 Agent 的 URL 或 STDIO 桥路径。

卸载清单在打包阶段从分发文件生成。只删除清单中的文件和空目录，不递归删除用户选择的安装目录；自行添加的文件会保留。Windows 卸载注册项和快捷方式均为当前用户创建。

## 构建

先执行 `scripts/build.ps1`，再执行：

```powershell
./scripts/package-release.ps1 -OutputDir dist/local/0.15.32 `
    -QtSourceRoot D:/lib/Qt/6.11.1/Src `
    -MakeNSIS E:/MinGW/w64devkit/bin/makensis.exe -InstallerOnly
```

NSIS Modern UI 提供目录、组件、安装和卸载页面，中文字体与浅绿色透明闪电图标统一用于程序、安装器、卸载器和快捷方式。原始 SVG 和 ICO 在 `resources/brand/`，图标九种分辨率从 16 到 256 像素，源文件可通过 `scripts/generate-icon.py` 重新生成。

## 验证

`tests/integration/smoke_installer.py` 使用独立的 Installer Test 注册项及快捷方式，安装到工作区中文临时目录，先注册旧版本，再不传入 `/D` 运行新版本，检查原自定义路径自动识别、设置保留、安装、升级、启动、HTTP/STDIO、运行程序保护和卸载。测试结束会卸载清理，不更改正式应用或真实 Agent 配置。注册项/快捷方式测试需要当前用户的普通 Windows 写权限。

传入打包脚本返回的 payload、uninstall manifest 和 installer 路径：

```powershell
./.venv/Scripts/python.exe tests/integration/smoke_installer.py `
    --payload build/release-<版本>-<阶段ID>/SerialWorkbench `
    --remove-manifest build/release-<版本>-<阶段ID>/remove-files.nsh `
    --installer dist/local/0.15.32/SerialWorkbench-0.15.32-windows-x64-setup.exe `
    --expected-version 0.15.32 --makensis E:/MinGW/w64devkit/bin/makensis.exe
```

参考：[NSIS Modern UI 文档](https://nsis.sourceforge.io/Docs/Modern%20UI%202/Readme.html)。

## 可选 Agent skill

安装组件页默认勾选本机已检测到的 Agent 使用指南，用户可取消；未检测到的也可手动选择。当前开发版提供 17 个 skill 目标：Codex、Claude Code、OpenCode、Pi、Hermes、OpenClaw、Grok Build、Gemini CLI、Cursor、Copilot、Windsurf、Devin、Cline、Kilo、Kiro、Antigravity、Kimi Code。指南包含软件、CLI、MCP、原生搜索、C 风格脚本、3D 模型与 Agent 接入的用法；写入对应用户目录并遵循环境覆盖，已有不同内容先备份，相同内容不重复写入。此步骤不修改 MCP 配置；安装后重载 Agent。卸载软件时保留已安装的 skill。其他客户端可直接通过 read_usage_guide 读取指南。详情与目录见 [Agent 接入说明](agent-integration.md)。

无需重装即可更新指南：设置 → AI / MCP → Agent 接入 → 配置 skill。检测、目标目录与备份规则和安装器共用原生实现；设置中的安装不需要 MCP 已启动。

## 命令行

安装目录包含 SerialWorkbenchCLI.exe；`start` 在后台运行，`status` 显示实际 MCP 地址，`show` 打开同一个 GUI，`stop` 停止共享实例。升级/卸载向导可以代为关闭本安装目录的后台实例及 Agent STDIO 桥；其他安装目录及 Agent 本身不会结束。CLI 和桥的用法随 skill 安装，见 [命令行指南](../skills/serial-workbench/references/cli.md)。

## 升级占用处理（开发版）

- 所有替换/删除开始前，独立的原生维护工具检查 EXE 和 DLL。它不依赖待更新目录中的 Qt 或 MinGW DLL。
- 关闭只针对路径一致的本安装目录。工作台通过现有本地 IPC 正常保存退出；多个绝对路径 `--settings` 配置分别关闭。无响应、无法识别的相对配置路径或权限不足时保留文件并提供重试/取消，不强杀工作台。普通 CLI 配置命令等待自然结束；仅明确的 MCP 桥可结束其管道进程。
- 非交互部署默认不关闭程序，检测到占用返回退出码 2；只有明确指定 `/S /CLOSEAPPLICATIONS` 才授权关闭上述进程。卸载支持相同参数。权限/文件占用无法解除时仍返回失败，不能保证绕过杀毒软件或系统权限。
- 临时维护标记由 Windows delete-on-close 句柄持有，取消和进程退出后释放，不会永久禁止启动。设置安装 skill 等非运行实例命令仍可执行。
- 回归覆盖中文与空格路径、GUI + 自定义后台配置 + 多个桥接、CLI mcp、其他安装目录隔离、升级前取消、锁定 DLL、维护期禁止重启、升级保留数据和带后台进程的卸载。
