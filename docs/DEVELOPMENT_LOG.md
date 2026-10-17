# 开发交接记录

每项任务完成或代理交接前，在本文件顶部新增记录。保持简洁，写事实与准确停点，不把聊天全文放进仓库。

## 2026-10-17 — MCP 搜索、脚本、可选 skill 与启动地址检查

- 执行者：Codex；先完成稳定版 v0.15.30 发布（提交 `d8d7084`，GitHub Release ID `415571124`），核对四个附件大小及 SHA-256 后正式发布；随后从保留的开发分支建立 `codex/mcp-guided-workflow`，开发版本 `0.15.31`，未创建 0.15.31 Release。
- MCP 新增 5 个工具（合计 17）：`list_search_views` / `search_logs` 通过受协议权限约束的 channel_id、当前 GUI 的 view_id，同步收发页、JSON/XML 和已打开小窗的文本、大小写、全词、正则与逻辑开关；结果来自 GUI 的有界显示缓存，保留暂停/清空/编码/HEX 语义。非活动页面可刷新自己的显示缓存，不切用户标签；分页提供 snapshot、next_offset 和截断标记，未消费其他读取者的原始游标。
- 脚本工具：`get_frame_script` 读取代码/状态/revision；`validate_frame_script` 在独立运行时离线编译和分块试解析（64 KiB、128 块、1024 帧及总耗时限制），不发送字节或修改在线状态；`set_frame_script` 明确 enabled，要求串口访问、AI 控制、当前 expected_revision，拒绝文件传输、GUI 未保存代码/启用状态及正在试解析时覆盖。成功后在线脚本、GUI 编辑器/开关/状态和设置保存同步；审计省略代码与 HEX 样本正文。
- 安装 skill：组件页增加默认不勾选的 Codex、Claude Code、OpenCode、Pi 选项；原生 `--install-skill` 使用同一份嵌入指南和脚本参考，遵循各 Agent 的环境目录覆盖。不同内容先备份，重复相同内容幂等，不改 MCP 配置；卸载应用时保留 skill。其他客户端可继续通过 read_usage_guide 获取完整正文，不猜测未知客户端的 skill 目录。
- 地址检查：启动、再次唤起、端口变化和打开设置时只读检查已检测 Agent 的用户级配置；状态栏、Agent 卡片和 get_status.agent_config_checks 显示匹配结果；手动配置/重新检测后刷新。不会自动覆盖地址，用户仍可一键更新并重载 Agent；项目级覆盖和实际客户端是否重载需在客户端核对。解析异常向 MCP 返回通用提示，避免泄露配置片段。
- 文档：软件 skill、脚本参考/AI 提示词、中英 README、Agent 接入、安装与开发交接入口同步更新。skill-creator 校验通过（项目 .venv 安装 PyYAML，仅为开发验证；以 Python UTF-8 模式运行）。
- 验证：Release 构建成功；16 组 CTest 全过，共 397 项通过、4 项可选联网/真机测试跳过；Windows 原生工作流 7 项通过并检查搜索/脚本同步及紧凑设置窗口截图。首轮旧工具数量断言 12 已更新为 17，原失败日志保留，不移除断言。
- 实际分发验证：smoke_installer.py 通过选中 Codex skill 的安装、中文路径、原目录升级、设置保留、无 SDK 启动、运行中保护、HTTP/STDIO 17 工具及原生搜索/脚本应用结果、卸载保留 skill/用户文件。测试只使用独立注册项、临时目录及隔离 CODEX_HOME，没有改真实 Agent 配置。载荷三个 EXE 与构建相同，无个人设置或 Demo EXE。
- 产物：`dist/local/mcp-workflow-0.15.31-installer/SerialWorkbench-0.15.31-windows-x64-setup.exe`（15,431,033 字节），SHA-256 `faf7e043965e34d1de5fbd648e1ea4f8ca2e81cde1d81b2754dcd4652b5ed88d`；直接测试入口 `dist/local/mcp-workflow-0.15.31/SerialWorkbench.exe`。证据在 `build/mcp-workflow-20261017/`。
- 风险与范围：未连接额外物理串口或外部 SSH；搜索仅代表 GUI 当前显示范围，更早原始历史仍用 read_events。skill 目录支持上述四种客户端，安装后需重载 Agent/MCP 工具列表。默认不选 skill、不自动修改 Agent 地址；未操作网站或国内镜像。
- 提交与同步：按用户授权完成后提交推送 deepseek-dev，保留开发分支及历史；main / v0.15.30 稳定版本保持不变。最终开发提交见交接回复。

