# 开发交接记录

每项任务完成或代理交接前，在本文件顶部新增记录。保持简洁，写事实与准确停点，不把聊天全文放进仓库。

## 2026-10-16 — 收发页从 SSH 切回串口时恢复日志视图

- 执行者：DeepSeek。
- 分支与起点：`deepseek-dev`，起点 `43f3dc4184e59349380cd80479ad5f9259faa53e`（v0.15.29）。
- 用户目标：收发页面处于串口“收发日志”时切到 SSH 会进入终端模式；切回串口后仍停留在终端模式，没有恢复收发日志界面。要求修正。
- 根因：`Pane::tick()` 与 `Pane::setChannel()` 只强制 SSH 进入终端，没有在非 SSH 时把终端模式还原；`Pane::restore()` 还会把保存的 `terminal_mode`（可能来自上次的 SSH 页面）套用到新的串口页面。`terminalMode` 因此具有粘性。
- 已完成：
  - `src/ui/window.h` 为 `Pane` 增加 `userTerminalMode`，记录用户对非 SSH 页面的显式终端选择。
  - `src/ui/window.cpp`：`Pane::setTerminal()` 在非 SSH 情况下记录该偏好；`Pane::tick()` 在非 SSH 且与偏好不一致时还原为偏好模式；`Pane::restore()` 仅在已有非 SSH 连接时读取保存的 `terminal_mode`，不再让空/串口页面继承终端模式。
  - 保留 SSH 单终端行为：SSH 仍强制终端并隐藏模式按钮、发送栏；用户手动切到终端的串口页面在经历 SSH 后仍保持终端。
- 验证：
  - 构建：`cmake --build build/native-release --parallel 4`，成功。
  - 测试：`ctest --test-dir build/native-release --output-on-failure` 全部 14 组通过；新增 `terminal_keyboard_tests` 用例 `switchingFromSshBackToSerialRestoresLogView`、`manualTerminalPreferenceSurvivesSshExcursion` 通过。
  - 实际窗口截图（本机 Qt 6.11.1 / MinGW 13.1）：`--view terminal` 的串口页经多次 tick 后仍为终端；默认页为收发日志；`--view ssh` 的无连接草稿页为终端；带 `appearance.terminal_mode=true` 启动的串口首页显示收发日志而非终端。
- 未完成：无。
- 风险与兼容：不改协议、设置键或持久化格式；`terminal_mode` 仍按原格式读写。行为上，全新/无连接的串口页面不再继承上次会话的终端模式，这是修复目标本身。真实 SSH 服务与真机串口交互未在本机复测，按项目约定应在对应环境验证。
- 提交与同步：见本次交接回复中的提交哈希；已推送 `deepseek-dev`，未推送 `main`，未发布 Release。
- 下一步：Codex 审查 `origin/main..origin/deepseek-dev`，在从 `main` 建立的临时分支集成并运行原生测试。

## 2026-10-16 — 建立 DeepSeek 接续分支

- **执行者**：Codex。
- **分支**：`deepseek-dev`，从 `origin/main` 的 `43f3dc4184e59349380cd80479ad5f9259faa53e` 创建。
- **正式版本**：`v0.15.29` 已发布，`main` 继续保留该发布状态。
- **本次改动**：新增根 `AGENTS.md`、`DEEPSEEK.md` 和本记录；建立 DeepSeek 开发、Codex 审查集成的协作流程。
- **当前业务任务**：无未完成项；等待用户提供新的开发需求。
- **验证**：本次为交接文档改动，检查 Markdown 相对链接、关键源码入口、分支基线和 Git diff；不把此前的构建结果当作本次重新运行。
- **基线验证参考**：v0.15.29 的 14 组原生测试与发行包安装、升级、卸载、便携版、单文件版验证均已通过。
- **后续接手**：阅读根交接文档，确认本地状态和远程 tip，再按用户任务开发。

## 新记录模板

```markdown
## YYYY-MM-DD — 任务名称

- 执行者：DeepSeek / Codex。
- 分支与起点：实际分支、开发开始时的提交。
- 用户目标：本次具体需求。
- 已完成：最终行为和关键文件。
- 验证：实际命令、结果；未运行的检查及原因。
- 未完成：明确剩余事项；已完成则写“无”。
- 风险与兼容：设置迁移、协议影响、真实设备验证等实际限制。
- 提交与同步：相关提交；是否已推送。写入记录后产生的最终提交可在交接回复中补充，不需要循环修改本提交的哈希。
- 下一步：给下一位代理的准确起点。
```
