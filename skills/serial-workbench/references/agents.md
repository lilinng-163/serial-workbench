# Agent 接入

在设置 → AI / MCP → Agent 接入中搜索客户端。支持自动检测、配置备份、只读检查、导出配置，以及已确认目录的 skill 安装。默认 STDIO 自动启动共享后台实例，HTTP 适合已启动且固定地址的工作台。每次启动及端口改变时检查本机配置，但不会自动覆盖用户文件。

命令行：

```powershell
SerialWorkbenchCLI.exe agents
SerialWorkbenchCLI.exe export-agent cursor
SerialWorkbenchCLI.exe configure-agent cursor
SerialWorkbenchCLI.exe install-skill cursor
SerialWorkbenchCLI.exe export-agent generic
```

`agents` 返回 ID、检测状态、实际配置路径、是否需要导入，以及可安装的 skill 目标。`export-agent` 只输出 configuration 文本，不写配置，也不启动运行时。`configure-agent` 仅写选定客户端的 serial-workbench 条目并先备份；请关闭/重载对应客户端配置，检查项目配置是否覆盖了用户配置。

自动适配：Codex、Claude Code、Claude Desktop、OpenCode、Pi、Hermes、OpenClaw、Grok Build、Gemini CLI、Cursor、VS Code Local / Insiders、GitHub Copilot（CLI / VS Code 的 Copilot 模式）、Devin、Cline、Kilo Code、Kiro、Qoder、Zed、Antigravity、Qwen Code。Kimi Code 检测已有配置或 KIMI_CODE_HOME 后可自动更新；无法确定新旧版本时使用导出。Cline 旧版 VS Code 条目只在已有旧配置时提供自动更新。

Windsurf 旧版、TRAE 和 Continue 提供配置导出：在对应客户端的 MCP 配置入口导入，避免假定版本相关的内部存储路径。其他支持本地 STDIO 或 Streamable HTTP 的客户端使用通用导出；仅支持云端连接的服务不能直接访问电脑的 localhost。

VS Code 的 `servers` 与 Copilot 的 `mcpServers` 是不同入口；新版 Copilot 模式选择 Copilot 卡片，Local 模式选择 VS Code 卡片。Devin 和旧 Windsurf 分开。Cline 新版共享目录与旧扩展目录分开，避免把配置写到已经废弃的位置。Kilo 新版使用 `mcp` 和 command 数组。Antigravity HTTP 使用 serverUrl，Qwen 使用 httpUrl，Cline 使用 type=streamableHttp。Continue 可把导出的 JSON 放入项目 .continue/mcpServers/；不要覆盖已有文件。

skill 安装支持 Codex、Claude Code、OpenCode、Pi、Hermes、OpenClaw、Grok Build、Gemini CLI、Cursor、Copilot、Windsurf、Devin、Cline、Kilo、Kiro、Antigravity、Kimi Code；其他客户端通过 `read_usage_guide` 读取同一套指南，或在客户端支持的 UI 中导入打包的 skill。不伪造未确认的 skill 目录。skill 与 MCP 连接分别配置，安装指南不会自动授予 AI 控制权限。

当前提供 26 个客户端适配入口（含不同版本/模式）和 17 个 skill 安装目标。Antigravity 的 CLI 与 IDE 使用不同 skill 目录：检测到 agy 时选择 CLI 目录，否则为 App / IDE；同机两者并用时可在另一端导入打包的 skill。Kimi 保留旧版 .kimi 路径兼容。

检测范围是 Windows 当前用户的常见程序/CLI、默认 VS Code 扩展目录。便携安装、自定义 VS Code Profile、WSL、远程开发主机或编辑器分支可能无法自动检测；使用导出配置，在实际运行 Agent 的环境导入。文件格式验证不代表每一个第三方版本都已经完成真实客户端连接验证。全部原生工具通过相同 MCP 协议暴露，客户端工具数量限制、图片能力和权限仍由客户端决定。

项目文档 `docs/agent-integration.md` 记录默认路径、官方来源和验证范围。