## 2026-10-17 — 发布 0.15.30 搜索与界面修复版

- 执行者：Codex；用户要求在 MCP 扩展之前先发布当前完成版。
- 集成：发布分支 `codex/release-0.15.30` 从 `origin/main` 创建，合并已验证的 `deepseek-dev` tip `3255a1e`，保留双方提交历史及 DeepSeek 分支。版本在根 CMake 升为 `0.15.30`；双语 README 测试命令、更新记录、安装说明、交接入口与 `docs/releases/0.15.30.md` 同步更新。
- 范围：发布现有搜索开关、逻辑/正则组合、暂停筛选、JSON/XML 详情同步，以及此前的 SSH 草稿切回串口、窗口高度和标签重命名改进。MCP 原生搜索、脚本写入接口、可选 skill 安装和启动地址检查尚未实现，发布后再继续。
- 验证：原生 Release 构建成功，最终 CTest 15/15，通过 390 项、4 项可选联网/真机测试跳过；Windows 原生搜索 28 项通过并留存实际窗口截图。未进行额外物理串口或外部 SSH 联调。
- 分发：`dist/local/0.15.30/` 包含 setup 安装包、单文件 EXE、便携 ZIP 和 `SHA256SUMS.txt`。`smoke_installer.py` 全部通过（中文路径、原目录升级、设置保留、运行中保护、无 SDK 启动、HTTP/STDIO、卸载保留用户文件）；`smoke_release.py` 全部通过（干净 ZIP、完整载荷校验、独立运行、单文件首次/缓存启动、Unicode 路径与损坏缓存修复）。
- 安装包 SHA-256：`f978a5f95c22e0286494b04b0e7872c4a75755327a4db436e171cd9b34e3a199`。载荷无个人设置或 Demo EXE，源码构建与载荷主程序相同。证据在 `build/release-0.15.30-verification/`；`ctest/current-suites.json` 列出本次实际 15 组结果，其他历史日志不计入本次结果。
- 发布与同步：本记录随发布提交；以普通推送同步 `main` / `deepseek-dev`，创建 `v0.15.30` 标签，将四个附件上传到 GitHub Release 并核对远程摘要。最终提交、发布链接与远程核验结果见本次交接回复。网站和国内镜像不更改。
- 下一步：保留 `deepseek-dev` 供后续开发；在稳定版本发布完成后处理用户的 MCP / skill / 脚本 / 地址一致性需求。

## 2026-10-17 — 搜索逻辑、正则与匹配开关

