# Agent 配置

设置 → AI / MCP → Agent 接入按两列显示客户端。程序只写入本应用的 `serial-workbench` 节点，不写入 API Key，不启动 AI 对话。

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

遵循 `CODEX_HOME`、`CLAUDE_CONFIG_DIR`、`OPENCODE_CONFIG`、`OPENCODE_CONFIG_DIR`、`XDG_CONFIG_HOME`、`PI_CODING_AGENT_DIR`、`HERMES_HOME`、`OPENCLAW_CONFIG_PATH`、`OPENCLAW_STATE_DIR`、`GROK_HOME`。这些变量由启动软件的环境提供。项目级配置仍可能覆盖用户配置，需要在相应 Agent 内检查最终生效的来源。

OpenCode 已有 `mcp.servers` 时保留 V2 结构；新建配置通过已安装 npm 包版本识别 V2；独立 EXE 在点击配置/检查时异步读取 `--version`，避免阻塞设置界面。V2 对应的现有文件若仍使用 V1 结构，会提示先迁移并保持原文件。无法确定版本时可先使用 OpenCode 自己生成一次配置再更新。

检测基于可执行文件或已注册桌面应用，不依赖配置目录是否存在。未检测到安装时按钮禁用；安装后点击重新检测。配置检查验证文件中的地址并发起本地 HTTP 握手；不声称已经在各 Agent 中完成对话。

端口切换后使用一键更新同步，再在 Agent 中重载或重启。新端口被占用时，切换失败并继续使用原端口。默认启动占用处理使用操作系统分配的空闲回环端口，并立即保存。

语法校验库固定为 toml++ 3.4.0 和 fkYAML 0.4.2（MIT），许可证在 `third_party/`，打包时复制到 `licenses/`。JSONC/常见 JSON5 使用保留文本位置的局部编辑；YAML 仅编辑块映射，特殊流式映射/合并键等会提示未完成，保留原文件。商标图标来源见 `resources/agents/README.md`。


## 安装时选择 skill

安装向导在组件页默认勾选检测到的 Agent 使用指南，用户可取消；设置 → AI / MCP → Agent 接入 → 配置 skill 可随时安装或更新。支持 Codex、Claude Code、OpenCode、Pi、Hermes、OpenClaw、Grok Build 和 Gemini CLI。未检测到的 Agent 也可手动勾选。不修改 MCP 配置或密钥；不同内容先备份再替换，相同内容不重复备份，卸载应用时保留 skill。安装失败会提示，主程序仍可使用。

| Agent | 默认 skill 目录 | 环境覆盖 |
| --- | --- | --- |
| Codex | `~/.codex/skills/serial-workbench` | `CODEX_HOME` |
| Claude Code | `~/.claude/skills/serial-workbench` | `CLAUDE_CONFIG_DIR` |
| OpenCode | `~/.config/opencode/skills/serial-workbench` | `OPENCODE_CONFIG_DIR` / `XDG_CONFIG_HOME` |
| Pi | `~/.pi/agent/skills/serial-workbench` | `PI_CODING_AGENT_DIR` |

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

安装目录的 `SerialWorkbenchCLI.exe` 提供 `start/status/show/stop/tools/call/mcp/install-skill/configure-agent`。默认不提升 AI 控制权限；显式 `start --allow-control` 仅为新实例开启并保存权限。CLI 使用 JSON 输出和退出码，完整步骤见 [内置 CLI 指南](../skills/serial-workbench/references/cli.md)。

Windows MCP 客户端可能用 Job Object 回收子进程。后台实例在这种情况下通过系统 WMI 创建，属于用户会话，Agent 退出后仍可被其他客户端使用；WMI 不可用时明确报错，可从普通终端先执行 `start`。不创建开机任务或 Windows 服务，不修改系统权限。新模式已用官方 MCP Python SDK 和打包运行库验证；各 Agent 配置格式用隔离文件验证，不代表所有第三方客户端版本都做过实际连接测试。
