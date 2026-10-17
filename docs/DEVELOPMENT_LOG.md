# 开发交接记录

每项任务完成或代理交接前，在本文件顶部新增记录。保持简洁，写事实与准确停点，不把聊天全文放进仓库。

## 2026-10-17 — 发送区底部按实际宽度换行，保持发送框比例

- 执行者：DeepSeek。
- 分支与起点：`deepseek-dev`，起点 `fcfae6f`。
- 用户目标：当前比例下，编码/CRLF/0x 为了保持距离把上面的发送框压矮；希望保持比例而不是固定距离。
- 根因：`SendFooter` 固定用 730px 阈值切换两行；发送区约 630px 时把编码/CRLF/0x 与操作按钮拆成两行，挤压发送框；编码/CRLF 还是固定宽度 108/152。
- 已完成：`src/ui/window.cpp` 的 `SendFooter` 改为按各子项实际最小宽度计算一行阈值，只有真的放不下才换行；编码/CRLF 从固定宽度改为最小宽度 84/116，空间紧时可收缩；footer 增加 `sendFooter` 对象名。新增 `sendFooterKeepsPayloadProportion` 测试（1416×968、参数栏 700）：底部一行高度 <60，发送框高度 ≥80。
- 验证：构建成功；完整 `ctest` 17/17 通过；截图 `build/sender-captures/send-footer-one-row.png` 显示编码/CRLF/0x/模板/循环/间隔/发送在同一行，发送框明显变高。
- 未完成：无。
- 风险与兼容：极窄宽度下仍会换行；不改变发送内容、编码、换行或循环发送行为。只影响发送区布局。
- 提交与同步：见交接回复中的提交哈希；推送 `deepseek-dev`，未推送 `main`。
- 下一步：Codex 审查并集成。

## 2026-10-17 — 修正拉宽参数栏后的分界线与拖拽柄重叠

- 执行者：DeepSeek。
- 分支与起点：`deepseek-dev`，起点 `957b85c`。
- 用户目标：拉大左侧参数区后边界不清楚，甚至感觉能拖动“校验”等控件。
- 根因：参数栏最大宽度按固定 320px 预留右侧工作区，但右侧实际最小宽度约 655px；拖大后布局压缩滚动区，右侧内边距被裁掉，拖拽柄压到表单控件右缘。
- 已完成：
  - `src/ui/window.cpp`：`resizeParameters()` 保存用户请求宽度（220..4096），`arrangeParameters()` 按右侧工作区 `minimumSizeHint()`（至少 320）限制**可视**宽度；每次都将拖拽柄几何对齐到滚动区右缘；参数内容右边距 16→24；拖拽柄占位 8→10。
  - `src/ui/parameter_grip.cpp`：宽度 10；分界线画在柄的左缘并加深（120/200），三个圆点放在工作区一侧。
  - `tests/native/tests.cpp`：新增 `parameterSidebarKeepsControlsClearOfGrip`，在 220/400/692/700/900/1034 宽度下断言控件与拖拽柄至少 20px 间隔、柄宽 10 且不越入滚动区。
- 验证：构建成功；完整 `ctest` 17/17 通过；截图 `build/sidebar-captures/sidebar-wide-boundary.png`（700px 参数栏，24px 内缩 + 清晰分界线）；实窗 UI 自动化得到控件最右 944、拖拽柄 x=968/宽 10，间隔 24px 且无重叠。
- 未完成：无。
- 风险与兼容：窄窗口下可视宽度会回落到 220px，但保存的偏好保留，窗口变宽后恢复；右侧工作区实际最小宽度会限制参数栏最大可视宽度。只影响布局与拖拽柄，不改连接、设置格式或数据。
- 提交与同步：见交接回复中的提交哈希；推送 `deepseek-dev`，未推送 `main`。
- 下一步：Codex 审查并集成。

## 2026-10-17 — 新建收发页面菜单图标改为收发日志页

