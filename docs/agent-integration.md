# Agent 配置

设置 → AI / MCP → Agent 接入可搜索 26 个适配入口，按两列滚动显示。默认使用 STDIO 自动启动；取消该选项可生成下表中的 HTTP 配置（Claude Desktop 继续使用桥）。每张卡片都有“导出配置”，未安装的客户端也可预先查看。程序只写入本应用的 `serial-workbench` 节点，不写入 API Key，不启动 AI 对话。

| Agent | 默认用户配置 | 本应用接入方式 | 官方参考 |
| --- | --- | --- | --- |
| Codex | `~/.codex/config.toml` | `mcp_servers.serial-workbench.url` | https://developers.openai.com/learn/docs-mcp |
| Claude Code | `~/.claude.json` | `mcpServers` / `type: http` | https://code.claude.com/docs/en/mcp |
| Claude Desktop | `%APPDATA%/Claude/claude_desktop_config.json` | STDIO 原生桥接程序 | https://modelcontextprotocol.io/docs/develop/connect-local-servers |
| OpenCode CLI | `~/.config/opencode/opencode.json(c)` | V1 `mcp`；V2 `mcp.servers` | https://opencode.ai/docs/mcp-servers/ |
| Pi | `~/.pi/agent/mcp.json` | `mcpServers` / `url` | https://github.com/badlogic/pi-mono/blob/main/packages/coding-agent/docs/mcp.md |
| Hermes | `~/.hermes/config.yaml` | `mcp_servers` / `url` | https://hermes-agent.nousresearch.com/docs/user-guide/features/mcp |
| OpenClaw | `~/.openclaw/openclaw.json` | `mcp.servers` / `transport: streamable-http` | https://docs.openclaw.ai/tools/mcp |
| Grok Build | `~/.grok/config.toml` | `mcp_servers` / `url` | https://docs.x.ai/build/features/mcp-servers |
| Gemini CLI | `~/.gemini/settings.json` | `mcpServers` / `httpUrl` | https://geminicli.com/docs/tools/mcp-server/ |
| Cursor | `~/.cursor/mcp.json` | `mcpServers` / `url` | [MCP](https://cursor.com/docs/mcp) |
| VS Code Local / Insiders（2 项） | `%APPDATA%/Code/User/mcp.json`；Insiders 为 `Code - Insiders` | `servers` / `type: http` | [配置参考](https://code.visualstudio.com/docs/agents/reference/mcp-configuration) |
| GitHub Copilot（CLI / VS Code Copilot 模式） | `~/.copilot/mcp-config.json` | `mcpServers` / `type: http`；tools 列表不等于自动批准 | [MCP](https://docs.github.com/en/copilot/how-tos/copilot-cli/customize-copilot/add-mcp-servers) |
| Windsurf 旧版 | 在客户端导入 | `mcpServers`，导出 command/args 或 url | [现行 Cascade 文档](https://docs.devin.ai/desktop/cascade/mcp) |
| Devin | `%APPDATA%/devin/mcp_config.json` | `mcpServers` / `url` | [配置](https://docs.devin.ai/cli/extensibility/mcp/configuration) |
| Cline | `~/.cline/data/settings/cline_mcp_settings.json` | `mcpServers` / `type: streamableHttp` | [MCP](https://docs.cline.bot/mcp/mcp-overview) |
| Cline · VS Code 旧版 | `%APPDATA%/Code/User/globalStorage/saoudrizwan.claude-dev/settings/cline_mcp_settings.json`，仅更新已有文件 | 同上；新版改用共享目录 | [迁移记录](https://github.com/cline/cline/pull/11818) |
| Kilo Code | `~/.config/kilo/kilo.json(c)`，已有 jsonc 优先 | `mcp` / `type: remote`；STDIO 为 local 和 command 数组 | [MCP](https://kilo.ai/docs/automate/mcp/using-in-kilo-code) |
| Kiro | `~/.kiro/settings/mcp.json` | `mcpServers` / `url` | [配置](https://kiro.dev/docs/mcp/configuration/) |
| Qoder | `~/.qoder/settings.json` | `mcpServers` / `type: http` | [MCP](https://docs.qoder.com/cli/mcp-reference) |
| Zed | `%APPDATA%/Zed/settings.json` | `context_servers` / `url`；STDIO 为 command 字符串和 args | [MCP](https://zed.dev/docs/ai/mcp) |
| TRAE | 在客户端导入 | `mcpServers`，导出 STDIO 或 Streamable HTTP | [MCP](https://docs.trae.ai/ide/model-context-protocol) |
| Antigravity | `~/.gemini/config/mcp_config.json` | `mcpServers` / `serverUrl` | [MCP](https://www.antigravity.google/docs/mcp) |
| Qwen Code | `~/.qwen/settings.json` | `mcpServers` / `httpUrl`（url 表示旧 SSE） | [MCP](https://qwenlm.github.io/qwen-code-docs/en/users/features/mcp/) |
| Kimi Code | `$KIMI_CODE_HOME/mcp.json`，默认 `~/.kimi-code/mcp.json`；兼容已有 `~/.kimi/mcp.json` | `mcpServers` / `url`；新旧版本无法判定时导出 | [MCP](https://moonshotai.github.io/kimi-code/en/customization/mcp) |
| Continue | 项目的 `.continue/mcpServers/`，用户选择位置导入 JSON | Claude/Cursor 兼容 JSON；HTTP type 为 streamable-http | [MCP](https://docs.continue.dev/customize/deep-dives/mcp) |

Windows 用户目录遵循进程的 `USERPROFILE`，其他平台沿用系统用户目录。各客户端遵循 `CODEX_HOME`、`CLAUDE_CONFIG_DIR`、`OPENCODE_CONFIG`、`OPENCODE_CONFIG_DIR`、`XDG_CONFIG_HOME`、`PI_CODING_AGENT_DIR`、`HERMES_HOME`、`OPENCLAW_CONFIG_PATH`、`OPENCLAW_STATE_DIR`、`GROK_HOME`、`COPILOT_HOME`、`CLINE_MCP_SETTINGS_PATH`、`KIMI_CODE_HOME`。这些变量由启动软件的环境提供。项目级配置仍可能覆盖用户配置，需要在相应 Agent 内检查最终生效的来源。

OpenCode 已有 `mcp.servers` 时保留 V2 结构；新建配置通过已安装 npm 包版本识别 V2；独立 EXE 在点击配置/检查时异步读取 `--version`，避免阻塞设置界面。V2 对应的现有文件若仍使用 V1 结构，会提示先迁移并保持原文件。无法确定版本时可先使用 OpenCode 自己生成一次配置再更新。

检测基于 CLI、常见桌面安装位置，以及默认 VS Code 扩展目录中的已识别 package.json；仅残留配置目录不算已安装。未检测到安装时禁用自动配置/检查，但始终可导出。便携程序、WSL、自定义 VS Code Profile、远程开发环境及编辑器分支可能需要手动导入。配置检查验证文件中的地址并发起本地 HTTP 握手；不声称已经在各 Agent 中完成对话。

端口切换后使用一键更新同步，再在 Agent 中重载或重启。新端口被占用时，切换失败并继续使用原端口。默认启动占用处理使用操作系统分配的空闲回环端口，并立即保存。

语法校验库固定为 toml++ 3.4.0 和 fkYAML 0.4.2（MIT），许可证在 `third_party/`，打包时复制到 `licenses/`。JSONC/常见 JSON5 使用保留文本位置的局部编辑；YAML 仅编辑块映射，特殊流式映射/合并键等会提示未完成，保留原文件。商标图标来源见 `resources/agents/README.md`。


## 安装时选择 skill

安装向导在组件页默认勾选检测到的 Agent 使用指南，用户可取消；设置 → AI / MCP → Agent 接入 → 配置 skill 可随时安装或更新。支持下列 17 个目标。未检测到的 Agent 也可手动勾选。不修改 MCP 配置或密钥；不同内容先备份再替换，相同内容不重复备份，卸载应用时保留 skill。安装失败会提示，主程序仍可使用。

| Agent | 默认 skill 目录 | 环境覆盖 |
| --- | --- | --- |
| Codex | `~/.codex/skills/serial-workbench` | `CODEX_HOME` |
| Claude Code | `~/.claude/skills/serial-workbench` | `CLAUDE_CONFIG_DIR` |
| OpenCode | `~/.config/opencode/skills/serial-workbench` | `OPENCODE_CONFIG_DIR` / `XDG_CONFIG_HOME` |
| Pi | `~/.pi/agent/skills/serial-workbench` | `PI_CODING_AGENT_DIR` |
| Hermes | `~/.hermes/skills/serial-workbench` | `HERMES_HOME` |
| OpenClaw | `~/.openclaw/skills/serial-workbench` | `OPENCLAW_STATE_DIR` |
| Grok Build | `~/.grok/skills/serial-workbench` | — |
| Gemini CLI | `~/.gemini/skills/serial-workbench` | — |
| Cursor | `~/.cursor/skills/serial-workbench` | — |
| GitHub Copilot | `~/.copilot/skills/serial-workbench` | `COPILOT_HOME` |
| Windsurf | `~/.codeium/windsurf/skills/serial-workbench` | — |
| Devin | `%APPDATA%/devin/skills/serial-workbench` | — |
| Cline | `~/.cline/skills/serial-workbench` | — |
| Kilo | `~/.kilo/skills/serial-workbench` | — |
| Kiro | `~/.kiro/skills/serial-workbench` | — |
| Antigravity | App / IDE：`~/.gemini/config/skills/serial-workbench`；检测到 agy CLI 时：`~/.gemini/antigravity-cli/skills/serial-workbench` | 两者并用时可在另一端手动导入 |
| Kimi Code | `~/.kimi-code/skills/serial-workbench`；已有旧 MCP 配置时用 `~/.kimi/skills/serial-workbench` | 新版 `KIMI_CODE_HOME` |

目录规则参考 [Claude Code skills](https://code.claude.com/docs/en/skills)、[OpenCode skills](https://opencode.ai/docs/skills/)、[Pi skills](https://github.com/earendil-works/pi/blob/main/packages/coding-agent/docs/skills.md)；Codex 跟随本机 CODEX_HOME 的 skills 目录。其他 Agent 可直接调用 `read_usage_guide` 获取指南，不假设客户端会扫描某个未经确认的目录。复制 skill 后重载 Agent；skill 不会自动建立 MCP 连接。

## 原生搜索与脚本接口

先 `list_connections` 选准确 channel_id，再 `list_search_views(channel_id)` 取得本次 GUI 的 view_id。`search_logs(channel_id, view_id, filter={text, case_sensitive, whole_word, regex, logical})` 同步搜索框和开关；省略 filter 则沿用 GUI 条件。结果是 GUI 显示缓存中的匹配行/解析记录，保留暂停、清空、HEX/编码及缓存范围，不等于完整原始历史。读取非活动页面时会刷新该页面的显示缓存，不切换用户的当前标签；暂停/冻结仍有效。

使用 offset/limit/snapshot 分页；显示变化后从 0 重试，更新 filter 时不传旧 snapshot。文本截断查看 truncated，原始字节及更早历史仍通过 read_events 的独立游标读取。SSH 保持交互终端，通过 read_events 读取输出，不强行切成日志页。

`get_frame_script(channel_id)` 返回代码、enabled、revision 与错误。`validate_frame_script(code, sample_chunks=[HEX,...])` 离线编译和试解析，支持跨块缓存，不发送数据、不修改在线状态。`set_frame_script(channel_id, code, enabled, expected_revision)` 需要串口开放、AI 控制权限、最新 revision，且没有传文件或 GUI 未保存编辑。应用后 GUI、持久化脚本与编辑器同步，仅处理新收到的数据。原始报文与其他读取者不受试解析影响。

应用每次启动、再次唤起已运行实例及切换 MCP 端口后会检查已检测 Agent 的用户配置。状态栏、Agent 接入卡片及 `get_status.agent_config_checks` 显示结果；自动检查只读，不会自动覆盖地址。用户可通过现有一键更新并重载 Agent。项目级覆盖或 Agent 尚未重载时，仍需在客户端核对实际连接来源。

新增 skill 目录：Hermes 为 `$HERMES_HOME/skills/serial-workbench`（默认 `~/.hermes/skills`），OpenClaw 为 `$OPENCLAW_STATE_DIR/skills/serial-workbench`（默认 `~/.openclaw/skills`），Grok Build 为 `~/.grok/skills/serial-workbench`，Gemini CLI 为 `~/.gemini/skills/serial-workbench`。设置中显示实际目标目录和内容状态，打开设置或安装器检测阶段只读扫描，不写入指南或 MCP 配置；确认安装/更新后才写入。

Claude Desktop 的自定义 skill 在客户端导入；没有将其当作 Claude Code 的目录来安装。它也可通过 MCP 的 read_usage_guide 直接读取本指南。

目录依据：[Hermes](https://hermes-agent.nousresearch.com/docs/user-guide/features/skills)、[OpenClaw](https://docs.openclaw.ai/tools/skills)、[Grok Build](https://github.com/xai-org/grok-build/blob/main/crates/codegen/xai-grok-shell/README.md#skills)、[Gemini CLI](https://geminicli.com/docs/cli/using-agent-skills/)。

## 无窗口接入（0.15.36）

Agent 接入页默认勾选“后台自动启动（STDIO）”，配置为原生桥命令并携带当前配置路径。桥按需启动或复用后台工作台，发现实际端口，打开 GUI 后保留连接、搜索与脚本。取消勾选可保留固定 HTTP 接入。检查同时识别本配置的自动启动条目和正确的 HTTP 地址，不把端口变更误报为 STDIO 失效。

安装目录的 `SerialWorkbenchCLI.exe` 提供 `start/status/show/stop/tools/call/mcp/agents/export-agent/install-skill/configure-agent`。默认不提升 AI 控制权限；显式 `start --allow-control` 仅为新实例开启并保存权限。CLI 使用 JSON 输出和退出码，完整步骤见 [内置 CLI 指南](../skills/serial-workbench/references/cli.md)。

Windows MCP 客户端可能用 Job Object 回收子进程。后台实例在这种情况下通过系统 WMI 创建，属于用户会话，Agent 退出后仍可被其他客户端使用；WMI 不可用时明确报错，可从普通终端先执行 `start`。不创建开机任务或 Windows 服务，不修改系统权限。新模式已用官方 MCP Python SDK 和打包运行库验证；各 Agent 配置格式用隔离文件验证，不代表所有第三方客户端版本都做过实际连接测试。


## 导出与版本兼容

`SerialWorkbenchCLI.exe agents` 返回当前 ID、检测状态、配置路径、格式、手动导入标记和 skill 目标；`export-agent cursor` 输出该客户端配置，`export-agent generic` 输出通用 `mcpServers` JSON。导出不写文件、不启动运行时；`configure-agent ID` 仍仅为已检测且有确定路径的客户端写入，先备份、保留其他节点，重复配置不重复备份。

VS Code 的 Local 与 Copilot 模式分别使用 `servers` 和 `mcpServers`，应选择对应卡片。[VS Code 迁移说明](https://code.visualstudio.com/docs/agent-customization/migrate-customizations)记录了模式区别。Cline 旧存储条目仅用于旧扩展；现代 CLI / IDE 共享目录选择 Cline。Devin 与旧 Windsurf 分列；Windsurf、TRAE、Continue 的路径未强行推断，导出后在客户端合并导入。配置不添加 autoApprove、不自动启用 AI 写权限。

新增 skill 路径依据：[Cursor](https://cursor.com/help/customization/skills)、[Copilot](https://docs.github.com/en/copilot/reference/copilot-cli-reference/cli-config-dir-reference)、[Devin / Windsurf](https://docs.devin.ai/cli/extensibility/skills/overview)、[Cline](https://docs.cline.bot/customization/skills)、[Kilo](https://kilo.ai/docs/customize/marketplace)、[Kiro](https://kiro.dev/docs/skills/)、[Antigravity](https://www.antigravity.google/docs/skills?tab=ide)、[Kimi Code](https://moonshotai.github.io/kimi-code/en/customization/skills)、[Kimi 旧版](https://moonshotai.github.io/kimi-cli/en/customization/skills)。未确认目录的客户端通过 `read_usage_guide` 读取内嵌指南，或在客户端 UI 导入 skill。

## 3D 原生接口

`list_model_views`、`open_model_view`、`get_model_state`、`configure_model_view`、`capture_model_view` 与 GUI 共用模型页面和数据来源；支持导入本机模型、配置映射、读取姿态矩阵与采用帧序号，返回原生 PNG image 内容。完整参数与验证步骤见 [3D MCP 指南](../skills/serial-workbench/references/model3d.md)。`read_usage_guide` 可选 overview / frame_script / cli / model3d / agents。

客户端路径和格式参考于 2026-10-27 核对；测试覆盖隔离配置的结构、备份、无关字段保留、幂等及工作台官方 MCP SDK 调用。没有逐一安装全部第三方客户端进行对话测试，不能把“配置匹配”当作客户端实际连通。图片分析还取决于 Agent/模型是否支持 MCP image 内容。后续客户端更改格式时以其官方文档和实际生效配置为准。