- 执行者：Codex；开发分支 `deepseek-dev`，起点 `ce4b1a3`。应用版本保持 `0.15.29`，本次提供本地测试安装包。
- 用户目标：搜索支持 `||`、`&&`、正则、大小写等开关，采用紧凑的 Aa / ab / .* 风格。
- 已完成：新增统一的 `SearchFilter` / `FilterBox`，收发日志、JSON/XML 记录和串口收发小窗均支持区分大小写、全词、正则与逻辑匹配，默认全部关闭。逻辑模式中 `&&` 优先于 `||`；双引号保护包含运算符的搜索项，正则与逻辑可组合。无效表达式使用警示边框与悬停说明，保留记录显示。
- 行为与兼容：查询及开关随原有页面/小窗设置保存，旧设置默认普通文本匹配。日志在现有显示块上重新筛选，暂停时也能使用；部分数据分多次到达时会重新判断整行。不会重置终端、重置独立读取游标、清空共享历史或发送设备数据。SSH 保持原有交互终端，不加入日志搜索栏。
- 联动修复：JSON/XML 不再选中被筛选隐藏的新记录；当前记录被隐藏时切换到可见记录，无匹配则清空详情，避免列表与详情不一致。中英 README 同步说明用法。
- 构建与验证：Release 构建成功，最终完整 CTest 15/15 通过；Windows 原生搜索测试 28 项通过，覆盖逻辑优先级、中文、正则与反向引用、全词边界、错误输入、分段收包、暂停筛选、独立历史、实际点击与关闭重开持久化、结构化记录和小窗一致性。浅色、深色、1000px 窄窗口、错误状态、JSON 详情和 430px 小窗均检查实际截图。
- 本地证据：`build/search-filter-20261017/` 保留首次夹具失败、首次截图用例失败和修正后的结果；截图在其 `captures/` 子目录。未连接真实硬件或外部 SSH 主机。
- 安装包：`dist/local/search-filter-20261017-installer/SerialWorkbench-0.15.29-windows-x64-setup.exe`（15,404,993 字节），SHA-256 为 `07344c393698902234e0f1131fc385db4f707caebdf03740afca9a0e0e3da06f`；直接测试入口 `dist/local/search-filter-20261017/SerialWorkbench.exe`，需保留整个运行库目录。
- 安装验证：最终 `smoke_installer.py` 完整通过：中文目录安装、发现原路径升级、保留设置/背景/用户文件、运行中保护、无 SDK 启动、HTTP / STDIO 与权限及共享读取、卸载清理；不覆盖正式安装。源码构建、部署目录和最终安装载荷的主程序 SHA-256 一致，载荷无个人设置和 Demo EXE。
- 提交与同步：本次代码、双语文档、回归测试和记录提交推送 `deepseek-dev`；不删除分支、不强推，不创建 GitHub Release，不修改网站或国内镜像。最终提交见交接回复。

## 2026-10-17 — 合并版本地安装包与测试程序

- 执行者：Codex；应用源码基线 `af906ff`，版本保持 `0.15.29`。
- 用户目标：提供刚合并版本的安装包或测试 EXE。
- 产物：`dist/local/deepseek-review-20261017-installer/SerialWorkbench-0.15.29-windows-x64-setup.exe`（15,389,619 字节）；直接测试入口为 `dist/local/deepseek-review-20261017/SerialWorkbench.exe`，需保留同目录运行库。
- 安装包 SHA-256：`5dab43bb1def7cf27af462e022749f3bdaaaf33a07cc676f2702d8e830c3b16d`。
- 验证：`smoke_installer.py` 完整通过，涵盖中文自定义路径安装、升级发现原目录、设置和背景保留、运行中保护、无 Qt SDK 启动、HTTP / STDIO 与共享独立读取、卸载清理及用户文件保留。测试使用独立产品名和注册表项，没有覆盖正式安装。首次沙箱内运行在注册卸载入口时返回 5，允许隔离测试所需的系统写入后完整通过；日志位于 `build/deepseek-review-20261017/installer-verification-unsandboxed.txt`。
- 提交与同步：仅交接记录随 `deepseek-dev` 提交推送；没有改应用源码，没有创建新 Release 或修改网站、国内镜像。
- 剩余工作：用户可安装或直接启动测试；物理设备验证范围与上条审查记录相同。

## 2026-10-17 — Codex 审查并集成 deepseek-dev