- 执行者：DeepSeek。
- 分支与起点：`deepseek-dev`，起点 `7724d6e`。
- 用户目标：加号图标过于简单，要求换成更贴切的图标。
- 已完成：`src/ui/controls.cpp` 新增 `receive` 图标（页面轮廓 + 日志行 + 双向箭头，表示 TX/RX）；`src/ui/window.cpp` 的“新建收发页面”动作改用该图标，不再使用 `plus`；控件页面仍为 `widgets`。
- 验证：构建成功；完整 `ctest` 17/17 通过；实际窗口截图 `build/new-page-menu2.png` 显示菜单项为“页面 + 日志行 + 双向箭头”。
- 未完成：无。
- 风险与兼容：纯图标改动；动作、对象名、快捷键和新建页面行为不变。
- 提交与同步：见交接回复中的提交哈希；推送 `deepseek-dev`，未推送 `main`。
- 下一步：Codex 审查并集成。

## 2026-10-17 — 新建收发页面菜单图标改为加号

- 执行者：DeepSeek。
- 分支与起点：`deepseek-dev`，起点 `539d2a0`。
- 用户目标：新建收发页面菜单项前的闪电图标不贴切，要求更换。
- 已完成：`src/ui/window.cpp` 的 `newPageMenu()` 将“新建收发页面”动作图标由 `lightning` 改为 `plus`；控件页面仍使用 `widgets`。仅图标变化，动作、快捷键和对象名不变。
- 验证：构建成功；完整 `ctest` 17/17 通过；实际窗口截图确认菜单项显示加号。
- 未完成：无。
- 风险与兼容：纯视觉改动，不影响新建页面行为或标签状态。
- 提交与同步：见交接回复中的提交哈希；推送 `deepseek-dev`，未推送 `main`。
- 下一步：Codex 审查并集成。

## 2026-10-17 — 关于页版本信息增加来源链接

- 执行者：DeepSeek。
- 分支与起点：`deepseek-dev`，起点 `02d94dc`。
- 用户目标：在设置里的版本信息旁增加来源：GitHub 仓库和 `https://162.14.72.194/`。
- 已完成：`src/ui/window.cpp` 关于页在版本行下新增 `aboutSources` 富文本链接，使用主题强调色，点击由系统浏览器打开：
  - `GitHub 仓库` → `https://github.com/lilinng-163/serial-workbench`
  - `项目主页 / 镜像站` → `https://162.14.72.194/`
- 验证：构建成功；完整 `ctest` 17/17 通过；`settingsResizeAndRestore` 新增断言检查两个 URL 与 `openExternalLinks`；截图 `build/about-captures/settings-about.png` 显示版本下方两个可点击链接。
- 未完成：无。
- 风险与兼容：仅关于页展示与外部跳转，不改更新服务（仍使用 GitHub API 与 `/updates/latest.json` 镜像清单）。
- 提交与同步：见交接回复中的提交哈希；推送 `deepseek-dev`，未推送 `main`。
- 下一步：Codex 审查并集成。

## 2026-10-17 — 数据字体对话框默认尺寸加大

- 执行者：DeepSeek。
- 分支与起点：`deepseek-dev`，起点 `7c1d209`（v0.15.32）。
- 用户目标：每次打开“数据字体”对话框都太小，“效果”一栏被裁切，需要加大。
- 已完成：`src/ui/window.cpp` 中数据字体的 `QFontDialog` 增加 `setMinimumSize(560, 560)` 和 `resize(620, 620)`，确保主题样式下“效果 / 示例 / 文字系统”完整显示；仅影响该对话框，其他字体/颜色选择器不变。
- 验证：构建成功；完整 `ctest` 17/17 通过；新增 `dialog_resize_tests::dataFontDialogOpensWithEffectsVisible`，断言对话框宽高 ≥560，并确认含两个复选框的“效果”组完整位于对话框内容区；实际截图 `build/font-dialog-captures/data-font-dialog.png` 显示“删除线 / 下划线”未被遮挡。
- 未完成：无。
- 风险与兼容：对话框最小 560×560，极矮屏幕可能需要滚动/调整；不影响字体值、保存格式或默认字体。
- 提交与同步：见交接回复中的提交哈希；推送 `deepseek-dev`，未推送 `main`。
- 下一步：Codex 审查并集成。

