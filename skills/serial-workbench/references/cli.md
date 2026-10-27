# 无窗口启动与命令行

安装包和便携包包含 `SerialWorkbenchCLI.exe`、`SerialWorkbenchMCP.exe` 及桌面运行库，运行不需要 Python 或 Qt SDK。使用安装目录中的程序；不能单独复制一个 EXE。不要猜测机器上的安装路径：检查现有 Agent 配置中的 command、Windows 卸载注册项 InstallLocation，或用户提供的安装目录。

## 建立接入

先使用已注册的 Serial Workbench MCP。如果未启动，配置为 STDIO 的 `SerialWorkbenchMCP.exe` 会自动启动后台实例；GUI 的「设置 → AI / MCP → Agent 接入 → 后台自动启动（STDIO）」默认勾选，配置后重载 Agent。取消勾选继续使用固定 HTTP 地址，HTTP 客户端自身不能启动程序。

也可在安装目录执行：

```powershell
.\SerialWorkbenchCLI.exe status
.\SerialWorkbenchCLI.exe start
.\SerialWorkbenchCLI.exe tools
.\SerialWorkbenchCLI.exe call get_status
.\SerialWorkbenchCLI.exe show
```

`status` 不启动程序；未运行时返回 `running:false`、退出码 3。`start`、`tools`、`call`、`mcp` 按需启动或复用后台实例。输出为 JSON；`call` 返回 MCP 的 CallToolResult（`structuredContent` / `content` / `isError`），工具错误退出码 4，参数或启动错误退出码 2。`mcp` 是纯 STDIO JSON-RPC，供客户端注册，不是普通交互命令。

同一配置文件的 GUI、CLI 和多个 Agent 共用一个进程、连接与 Journal。`show` 打开原实例的界面，连接、游标和脚本不会重建；显示界面后关闭窗口会正常退出程序。Agent 断开不会停止后台服务。`stop` 会关闭该实例的所有连接，只有任务确实要求停止时才调用，不能在自己读完后自动关闭其他读者共享的实例。

## 选实例与权限

安装版默认使用 `%LOCALAPPDATA%/SerialWorkbench/settings.json`，便携版使用程序目录的 `settings.json`。`--settings <绝对路径>` 可选择隔离配置；GUI 与 Agent 应传同一路径。`start` 返回实际 `mcp_url`，端口冲突时会选空闲端口，不能假设始终是 8765。`--url <地址>` 可用于 `call`、`tools`、`mcp`，也可传给桥；显式地址只连接该服务，不会启动本地实例。不要在用户现有配置仍运行时用另一个配置重新打开相同串口。

后台模式沿用保存的 AI 控制与各协议开关，新配置默认只读。用户已明确授权发送/控制且需要创建新实例时，使用：

```powershell
.\SerialWorkbenchCLI.exe start --allow-control
```

这会保存当前配置的 AI 写权限，后续启动继续沿用。不能为了读取或排查失败擅自开放权限；正在运行且禁止控制的实例不会被此参数提权，请使用其 GUI 设置。启动本身不自动连接硬件。SSH 可使用已有密钥/认证缓存；需要密码或新主机确认时，后台会取消交互请求，应 `show` 后在界面连接，不能在不可见窗口里等待或传递密码。

## 调用工具和安装指南

含脚本、引号或多行文本的参数优先写入 UTF-8 JSON 文件，避免命令行转义或敏感内容进入进程参数：

```powershell
.\SerialWorkbenchCLI.exe call search_logs --args-file search.json
.\SerialWorkbenchCLI.exe call validate_frame_script --args-file parser-test.json
.\SerialWorkbenchCLI.exe agents
.\SerialWorkbenchCLI.exe export-agent cursor
.\SerialWorkbenchCLI.exe export-agent generic
.\SerialWorkbenchCLI.exe install-skill codex
.\SerialWorkbenchCLI.exe configure-agent codex
```

`install-skill` 更新指南并备份不同内容，不修改 MCP 配置。`configure-agent` 配置 STDIO 自动启动并备份原配置；保留其他服务与设置，完成后重载对应 Agent。`agents` 返回当前版本支持的 ID、检测结果、配置路径、手动导入标记和 skill 目标；覆盖 Cursor、Copilot、Cline、Kilo、Kiro 等常见客户端。`export-agent ID` 只返回 configuration 文本，`generic` 输出通用 MCP JSON，不启动运行时也不改客户端文件。需要手动导入的客户端在其 MCP 设置中合并配置。完整范围见 [Agent 接入](agents.md)。不要把安装 skill、配置文件校验成功当作 Agent 实际握手成功；仍需调用 `get_status` 验证。

搜索、连接选择、独立游标、脚本 revision 和试解析样本遵循主指南。后台搜索使用同一页面模型，仍受显示缓存限制；`show` 后可以看到相同的筛选、连接和已应用脚本。