- 执行者：Codex。
- 审查范围：`origin/main` 的 `43f3dc4` 到 `origin/deepseek-dev` 的 `687d06d`，共 6 个提交。审查分支为 `codex/review-deepseek-20261017`，以 `--no-ff` 生成合并提交 `18193e9`，保留 DeepSeek 的全部提交历史和开发分支。
- 已审查：SSH 强制终端与串口显式终端偏好的区分、协议切换与恢复、默认窗口高度与已有窗口几何恢复，以及标签重命名的字体、图标和关闭按钮边界。未发现阻塞合并的业务问题，保留本次 DeepSeek 的业务实现；共享记录、独立读取游标和 MCP 权限代码未改动。
- 补充回归：真实协议选择器从保存过 SSH 状态的草稿切回串口后恢复收发日志；已连接串口恢复显式终端偏好时，历史记录不被消费，也不产生额外发送。修正 `DEEPSEEK.md` 中的源码路径拼写。
- 测试稳定性：按钮、复选框和弹窗动画的结束检查改为最多等待 1000ms 的明确结束状态，保留精确的 0/1 终值与中间帧断言。原固定等待曾分别捕获 `0.9999976296` 和 `0.000421875` 的未结束帧，失败日志保存在忽略目录 `build/deepseek-review-20261017/`。
- 构建与测试：原生 Release 构建成功；最终完整顺序 CTest 为 14/14 通过，共 362 项通过、4 项可选联网或真机测试跳过。Windows 原生终端整套复核 26 项通过；首次系统剪贴板占用（`OleSetClipboard 0x800401d0`）的失败日志保留，独立完整重跑通过。
- 实际界面：重命名的 F2、右键入口及图标边界检查通过，并检查 100%、125%、150% 缩放截图；默认串口窗口可完整显示参数栏，SSH 窗口保持交互终端且没有串口发送栏。
- 包装验证：部署到隔离目录 `dist/local/deepseek-review-20261017/`，在 PATH 不含 Qt SDK 的环境运行 `smoke_native.py --expected-version 0.15.29`，HTTP / STDIO、独立共享读取、写权限、波形、重连与参数校验通过。该目录仅用于验证。
- 剩余限制：本轮未进行物理串口设备和外部真实 SSH 主机联调；没有阻塞集成的已知问题。
- 提交与同步：本记录随审查补充提交；随后使用普通推送集成 `main`，再将结果快进同步到 `deepseek-dev`。不删除分支、不变基、不强推；最终提交与远程同步结果见本次交接回复。
- 发布范围：版本仍为 `0.15.29`，没有创建新 Release，也没有修改网站或国内更新镜像。
- 下一步：DeepSeek 继续使用保留的 `deepseek-dev`；开始开发前先获取远程状态并检查工作区。

## 2026-10-16 — 标签重命名下划线紧贴文字

- 执行者：DeepSeek。
- 分支与起点：`deepseek-dev`，起点 `b9323a8`。
- 用户目标：重命名时的下划线与文字仍有间距，要求贴近文字。
- 已完成：`src/ui/window.cpp` 的 `beginTabRename()` 不再按标签框上下内缩，改为按字体高度以文字垂直中心定位：`top = center - textHeight/2 - 3`、`bottom = center + textHeight/2 + 2`，下划线落在文字下方约 2px。左右边界、下划线样式不变。
- 验证：构建成功；`ctest` 14/14 全过；截图 `build/rename-captures/rename-{xml,json,waveform}.png` 显示下划线紧贴文字，中文与拉丁标题均无残影，图标与 ✕ 不受影响。
- 未完成：无。
- 风险与兼容：仅编辑器垂直定位；仍以字体度量对齐，标签字体变化时自适应。
- 提交与同步：见交接回复中的提交哈希；推送 `deepseek-dev`，未推送 `main`。
- 下一步：Codex 审查并集成。

## 2026-10-16 — 标签重命名改为文字下划线样式