## 2026-10-17 — 发布 0.15.32 并交接 DeepSeek

- 用户目标：先发布正式 Release，再切回 deepseek-dev 接续开发；保留开发分支，网站和国内镜像继续冻结。
- 集成：从 origin/main 的 `571ecc9` 建立 `codex/release-0.15.32`，以非快进合并保留 origin/deepseek-dev 的 `a400f7b` 及其四个开发提交历史；合并提交 `d731e21`。复核脚本样本的权限、revision 与未保存编辑保护，以及发行改动范围；本轮仅补充发行文档。
- 范围：版本回溯和状态栏版本；设置/安装器的 Agent skill 配置；GUI/MCP 试解析样本同步；系统文件选择；控件组下拉、删除和宽度跟随；清空、图层和主题滑块。更新 CHANGELOG、中英 README、交接基线与 `docs/releases/0.15.32.md`。
- 构建与测试：Release 构建成功，17/17 CTest，413 项通过、5 项可选联网/真机测试跳过。Windows 原生 MCP 工作流 11 项、历史设置 3 项、控件布局 3 项通过，检查本次构建的实际窗口截图。应用源码与已验证开发版一致。
- 安装验证：smoke_installer.py 通过 Agent skill 预选、中文自定义路径、旧版本注册后原路径升级、配置/用户文件保留、无 SDK 启动、运行中保护、HTTP/STDIO 17 工具与搜索/脚本同步及卸载。隔离环境测试，没有修改真实 Agent 配置或用户安装。
- 分发验证：smoke_release.py 通过便携 ZIP 的完整清单和无 SDK 运行，单文件 EXE 的中文路径、首次启动、缓存复用、CLI 转发、共享 MCP 和损坏缓存修复。三个原生 EXE 与构建哈希一致，payload 不含个人设置或 Demo；未进行物理串口或外部 SSH 联调。
- 产物：`dist/local/0.15.32/` 含安装包、单文件 EXE、便携 ZIP、SHA256SUMS.txt。安装包 15,464,069 字节，SHA-256 `499fcc9217c1d711a363266cf213a5f9af056f250284cb255e27ee1706f313cc`。证据在 `build/release-0.15.32-verification/`，payload 在 `build/release-0.15.32-a283307f/SerialWorkbench/`。
- 发布与交接：发行提交后普通推送 main、deepseek-dev 和 v0.15.32 标签；GitHub Release 上传四项产物并核对远端大小/哈希后公开，最终提交和发布结果见交接回复。保留 deepseek-dev 并回到该分支，不启动或切换其他模型，不更新网站、服务器或国内镜像。

## 2026-10-17 — 图层按钮移除菜单小箭头

- 用户目标：去掉「图层」文字右下角的小箭头，保留点击展开菜单。
- 分支与起点：从 deepseek-dev 的 `14952f6` 建立 `codex/layer-menu-label`，版本仍为 0.15.32。
- 改动：仅为 `QToolButton#controlLayerMenu::menu-indicator` 隐藏图像并将尺寸置零，保留菜单和上下图层操作。
- 验证：Release 构建成功，serial_control_tests 与 button_motion_tests 2/2 通过；Windows 原生控件回归 4 项通过，检查 PID 卡片截图。实际打包程序在不含 Qt/MinGW SDK 的 PATH 下启动并生成串口控制截图，确认箭头消失；三个打包 EXE 与构建哈希一致，没有覆盖用户运行中的 `E:/SerialWorkbench/SerialWorkbench.exe`。本次仅改显示样式，未重复运行完整安装升级测试。
- 产物：`dist/local/layer-label-0.15.32-installer/SerialWorkbench-0.15.32-windows-x64-setup.exe`，15,467,589 字节，SHA-256 `d7888424ffd666defa95151e8d6aaa1fa773f9f9a4a0ea814f90864b803e1df3`。证据在 `build/layer-label-20261017/`，payload 在 `build/release-0.15.32-e5104a03/SerialWorkbench/`。
- 交接：推送并保留 deepseek-dev，main、正式 Release、网站和国内镜像不变。

