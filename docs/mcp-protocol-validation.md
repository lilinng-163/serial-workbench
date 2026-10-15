# MCP 协议开放开关验证

本地版本：0.15.14，2026-10-15。网站与发布镜像保持冻结。

「设置 → AI / MCP → 向 AI 开放」提供串口、TCP、UDP、SSH 四个开关。
TCP 客户端和服务端共用 TCP 开关。旧配置默认全部开放，写入和控制仍受原有总权限约束。

## 验证结果

- 四组原有 Qt 测试共 181 项通过，3 项可选测试跳过。
- 新增 `mcp_protocol_tests`：10 项通过，覆盖串口、TCP 客户端、TCP 服务端、UDP、SSH。
- 验证关闭后的连接总览和计数、聚合与单连接历史、波形和 JSON/XML 及脚本读取、发送、建立和关闭连接均遵循开关。
- 混合协议历史以单条分页读取，隐藏记录不占返回页；关闭标签后的历史仍按记录所属协议过滤。
- 关闭协议不影响本地收数和 GUI 历史；重新开放后可重新读取保留的历史。
- 真实本机 HTTP 请求验证长轮询忽略未开放协议的数据，并在指定连接的访问被关闭后及时返回错误。
- 设置控件即时同步并写入文件，重新启动后恢复，TCP 客户端和服务端的设置一致。
- 打包程序通过官方 MCP Python SDK 的 HTTP / STDIO 联调，覆盖独立读者、总写权限、回环收发、分页游标、长轮询和 wave/1 数据。
- 浅色和深色设置页已渲染检查，四个协议开关与提示完整显示。

权限矩阵使用内存会话验证，不连接真实 SSH 主机或实体串口；打包联调使用 `loop://` 和本机网络。

## 运行

```powershell
ctest --test-dir build/native-release --output-on-failure
.venv/Scripts/python.exe tests/integration/smoke_native.py --app-dir dist/SerialWorkbenchNative --expected-version 0.15.14
```