- 执行者：DeepSeek。
- 分支与起点：`deepseek-dev`，起点 `b80b831`。
- 用户目标：重命名输入框改为更矮的观感，或直接改成文字下方的横线。
- 已完成：`src/ui/controls.cpp` 的 `#tabTitleEditor` 样式由圆角方框改为仅底部下划线（`background: $panel` 与选中标签同色、`border: none; border-bottom: 1px solid $edge; border-radius: 0`）；`src/ui/window.cpp` 的 `beginTabRename()` 垂直内缩 `+6/-6`。下划线宽度仍止于关闭按钮左侧，标签图标不受影响。
- 验证：构建成功；`ctest` 14/14 全过；`instrumentRenameLeavesIconsVisible`、`compactSerialFieldsAndIntegratedTabs`（断言编辑器底部像素非强调色）仍通过。截图 `build/rename-captures/rename-{xml,json,waveform}.png` 显示为文字下横线，无方框、无残影，图标与 ✕ 清晰。
- 未完成：无。
- 风险与兼容：仅编辑器观感与 2px 垂直内缩；背景沿用 `$panel` 与选中标签同色，避免原标签文字透出；重命名逻辑、触发方式与标签自适应未改动。
- 提交与同步：见交接回复中的提交哈希；推送 `deepseek-dev`，未推送 `main`。
- 下一步：Codex 审查并集成。

## 2026-10-16 — 标签重命名输入框放大且不挤出图标

- 执行者：DeepSeek。
- 分支与起点：`deepseek-dev`，起点 `7c939ad`。
- 用户目标：标签内联重命名的输入框太小，标题文字被裁切；要求框大一点，但不能挤压左侧标签图标与右侧取消（✕）图标。
- 已完成：`src/ui/window.cpp` 的 `Window::beginTabRename()` 调整编辑器几何：左边界仍取样式文本区（保持与标签图标间距），右边界扩展到关闭按钮左侧 4px，垂直内缩由 `+8/-7` 收窄为 `+4/-4`，让标题完整显示。
- 验证：构建成功；`ctest` 14/14 全过；`instrumentRenameLeavesIconsVisible` 仍断言编辑器左边界不越过图标区、右边界不越过关闭按钮（16/20px 图标、json/xml/waveform 均通过）。实际截图 `build/rename-captures/rename-xml.png`、`rename-json.png` 显示标题完整、图标与 ✕ 不受影响。
- 未完成：无。
- 风险与兼容：仅影响重命名输入框的几何；重命名触发方式（右键/F2）、取消与确认逻辑、标签宽度自适应均未改动。
- 提交与同步：见交接回复中的提交哈希；推送 `deepseek-dev`，未推送 `main`。
- 下一步：Codex 审查并集成。

## 2026-10-16 — 默认窗口高度略增，消除左侧参数面板滚动条

- 执行者：DeepSeek。
- 分支与起点：`deepseek-dev`，起点 `5ba3cce`。
- 用户目标：默认打开的窗口偏小，导致左侧“连接参数”面板出现上下滚动条；要求纵向稍微加高。
- 已完成：`src/ui/window.cpp` 默认窗口高度由固定 `900` 改为 `960`，并以主屏可用工作区高度封顶（`qMin(960, availableGeometry().height())`），避免在矮屏上开出屏幕外；新增 `<QGuiApplication>`、`<QScreen>` 头文件。
- 验证：`cmake --build build/native-release --parallel 4` 成功；`ctest --test-dir build/native-release --output-on-failure` 14/14 通过。实际窗口截图（Qt 6.11.1 / MinGW 13.1）：串口与 SSH 两种参数面板在 960 高下右侧边缘均无滚动条滑块；对照 900 高旧截图的同区域放大图可见滑块，确认修好。
- 未完成：无。
- 风险与兼容：仅改默认窗口尺寸，不影响已保存的窗口/设置；宽度仍为 1400。屏幕可用高度不足 960 时窗口按屏幕高度开满，此时极矮屏仍可能出现滚动条（受物理高度限制）。
- 提交与同步：见交接回复中的提交哈希；推送 `deepseek-dev`，未推送 `main`。
- 下一步：Codex 审查并集成。

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