## 2026-10-17 — 版本回溯、底部版本与控件操作整理

- 用户目标：设置增加最近三个版本的特性和安装包；窗口底部显示当前版本；串口控制只保留「清空全部」，省略号菜单换成明确入口，背景设置滑块统一主题。
- 分支与起点：从 deepseek-dev 的 `554da02` 建立 `codex/release-history`；开发版本仍为 0.15.32，正式 Release 仍为 v0.15.31。
- 版本回溯：设置新增独立页面，按版本号倒序展示最近三个有完整安装包、大小和 SHA-256 的正式 Release，排除草稿/预发布和重复版本。内置从 GitHub 核对的 0.15.31、0.15.30、0.15.29 真实发布记录供离线查看，首次打开和手动刷新读取 GitHub；失败保留现有列表，未来构建后发布的新版本需联网刷新。
- 下载与安装：复用 UpdateService 的后台下载、取消、大小和 SHA-256 校验；历史页拥有独立服务状态，不覆盖自动更新的最新版本。下载和刷新互斥，下载时固定所选版本；校验完成后由用户点击关闭工作台并安装，不自动降级。安装入口与原更新窗口共用，设置页关闭后下载继续。
- 界面：说明从开头展示并保留阅读位置，支持浅色、深色和 440×320 设置窗口滚动访问。各工作台窗口的状态栏最右侧预留当前编译版本，临时消息不覆盖它；版本号不随历史版本选择变化。
- 串口控制：移除管理组里的清空入口，清空 API 只处理本页面全部组中的控件；保留确认、默认取消、组/连接/历史保留及取消待发送命令。卡片菜单由「⋯」改为「图层」，明确提供上下层级操作。
- 滑块：原先仅 controlSlider 匹配主题，现统一所有水平滑块的圆角轨道、主题进度色、圆形手柄及悬停/按下/禁用状态，背景设置和串口控件共用；不改变拖动数值与发送逻辑。中英文 README、版本回溯与串口控制文档同步。
- 验证：Release 构建成功，17/17 CTest，413 项通过、5 项可选联网/真机测试跳过；新增历史测试覆盖排序筛选、离线、刷新失败/重试、下载校验、损坏/取消、显式安装请求及小窗口。另单独启用真实联网测试，从 GitHub 刷新并下载最旧的所列安装包，通过大小与 SHA-256 校验，3 项通过且没有启动安装器。Windows 原生历史工作流 8 项通过、1 项联网用例默认跳过（已另行运行），原生清空/控件回归 4 项通过；检查历史页、浅深色滑块、PID 图层入口和底部版本截图。
- 安装验证：首轮测试编译升级包时遇到刚运行过的基线 EXE 占用，改为基线/升级使用不同文件名；按精确路径确认并清理失败测试的专用注册项和快捷方式。修正测试帮助函数的首组数量预期后相关回归通过。最终 smoke_installer.py 通过 Agent skill 预选、中文目录、原路径升级与配置保留、无 SDK 启动、运行中保护、HTTP/STDIO 17 工具、搜索/脚本同步及卸载，没有改动正式安装或真实 Agent 配置。
- 产物：`dist/local/version-history-0.15.32-installer/SerialWorkbench-0.15.32-windows-x64-setup.exe`，15,474,409 字节，SHA-256 `c8b6c312ddebfe4e78141ed8160d75367347f8628bca3045b29276799b14bcfd`；三个 EXE 与构建哈希一致，不含个人设置或 Demo。证据在 `build/release-history-20261017/`，payload 在 `build/release-0.15.32-af19653e/SerialWorkbench/`。
- 交接：普通推送并保留 deepseek-dev；不合入 main、不创建正式 Release、不修改网站或国内镜像。没有真实硬件串口或外部 SSH 联调；本轮真实联网仅读取 GitHub 并下载校验历史安装包，未执行历史安装器。

## 2026-10-17 — 原生文件选择与串口控件组交互修复

