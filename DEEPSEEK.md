# Serial Workbench：DeepSeek / Codex 项目交接

这份文档让下一位开发代理快速进入项目。先读本页与[开发交接记录](docs/DEVELOPMENT_LOG.md)，再根据用户任务读取对应模块。

## 1. 当前基线

| 项目 | 当前状态 |
| --- | --- |
| 仓库 | https://github.com/lilinng-163/serial-workbench |
| 本机源码 | `E:/demo/serial`；其他机器以实际克隆目录为准 |
| 正式主线 | `main` |
| DeepSeek 开发分支 | `deepseek-dev` |
| 交接基线 | 以 `v0.15.32` 标签及最新开发交接记录为准 |
| 已发布版本 | [v0.15.32](https://github.com/lilinng-163/serial-workbench/releases/tag/v0.15.32)，2026-10-17 |
| 技术栈 | C++17、Qt Widgets / Network / SerialPort / Core5Compat / Svg、CMake、Ninja |
| 主要分发平台 | Windows x64；Docker 用于 Linux 开发构建 |
| 用户文档 | [English](README.md) / [简体中文](README.zh-CN.md) |

**开始前重新执行 `git fetch origin`、`git status`、`git log`。上述版本和提交是交接快照，不代表未来远程的最新状态。**

v0.15.31 集成 MCP 原生搜索与 GUI 同步、脚本读写/离线验证、安装时可选 Agent skill、启动地址检查，以及 DeepSeek 完成的 AI 活动提示、连接/脚本界面同步与调用详情。Codex 审查时补充了调用日志选中项及回看位置的恢复。具体验证与发布结果以开发日志最新记录为准。

v0.15.32 新增设置中的 skill 安装/更新、安装器按已检测 Agent 默认勾选、脚本试解析 HEX 的 MCP 同步与保存，并移除脚本 AI 提示词窗口。修复系统文件选择、控件组下拉/删除与自由布局宽度跟随，新增版本回溯和底部版本号，并统一清空入口、图层文字和主题滑块。正式安装包、单文件和便携包的验证与发布结果见最新开发日志；deepseek-dev 保留用于接续开发。

## 2. 项目解决什么问题

一个应用同时管理串口、TCP 客户端/服务端、UDP、SSH，提供收发日志、交互终端、波形、JSON/XML、串口控制面板与帧解析脚本。

AI 经 MCP 读取同一应用拥有的连接和数据。**GUI 与多个 AI 不争抢端口，也不互相消费历史。** 桌面程序、STDIO 桥和 SSH 认证助手均为原生程序，运行发行包不需要 Python。

`legacy/python/` 保留历史 Python 版本，不能把它当作当前功能的主要实现位置。

## 3. 先建立这条调用链

```text
src/app/main.cpp
  ├─ Engine：管理 Channel 与 Journal
  ├─ McpServer：同进程 HTTP MCP
  └─ Window：工作台窗口、Pane、Instrument

设备 / 网络 / SSH
  → Channel::receive
  → Journal::append                     原始事件与全局序号
  ├─ Pane::tick → Journal::read          按连接读取，GUI 自有游标
  │    ├─ LogView                       串口 / 网络收发日志
  │    └─ TerminalView                  终端字符屏幕与独立滚动历史
  ├─ parseWave / parseStructured         按来源维护解析缓冲与记录
  ├─ FrameScriptProcessor               串口脚本后台执行
  └─ McpServer::call / readEvents        各 MCP 客户端自有游标

GUI / 串口控制 / MCP 写请求
  → Channel::send                       共用传输；MCP 另在入口检查权限

src/bridge/main.cpp
  → SerialWorkbenchMCP.exe              STDIO 转发至正在运行的 HTTP MCP
```

当前 C++ 类名是 **`Journal`**，不是旧 Python 版本的 `EventJournal`。GUI 和 AI 不应再次打开传输层或直接争读 socket/串口。

## 4. 按任务定位源码

| 任务 | 优先阅读 |
| --- | --- |
| 启动参数、单实例、演示与截图 | `src/app/main.cpp` |
| 连接、串口恢复、收发、历史、wave/1、JSON/XML 解析 | `src/core/core.h`、`core.cpp` |
| SSH PTY、配置别名、认证与缓存 | `src/core/ssh_process.*`、`ssh_profiles.*`、`ssh_auth.*`；`src/app/ssh_askpass.cpp` |
| MCP 工具、分页、协议权限 | `src/mcp/mcp.*`、`usage_guide.*`；`skills/serial-workbench/` |
| 主界面、连接页、控件页、设置与持久化 | `src/ui/window.*`；先定位 `Window` / `Pane` / `Instrument` 对应方法 |
| 终端字符屏、历史、选区、键盘输入 | `src/ui/terminal.*`：`TerminalView`；日志显示为 `LogView` |
| 波形、四路读数、通道选择与设置 | `src/ui/wave_plot.cpp`、`wave_settings.cpp`、`controls.h` |
| JSON 助手、控件迁移与数据源 | `src/ui/json_tools.*`、`instrument_config.*` |
| 串口控制卡片、控件组、模板与收发小窗 | `src/ui/control_card.*`、`serial_control_panel.*`、`serial_control_presets.cpp`、`serial_control_packet.cpp`、`serial_monitor_dialog.*` |
| C 风格脚本解释器与编辑器 | `src/core/frame_script.*`、`script_worker.cpp`；`src/ui/script_editor.cpp`、`script_results.cpp` |
| 主题、图标、按钮/勾选/下拉动画 | `src/ui/controls.*`、`button_motion.*` |
| 标签分离、恢复、拖动与圆弧衔接 | `src/ui/docking.*` |
| 背景、透明标题栏、原生窗口边界 | `src/ui/background_image.*`、`background_settings.cpp`、`window_chrome.*` |
| 更新与发行包 | `src/ui/update_service.*`、`update_dialog.*`；`scripts/package-release.ps1`、`windows-installer.nsi`、`release-launcher.nsi` |
| 构建、测试、打包规则 | 根 `CMakeLists.txt`、`CMakePresets.json`，各目录 `CMakeLists.txt`，`scripts/build.ps1` |

完整目录介绍见 [docs/project-structure.md](docs/project-structure.md)。某些旧设计说明可能滞后；以当前源码和实际测试为准。

## 5. 必须保留的行为

1. **共享读取**：一个应用拥有连接；GUI、各 AI、各控件独立记录游标。清空页面不清除共享 Journal。跨连接不要复用读游标或把半帧混在一起。
2. **MCP 权限**：串口、TCP、UDP、SSH 分别开放；发送、建立/关闭连接还需要 `allowControl`。协议过滤发生在分页前，保留历史也受当前权限约束。
3. **有界缓存**：Journal 最多 20000 条或 8 MiB；客户端处理 `gap`、`session_id`、`next_cursor`。终端的一万行回看是独立显示缓冲，不等于共享原始事件缓存。
4. **SSH 只有终端**：`Pane::setTerminal` 和 `tick` 强制 SSH 终端模式，隐藏模式按钮与发送栏。保留历史滚动、复制、IME、PTY 尺寸同步及替代屏恢复。历史选区不能当作远端输入删除。
5. **SSH 输入保密**：发送可能包含远端密码，`Channel::send` 不将 SSH 输入正文写入共享日志。认证缓存使用现有 Windows DPAPI 流程，不把密码或私钥写入设置、截图、日志和 MCP。
6. **波形协议**：仅解析完整 `@wave name=value,...` 行；最多 16 路。脚本结果、普通数字、JSON/XML 不自动变成波形。顶部读数默认前四路、定宽、可换通道。
7. **脚本边界**：C 风格脚本只用于串口，后台执行且有资源限制；MCP 可读取脚本、离线编译试解析；修改/启用需要串口开放、AI 控制权限和最新 revision，不能覆盖 GUI 未保存编辑或正在执行的试解析，文件传输期间也拒绝修改。
8. **发送与布局**：控件默认手动，各控件自动发送独立；恢复配置、移动或缩放控件不发送命令。文件传输期间遵守原连接独占规则。
9. **交互约束**：重命名仅右键或 F2，输入框避开图标与关闭按钮；中键关闭页。动画不挤压相邻控件、不改变点击几何区域。圆角检查实际的弹窗和标签底部过渡。
10. **运行与打包**：启动不自动连接硬件；发行包不包含个人设置、背景图片或测试 Demo。不要覆盖正在运行的程序。发布前验证原生程序和实际下载包。

终端实现不是完整 xterm 兼容层；真机串口、真实 SSH 服务及全屏应用兼容性，应在对应环境验证，不从本地测试推导所有设备都已支持。

## 6. 已完成的最近工作

v0.15.31 将原生搜索与串口脚本工作流开放给 MCP，新增可选 skill、只读地址检查、AI 活动提示、调用详情及界面同步。v0.15.30 增加收发页、小窗和 JSON/XML 的大小写、全词、正则、逻辑搜索及设置保存；支持暂停筛选和分段行判断，保留独立共享历史；修复结构化详情选择、SSH 草稿切回串口和标签重命名，并改善默认窗口高度。验证证据和发布结果见最新开发交接记录。

v0.15.29 已包含：SSH 单一终端模式及一万行历史；串口控制组/预设/自由布局/独立收发小窗；定宽波形读数和通道选择；波形/JSON/XML 本地清空；标签重命名修复与特征图标；设置/下拉圆角和更平稳的控件反馈；MCP 使用指南与 skill；自定义背景；中英双语 README 和七张实图。

基线验证已通过：14 组原生 CTest、Windows 原生终端用例、本地 SSH 测试会话、HTTP/STDIO 集成、便携 ZIP/单文件启动器，以及隔离的安装/升级/卸载检查。它们是 **v0.15.29 的历史结果**，不代表后续修改已经自动通过。

## 7. 构建与验证

以下为本机已使用的 PowerShell 命令，从仓库根目录执行。新环境先确认路径，不能照抄后声称验证完成。

```powershell
$env:QTDIR = 'D:/lib/Qt/6.11.1/mingw_64'
$env:MINGW_BIN = 'D:/lib/Qt/Tools/mingw1310_64/bin'
$env:PATH = "$env:MINGW_BIN;$env:QTDIR/bin;$env:PATH"

# 首次配置、编译、测试和部署；Ninja 路径与现有缓存保持一致
.\scripts\build.ps1 -Ninja 'E:/MinGW/w64devkit/bin/ninja.exe'

# 已配置后的增量构建与测试
cmake --build build/native-release --parallel 4
ctest --test-dir build/native-release --output-on-failure

# 只运行相关测试，例如终端
ctest --test-dir build/native-release -R terminal_keyboard_tests --output-on-failure

# 打包目录的集成测试；Python 仅供测试，不是产品运行依赖
.\.venv\Scripts\python.exe tests/integration/smoke_native.py --expected-version 0.15.32
```

Qt 最低 6.9；本机为 Qt 6.11.1 / MinGW 13.1。首次准备 Python 测试环境：

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r tests/integration/requirements.txt
```

版本只在根 CMake 项目里定义。后续升级时，测试参数使用当时实际版本，不固定照抄 `0.15.29`。测试失败请读取 `build/native-release/*-tests.txt` 和 `Testing/Temporary/LastTest.log`，保留首次失败证据，不删除断言或用重复运行掩盖问题。

| 修改范围 | 重点测试 |
| --- | --- |
| 终端 / SSH | `terminal_keyboard_tests`、`ssh_auth_tests`、`native_tests` |
| MCP / 历史 / 权限 | `mcp_protocol_tests`、`native_tests`、`smoke_native.py` |
| 波形 / JSON / 脚本 | `wave_settings_tests`、`json_tools_tests`、`frame_script_tests` |
| 串口控制 / 文件传输 | `serial_control_tests`、`serial_transfer_tests` |
| 外观 / 布局 / 背景 | `button_motion_tests`、`window_chrome_tests`、`background_tests`、`dialog_resize_tests`，加实际窗口检查 |
| 安装与发布 | `smoke_release.py`、`smoke_installer.py`；用当次打包返回的 payload/manifest/installer 路径 |

开发预览可使用独立设置和端口，避免触碰个人会话：

```powershell
.\build\native-release\bin\SerialWorkbench.exe --demo --view waveform `
  --mcp-port 0 --settings build/preview-settings.json `
  --screenshot build/preview.png --quit-after 2200
```

发布步骤见 [安装器说明](docs/windows-installer.md)。网站与国内镜像在本次交接时保持冻结；v0.15.32 只发布 GitHub，不能擅自更新服务器或镜像。

## 8. DeepSeek 开发流程

```powershell
git fetch origin
git switch deepseek-dev
git pull --ff-only origin deepseek-dev
git status
```

如果是新克隆，使用 `git switch --track origin/deepseek-dev`。若有本地未提交修改或历史分叉，先检查并保留工作，不能 `reset --hard`、`clean -fd` 或强制覆盖。

1. 读根 `AGENTS.md`、本页和交接记录，确认用户本次任务。
2. 只改任务涉及的模块；不随意替换 UI 框架、数据流或已发布协议。
3. 构建、运行相关检查；UI 改动看实际截图。明确哪些测试没运行、为什么。
4. 更新 `docs/DEVELOPMENT_LOG.md`，记录内容、验证、剩余项和风险；无变化的基线说明无需反复重写。
5. 按完整任务提交，检查 diff 后推送 `deepseek-dev`。不要把编译缓存、个人配置或凭据加入 Git。

```powershell
git diff --check
git diff --stat
# 明确列出本次应提交的路径，再执行 git add
git commit -m "Describe the completed task"
git push origin deepseek-dev
```

需要吸收新的主线时，先确认工作区干净，再 `git merge origin/main`，处理冲突并重新验证；不对已推送的共享分支强制变基。若只完成一半，交接记录写出准确停点，不能声称已经完成。

## 9. Codex 恢复后的审查与合并

模型切换由用户在相应客户端发起，本分支不轮询额度。用户让 Codex 接续时：

1. Fetch，阅读 DeepSeek 新增的交接记录，检查 `origin/main..origin/deepseek-dev` 的提交和 diff。
2. 在从 `origin/main` 建立的临时审查分支集成 `origin/deepseek-dev`，保留双方提交历史。不要直接在旧本地 `main` 上盲目合并。
3. 审查连接隔离、权限、SSH、设置迁移和 UI 行为；运行相关测试，必要时修复冲突或问题。
4. 更新交接记录，注明已集成的 DeepSeek tip、测试和后续任务；确认后以普通非强制推送更新 `main`。
5. 将已验证主线同步回 `deepseek-dev`，让下次接手从同一基线开始。若同步遇到新的提交，重新检查分叉，不能覆盖。

示意命令（审查分支名使用实际日期和唯一名称）：

```powershell
git fetch origin
git log --oneline origin/main..origin/deepseek-dev
git diff --stat origin/main...origin/deepseek-dev
git switch -c review/deepseek-YYYYMMDD origin/main
git merge --no-ff origin/deepseek-dev
# 完成代码审查、构建测试、问题修复和交接记录更新
git push origin HEAD:main
```

不要把“额度恢复”写成自动合并成功；实际审查、验证、推送完成后才报告合并。发布新版本仍按用户任务单独执行。

## 10. 可直接给 DeepSeek 的开场提示

> 在 Serial Workbench 的 deepseek-dev 分支接续开发。先阅读 AGENTS.md、DEEPSEEK.md 和 docs/DEVELOPMENT_LOG.md，确认分支与最新状态，再处理我接下来给出的具体需求。保留共享连接与独立读取、MCP 权限、SSH 单终端及滚动历史。按任务提交并推送 deepseek-dev，更新交接记录，写清真实测试结果和未完成项，方便 Codex 后续审查合并。
