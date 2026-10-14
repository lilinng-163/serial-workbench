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