- 用户目标：文件选择使用 Windows 系统窗口；修复控件组下拉条目显示不稳定、切换难以操作、删除确认按钮被遮挡，以及自由布局在窗口拉宽后不跟随的问题。
- 分支与起点：从 deepseek-dev 的 `df54a26` 建立 `codex/native-file-dialogs`；开发版本仍为 0.15.32，正式主线和 Release 仍为 v0.15.31。
- 文件窗口：移除全局禁用原生对话框的设置，QFileDialog 不再参与自定义标题栏和背景处理；Windows 使用系统文件选择器。已有字体、颜色窗口继续显式使用应用内界面。
- 控件组：使用已有 Combo 的菜单式下拉，显示全部组名和当前选中项；先关闭菜单并释放鼠标，再按组 ID 重建控件，避免在下拉 activated 调用栈内清空列表。重复添加的 PID 预设仍各自保留并自动编号，不删除用户控件组。
- 布局：按稳定基准宽度缩放自由布局卡片的横向位置和宽度，保留纵向位置、高度、最小尺寸、拖动缩放和图层。视口完成尺寸更新后合并重排，避免使用旧视口宽度；重新恢复设置时先保留已保存的像素位置。布局更新不发送串口数据。
- 删除：控件组确认改为有明确最小尺寸、可换行说明和中文删除/取消按钮的对话框，默认取消；取消不改动组或控件。中英文 README 及串口控制文档同步说明窗口宽度跟随行为。
- 验证：原生 Release 构建成功，16/16 CTest，405 项通过、4 项可选联网/真机测试跳过。新增用例先验证旧实现的异常，再验证连续 12 次切换四组、所有条目完整可见、删除取消/确认和窗口变宽/还原且无发送。Windows 原生控件回归 7 项通过，标题栏/系统文件选择回归 5 项通过，最终截图用例 4 项通过；检查菜单、正常/宽布局、删除框和 Explorer 文件窗口截图。
- 安装验证：隔离环境中 smoke_installer.py 通过已检测 Agent skill 预选、中文路径安装、原路径升级和配置保留、无 SDK 启动、运行中保护、HTTP/STDIO 17 工具、搜索/脚本同步及卸载。三个打包 EXE 与本次构建哈希一致，不含个人设置或 Demo。
- 测试安装包：`dist/local/control-layout-0.15.32-installer/SerialWorkbench-0.15.32-windows-x64-setup.exe`，15,461,998 字节，SHA-256 `36940b9870cc4a87e3085b68564121635365c13d46f680c27c4dd8d4d867b234`。证据在 `build/control-layout-fixes-20261017/`，payload 在 `build/release-0.15.32-13450ac1/SerialWorkbench/`。
- 交接：普通推送 deepseek-dev 并保留该分支；不合入 main、不创建正式 Release、不修改网站或国内镜像。没有关闭或覆盖用户安装的工作台，没有真实物理串口或外部 SSH 联调；对随机卡住仅验证本次涉及的下拉/切换/确认窗口路径，若仍发生需按具体操作继续定位。

## 2026-10-17 — skill 配置、安装预选与脚本试解析同步

- 用户目标：删除脚本 AI 提示词入口，skill 要求修改脚本时同步试解析样本；区分新建控件菜单与串口控制图标；设置提供 skill 配置，安装器默认勾选本机已有的 Agent。
- 分支与起点：从 `571ecc9` 建立 `codex/skill-settings-and-parser-samples`，开发版本 0.15.32；正式 Release 仍为 v0.15.31。
- 脚本：移除提示词按钮、弹窗和无调用的生成函数；GUI 保存并恢复试解析 HEX。get_frame_script 返回 sample_chunks；set_frame_script 可同步保存样本，最多 64 块、合计 64 KiB，省略保留、空数组清空；revision 包含已保存样本，未保存的 HEX 编辑和正在试解析均拒绝覆盖。脚本、样本和启用状态同步到已有编辑器，保持原有协议/写权限与独立读取，不向设备发送试解析数据。
- skill：指南明确每次写/改脚本都要更新匹配协议的试解析样本，覆盖有效、半帧、连帧、噪声和错误帧，先离线验证再按授权部署。skill-creator 校验通过。中英 README、脚本和 Agent/安装文档同步更新。
- 配置：设置 → AI / MCP → Agent 接入 → 配置 skill，显示目标目录和内容状态，默认勾选已检测 Agent，可手动增选；按需安装/更新、保留备份及同内容幂等，不修改 MCP 配置。扩展至 Codex、Claude Code、OpenCode、Pi、Hermes、OpenClaw、Grok Build、Gemini CLI 八种已确认的本地目录；Claude Desktop 在客户端导入或通过 MCP 读取指南，不冒充 Claude Code 目录。
- 安装器：组件页前使用临时原生程序共用 Agent 检测，默认勾选检测到的八种受支持 Agent，允许取消，未检测到的可手动选择；检测不打开工作台或写用户配置。安装错误提示指向设置中的重试入口。
- 图标：新建控件页面保留网格，串口控制标签及子菜单改用滑杆图标，原生窗口和图标 alpha 差异检查通过。
- 验证：Release 构建成功，16/16 CTest，共 401 项通过、4 项可选联网/真机测试跳过；Windows 原生工作流 11 项通过，检查设置的正常/紧凑/深色、脚本窗口及两种图标截图。首轮指南 CRLF 导致示例提取失败，恢复指南 LF 后通过；失败日志保存在忽略目录。
- 安装验证：smoke_installer.py 通过隔离用户环境中模拟 Codex/Pi 的自动勾选与 skill 安装、中文安装路径、原路径升级、设置与用户文件保留、无 SDK 启动、运行中保护、HTTP/STDIO 17 工具及新增样本同步、卸载检查。没有更改真实 Agent 配置或正在运行的 `E:/SerialWorkbench/SerialWorkbench.exe`。
- 产物：`dist/local/skill-settings-0.15.32-installer/SerialWorkbench-0.15.32-windows-x64-setup.exe`，15,453,994 字节；SHA-256 `ef9f548541691212c33b3410ba364194411f75755c755cd406973849df28f911`。安装包三个原生 EXE 与本次构建一致，不含个人设置或测试 Demo。证据与 package.json 在 `build/skill-settings-20261017/`，payload 在 `build/release-0.15.32-a4af6294/SerialWorkbench/`。
- 交接：提交并推送 deepseek-dev，保留该分支；不合入 main，不创建 GitHub Release，不修改网站或国内镜像。本轮没有真实物理串口或外部 SSH 主机联调；安装后重载 Agent 的 MCP 连接以获取更新的参数说明。

## 2026-10-17 — Codex 审查 DeepSeek 并发布 0.15.31

- 用户目标：查阅并合并 `deepseek-dev`，随后发布正式 Release；保留开发分支，网站和国内镜像继续冻结。
- 集成范围：从 `origin/main=d8d7084` 创建独立审查分支 `codex/review-release-0.15.31`，非快进合并 `origin/deepseek-dev=e95c1bf`；保留 `e1f402b` 的 MCP 搜索/脚本/可选 skill 工作，以及 DeepSeek 的 `877a437`、`1e1402d`、`e95c1bf` 三个提交历史。
- 审查：按 codebase-memory 查询入口和调用链，针对索引标记的解析与时效缺口回读源码；检查权限、独立读取、脚本 revision/未保存编辑保护、非模态脚本窗口、AI 提示及审计展示。发现调用日志刷新会清除选中项，补充按调用序号恢复选中项和回看锚点；最旧记录淘汰后仍保留存续锚点，回到底部后恢复自动跟随。
- 文档：补齐中英文 README 的新工具表格，移除“脚本只能在 GUI 编辑”的过时说明；更新交接入口、更新记录和 `docs/releases/0.15.31.md`。
- 构建与测试：原生 Release 构建成功；16/16 CTest 通过，共 399 项通过、4 项可选联网/真机测试跳过。Windows 原生工作流 9 项通过，检查 AI 提示、脚本编辑器和调用详情截图。新增回看测试先复现选中项丢失，再验证修复、旧行淘汰和恢复跟随；首轮新增测试的临时 JSON 引用错误已修正，相关失败日志保留。
- 分发验证：`smoke_installer.py` 通过可选 Codex skill、中文目录安装、原路径升级、设置/用户文件保留、无 SDK 启动、运行中保护、HTTP/STDIO 17 工具和卸载检查；使用隔离配置与独立测试注册项，没有修改真实 Agent 配置。`smoke_release.py` 通过便携 ZIP、单文件首次启动/缓存复用/损坏缓存修复及 HTTP/STDIO 工作流。
- 产物：`dist/local/0.15.31/` 包含安装包、单文件 EXE、便携 ZIP 和 `SHA256SUMS.txt`；三个原生 EXE 与本次构建一致，payload 不含个人设置或测试 Demo。打包及验证记录在忽略目录 `build/release-0.15.31-verification/`，payload 为 `build/release-0.15.31-be0faecd/SerialWorkbench/`。
- 安装包 SHA-256：`1abc1b811e5339ddc2d6adf469228fad6859bc699059f39949d4bcb3928f6353`；大小 15,439,442 字节。
- 提交与发布：本记录随发行提交，使用普通推送同步 `main`、`deepseek-dev` 与 `v0.15.31` 标签；GitHub Release 上传上述四项产物并核对远端大小和 SHA-256 后公开。最终提交、上传核验和发布结果见交接回复。
- 限制与后续：本轮没有物理串口或外部真实 SSH 主机联调；升级后 Agent 需重载 MCP 连接以发现新增工具。未更新网站、服务器或国内镜像；不删除 `deepseek-dev`，下次 DeepSeek 从同步后的分支继续。

## 2026-10-17 — 调用日志自动跟随最新记录

- 执行者：DeepSeek。
- 分支与起点：`deepseek-dev`，起点 `1e1402d`。
- 用户目标：打开“调用日志”时只看到旧记录，新发生的 AI 调用在表格底部不可见。
- 已完成：`src/ui/window.cpp` 的日志刷新先记录用户是否停在底部；重建行后，若原来跟随底部（或首次打开），自动滚动到最后一行；用户主动上滚阅读历史时不会被新记录拉回底部。
- 验证：构建成功；完整 `ctest` 16/16 通过；新增 `mcp_workflow_tests::auditTableFollowsNewestEntries`，注入 40 条记录后确认表格行数为 40 且滚动条位于最大值。
- 未完成：当前正在运行的 `dist` 实例仍为 `1e1402d` 构建；应用运行中无法覆盖 EXE，且重启会断开正在使用的 MCP 会话，故本次未重启部署。
- 风险与兼容：仅影响调用日志的滚动位置；不改变审计内容与排序。
- 提交与同步：见交接回复中的提交哈希；推送 `deepseek-dev`，未推送 `main`。
- 下一步：重启应用后生效；Codex 审查并集成。

## 2026-10-17 — AI 活动浮层与一键打开调用日志

- 执行者：DeepSeek。
- 分支与起点：`deepseek-dev`，起点 `877a437`。
- 用户目标：状态栏右下角的 AI 调用提示太不显眼，实际使用中“看不见命令”；需要更醒目的 GUI 反馈。
- 已完成：
  - `src/ui/window.*` 新增工作区浮层 `aiToast`：每次 MCP 工具调用显示 `AI · 操作 · 成功/失败`，10 秒后自动隐藏；提供“详情”（最近一次调用的参数/返回/错误）与“全部记录”（打开 设置 → AI / MCP → 调用日志）；可手动关闭；窗口缩放时重新定位在标签栏下方、页面工具栏之下，不遮挡顶部按钮。
  - `src/ui/mcp_ui.cpp` 的 `toolCalled` 统一调用 `Window::showAiActivity()`，同时更新浮层与状态栏标签。
  - `src/ui/window.*` 新增 `openAiAudit()`，从浮层直接切到设置中的 AI/MCP 调用日志页。
- 验证：
  - 构建成功；完整 `ctest` 16/16 通过。
  - `mcp_workflow_tests` 新增断言：HTTP 调用后浮层可见且文本含 AI；点击“全部记录”后设置页停在 AI / MCP、MCP 页停在调用日志。
  - 截图 `build/ai-sync-captures/ai-gui-sync.png`：浮层位于工具栏下方右侧，显示 `AI · get_status · 成功` 与两个按钮；状态栏同时保留 `AI · get_status · 成功`。
- 未完成：无。
- 风险与兼容：浮层是临时提示（10 秒），不拦截输入；频繁调用会不断刷新为最近一条；历史与详情仍以调用日志为准。写操作权限、共享连接与读取游标未改动。
- 提交与同步：见交接回复中的提交哈希；推送 `deepseek-dev`，未推送 `main`。
- 下一步：Codex 审查并集成。

## 2026-10-17 — GUI 同步 AI 的 MCP 操作与可查看调用详情

- 执行者：DeepSeek。
- 分支与起点：`deepseek-dev`，起点 `e1f402b`。
- 用户目标：AI 调用 MCP 时界面全程不可见；AI 打开串口、写入并启用脚本后 GUI 应同步显示；调用日志信息太少，需点击查看更详细的参数与结果。
- 已完成：
  - `src/mcp/mcp.h`、`mcp.cpp`：新增 `guiConnectionOpened`、`toolCalled` 信号；`open_connection` 成功后通知 GUI；每次 HTTP 工具调用在审计中记录 `summary`、`arguments`、`result`（结果最多 8 KiB，超出截断）；`open_connection` 工具说明补充“GUI 会打开并激活对应页面”。
  - `src/ui/window.*`：新增 `showPane()` 激活并置前对应标签页（含分离窗口）；状态栏新增常驻 `AI · 操作 · 成功/失败` 标签，悬停显示客户端、时间、耗时和错误；调用日志表保存完整审计对象，双击打开 `showAuditDetail()` 详情对话框（格式化参数、返回、错误，可复制）。
  - `src/ui/mcp_ui.cpp`：AI 打开连接时激活页面；AI 写入/启用脚本时激活页面，并在编辑器未打开时以非模态窗口展示 AI 代码；状态栏同步显示最近 AI 操作。
  - `src/ui/script_editor.cpp`：`openScriptEditor(bool nonModal)`，手动打开仍为窗口模态，AI 同步使用非模态。
  - `src/ui/button_motion.cpp`：无动画运行时按实际勾选状态绘制，修复程序化（信号被阻断）更新脚本开关后复选框仍显示未勾选的问题。
- 验证：
  - `cmake --build build/native-release --parallel 4` 成功；完整 `ctest` 16/16 通过。
  - 新增 `mcp_workflow_tests::guiSyncsAiConnectionScriptAndAudit`：验证 AI open_connection 激活对应页、set_frame_script 写入并启用后侧栏开关勾选、脚本编辑器非模态并显示代码、审计含 summary/参数/返回、状态栏 AI 标签可见、详情对话框含“参数/返回”。
  - 实际进程联调：启动应用后经 HTTP MCP 打开 `loop://`、写入并启用帧脚本、读取状态；截图 `build/ai-live.png` 显示连接页被激活、脚本解析面板出现、侧栏“脚本解析”已勾选、状态栏显示 `AI · get_status · 成功`；`build/ai-sync-captures/` 另含脚本编辑器、审计详情与主窗口截图。
- 未完成：无。
- 风险与兼容：AI 打开连接或写入脚本会主动置前窗口与标签（按用户要求）；写操作仍需 GUI 的 AI 控制权限。审计结果截断到 8 KiB，参数中的 payload 长度与脚本正文仍按原规则脱敏。脚本编辑器仅在没有已打开编辑器时自动弹出，且为非模态。未连接真实外部 SSH 主机或物理串口复测。
- 提交与同步：见交接回复中的提交哈希；推送 `deepseek-dev`，未推送 `main`，未发布 Release。
- 下一步：Codex 审查、运行原生测试并集成。

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
