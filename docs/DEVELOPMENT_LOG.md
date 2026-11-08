# 开发交接记录

## 2026-11-08 — 移除 Gemini CLI 专用接入

- 移除 Gemini CLI 的 Agent 适配器、设置入口、Skill 安装目标、Windows 安装器组件和专用图标。Antigravity 保留其独立接入和目录。当前共有 25 个客户端适配入口、16 个 Skill 安装目标。
- Windows 构建、21 项 Agent 测试、Skill 检测及设置页紧凑布局检查通过。旧 Gemini CLI 文件作为回归样本。NSIS 安装器编译成功，从便携解压包启动设置并核对 CLI 列表和已退役的导出 ID。
- 本地测试包 dist/previews/SerialWorkbench-0.15.43-agent-list-preview-windows-x64.zip，运行 Start-Preview.cmd 进入 Agent 设置。

## 2026-11-08 — v0.15.43 已发布至 GitHub 与网站

- PR #12 合入快速上手教程和发送换行选项修复。标签对应发行提交 2f4040779ad0588cebab9c3ce3a95e777e54661f，GitHub latest 为 v0.15.43。安装包、便携 ZIP、单文件 EXE 和 SHA256SUMS 的远端大小与摘要均匹配本地。
- Windows Release 构建、版本回溯测试、正式 ZIP 和单文件启动检查通过。复用教程和换行框的实窗验证。包内 HTTP 与 STDIO 均确认 37 个 MCP 工具。
- 通过 Serial Workbench MCP SSH 从本机上传安装包。网站下载、首页、项目页和软件更新清单均显示 v0.15.43。公网完整下载为 17227708 字节，SHA-256 为 e8409cc39df8bb5c2fbb4137e1c648d9aff7260f257bfbe777beea5eb495ddbf，与 GitHub 一致。部署 releases/20261108-manual-0-15-43-b15c68c6，并核对备案链接和校庆媒体。
- 下载后备元数据已同步。站点构建与两项部署测试通过。发行文件位于 dist/releases/0.15.43，验证记录位于 build/release-0.15.43-verification 和 dist/website-sync/0.15.43。

## 2026-11-08 — 修复发送栏换行文字截断

- 换行下拉框按选项内容调整宽度，并留出主题内边距与下拉箭头所需的最小宽度。四种发送行尾及其报文字节映射沿用原值。
- Windows Release 构建与 `sendNewlineFitsAllLabels` 通过。验证 480 像素宽窗口中四种标签完整显示，并使用解压包实窗检查 CRLF。
- Windows 便携预览位于 `dist/previews/SerialWorkbench-0.15.42-send-newline-preview-windows-x64.zip`，解压运行 `Start-Preview.cmd`。

## 2026-11-08 — 软件内快速上手教程

- 左侧连接区提供「快速上手」，也可按 F1 打开。内置离线教程覆盖串口参数、收发和常见问题，并提供波形、RTT、MCP 的入口指引。「打开演示波形」使用内置数据源。
- Windows Release 构建通过。实窗检查了默认和紧凑尺寸，`quickStartOpensLocalDemo` 验证从侧栏进入教程、窄窗口可滚动和演示仅建立本地数据源。
- 本地预览位于 `dist/previews/SerialWorkbench-0.15.42-quick-start-preview-windows-x64.zip`，解压后运行 `Start-Preview.cmd` 直达教程。

## 2026-11-07 — v0.15.42 已发布至 GitHub 与网站

- `v0.15.42` 标签指向 `f3035000c93e5847ccf93deff1f5d9d1e2cb3a98`，GitHub Release 于 2026-11-07T12:51:24Z 发布为 latest。安装包、便携 ZIP、单文件 EXE 和 SHA256SUMS 的远端大小与 SHA-256 均与本地 `dist/releases/0.15.42/` 一致。
- 网站经 Serial Workbench MCP 的 SSH 连接和本机文件上传更新，服务器未从 GitHub 下载安装包。公网 `/updates/latest.json` 与完整安装包下载校验为 17,213,676 字节、SHA-256 `b82208aaa4eb7cab1b5360b9cbfeadbbb076d2984b31b0632fea112430e21e16`；首页、项目页和下载页显示 v0.15.42。页面激活为 `20261107-manual-0-15-42-1b83c5ff`，上一版为 `20261107-openocd-01541`。
- 仓库内网站的下载后备元数据已同步版本、文件名、大小、日期、摘要和更新说明；站点构建及 2 项部署测试通过。发布与公网校验记录位于 `dist/website-sync/0.15.42/`。
- 本轮未重复编译、硬件测试或安装升级检查，沿用 0.15.42 发行前的构建与验证记录。

## 2026-11-07 — 扩大 OpenOCD 固件表格

- 固件表格预留三行，随分段数扩展至六行，更多分段可滚动。高度计入主题表头的实际尺寸。缩窄 BIN 地址列，长路径从中间省略，悬停显示完整路径。
- Windows Release 构建及 `openocd_tests` 通过。实窗用单个 ELF 和八个 BIN 占位文件检查文件名与地址，确认六行完整显示。使用解压包的运行库复查启动和表格布局。
- 本地测试包 `dist/previews/SerialWorkbench-0.15.41-openocd-firmware-table-preview-windows-x64.zip`，21,605,569 字节，SHA-256 `c75cc34de4b05fc1069378585e7440772f355782ae4f64e95418bba3c8cda0a7`。截图位于 `build/openocd-firmware-table/`。

## 2026-11-07 — RTT 全宽终端、搜索与断线恢复

- RTT 终端使用页面主区域，连接参数收进独立的「连接设置」窗口。连接日志和执行预览保留独立页签。终端和弹窗都支持 `Ctrl+F`、大小写、全词、正则及 `&&` / `||`。筛选结果按行显示，清空搜索可返回彩色终端。
- 新增 `search_openocd_rtt`，与界面共用每个板卡页的筛选条件。MCP 工具总数 37，搜索沿用 OpenOCD 读权限。`read_openocd_rtt` 保留独立游标和原始数据，GUI 暂停或清空仅影响当前视图。搜索处理跨 TCP 数据块的 UTF-8 和 ANSI 序列，中英 README 和随包 skill 已同步。
- OpenOCD 的 `dropped 'rtt' connection` 表示已接受的客户端连接被移除。工作台在服务仍可用时有界重试同一端点，保留原任务、日志和游标，沿用芯片当前状态。超过重试预算会在连接日志中报告。板子必须复位的原因仍待复现。
- Windows Release 编译通过。OpenOCD、RTT、MCP 协议、搜索和终端按键回归通过。模拟服务主动断开后从同一 OpenOCD 进程续收，原日志与游标保留。Windows 实窗验证全宽终端、独立设置、窄窗口和 GUI / MCP 搜索同步。解压包通过 HTTP / STDIO 功能检查；一次测试清理返回 CLI 退出异常，独立复查时 CLI 与工作台均正常退出。
- 通过 CMSIS-DAP 连接原 STM32F103 固件，以保持目标状态的模式收到 970 字节，并通过 MCP 搜到 10 条心跳。实机本轮验证收数和搜索，断线恢复由模拟服务覆盖。截图与验证记录位于 `build/rtt-terminal-preview/`。
- 本地测试包 `dist/previews/SerialWorkbench-0.15.41-rtt-terminal-preview-windows-x64.zip`，21,608,169 字节，SHA-256 `f537633a4f1ac9518ad3ec0d9dfae17b756046f5818771605755da1d9993dc75`。解压运行 `Start-Preview.cmd`，可沿用旧预览的 `preview-settings.json`。

## 2026-11-07 — RTT 连接等待与表单对齐

- 将 RTT 设置合并为一个表单。连接方式、端口、控制块参数和 GDB 字段使用同一列。已有服务模式显示主机，切回工作台启动模式时保留已保存的外部主机地址。
- 工作台启动模式在开放 TCP 端口前确认 OpenOCD 能读出 RTT 控制块及通道信息，固件就绪稍晚时短暂重试。TCP 阶段最多等待 5 秒，对暂时拒绝进行有界重试，取消会清理等待。状态分别显示初始化、TCP 连接和等待固件输出。启动失败会提供连接日志和相应错误，端口冲突提示包含端口号及自动分配选项。
- 首轮回归复现旧版两组字段相差 6 像素，并确认不可用端点的错误缺少目标地址。修复后 OpenOCD、RTT 和 MCP 协议三组测试通过，Windows 实窗验证两种连接方式的对齐、无输出提示和失败后的日志定位。解压测试包通过 HTTP / STDIO 的 OpenOCD 和 RTT 验证。
- 通过 CMSIS-DAP 和原 STM32F103 固件实测固定 9090。错误控制块标识在约 2.1 秒给出诊断，未开始 TCP 握手。正确标识收到彩色心跳，停止后再次使用同一端口成功收数。实测保持目标状态。
- 独立测试包 `dist/previews/SerialWorkbench-0.15.41-rtt-connection-preview-windows-x64.zip`，21,593,860 字节，SHA-256 `692c33079708feb43f074b0a9067bdfe4df377e5ef673610c31693ce73e1464d`。解压运行 `Start-Preview.cmd`，可复制旧预览的 `preview-settings.json`。Windows 截图、首轮复现和实机记录位于 `build/rtt-connect-preview/`。

## 2026-11-07 — 分开 OpenOCD 固件烧录与 RTT 工作区

- 在 `codex/openocd-rtt` 的 `9946127` 基础上把每个 OpenOCD 页划分为并列的“固件烧录”和“RTT 日志”。烧录页保留目标、初始化、工具和固件配置，RTT 页有独立终端、连接日志和执行预览。RTT 启停合并为一个按钮，新建入口统一称为 OpenOCD 页。
- 接收 RTT 时可以打开三个烧录配置页并查看既有烧录日志。只锁定配置编辑和会冲突的启动操作，切换页签不暂停 RTT。烧录页和 RTT 页各自保留最近一次任务的诊断输出。窗口窄时 RTT 设置和终端上下排布。
- Windows Release 构建、OpenOCD、RTT 和 MCP 协议测试通过。Windows 实窗确认两个工作区、紧凑布局和接收期间的页签切换。回归测试在烧录配置页停留期间发送样本，切回 RTT 后显示该样本，并检查停止后可恢复编辑。首轮测试的临时 JSON 引用已修正，初始日志和最终截图留在 `build/rtt-workspaces-preview/`。
- `dist/previews/SerialWorkbench-0.15.41-rtt-workspaces-preview-windows-x64.zip` 为独立测试包，21,588,942 字节，SHA-256 `62b0f2e1162c191343d42d00f28c12533e0d9b6d096189e6453d51461e265699`。解压后验证 GUI 启动和包内 OpenOCD / RTT HTTP、STDIO 流程，本轮使用模拟数据验证界面交互。运行 `Start-Preview.cmd`，原预览目录的 `preview-settings.json` 可复制到新目录继续使用。

## 2026-11-07 — OpenOCD RTT 查看窗开发预览

- 从 `8f496a7` 建立 `codex/openocd-rtt`。每个板卡页新增 RTT 配置与彩色终端。支持可调大小的独立窗口、暂停显示、清空当前视图、复制和导出文本。配置随板卡页保存和导入导出，启动工作台时保持空闲。
- 工作台启动模式沿用本页探针/目标配置，搜索指定 RAM 范围内的 RTT 控制块，可选通道、轮询间隔与 GDB 端口。RTT 默认保持目标状态，也可显式恢复运行或复位运行。初始连接有超时，连接成功后持续接收。已有服务模式使用独立 TCP 连接。停止只处理本页拥有的会话。
- MCP 复用 `configure_openocd`、`preview_openocd`、`start_openocd`、`cancel_openocd`，新增只读 `read_openocd_rtt`，工具总数 36。GUI 和 MCP 共用每页 512 KiB / 2000 段 RTT 缓存，读者独立分页。保留原始字节和增量 UTF-8 文本。OpenOCD 协议与 AI 控制权限沿用现有开关。更新了中英 README、随包 skill 与内置 OpenOCD 指南，skill 校验通过。
- Windows Release 编译通过。OpenOCD 测试 27 项、RTT 测试 8 项通过，另外完成 RTT Windows 实窗检查。解压包验证了 GUI 启动、HTTP/STDIO 工具清单、烧录模拟回归、RTT 模拟进程与已有服务、ANSI/UTF-8、独立游标、停止行为。
- 实机通过 CMSIS-DAP v2 接入参考 STM32F103 固件。从现有 map 核对 `_SEGGER_RTT=0x2000002C`，使用通道 0、保持目标状态读取启动信息与绿/黄/红 ANSI 日志。Windows 监听表确认 GDB 3333 和 RTT 自动端口由同一 OpenOCD 进程提供。结束后释放了测试会话。IDE 断点操作仍待验证。
- 独立 Windows x64 测试包 `dist/previews/SerialWorkbench-0.15.41-rtt-preview-windows-x64.zip`，21,583,603 字节，SHA-256 `64b1ad4ec31abda6a6d3a30f82c7d109db35be6b7ac5c13571bc444ef3798d49`。解压运行 `Start-Preview.cmd`，配置保存在解压目录。实窗和实机证据位于 `build/rtt-preview/`。本次交付为本地预览。

## 2026-11-07 — v0.15.41 已发布至 GitHub 与网站，清理本地测试包

- PR #9 保留历史合入主线，发行提交为 `811bb8bd614a286a162a47ee50e9a44647f34d72`。GitHub `v0.15.41` 已发布为 latest；安装包、便携 ZIP、单文件 EXE 和 SHA256SUMS 的远端大小、SHA-256 均与本地一致。
- 正式 Release 构建及 Qt 运行库部署完成。复用本轮 OpenOCD 27 项 Windows 检查、14 项 MCP 协议测试和包内 OpenOCD HTTP/STDIO 结果；正式 ZIP / 单文件发行检查通过，覆盖独立运行库、完整清单、35 个工具、写权限、中文路径、启动缓存和缺失缓存修复。本轮未重复安装升级、全量回归或实体烧录测试。
- 安装包为 17,175,999 字节，SHA-256 `40f502c16a2c269290c48cf311539badeaadd014935bc8786c113334a484d1cf`。正式发行文件位于 `dist/releases/0.15.41/`；本机运行库目录为 `dist/SerialWorkbench-0.15.41/`。
- 网站通过 Serial Workbench MCP SSH 控制及本机文件上传更新，服务器未从 GitHub 下载。公网完整安装包的大小与摘要一致；首页、项目页、下载页的 HTML 摘要与构建一致，版本和备案信息正确。页面激活为 `20261107-openocd-01541`，既有资源和校庆视频媒体保持不变。
- 按用户要求清空 `dist/local`：删除 1,063 个文件，共 2,250,421,792 字节（约 2.10 GiB）。先核对绝对路径、无链接及运行进程，并将 5 份个人测试设置按原目录结构备份至 `build/local-cleanup-backup-0.15.41/`；没有删除已安装工作台或正式 0.15.41 包。此前开发日志中的 local 测试包路径现已失效。
- 发布、公共摘要、网页检查和清理记录位于 `build/release-0.15.41-verification/`，镜像记录位于 `dist/website-sync/0.15.41/`。专用部署脚本、配置备份和测试证据仍在忽略目录，未提交 GitHub。发布交接完成后以普通快进同步并保留 deepseek-dev。

## 2026-11-06 — v0.15.41 发行准备

- 用户授权 push、Release 和网站同步，并要求清空 dist/local。沿用 codex/openocd-flashing；重新 fetch 后 main 与 deepseek-dev 没有新增差异，不重写共享历史。
- 版本提升至 0.15.41，新增中英发行说明，README 同步；离线版本回溯更新为已发布的 0.15.40 / 39 / 38。Windows Release 应用、CLI、MCP 桥及维护程序完成编译和 Qt 运行库部署。
- 复用此前 Windows OpenOCD 27 通过 / 1 可选跳过、MCP 协议 14 通过、权限与工具数检查及包内 HTTP/STDIO 结果，不重复硬件或全量回归。正式 ZIP 和单文件发行检查已通过，包含完整包内清单、独立运行库、35 工具 HTTP/STDIO、权限、中文路径和运行缓存修复；未重复安装升级测试。
- 安装包为 17,175,999 字节，SHA-256 为 40f502c16a2c269290c48cf311539badeaadd014935bc8786c113334a484d1cf。实际发布、网站与清理结果完成后补记。
- 正式文件改放 dist/releases/0.15.41，运行库目录为 dist/SerialWorkbench-0.15.41，均位于待清理的 dist/local 之外。网站使用本地安装包上传及 MCP 远端操作；私有维护脚本继续留在忽略目录。

## 2026-11-06 — OpenOCD 界面清理与配置导入导出

- 按用户连续反馈删除常驻分隔条提示和“专注日志 / 返回配置”按钮，保留分隔条拖动及宽屏/紧凑布局。OpenOCD 内部标签关闭原生底部基线，单独校正内容区顶部圆角和标签间距，清理截图中的残留边角；配置和输出标签顶部对齐。左侧参数栏继续沿用原来的展开/收起状态。
- 删除“快速方案”下拉和内置预设，改为“导入配置 / 导出配置”按钮，保留配置库重新扫描和开发板、调试器、芯片搜索。使用系统文件选择框，导出经 QSaveFile 写入；导入限制 1 MiB JSON，并在选择文件前记录 revision，避免覆盖等待文件选择期间发生的修改。
- 导出格式为 `format=serial-workbench.openocd, version=1`，包含页面名称及完整 configuration；文件仅引用本地程序、配置库和固件路径，不嵌入二进制。导入原子替换当前页，省略项采用默认值；不继承其他板子的探针/固件，不启动连接或烧录。格式/类型/字段/名称错误、过期 revision 或运行中均拒绝并保持原配置。
- GUI 与 MCP 共用服务层导入/导出逻辑。新增 `export_openocd_configuration`（只读返回 JSON）和 `import_openocd_configuration`（明确 view_id、当前 revision、AI 控制及 OpenOCD 权限），总工具数为 35。导入后的页名和控件即时同步，其他页面不受影响。内置指南、skill 和中英 README 同步更新。
- 应用编译成功。实际 Windows OpenOCD 测试 27 通过 / 1 可选真实 OpenOCD 检查跳过，覆盖 JSON 往返、Unicode 文件名、无效/过大/缺失文件、保存失败、完整替换、过期 revision、运行中保护、GUI/MCP 格式一致和多页隔离。MCP 协议测试 14 通过，工具数量及控制权限检查 3/3；包内 HTTP/STDIO 的导入导出、无效版本拒绝、独立日志和关闭/重建通过。本轮使用模拟任务，未操作真实硬件。
- 已查看浅色、深色及紧凑布局截图，证据在 `build/openocd-config-preview/`。测试包为 `dist/local/SerialWorkbench-0.15.40-openocd-config-preview-windows-x64.zip`，21,538,630 字节，SHA-256 `37cd566c72eee1dbdcad83ea72e8cb586d48d0e6735a74fed1f53a008f450cf5`；构建、目录和 ZIP 内主程序摘要一致：`f55cfeb53bb5a238e6c7b414a95f2badaf36c4dab0e82fb0ad95c6e537de958b`。解压运行 `Start-Preview.cmd`，可保留旧预览的 preview-settings.json；包内不含个人设置、测试进程或固件。继续 `codex/openocd-flashing`，未推送、发布正式 Release 或部署网站。

## 2026-11-06 — 切换 OpenOCD 时保持左侧参数栏

- 按用户反馈取消 OpenOCD 页自动隐藏侧栏的特殊处理。收发页与烧录页沿用同一展开/收起状态，保留侧栏宽度与标签栏位置；用户仍可手动收起侧栏。此前“隐藏串口参数栏”的设计由本条替代。
- 应用重新编译成功。实际 Windows 相关检查 5/5，包含切页前后侧栏几何和标签位置一致、手动收起后切页不自动展开、日志分栏与紧凑布局。已查看修复后的窗口截图，证据位于 `build/openocd-sidebar-preview/`；本轮没有重复整套回归或硬件测试。
- 新测试包：`dist/local/SerialWorkbench-0.15.40-openocd-sidebar-preview-windows-x64.zip`，21,536,675 字节，SHA-256 `cf5175e9b3f31d10b6f3f216b2bd69ed96ec9e331ff88ba7d4ea5f777d0db7cf`。ZIP 内主程序与实际验证的构建摘要一致：`6974094d63bf27c1be1a67c463f4245c1e0d0beaf1fbaf6971e51b0d888f4601`。解压运行 `Start-Preview.cmd`，可保留原预览目录的 `preview-settings.json`；包内未包含个人设置。仍在 `codex/openocd-flashing`，未推送、发布正式 Release 或部署网站。

## 2026-11-06 — OpenOCD 多板烧录标签页与自适应布局

- 在 `codex/openocd-flashing` 保留前两轮开发预览的全部改动，继续实现用户要求的多板标签页。本轮未推送、发布正式 Release 或修改网站，正式版本仍为 0.15.40。
- 顶部 ＋ 菜单改为“新建 OpenOCD 烧录页”，每页独立保存板子名称、探针、目标、固件、任务和有界日志，共用 GUI / MCP 状态。支持主工作台标签切换、重命名和既有分离流程；隐藏串口参数栏。配置按稳定 `view_id` 保存，重启恢复但不启动任务，迁移旧单页配置。关闭全部页面后允许空列表并可重建；缺失配置引用会跳过，预览启动复用已有页而不追加空白页。
- `Engine` 维护最多 32 个服务，每页独立进程、临时 Tcl 目录、超时和取消。并行连接/烧录前要求所有活动硬件任务填写不同的 `adapter_serial`，拦截未指定或重复探针；版本检查不占用探针。取消、关闭空闲页不影响其他页；活动任务必须先完成或取消才能关页。不自动关闭或重开串口。
- MCP 新增 `list_openocd_views`、`create_openocd_view`、`close_openocd_view`，总工具数为 33。所有修改/启动/取消/关闭要求明确 `view_id`；只读工具仅在单页时允许省略。保留独立 OpenOCD 访问开关、AI 控制权限、revision 和 job_id 检查，内置指南、skill 与中英 README 同步更新。
- 宽窗口左右显示配置与日志，分隔条可拖动并支持专注日志；窄窗口或高度不足时使用“配置 / 运行输出”完整页面，避免配置表单被压成细条。保留三页配置、搜索和固件表格。初轮实窗发现从紧凑标签转回分栏时 Qt 保留页面隐藏状态，已在转移后显式显示；日志宽度恢复、往返切换和 Esc 不隐藏内嵌页面均通过。
- Release 应用编译成功；本轮原生回归 97 通过 / 3 跳过、MCP 协议 14 通过；最终 OpenOCD CTest 24 通过 / 2 可选跳过。实际 Windows OpenOCD 首轮 25 通过、1 个布局项失败，保留首次日志；修复后布局与截图 4/4，最终生命周期 3/3。覆盖双探针模拟进程同时运行、重复/空序列号拦截、独立日志和取消、标签重排后精确关闭、配置原子恢复及空列表/缺失引用。真实 OpenOCD 仅检查版本和无硬件 Tcl，未执行实体多板烧录。
- 干净测试包的 HTTP / STDIO MCP 检查通过，覆盖两页配置隔离、异步模拟任务、独立读取及关闭/重建；包内程序实窗恢复两页且没有新增空白页，已检查浅深色、宽屏与紧凑截图。证据在 `build/openocd-tabs-preview/`，最终 CTest 输出另在 `build/native-release/openocd-tests.txt`。
- 新测试包为 `dist/local/SerialWorkbench-0.15.40-openocd-tabs-preview-windows-x64.zip`，21,536,830 字节，SHA-256 `59939f948ecbce6a92ecde17b788a4d49a35911eb51034dadffc55aacc76883e`。构建、干净目录和 ZIP 内主程序摘要一致：`7e6e7881babdfd49041c159899b8150ec249fa01e7a824b797acc63cf287d51c`。解压后运行 `Start-Preview.cmd`；可用原预览目录的 `preview-settings.json` 保留配置。包内不含个人配置、测试进程、固件、OpenOCD 或 USB 驱动；原测试包及已安装应用保留。

## 2026-11-06 — OpenOCD 日志布局与配置快速搜索

- 继续 `codex/openocd-flashing` 上上一轮未提交的开发预览，保留此前全部改动。本轮集中优化界面，没有改动烧录命令、权限或连接逻辑，没有推送、Release 或网站部署。
- 常用目标与固件并排显示，初始化/高级连接参数和 OpenOCD 路径分为独立标签页。收紧标题留白，固件表格按文件数量调整高度；常用校验/复位勾选项在默认窗口直接可见。日志默认分配更大空间，分隔条更容易拖动，新增“专注日志 / 返回配置”，恢复原有比例而不清空日志或改动配置。
- 开发板、调试器、芯片和常用方案采用独立搜索弹窗，支持忽略大小写、空格/连字符等分隔、Ctrl+空格、方向键、Enter 和 Esc。搜索不修改当前参数；确认后映射到原始项，保留预设数据和当前项高亮。自定义 cfg 路径仍可直接编辑；搜索 `ST Link` 不会混入仅分别包含 `st` 和 `link` 的无关配置。常用方案名称按实际配置匹配显示。
- Release 应用编译成功。相关离屏检查 5/5；实际 Windows 最终检查 6/6，覆盖搜索过程不写配置、无匹配和取消、预设映射、当前项高亮、日志放大/恢复、常用控件可见性、小窗口与 GUI/MCP 同步。初轮实窗发现日志高度未达目标，已调整并保留首次失败记录。已查看浅深主题、搜索弹窗、专注模式及紧凑布局；真实 OpenOCD 只运行版本读取，没有操作芯片。
- 包内 HTTP/STDIO 的共享配置、枚举、预览、模拟任务及独立日志检查通过；最终包自身使用独立配置启动并截图成功。MCP 工具仍为 30 个，内置 OpenOCD 指南和中英 README 已同步更新。证据位于 `build/openocd-layout-preview/`，包含原生测试、截图、包内检查和摘要。
- 新包为 `dist/local/SerialWorkbench-0.15.40-openocd-layout-preview-windows-x64.zip`，21,508,503 字节，SHA-256 `339b2516c54d0ec8b00d11afe5e10caa4703ae3bf90b573e14c4cf49ff747575`。目录、ZIP 内及构建的 EXE 摘要一致：`74c54c205ad4786bbbe5ee436296f170d665f4cb5130bc26b588e47606367e47`。解压运行 `Start-Preview.cmd`；包内不含个人 `preview-settings.json`，覆盖旧测试目录时可保留该配置。干净暂存目录为 `dist/local/openocd-layout-preview/SerialWorkbench-OpenOCD-preview/`，原测试包和已安装应用保留。

## 2026-11-06 — OpenOCD 烧录开发预览

- Codex 从干净的 `0555482` 建立 `codex/openocd-flashing`。Fetch 后 `origin/main..origin/deepseek-dev` 无新增提交，保留两条共享分支及已安装工作台；本轮没有推送、正式 Release 或网站部署。
- 新增原生 OpenOCD 工具窗，入口为顶部 ＋ 菜单。读取所选外部 OpenOCD 的开发板、调试器和芯片 `.cfg`，支持开发板或接口＋目标组合、常用快速选择、传输方式、时钟、探针序列号和 SRST 选项。可调整窗口和配置/日志区比例，浅深主题同步，高级连接选项按需展开。
- 通用 `program` 推荐流程不重复 `init/reset init`；可显式选择 `init → reset init`、`init → reset halt`、`init → halt`，使用直接擦写和校验。ELF/HEX/S19 使用内嵌地址，BIN 必须填写地址。Espressif 使用厂商 `program_esp`，支持最多 16 个 BIN 分段、校验与最终复位运行；缺失命令或选择 SWD 会拒绝。配置文件存在不代表该芯片/调试器组合已实测。
- `Engine` 持有 GUI/MCP 共用服务，使用参数列表和转义后的临时 Tcl 启动子进程，无 shell 拼接。支持异步有界日志、退出/完成标记检查、超时、取消和 revision 冲突检测。日志保持独立读取，修复换任务后空记录可能导致游标空转。保持原有串口、SSH、文件传输及权限语义，不自动连接或释放设备。
- MCP 新增配置枚举、状态、配置修改、执行预览、启动和取消 6 个工具，总数 30。独立 OpenOCD 访问默认关闭，写操作另需 AI 控制权限；原生 CLI 的通用 `call` 可直接使用。设置持久化、内置 `read_usage_guide(topic="openocd")`、随包 skill 和中英 README 同步更新；skill 格式校验通过。
- 本机 xPack OpenOCD 0.12.0 dev 的真实 Tcl 检查发现中文文件名不能直接打开。Windows 中文固件在启动时复制为临时英文文件名，结束后清理；配置库尝试系统短路径，无短路径时明确报错，避免破坏相对引用。真实 OpenOCD 已通过版本、英文/中文/特殊字符路径测试，测试使用替代 `program` 的无硬件 Tcl 函数，没有连接或烧录芯片。
- Release 构建成功。基础 `native_tests` 为 97 通过、3 跳过，`mcp_protocol_tests` 14 通过。OpenOCD 配置、复位顺序、失败退出、取消/重开、超时和日志上限用例通过；最初两处断言（权限项数量、序列号末尾换行）已修正并保留首次记录。Windows 首轮暴露中文路径问题，修复后相关最终用例 7/7；最后外观检查 3/3，已查看浅色、深色和紧凑实窗截图。包内 HTTP/STDIO 的 OpenOCD 模拟任务、独立日志和原有 30 工具集成均通过；后台测试通过 CLI 按独立配置退出，不影响用户实例。
- 最后补充重复取消的回归：Agent 重试取消不再重置强制结束计时，Windows 针对性 3/3 通过。替换最终包内程序后，OpenOCD HTTP/STDIO 模拟任务再次通过。
- 独立预览为 `dist/SerialWorkbench-OpenOCD-preview/Start-Preview.cmd`，使用专属配置且不包含 OpenOCD 或测试进程 fixture。ZIP 为 `dist/local/SerialWorkbench-0.15.40-openocd-preview-windows-x64.zip`，21,491,708 字节，SHA-256 `eb937ea34f842d1ef5e05e96a04a8964ec35df186f6ca76dbf692115890f68b1`。ZIP 中及目录中的 EXE 与构建产物摘要一致：`1fbcf0a768fbcd869dc081d0c1a0791b7005ce59cfa0447020b79d27cfa69af4`。日志、截图和摘要位于 `build/openocd-preview/`，均为本地忽略文件。
- 未完成验证边界：尚无真实调试器/目标芯片联调，未验证芯片擦写、USB 驱动和 Espressif 多分段真机；Linux 本轮未构建。当前为基于 0.15.40 的未发布开发预览，下一步应按用户指定硬件测试后再准备正式发行。

## 2026-11-05 — v0.15.40 已发布至 GitHub 与网站

- PR #7 保留历史合入主线，发行标签 `v0.15.40` 指向 `c8736e0`。GitHub 发布为 latest，安装包、便携 ZIP、单文件 EXE 和 SHA256SUMS 的远端大小及摘要与本地一致。
- 正式版完成 Release 编译和三种 Windows x64 打包，复用本轮 34 项串口传输测试、Windows 虚拟串口完整性及 CRC 重传验证。安装包大小为 17,077,313 字节，SHA-256 为 `01c4654837324a3962a317b4713e5fd583087b037ca8b02065b19aa3b519e73d`。
- 网站经 Serial Workbench MCP SSH 手动上传本机安装包。公网安装包的大小和摘要一致，更新清单及下载页已生效，页面切换至 `20261105-file-send-01540`。本地发行包位于 `dist/local/0.15.40/`，记录位于 `build/release-0.15.40-verification/` 和 `dist/website-sync/0.15.40/`。

## 2026-11-05 — 完善串口文件发送

- 原始发送支持 1–4096 字节分块及 0–60000 毫秒块间隔。按当前波特率、数据位、校验位和停止位估算线路时间，修正低速串口的发送超时。改用可取消的发送定时器，防止上一次任务的排队回调影响新传输。
- 新增显式 XMODEM-1K CRC 发送选项，兼容 1024 字节块与 128 字节尾块。重传使用同一数据块，发送进度按 ACK 计数。接收半包按数据间的空闲时间判断超时。设置页保留连接独占与原有日志隔离。
- 构建应用、传输测试及 Demo。最终 `serial_transfer_tests` 34 项通过，覆盖分块间隔、低速发送、取消后重开、混合块、重传和缓慢分段接收。Windows 虚拟串口 Demo 的原始发送和 XMODEM-1K 均传输 49,175 字节且 SHA-256 一致，1K 模式注入一次 CRC 错误并成功重传。证据位于 `build/serial-send-preview/`。代码图连接断开，按当前源码核对调用路径。
- 便携测试包 `dist/local/SerialWorkbench-0.15.39-serial-send-preview-windows-x64.zip` 已从解压目录使用包内运行库验证启动，内含独立配置的工作台和虚拟串口 Demo。本轮验证使用虚拟串口。

## 2026-11-05 — v0.15.39 已发布至 GitHub 与网站

- PR #5 保留历史合入主线，发行标签 `v0.15.39` 指向 `bb5da58`。GitHub 发布为 latest，安装包、便携 ZIP、单文件 EXE 和 SHA256SUMS 已上传，四个文件的远端大小及摘要与本地一致。
- 复用图标修复的 Windows 原生 Agent 测试、浅深主题检查和便携测试包启动验证。正式版完成 Release 编译和三种 Windows x64 打包。安装包大小为 17,074,211 字节，SHA-256 为 `983e383f154e66b5e6b9bbbc323c9856544b5bc90e5775519c30e0ae36b7f16b`。
- 网站使用 Serial Workbench MCP SSH 手动上传本机安装包。公网完整下载与本地摘要一致，更新清单及下载页均已生效，页面切换至 `20261105-agent-01539`。本地发行包位于 `dist/local/0.15.39/`，验证记录位于 `build/release-0.15.39-verification/` 和 `dist/website-sync/0.15.39/`。

## 2026-11-05 — 恢复设置页 Agent 品牌配色

- 对照产品官方品牌素材、文档或扩展图标核对配色。Antigravity 使用官方渐变 PNG，TRAE 使用绿标，Kilo Code 使用黄黑标识，Kimi Code 使用带蓝点的产品图标，Cursor 使用官方灰阶立体标，Zed 使用品牌蓝，VS Code Insiders 使用微软的绿标。Qwen Code 在深色主题继续使用官方蓝。原生设置页只对单色标识应用黑白主题适配。资产及来源见 `resources/agents/README.md`。
- 重新构建 Windows 应用和 `agent_tests`。原生 Agent 测试 22 项通过，补齐 Insiders 图标后通过最终的浅色与深色图标用例。查看了 Windows 实际组件截图。证据位于 `build/agent-brand-audit/captures/` 和 `build/agent-brand-audit/logos-final.txt`。
- 便携测试包为 `dist/local/SerialWorkbench-0.15.38-agent-logo-preview-windows-x64.zip`。解压后运行 `Start-Preview.cmd`，使用独立配置进入 Agent 接入页。已从解压目录验证仅用包内运行库启动，证据为 `build/agent-brand-audit/package-ready.json`。

## 2026-10-31 — v0.15.38 已发布至 GitHub 与网站

- PR #3 按保留历史的 merge 方式集成至主线，提交 `fb6ac40`；`v0.15.38` 指向该提交，正式 Release 于 2026-10-31 发布为 latest，包含安装包、便携 ZIP、单文件 EXE 和 SHA256SUMS。发行构建 CTest 22/22 已通过，按用户要求未重复安装升级或硬件验证。
- 安装包 16,989,819 字节，SHA-256 `43312063521bc2de299f171f076758141176a04763f28efa776f5f673be526a8`。网站通过 Serial Workbench MCP SSH 手动上传本机文件，完整公网下载的大小和摘要一致；服务器未从 GitHub 拉取。页面已切换为 `20261031-terminal-01538`，保留此前版本供回退，备案图标和视频媒体沿用原文件。
- 证据：`build/release-0.15.38-verification/` 与 `dist/website-sync/0.15.38/`；本机三个发行包及校验文件位于 `dist/local/0.15.38/`。网站专用维护脚本、SSH 细节、个人配置和本地证据仍在忽略目录，未提交 GitHub。交接基线更新至 0.15.38，DeepSeek 分支按已验证主线同步并保留。

## 2026-10-31 — v0.15.38 发行构建与发布准备

- 用户授权将终端横缝修复打包并发布 GitHub / 网站，随后要求不再重复验证。版本提升至 0.15.38，中英 README 和发行说明同步；离线回溯更新为 0.15.37 / 36 / 35。原已启动的正式 Release 构建及完整 CTest **22/22** 结束后直接打包，没有重复执行安装升级、便携/单文件启动或硬件测试。
- 网站经 Serial Workbench CLI 的 MCP 客户端重新握手同一后台实例；AI 控制与 SSH 权限已开启，已连接 `t_ecs`。注册工具调用报错后使用原生 CLI / MCP 客户端，未更改 Agent 权限。上传前核对当前 0.15.37、七个页面、共享媒体和两个约 1.7 GB 原片；发布继续使用本机手动上传，服务器不从 GitHub 下载。实际发布结果在完成后另行记录。

## 2026-10-31 — 修复收发终端选区中的一像素横缝

- 从干净的 `102bba2` 新建 `codex/terminal-selection-seam`；核对 `origin/main..origin/deepseek-dev` 无待集成提交。用户截图中的横线出现在 SSH 字符选区内部：原先每格独立绘制小数宽高，特定字体与缩放下相邻行会漏出一像素。Windows 原实现的 150% / 11pt 选区和 ANSI 背景均复现。
- `TerminalView::paintEvent` 将相邻字符背景的公共边界对齐到实际绘图设备像素，覆盖选区与 ANSI 底色；保留文字基线、选择范围、滚动和键盘行为。图索引提示元数据变化及部分解析缺口，已按当前源码核对相关实现。
- Release 构建通过；新增 24 组像素回归（100% / 125% / 150% / 200%，11 / 12 / 13pt，选区 / ANSI 背景），Windows 实际环境和 offscreen 均通过。Windows offscreen 测试显式加载系统等宽字体，避免缺字体的占位字符干扰像素判断。完整 `terminal_keyboard_tests` **61/61** 通过；实际独立窗口在 150% 缩放下鼠标大面积拖选，未再出现横缝。
- 证据：`build/terminal-seam/`、`build/terminal-seam-{windows-before,windows-after,after}.txt`、`build/native-release/terminal-keyboard-tests.txt`。修复测试包：`dist/SerialWorkbenchTerminalFixPreview/Start-Preview.cmd`，使用独立配置；主程序 SHA-256 `15d775860fd043d9d8cd97e28baa20629ce2072f21897d5ab89c16a47d61ac91`。未覆盖已安装软件，未连接真实设备，未推送、发布 Release 或部署网站；正式版本仍为 0.15.37。

## 2026-10-30 — v0.15.37 已发布至 GitHub 与网站

- GitHub PR #1 已按保留历史的 merge 方式集成，合并提交 `7999967`；其源码树与最终构建提交 `80f1ba0` 一致。`v0.15.37` 指向该合并提交，正式 Release 为非草稿、非预发行且已设为 latest。`deepseek-dev` 已由旧基线正常前进到已验证主线，保留原分支及全部历史。直接推送主线的首次命令被自动审批拒绝且未执行，随后通过 PR 审查合并流程完成。
- 四个 GitHub 资产均已比对远端大小与 SHA-256 digest：安装包 16,977,873 字节、单文件 15,676,048 字节、便携 ZIP 22,036,386 字节及 SHA256SUMS。安装包 SHA-256 `65305acd1cffc52dbb705f551e848ba14b35037b6b190b4244672a1c8f6d30f3`，完整构建/测试结果见下一节。
- 网站通过 Serial Workbench 的现有 SSH MCP 控制发布；本机上传已验证的安装包，SSH 转发仅承担二进制传输，服务器未从 GitHub 拉取。校验后切换镜像清单和静态页面，保留旧版本用于回退；本机从公网 HTTPS 完整下载的大小与 SHA-256 一致。
- 网站源码与线上七个 HTML 逐项摘要一致，页面显示 0.15.37 并更新 3D/安装升级说明；下载链接、Release 链接、校验文件、ICP备案、公安备案及其图标全部检查。两个 4K 原片与播放文件的大小、媒体链接和 MIME 保留；在线播放 HTTP 206 Range 检查通过。既有图片与样式仍使用各自实际版本的资源。
- 证据位于 `build/release-0.15.37-verification/` 与 `dist/website-sync/0.15.37/`。网站专用维护脚本、运维配置、模型、固件和个人设置未提交到 GitHub。后续 Agent 升级后需重载 MCP，并在设置中更新 skill；保留真实硬件、显卡和第三方客户端验证边界，不保证所有环境零卡顿或锁定文件均可自动解除。


## 2026-10-30 — v0.15.37 发行验证完成，准备推送与镜像部署

- 用户授权推送 GitHub 并同步网站。重新 fetch 后确认 `origin/main` 与 `origin/deepseek-dev` 同为 `643e9eb`，无新增 DeepSeek 提交；保留 `codex/model3d-telemetry` 的四个既有提交与全部未提交工作。版本从 0.15.36 提升为 0.15.37；本次汇总 3D、命名流、模型 MCP/配置交接、Agent/安装升级及实时拖动/收发页卡顿修复。
- 发布说明与中英 README 同步，离线回溯快照更新为 0.15.36 / 35 / 34。网站源码同步 0.15.37 下载链接、摘要及功能说明，保留备案和视频入口；生成网页并通过两个部署摘要回归。网站专用维护脚本、模型、固件、用户设置和本地证据仍被忽略，未纳入提交。
- 正式版本 Release 构建与全量 CTest **22/22** 通过。实际打包目录的 native / CLI / 3D MCP 集成通过，覆盖 24 工具、HTTP/STDIO、共享独立读取、权限、命名流、原生搜索/脚本、后台复用、模型导入/截图与 GUI 接续；检查最终模型 PNG。实机证据沿用上一节 ESP32-S3 连续 1317 帧验证，未重复烧录。
- 最终安装、升级及卸载测试通过：透明图标、Unicode 路径、17 个 skill 目标隔离安装、自动识别旧路径、后台与多个桥接退出、其他安装隔离、维护期禁止重启、DLL 锁定后重试、无 SDK 运行及保留个人文件。便携 ZIP 清单、单文件首次/缓存启动和缺失缓存修复均通过。原始日志及 package.json 保存在 `build/release-0.15.37-verification/`。
- 最终安装包 16,977,873 字节，SHA-256 `65305acd1cffc52dbb705f551e848ba14b35037b6b190b4244672a1c8f6d30f3`。另生成单文件 EXE、便携 ZIP 和 SHA256SUMS。当前停点：发行验证完成；后续 GitHub 发布和网站公网完整下载结果另行记录，不把准备工作当作已经部署。


## 2026-10-29 — 恢复 ESP32 测试分支并完成卡顿修复实机复测

- 用户说明 ESP32 曾用于其他工作，要求切回原分支。核对 MCU 仓库根目录为 `D:/MCU/esp32/esp32_practise`，当前 `test_any`、之前 `master` 均在 `d26ad0a`；已切回 `master`，保留 `test_any`、`.vscode/settings.json` 未提交修改及所有其他未跟踪工程。Serial Workbench 仍在 `codex/model3d-telemetry`，未切换或重置。
- 第一段实机观察中工作台测试进程中途关闭，未计为成功。重新连接后读到 `soft_i2c: no ACK from 0x50`，确认板上是另一份固件；原 `custom_serial` 源码仍是 fixture 演示。显式加载工程原有 ESP-IDF 6.1/Python 环境（系统默认已切到 IDF 5.5，第一次激活失败日志保留），构建后通过 MCP 释放 COM8，再烧录 bootloader、分区表和 app；三项写入摘要校验通过。app 200496 字节，SHA-256 `1c180166fc3019a6a041beb825c1df5028f2301d1a500d7bfd0085f9a546f2d0`。
- 使用修复版 `SerialWorkbenchStreamPreview` 重新连接 COM8 / 115200，原耳机夹具自动绑定 `fixture`。持续约 65 秒核对 74 个模型姿态状态，覆盖三个演示轴和三种相机位置；采集 1317 个连续设备帧，约 20.002Hz，无帧号缺口、解析拒绝或渲染错误，最大旋转矩阵元素误差 2.59e-7。实际执行正反向鼠标拖动并从模型页切回收发页；原生搜索返回的显示尾帧持续赶上当轮先前采样的接收帧，未观察到持续积压（不是逐帧显示延迟测量）。
- 原始 RX 字节验证 `STREAM_FIX_ECHO_20261029` 的 CRLF 回显成功；实际 MSAA 8×。已恢复测试前相机，保留手动位置、颜色和绑定，保持 COM8 连接供用户继续体验。证据位于 `build/stream-ui-performance/hardware-run2/`，包含构建/烧录、逐次状态、1317 帧数据及 `summary.json`。此记录完成上一节待补的 MCU 实测；未推送、发布或部署。

## 2026-10-29 — 实时 3D 拖动与收发页切换卡顿

- 保留 `codex/model3d-telemetry` 上已有未提交的模型、图标、标签动效工作。收发页原先隐藏时停止消费显示游标，返回时每次同步补画 500 条；日志逐条触发文本排版与滚动，并逐条刷新隐藏终端。现在隐藏页继续按 64 条小批更新，积压时用单次计时器让出事件循环后继续，用户暂停仍保留游标；日志合并编辑事务，终端字节合并但保留重置/文件传输边界。GUI 和 MCP 游标、历史上限及权限语义未变。
- Journal 按有序序号二分定位游标，避免每个页面实时读取都扫描完整历史。模型鼠标与遥测合并约 16ms 内的重绘请求；相机数值框在拖动中约 33ms 同步一次，松手和配置操作立即同步，MCP 始终读取最新相机状态。没有降低网格精度或抗锯齿设置。
- 新增 `stream_ui_tests`，覆盖权限过滤分页/历史淘汰、分段 UTF-8/ANSI/CRLF、多来源日志、筛选与选区、隐藏页持续更新、暂停/恢复、终端重置边界、相机字段拖动中刷新和松手同步。首次完整 CTest 21/22，唯一失败是沙箱 DPAPI 凭据测试；实际 Windows 用户环境补测通过。最后一轮重绘修改后相关 CTest 3/3，真实 Windows 串流测试 6/6、模型测试 16/16（含实际 OpenGL、0/2/4/8× MSAA）。
- 新增手动工具 `stream_ui_benchmark`。同一回环报文、1410 面耳机夹具和实际 8× MSAA：6000 条积压补画基线约 6173ms，最终约 1742ms；单批最长约 1164ms 降到 86ms，鼠标事件平均约 0.327ms 降到 0.036ms。不同轮次受桌面/GPU 调度影响仍有峰值，不能据此保证所有机器无掉帧。报告、各轮原始结果和 Windows 截图在 `build/stream-ui-performance/`。
- 独立可运行预览为 `dist/SerialWorkbenchStreamPreview/Start-Preview.cmd`，使用自己的配置；EXE SHA-256 为 `E0F8DEEE926D409C9E5D23DE06D2A5C9014CF0DF533CC8BE39F4BC83D2B8264F`。通过真实窗口拖动耳机夹具并经 MCP 核对相机变化，随后恢复原视角；模型手动平移 `[0,-2,-7]`、颜色和绑定保留。
- 本轮系统/MCP 串口列表为空，COM8 与上轮 ESP32 均未在线；已请求用户接回设备，当前不能声称完成修复版 MCU 实测。预览已打开，下一步连接 COM8 后验证真实 20Hz 数据下的连续拖动与切页。未提交、推送、发布或修改网站，个人预览配置和性能证据仍在 Git 忽略目录内。

## 2026-10-29 — ESP32-S3 串口与最新 3D 测试版实机联调

- 按用户要求继续原 ESP-IDF 6.1 `custom_serial` 工程，备份旧 `main.cpp` 后重写为可读的 UART0 演示：115200 / 8N1、20 Hz，24 秒依次绕 X/Y/Z 进行 0→60°→0° 演示；同一 `fixture` 流输出 roll/pitch/yaw 和等价 WXYZ 四元数、轴标记及帧号，保留原始串口回显。不输出虚构的 IMU 位移。工程内 `MODEL_DEMO.md` 同步说明字段、绑定、回显和烧录。
- ESP-IDF 6.1 编译通过；COM8 实机识别为 ESP32-S3，bootloader / partition / app 烧录摘要验证全部通过。app 为 200496 字节，SHA-256 `9e8e318fe560ec2d37b281dfe5c75f8f3f77caacf906e3a95fb7894cc879f882`。第一次手动环境缺少 ESP_IDF_VERSION，随后加载官方 export.ps1 并设置本次进程 UTF-8 后成功，原失败日志保留。
- 使用已验证的 `SerialWorkbenchIconTabsPreview` 与独立联调配置，通过其 MCP 共享真实串口并导入 1410 面耳机夹具。欧拉角 77 个状态、四元数 76 个状态逐一按实际采用的串口样本序号核对旋转矩阵，覆盖三个轴 60°；最大矩阵元素误差分别为 2.27e-7 / 1.07e-6。连续 600 帧无帧号间断，实测速率 20.012 Hz，拒绝行数为 0；发送测试文本后从 RX 原始字节确认相同 CRLF 回显。没有使用 loopback 代替实机证据。
- 已查看实际 Windows 窗口和逐轴 MCP 截图，8× MSAA 请求/实际均为 8。测试保留用户实时调整的手动平移和相机；初次验证脚本要求位置恒为零，在用户调节平移后失败，已改为核对当前绑定与手动平移之和，未回写覆盖用户位置。完成后恢复欧拉角、保持实时更新并导出版本化模型配置供新会话使用。
- 证据、截图、配置和本地联调脚本位于 `build/esp32-model-live-20261029/`；固件和说明在用户原 MCU 工程。当前工作区原有 3D / 图标 / 动效改动保留，本轮未修改工作台业务代码，未重复原生全套测试，未提交、推送、发布或部署网站。实测为生成姿态的 MCU 串口联调，不是 IMU 精度验证。

## 2026-10-29 — 透明任务栏 Logo 与模型参数标签动效

- Codex 在 `codex/model3d-telemetry` / `f3fa579` 及上一轮未提交的 3D 改动上继续，保留全部既有工作。本轮不提交、不推送、不发布，也不覆盖用户已安装程序。
- 图标：SVG/ICO 源资源本就透明；深绿色圆角底来自 `WindowChrome::captionIcon()` 的运行时绘制。去除该底层，保留多尺寸矢量 Logo 的 alpha，所有使用窗口装饰的主窗口/对话框同步受益。未改动 Logo 形状或安装位图。
- 模型参数标签：新增局部 `ModelTabs` / `ModelTabBar`，保持原生点击与键盘选择。选中圆角底和强调指示条在 180ms 内平滑移动，快速连续选择从可见位置重定向；悬停约 110ms，内容仅在参数页内部做 130ms 轻淡入，结束清除效果，不移动页面或作用于 OpenGL 视口。隐藏、布局变化及时停止/对齐，主题变化使用当前配色，系统禁用控件动画时不执行选中/内容过渡。其他标签、拖拽分离与连接逻辑未改动。
- 验证：Release 构建成功，完整 CTest **21/21**（`build/icon-tabs-final-ctest.txt`）；Windows 模型 **16/16**、窗口装饰 **5/5**。验证快速切换、键盘操作、页面几何不变、多尺寸图标透明像素，以及 Windows `WM_GETICON` 实际 HICON 的透明通道。已查看真实窗口截图，证据在 `build/model3d-tabs-polish/`、`build/model3d-tabs-windows.txt`、`build/transparent-icon-windows.txt`。
- 独立测试包：`dist/SerialWorkbenchIconTabsPreview/Start-Preview.cmd` 使用独立配置启动，避免复用运行中的旧版。包内程序与构建产物 SHA-256 一致：`93474a40e9642f3f8b0e0a7ecfa38da5ba9bb3a613a58fe539507ddfe59ff621`。HTTP/STDIO、24 工具、权限及共享读取集成验证通过（`build/icon-tabs-package-smoke.txt`），仅使用隔离配置和回环。
- 代码探索使用项目图和精确覆盖检查；记录显示部分解析及 metadata_changed，依赖处均用当前源码补核。测试包仍是 0.15.36 开发预览，未声称已更新正式安装或网站。

## 2026-10-28 — 3D 视图改进、配置交接与四元数常量修复

- 执行者：Codex；继续 `codex/model3d-telemetry` / `f3fa579`，保留上一轮仅记录的待办。用户随后明确要求实现全部待办。
- 界面：通道空项明确显示“未绑定（使用偏移常量）”，底层仍为空字段；标签面板消除额外衔接间隙。隐藏常用界面的旋转顺序与内外禀选择框，显示实际约定，保留旧配置及 MCP 参数，避免悄悄改变已有姿态。
- 视图：适配使用完整变换后的模型中心，并在手动调整相机前按视口宽高比变化重新适配；新增视角页显示、编辑方位、仰角、距离、观察中心，鼠标/适配/MCP 修改后同步，不改串口模型姿态。新增关闭/2×/4×/8× MSAA，使用动态多采样帧缓冲，GUI/MCP 报告实际档位，截图沿用请求质量。
- 姿态：四元数常量 W 原先在偏移上额外叠加默认 1，导致绕 Z 轴 60°/90° 变为 30°/45°；统一为空字段直接使用偏移常量，常量单位四元数需明确 W=1。新增 0°/60°/90°/180° 回归。
- 配置：GUI 导入/导出版本化 JSON，包含路径、字段与单位、变换、外观、相机/质量以及 Agent 字段指南和示例帧；不包含网格、密码、私钥，不自动连接设备。新增 `export_model_configuration` / `import_model_configuration` MCP 工具，共用格式校验、协议权限和最新 revision。文件缺失/损坏、非法设置、版本冲突及只读拒绝均验证；安装 skill、内置指南、中英 README、模型说明同步。
- 验证：Release 构建通过；完整 CTest **21/21**（真实用户环境，`build/model3d-refinement-final-ctest.txt`）；新增几何适配用例后相关两组再次通过。Windows 模型 **15/15**、Windows MCP **6/6**，截图已实际查看，显卡 MSAA 实际档位为 0/2/4/8。证据在 `build/model3d-refinement-windows-final.txt`、`build/model3d-refinement-mcp-windows.txt` 和 `build/model3d-refinement-captures/`。
- 首次失败保留：原生工具数量断言随新增接口从 22 调整为 24；受限账户 SSH DPAPI 失败在真实用户环境通过。新增测试保存临时 QJsonValueRef 导致访问异常，经 GDB 定位后改为持有 QJsonValue，保留最初日志；不是以重跑替代修复。抗锯齿 GL 资源重建推迟到有效当前上下文中进行。
- 独立测试包：`dist/SerialWorkbench3DRefinementPreview/SerialWorkbench.exe`。包内 CLI/后台启动、HTTP/STDIO、权限、共享读取、24 工具和 skill 安装的集成测试通过；在精简 PATH 下验证新增配置工具真实 HTTP 往返、常量四元数 Z90 和 PNG 截图。证据为 `build/model3d-refinement-{cli,package}-smoke.txt`、`build/model3d-refinement-wire/`；测试只使用隔离配置及回环，不影响用户现有实例。
- 边界：仍为 0.15.36 开发测试包，不是正式新版；未覆盖已安装软件，未提交或推送 GitHub，未发布 Release 或部署网站。未声称其他显卡、Linux 或实际 IMU 角度解算协议已实测；代码图服务断开时采用源码读取，未声称图验证完成。下一步如需交接/发布，在当前工作区先审查 diff 并按用户要求提交、同步分支或打包安装器。

## 2026-10-28 — 记录 3D 模型后续待办，暂未实现

- 用户先要求记录、暂不实现。待办汇总到 [3D 模型后续改进清单](MODEL3D_TODO.md)，包括空字段说明、标签衔接、旋转顺序入口简化、适配视图、相机参数同步、可调抗锯齿、配置导入导出及 MCP 支持、四元数角度异常。
- 起点为 `codex/model3d-telemetry` / `f3fa579`，工作区起始干净；已 fetch 并阅读入口与交接记录。图工具断开后直接读源码，确认旋转顺序实际参与计算，不能用单轴演示推断无效，也不能破坏已有配置。
- 本次只写待办文档，不修改业务代码，不运行功能测试，不提交或推送，不发布 Release 或部署网站。开始实现前以用户后续确认的范围为准。

每项任务完成或代理交接前，在本文件顶部新增记录。保持简洁，写事实与准确停点，不把聊天全文放进仓库。

## 2026-10-28 — 3D 建页窗口稳定、Agent 图标与占用升级修复

- 从 `codex/model3d-telemetry` 的 `eb36797` 继续，工作区起始干净；检查 `origin/main..origin/deepseek-dev` 无待集成提交。仍为 v0.15.36 的本地开发测试包，本轮未创建 Release 或部署网站。
- 首次给已显示窗口添加 QOpenGLWidget 会触发 Qt 原生窗口重建，已用 Windows 回归复现句柄改变。Window 在首次创建原生窗口前加入不渲染的隐藏 OpenGL 标记，主窗口及分离窗口预先选择正确表面类型。修复后建页不 Hide、不换句柄、不改变位置/尺寸；最大化、渲染、模型操作、分离/回收均通过。依据：[Qt QOpenGLWidget](https://doc.qt.io/qt-6/qopenglwidget.html#limitations-and-other-considerations) 及当前 Qt 6.11.1 源码；没有使用在本版本不足以解决问题的全局共享上下文属性。
- 补齐新增客户端的 15 种透明矢量品牌图标，26 个入口均有实际资源；单色标识随浅深主题切换，Qwen 深色使用白色标识保证可读性。图标离线打包，来源及许可证见 `resources/agents/README.md`，新增 Codicons CC BY 4.0 许可。实际查看浅/深主题和滚动到新增 Agent 的截图。
- 新增独立 Windows 原生安装维护程序，不加载目标目录 Qt/MinGW DLL。安装/卸载前按完整路径检查 EXE/DLL；明确确认后逐配置正常保存退出 GUI/后台实例，再结束同目录的 MCP 桥（含 CLI mcp）。普通 CLI 写配置等待自然退出；其他目录同名程序保持运行；无 CLI 的 GUI 可通过正常 WM_CLOSE 关闭。无响应、无法识别的相对配置、权限或其他文件锁仍提供重试/取消，不强杀工作台。
- 安装期使用 Windows delete-on-close 文件句柄阻止新的 GUI/后台/STDIO 启动，完成/取消/进程退出自动释放；保留 skill 安装流程。静默模式默认拒绝关闭，只有 `/CLOSEAPPLICATIONS` 显式授权才关闭。中英 README 和安装文档同步。维护工具由 CMake 安装、正式打包脚本包含，并加入卸载清单。
- 验证：Release 构建通过；全量 CTest 20/21，SSH DPAPI 缓存测试在受限账户异常，随后在真实 Windows 用户下独立复核通过，21 组已完成验证。Windows 首次建页/渲染组 4/4，图标组 3/3。最终包 `smoke_cli.py`、`smoke_model_mcp.py` 通过，HTTP/STDIO、权限、独立游标、后台 GUI 接续与 3D PNG 正常。
- 最终安装包隔离回归通过：中文空格目录、原路径升级、默认 skill、GUI + 自定义后台配置 + 两个 MCP 桥 + CLI mcp 占用；取消不关闭程序；其他安装目录存活；维护期禁止启动；锁定 Qt DLL 在写文件前停止，解锁后可安装；升级/卸载保留设置及用户文件。另复制修复前测试包验证旧后台/桥关闭、缺 CLI 的 GUI 正常退出。测试使用独立注册项和临时配置，没有更新用户正式安装。证据：`build/upgrade-polish/`；最初夹具失败和最终通过日志均保留。没有验证所有机器、杀毒软件或显卡环境，也未做真实设备联调。
- 最终安装包：`dist/local/upgrade-preview/SerialWorkbench-0.15.36-upgrade-preview-setup.exe`，16,959,045 字节，SHA-256 `316a24b6ca82912b6acf27240426eeb917023ad3db3788b04c9b5b3a26c1ad2c`。便携 ZIP 同目录 `SerialWorkbench-0.15.36-upgrade-preview-windows-x64.zip`，21,332,723 字节、275 文件，SHA-256 `4ba176a99b44338a7faece09995d19312126498b4c72559ae669302cf895b28e`。直接测试 EXE：`dist/SerialWorkbenchUpgradePreview/SerialWorkbench.exe`，SHA-256 `330c97b0044121440e421866b0fb02acb6f88d6b4a1fc98c98a0a286b0b54729`。ZIP CRC、逐文件摘要、5 个 EXE 的构建/便携/安装 payload 一致性验证通过；无个人设置。安装完成后需在 Agent 重新连接 MCP。


## 2026-10-28 — 3D MCP 与常见 Agent 接入，本地测试包

- 从 `codex/model3d-telemetry` 的 `ac01ff5` 继续；开始时工作区干净，检查 `origin/main..origin/deepseek-dev` 无待集成提交。用户要求 AI 能直接导入模型、设置绑定并分析，以及扩大常见 Agent 接入范围。本轮仍为基于 v0.15.36 的开发测试版，未创建新 Release、未更新网站或修改用户当前安装。
- 新增 `list_model_views`、`open_model_view`、`get_model_state`、`configure_model_view`、`capture_model_view`，共 22 个 MCP 工具。通过本机绝对路径异步导入 STL/OBJ/PLY，读取加载错误、三角面、最终矩阵、保守包围盒、颜色和采用帧序号；局部配置补丁支持精确连接/对端/命名流、位置、欧拉角、四元数、轴转换、比例、颜色与视角。导入/设置需要 AI 控制，关闭协议隐藏对应页面；revision 与正在编辑的输入保护 GUI 设置，无效补丁整体拒绝，导入失败保留旧模型。
- 截图使用与 GUI 相同的渲染器，在不显示的 OpenGL 视口返回原生 MCP PNG image 与对应状态；HTTP/STDIO 保持图片内容，不将 base64 塞进状态正文。后台实例可导入、配置、读取和截图，打开 GUI 后沿用同一页面。修正窄视口适配时模型裁边；外观/暂停更新保留最近无效帧的错误与采用序号，防止把旧姿态当作新反馈。
- Agent 页扩展为 26 个适配入口（包含不同客户端版本/模式），新增搜索、逐项配置导出和通用导出；CLI 增加 `agents` / `export-agent`。涵盖 Cursor、VS Code Local/Insiders、Copilot、Devin、Cline 新旧入口、Kilo、Kiro、Qoder、Zed、Antigravity、Qwen、Kimi 等。Windsurf 旧版、TRAE、Continue 与无法判定版本的 Kimi 采用导入；自动配置保留其他字段、先备份、重复配置幂等。官方路径/格式来源、手动入口及限制记录在 `docs/agent-integration.md`，没有声明所有第三方客户端已经逐一实连。
- 安装器及设置页支持 17 个 skill 目标；新增 3D / Agent 指南，共 5 个内嵌主题和随包文件。中英 README、模型/安装/接入文档同步。安装测试先断言全部目标位于隔离用户目录，发现 Qt Windows home 查询忽略进程 USERPROFILE 后，修正 Agent 目录解析遵循该变量；首轮在写入前中止，没有触碰真实 Agent skill 或 MCP 配置。
- 验证：Release 构建通过；最终全量 CTest 20/21，新增测试中的临时 `QJsonValueRef` 夹具悬空问题改为值拷贝后，3D MCP 组单独复核通过，21 组均完成验证。更早一轮主题组超时，单独复核 16/16，最终全量中正常通过。Windows 3D MCP 最终 **5/5**、Agent 窗口 **21/21**（其后 USERPROFILE 回归随 CTest Agent 组通过）、skill 对话框 **3/3**。已实际查看模型、窄视口、Agent 搜索/导出和紧凑 skill 对话框截图。skill 结构与 Python 测试语法检查通过。
- 打包验证：精简 PATH 下 `smoke_native.py`、`smoke_cli.py`、新增 `smoke_model_mcp.py` 通过；官方 MCP SDK 验证共享独立读取、命名流隔离、欧拉角/四元数、修订冲突、HTTP/STDIO PNG、后台无窗口与 GUI 接续。最终安装器通过 Unicode 路径安装、默认勾选新增 Agent skill、升级保留用户数据、无 SDK 启动、运行中保护、22 工具及卸载保留个人文件。只使用 loopback，未做真实 MCU/IMU 联调。初次失败、复核和最终证据均在 `build/model3d-mcp/`。
- 测试 EXE：`dist/SerialWorkbenchMcp3DPreview/SerialWorkbench.exe`，SHA-256 `98a34d3718556ac5297c40ba1712252465b1672de0346f43aa602985b93007cb`。ZIP：`dist/local/mcp3d-preview/SerialWorkbench-mcp3d-preview-windows-x64.zip`，21,134,751 字节，276 文件 CRC/逐项摘要通过，SHA-256 `77a3a0f29db1afcd200fac7ef1ea8a48d58e3551c3a7574bb09d99e0eca3bb36`。测试安装包同目录 `SerialWorkbench-0.15.36-windows-x64-setup.exe`，15,831,156 字节，SHA-256 `361492cba006dcd0b1304e437fa23175759a54b2e445a99fc3b7ea2ecc36327c`。构建、便携目录和安装 payload 的四个 EXE 摘要一致；不带个人设置。
- 使用与边界：运行新测试版，在设置 → AI / MCP 更新所需 Agent 配置并重载，才能发现新增工具；旧安装的运行进程不自动升级。截图仍需要桌面 OpenGL 环境，视觉分析取决于客户端和模型能力；数值状态可独立读取，保守包围盒不是碰撞/结构安全分析。Windows 默认安装位置、CLI 和部分 VS Code 扩展可自动检测，其他 Profile/WSL/远程/便携客户端使用导出。后续若发布正式版，需另行提升版本号、生成发行说明并执行发布流程。

## 2026-10-26 — 3D 调节修复与命名 wave 数据流，测试版 2

- 在 `codex/model3d-telemetry` 的 `840efbe` 上继续。用户反馈模型设置数字截断、位置不生效，希望独立调整颜色，并明确两个串口或同一串口的不同 wave 不能混用。本轮仍为基于 v0.15.36 的本地测试版。
- 模型设置将倍率、偏移改为整行输入，去掉固定六位尾零但保留输入精度，欧拉顺序与旋转约定分行，右侧可拉宽。新增世界坐标手动 XYZ 平移与“模型”页：跟随主题/选色盘/#RGB/#RRGGBB，颜色、平移随页面保存。
- 修复缺少角度字段导致位置整帧拒绝的问题：位置与姿态独立验证和更新，各自保留最近有效数据；无数据时手动位置、常量偏移也立即生效。多帧批次从后向前选择各部分最新有效帧，不跨帧拼接姿态分量；最后一帧无关数据不会遮住前面的有效帧。调节倍率可使用上次有效位置，不因最近仅有姿态帧而失效。
- 新增兼容格式 `@wave[imu] roll=10,pitch=20,yaw=30`。连接、网络对端、流名分别隔离，模型页选择精确流名；空名只匹配旧 `@wave ...`。曲线与实时数值显示 `[流名]/字段`，不同流的同名数值不会连在一起。每个来源最多 32 种流名，所有流共用 5000 条历史与累计序号，沿用 8192 字节/16 字段上限。
- MCP `read_waveform` 增加可选 `stream`，先筛选再限制条数，返回 `available_streams` 和命名样本的 `stream`；省略参数读取全部，空串只读旧格式。保持共享历史、协议开关与写权限。中英 README、模型/波形指南、随包 skill 和内嵌 `read_usage_guide` 同步；skill 结构校验通过。
- 验证：Release 构建通过，完整 CTest **20/20**（`build/model3d-refinement/full-ctest-initial.txt`）；补齐最近有效位置缓存后，最终 Windows 模型组 **12/12**，125% DPI 渲染组 **3/3**。已检查默认与 125% 的长数字、手动位置、颜色及浅深主题截图。保留 Qt 6.11.1 初次建窗的 wrapper texture / No fbo 诊断；随后有效 OpenGL 上下文、帧缓冲与屏幕结果正常。未做真实 MCU / IMU 联调。
- 打包：新目录 `dist/SerialWorkbenchModel3DPreview2/SerialWorkbench.exe`，保留原测试版目录；包含运行库、许可证、说明与更新的 skill，不带个人设置。最终包在精简 PATH、无 SDK 的环境中通过 `smoke_native.py`，验证 HTTP/STDIO 多流筛选、旧格式、共享独立读取、权限、搜索与脚本同步（`package-smoke.txt`）。EXE SHA-256：`e5c3351888a43129568732469119d2b18ddc56e9b46b24398e3fcdc185701955`。
- ZIP：`dist/local/model3d-preview2/SerialWorkbench-model3d-preview2-windows-x64.zip`，20,506,525 字节，273 个文件 CRC/逐文件摘要与包目录一致。SHA-256：`943ff5282c37e8d05e40e42ede0605071f462658eff662c08eed31d7d7e4056c`。证据统一在 `build/model3d-refinement/`。同名、同格式且无流标记的不同含义报文无法推断用途，需发送端标记或脚本依据真实协议识别。

## 2026-10-26 — 3D 模型与通道姿态，本地测试版

- 用户要求：导入 STL 等常见模型，通过串口通道数值设置位置和姿态，支持欧拉角、四元数与自定义 XYZ。开始时工作区干净，`origin/main..origin/deepseek-dev` 无待集成提交；从 `643e9eb` 建立 `codex/model3d-telemetry`。正式版本仍为 v0.15.36，本项未创建 Release、未发布网站。
- 新增“+ → 新建控件页面 → 3D 模型”：后台导入 STL（文本/二进制/多 solid）、OBJ、PLY（文本/大小端），三角化多边形，拒绝损坏、越界和超限文件，失败保留当前模型。限制 64 MiB / 50 万三角面，当前不导入纹理、材质或骨骼动画。
- 模型页直接读取选定连接、对端的最新 `@wave` 或脚本结果，不新开串口、不消费日志、不自动发送或启用解析器。独立绑定位置 XYZ、欧拉角 XYZ 或四元数 WXYZ，支持倍率/偏移、六种旋转顺序、内禀/外禀、角度/弧度、四元数归一化、轴交换/反向；初始模型对齐、比例和旋转原点单独设置。缺字段和无效帧保留上一有效姿态，重连等待新帧，切换来源立即更新动画定时器的数据指针。
- Qt OpenGL 视图支持鼠标旋转/平移/缩放、适配与预设视角、主题配色、标签分离/回收；模型路径、绑定、坐标系、暂停和视角随页面保存。增加 OpenGLWidgets / Concurrent 构建依赖与 Docker OpenGL 开发包；中英 README、模型指南、脚本指南和工程结构同步。
- 验证：Release 构建通过；完整 CTest **19/19**（`build/model3d-full-ctest.txt`）。随后修正来源切换与 16ms 动画定时器的短暂竞争，新增专项回归；最终 Windows 模型组 **10/10**（`build/model3d-delivery-windows-tests.txt`），离屏组 9 通过、1 渲染用例按平台跳过。覆盖 STL/OBJ/PLY、损坏文件、欧拉角/四元数、左右手轴转换、共享历史、暂停/复位、重连、来源切换、文件失败回退、页面保存恢复。
- 真实窗口：已查看浅/深主题截图，修复网格深度写入造成的模型切边和透明混合；姿态 A→B→A 的帧缓冲逐像素一致，鼠标旋转、分离及回收通过。Qt 6.11.1 首次建窗仍记录短暂的 wrapper texture / No fbo 诊断，随后有效上下文、帧缓冲与屏幕渲染验证通过；日志保留。截图在 `build/model3d-verification/delivery-captures/`。未声称其他显卡、Linux Docker 实机渲染或真实 MCU/IMU 联调。
- 测试包：`dist/SerialWorkbenchModel3DPreview/SerialWorkbench.exe`，含 OpenGL 运行库、许可证和使用说明；EXE 与构建产物 SHA-256 一致：`060f2c7a2b34dd87bf4dca7a19269e8ae2c0e043ced3ca1a751058c970417c0f`。精简 PATH 后 `smoke_native.py` 与 `smoke_cli.py` 全部通过，使用隔离配置和 loopback 验证 HTTP/STDIO、17 工具、权限、独立读取、搜索/脚本、后台复用及 GUI 交接；日志为 `build/model3d-package-smoke.txt` / `build/model3d-cli-smoke.txt`。
- 便携 ZIP：`dist/local/model3d-preview/SerialWorkbench-model3d-preview-windows-x64.zip`，21,160,769 字节，SHA-256 `b536ad8a8944d270ac4b827da9790e545a8ed250b3060130b7614201ac9b8fd0`。ZIP CRC 与 273 个文件逐项摘要匹配，未包含个人配置；核验记录 `build/model3d-verification/artifact-verification.json`。

## 2026-10-25 — v0.15.36 原生 CLI 与后台 MCP 已发布

- 用户要求：软件未打开时 Agent 可自行接入 MCP，验证后更新 skill、打包推送并通过 MCP 同步网站。独立分支 `codex/headless-cli` 已合并最新网站主线 `408bac9` 与先前 SSH 拖拽修复 `d9a8376`，保留两边历史；唯一冲突为交接日志，双方记录均保留。
- 新增原生 `SerialWorkbenchCLI`，支持 start/status/show/stop/tools/call/mcp/install-skill/configure-agent，JSON 输出与 UTF-8 参数文件。桌面支持 --headless；同一配置文件通过本地 IPC 复用同一实例，不依赖固定端口；显示 GUI 后连接、搜索、脚本和游标保持原状态。启动不连接硬件。
- 权限：沿用已保存的 AI 控制与协议开关，新配置只读；显式 start --allow-control 为新实例开启并保存权限，已有只读实例不会被命令行提权。后台 SSH 需要交互认证时取消请求，提示打开 GUI 后重连。
- STDIO 桥默认自动启动或复用，显式 --url 保留只转发行为；GUI Agent 页新增自动启动选项，配置写入当前 profile，检查识别 STDIO 与 HTTP。九类 Agent 配置使用临时文件验证保留其他字段、正确转义和幂等备份；未声称逐一完成第三方客户端实连。
- 首轮验证发现 Windows 子进程继承管道使 CLI 调用方等待，以及 Hermes 重复配置生成备份；均已修复。官方 MCP SDK 在 Windows 使用关闭即回收子进程的 Job Object，后台在这种环境通过系统 WMI 独立创建，验证 Agent 退出后服务仍可共享；无开机任务、Windows 服务或权限修改。WMI 不可用时明确报错，不绕过系统访问控制。
- 验证：完整原生 CTest 18/18；沙箱账户的 DPAPI 缓存测试失败后，在正常用户环境复核通过，保留首次日志。CLI 使用完整打包运行库通过并发启动、动态端口、无窗口、权限拒绝、HTTP/STDIO 独立读取、loop:// 回环、大小写/逻辑/正则搜索、脚本试解析/应用与 GUI 接续、停止/重启；扩展的 CLI skill 安装和 Agent 配置也通过。Windows Agent 面板实际查看截图，4/4 通过。skill 结构校验通过；证据位于 `build/cli-verification/`。
- 指南：skill 新增 references/cli.md 并嵌入 read_usage_guide(topic="cli")，安装器和 GUI skill 更新同步；中英文 README 与安装/Agent 文档已更新。包清单纳入 CLI，安装器检查运行中的 CLI，离线回溯更新到 0.15.35/34/33。
- 发行验证：最终 payload 的 CLI 全流程在无 SDK 环境通过；隔离安装、升级、运行中保护、卸载保留用户文件全部通过，便携 ZIP 与单文件的 Unicode 路径、首次/缓存启动、损坏缓存修复和 MCP 测试通过。最终安装包为 15,555,295 字节，SHA-256 `25d652923248ab6ce69f2b6a2e340a0c266b298d878cd4af4bc897374c2cadf9`。完整日志 `installer-smoke.txt`、`release-smoke.txt`、`cli-package.txt` 位于上述证据目录。
- GitHub：正式 v0.15.36 指向 `e2a1922`，已普通推送 main、deepseek-dev、codex/headless-cli 与标签；保留 DeepSeek 和网站历史。四个 Release 资产的远端大小与 SHA-256 digest 全部匹配本地，latest 为 0.15.36，非草稿/预发行。
- 网站：使用最终包 CLI 从关闭状态启动用户原配置的隐藏 MCP，并通过其 SSH 连接实际部署；沿用已开启的 AI 控制，不需先手动打开 GUI。服务器从本机接收已校验的安装包，不拉取 GitHub。完整公网 HTTPS 下载为 15,555,295 字节，SHA-256 与本地/GitHub 一致。
- 页面：更新 CLI 说明、版本、下载链接与摘要；通过 MCP 原子切换网站，旧 release 保留回退。七个公网 HTML 摘要吻合，两个原片及播放文件的大小、MIME、媒体目录链接未变，ICP/公安备案与图标保留；实际浏览器核对版本、下载/校验链接、说明及视频入口，截图 `build/cli-verification/website-0.15.36.png`。
- 接续：实现和发行已完成；本机 Codex skill 已更新，已有修改先备份。其他用户升级后在 Agent 接入页选择后台自动启动、更新 skill 并重载 MCP；第三方 Agent 的逐版本实连与真实串口硬件仍未覆盖。网站专用维护工具、连接配置、凭据与视频继续仅本地保存，不提交 Git。

## 2026-10-23 — 校庆视频改为 4K 原片下载

- 用户指出“下载视频”实际下载的是 1080p 播放版，授权改为原片。两个下载按钮改为“下载 4K 原片（1.71 GB / 1.72 GB）”，保存名沿用用户原始文件名；在线播放继续使用独立 1080p 文件。除两个下载链接外，七页 HTML、主题、页脚及软件版本清单保持原内容。
- 通过工作台 MCP 确认当前 SSH 会话后上传两个原片，本地 SSH 仅作加密端口转发。原片未经转码，大小分别为 1,706,330,343 / 1,715,342,420 字节；SHA-256 分别为 `ef0998ee725dbfe697bfcb5d03de9d3de87ccfdf5745f5577fbb256d36d12318` / `72e22bc6cb778f4e8ef3dc7590dfa3efc2600a0ee3bf80f63c1bc563f5085103`，服务器完整校验与本地一致。原片、播放版及私有上传工具均不提交 Git。
- 首次最终校验失败：通用部署脚本 `digest()` 使用 `read_bytes()`，把 GB 级原片整块读入内存，服务器内核确认因 OOM 终止了 Python 校验进程；发布记录仍为未激活，原站点保持可用。修复为 1 MiB 分块 SHA-256，复用已上传文件与已暂存 release，经完整摘要复核后恢复原子激活，不重传、不重写历史。实际校验进程峰值 RSS 为 17,360 KiB。
- 回归：新增摘要测试，256 MiB 文件摘要准确且峰值 Python 分配低于 8 MiB，空文件摘要正确，**2/2** 通过；本机实测大文件校验峰值分配 2,229,026 字节。构建及七页内容检查通过，证明仅两个下载锚点变化、播放 source 不变。
- 公网验证：七个 HTML 摘要匹配部署，两个原片 HEAD 大小正确、类型 video/mp4；开头/中段/末段各 64 KiB Range 请求均返回 206，字节与本地原文件一致。两个 1080p 播放文件大小与软件更新清单保持原值。浏览器确认两个按钮文案、原片 URL 和原文件保存名正确，下载入口截图及检查记录位于本机 `build/anniversary-originals/`。
- 网站切换完成，旧 release 保留可回退；一次性接收端、转发及上传临时文件已清理。源码使用独立分支 `codex/website-original-video-downloads`，未改本地 SSH 测试分支。

## 2026-10-23 — 页脚校庆视频 001 / 002

- 用户提供两个校庆视频，要求放在网站小角落、小字入口并编号 001 / 002。共享页脚在原链接下方靠右显示“校庆视频链接 001 002”，不加入主导航；所有七个页面（含 404 和新增两个播放页）均保留 ICP / 公安备案链接及图标。
- 两个原片均约 1.7GB、4K / 60fps HEVC。保留本机原片，生成 1080p / 30fps H.264 + AAC 播放版及 JPG 封面，时长 04:01 / 04:03，MP4 moov 前置。播放页沿用浅深主题，提供编号切换、原生播放/全屏/进度控件和下载，不自动播放，预加载仅 metadata。
- 上传：通过用户当前工作台 MCP 的已确认 SSH 会话创建仅回环监听的一次性接收端，本地 SSH 仅作加密端口转发。两个 MP4 分别为 185,840,726 / 187,609,686 字节，服务器大小及 SHA-256 与本地一致；封面同样校验。媒体独立保存，页面 release 使用相对软链接，后续静态发布应保留媒体与链接。上传端已退出、转发已停止、临时文件已清理。
- 发布：复制当前 release、保留旧资源及安装包，仅替换现有页面页脚和 CSS 引用，加入两个播放页和主题样式。原子切换前核验全部 12 个文件摘要，旧 release 留作回退。专用转码/上传工具、媒体原片及播放文件未提交 Git，公开仓库仅包含页面、样式和说明，并忽略媒体目录。
- 验证：静态构建、七页入口/相对路径/备案检查、现有线上正文未改动校验通过；公网七个 HTML 与新 CSS 摘要吻合，两个封面摘要正确。视频 MIME 为 video/mp4，大小准确，开头/中段/末段 Range 请求均返回 206 且字节匹配。浏览器验证两个视频加载到 1920×1080、播放进度推进与键盘跳转后恢复可播放状态、编号切换及浅深主题；桌面无横向溢出，窄屏样式已检查源码，未声称手机实机测试。下载版本清单逐字段保持原值 v0.15.35。
- 本机证据：`build/anniversary-videos/` 中保留转码、传输、发布及公网核验记录，主页页脚与播放页截图。网站修改使用独立分支 `codex/website-anniversary-videos`，不包含本地 SSH 拖拽测试包改动。

## 2026-10-20 — 网站公安备案号与图标上线

- 用户确认公安备案通过，配置 `川公网安备51162302000307号`，查询地址采用用户提供的 `https://beian.mps.gov.cn/#/query/webSearch?code=51162302000307`，保留新窗口打开与 noreferrer/noopener。
- 使用用户提供的备案 PNG 原图（36×40，SHA-256 `8dfecad0dfcb3dc584f2c2447943eefb1fd65a058856eb0611e2c56ddc4c1fe1`），按 18×20 等比显示在号码前，与号码共用链接。共享页脚在主页、项目、下载、关于及 404 页面构建时写入，不依赖 JavaScript。部署校验同时支持办理中状态及已批准的图标/号码/查询 code 一致性。
- 验证：静态构建与五页链接/相对资源检查通过；Linux 临时目录回归四种场景通过（已备案、办理中、缺少图标拒绝、查询 code 不一致拒绝），确认发布切换保留旧安装包。
- 部署：通过 Serial Workbench MCP 的已确认 SSH 会话上传并校验五个 HTML 与图标，只替换线上备案占位，其余页面内容逐字保留；复制现有 release 后原子切换，保留旧 release 可回退，上传临时文件已清理。
- 公网：五页 HTML、图标响应摘要全部匹配部署内容，多层不存在 URL 正确返回 404 且显示备案页脚；浏览器确认链接和图标加载成功，原图 36×40、显示 18×20。下载清单逐字段与部署前一致，仍为正式 v0.15.35。核验记录及截图保存在本机 `build/website-police/`。本项使用独立网站分支，不包含本地 0.15.36 SSH 修复测试包，也未公开专用维护脚本。

## 2026-10-20 — SSH 拖拽跨屏选中与 0.15.36 测试包

- 用户反馈：SSH 终端无法向上拖动鼠标，连续选中并复制大量历史行。独立分支 `codex/ssh-drag-selection` 从已同步的 `7042e77` 开始，保留本地私有维护脚本及忽略规则。
- 原因：`mouseMoveEvent` 只用 `cellAt()` 将位置夹在可见行，没有越界后的滚动；滚轮滚动也未同步固定指针下的选区端点。
- 修复：40ms 定时器在左键拖过文本区域上下边缘时持续滚动，按越界距离调整速度，固定文档选区起点、随滚动更新终点；返回区域内或到达历史边界时停止。滚轮拖拽同步扩展选区。拖拽期间新输出不拉回底部；松手、清空/重置、隐藏、失去鼠标捕获、窗口停用或切换替代屏均停止自动滚动。复制和选择不发送远端输入，原有键盘选择、IME、历史上限及独立读取保持原逻辑。
- 回归证据：修改前新增用例确认向上拖动后滚动条仍停在 100，失败记录 `build/ssh-drag-selection/before-fix.txt`。首次实现测试发现 followLatest 文本替换错误与拖拽测试列坐标偏差，已修正，保留 `after-fix.txt`。随后全量 CTest 其余 17 组通过，终端集成用例误将模拟 SSH 欢迎行假设为空；改为对比实际首行、精确 208 行及 output 1 内容后，终端组 **37/37** 通过，首次集成失败留存 `first-ctest-terminal.txt`。
- Windows 实窗口：运行完整终端组并查看独立终端及工作台截图；新增 109 行和 SSH 页面 208 行的跨屏选择、剪贴板精确匹配、滚轮、反向拖动及六种停止场景均通过，发送信号数为 0。首次全组 36 通过、1 个原有剪贴板用例遭 Windows `OpenClipboard` 临时占用；单独复核该用例 **3/3** 通过。原始与复核日志分别为 `windows-terminal-tests.txt` / `windows-clipboard-recheck.txt`。截图位于 `build/ssh-drag-selection/captures/`，使用隔离配置和模拟 SSH，不涉及真实服务器操作。
- 测试包：`dist/local/0.15.36-ssh-drag/SerialWorkbench-0.15.36-windows-x64-setup.exe`，15,480,223 字节；SHA-256 `cd444677f42827ff01a253bb591796cceaa0b11034f2b5272628c3da43b64358`，同目录有校验文件。构建、部署及最终 payload 的 EXE 摘要一致，记录 `artifact-verification.json`。
- 安装验证：最终 payload 的 smoke_installer.py 全部通过，覆盖隔离 Unicode 路径安装、原路径升级与配置/用户文件保留、运行中保护、无 SDK 启动、Agent skill、HTTP/STDIO、17 个 MCP 工具、共享独立读取、原生搜索与脚本、卸载保留个人数据。日志 `build/ssh-drag-selection/installer-smoke.txt`，未覆盖用户当前运行的正式应用。
- 范围：0.15.36 为本地测试安装包；中英 README 与 CHANGELOG 已同步。未发布正式 Release、未改网站、未上传私有维护工具；改动保存在上述独立分支。

## 2026-10-19 — 网站维护脚本改为仅本地保存

- 用户要求从 GitHub 移除专用网站同步脚本。已保留本地副本及独立备份，对原脚本路径增加 Git 忽略规则，移除公开分支中的专用同步入口、接收辅助脚本及其专用测试；通用镜像校验代码保留。
- 中英文 README、开发入口及镜像/网站说明不再指向私有脚本，并明确未经用户授权不得重新提交这类专用工具。详细本机维护记录随私有备份保存。
- 网站已发布版本、安装包、服务器本地检查模式及 SSH 会话不变。本次通过普通删除提交同步 main 与 deepseek-dev；未重写历史，旧提交仍包含先前公开内容。

## 2026-10-19 — v0.15.35 已发布，MCP 手动上传网站镜像

- GitHub：正式 [v0.15.35](https://github.com/lilinng-163/serial-workbench/releases/tag/v0.15.35) 已公开，非 draft / prerelease；tag 指向 `f9adb02`。`main` 与 `deepseek-dev` 普通推送到该发行提交，保留 DS 和网站提交历史，没有强推或删除分支。安装包、单文件 EXE、便携 ZIP、SHA256SUMS 四个远端资产的大小与 GitHub SHA-256 digest 全部匹配本地；从公开 GitHub 链接完成整个 setup 下载并复核摘要。
- 用户补充国内服务器拉取 GitHub 会超时，要求手动传文件。已通过 MCP 的 `t_ecs` SSH 创建只监听服务器回环地址的一次性接收端；本地 SSH 仅做端口转发，将正式本地 setup 原样上传。网站控制、校验、远端发布及页面切换均走 Serial Workbench MCP，没有通过本地 SSH 执行网站命令。接收端按 15,477,900 字节及 `deef435aa71f1f12ca787dd170dd5b4ea15685e73f9dee0aa4420ac94e3b3ac6` 验收。
- 此前手动触发的自动同步从 17:02:42 持续到 17:12:42，因既有 `TimeoutStartSec=10min` 超时退出。手动发布任务使用同一个 flock 等待释放，没有关闭定时器；之后用已上传文件发布，显式禁止手动发布函数再从 GitHub 下载。setup 先就位并校验，再原子更新 `latest.json`、单包 `.sha256` 和 `SHA256SUMS.txt`。
- 网站：五页 HTML 经校验后，`current` 原子切到 `releases/20261019-release-01535`；旧 `releases/20261019-icons-motion`、旧安装包和摘要资源保留。HTML 后备版本、安装包链接、大小、日期、摘要均为 0.15.35，动态更新说明与 GitHub 一致。备案号及工信部链接保留。
- 公网验收：`https://www.lilinng.cn/updates/latest.json` 与 `https://162.14.72.194/updates/latest.json` 均返回 0.15.35，大小与 SHA-256 一致；从域名 HTTPS 完整下载 setup，逐字节摘要匹配本地/GitHub。五个 HTML（含实际 404）与五个 CSS/JS/SVG/PNG 资源的公开响应摘要匹配本次构建；两个校验文件内容准确。实际浏览器显示 v0.15.35、正确下载链接、摘要及新说明。公开验证记录、完整下载与页面截图在 `build/release-0.15.35-verification/`。
- 服务器留存：`/srv/lilinng-web/deployments/release-0.15.35-manual-upload.json` 记录来源 `manual-local-upload`；发布记录标明当前/回退路径。一次性接收端已退出，无新增公网上传端口；临时传输文件清理，发布日志留在 deployments。`lilinng-release-sync.timer` 仍 enabled + active，后续定时检查保留；不保证国内 GitHub 下载一定及时完成，手动发布步骤已补充到镜像说明。
- 完成状态：GitHub 正式发行、主线/DeepSeek 分支同步、MCP 手动上传网站镜像和公网验证均完成；当前用户工作台及 SSH 会话保留。本次未更改设备协议或进行真实 MCU 联调。

## 2026-10-19 — 0.15.35 正式发行验证与主线集成

- 用户授权：将本次主题修复推送 GitHub 并更新网站；网站/服务器操作必须走 Serial Workbench MCP。
- 集成：保留 `origin/main..origin/deepseek-dev` 已发布提交和 `6b445c7` 交接记录；在 v0.15.34 基础上加入 `df67a64` / `ec5b420` 主题修复。合并 `codex/website-ui-20261019` 的三项已部署网站提交及五分钟镜像定时器，冲突只涉及开发记录与镜像说明，保留双方历史。未重新引入用户要求丢弃的 DS 未提交改动。
- 发行准备：中英文 README、CHANGELOG、DEEPSEEK 与 `docs/releases/0.15.35.md` 同步；离线版本回溯更新为已发布的 v0.15.34 / v0.15.33 / v0.15.32，避免在当前程序中嵌入自身安装包摘要。网站 HTML 后备版本、下载路径、大小、日期与 SHA-256 更新到 0.15.35；实际 v0.15.32 展示截图仍保留准确版本标注。
- 最终验证：Release 构建成功，完整 CTest **18/18**；原生 Windows 主题 **16/16** 和浅深截图已在前两项记录验证。正式 payload 重新完成 `smoke_installer.py` 全部检查（隔离安装、原路径升级、配置/用户文件保留、运行中保护、Agent skill、HTTP/STDIO、17 工具、共享独立读取、权限、原生搜索/脚本及卸载）；`smoke_release.py` 全部通过（便携 ZIP 全文件清单、无 SDK、单文件 Unicode 路径首次/缓存启动、MCP 与缓存损坏修复）。构建 EXE 与正式 payload SHA-256 一致。镜像回归 **3/3**，网站五页备案页脚、资源引用和静态后备版本校验通过。
- 正式产物 `dist/local/0.15.35/`：
  - setup EXE：15,477,900 字节，SHA-256 `deef435aa71f1f12ca787dd170dd5b4ea15685e73f9dee0aa4420ac94e3b3ac6`。
  - 单文件 EXE：15,006,786 字节，SHA-256 `668a28097ae9e3272b6c20584327be419a5bd1d2ae531d494e92d9a5692e3723`。
  - 便携 ZIP：20,600,127 字节，SHA-256 `7ca66f90ef413ccea7c3a84b950c2a7e18549a6ab29058f715efc3657205ce25`。
  - `SHA256SUMS.txt`。正式 payload：`build/release-0.15.35-fa59d3ce/SerialWorkbench/`；验证日志：`build/release-0.15.35-verification/`。
- 当前停点：正式包验证完成；下一步普通推送 main/deepseek-dev 和 v0.15.35，上传并核验 Release 资产，通过 MCP 的 t_ecs SSH 同步网站镜像，再补记公网核验事实。当前运行中的用户工作台不受安装测试影响。

## 2026-10-19 — 协议下拉框底色与输入框统一

- 用户反馈：“串口”协议下拉框始终单独一块颜色。
- 原因与修复：`QComboBox#protocolSelector` 单独指定 `$hover` 背景和 `$edge` 边框；移除这两项覆盖，沿用所有输入框的 `$panel` 背景、普通/悬停/焦点边框，仅保留原内边距和尺寸。
- 验证：增量构建成功；Windows 实平台主题测试 16/16 通过，实际查看浅色与深色工作区截图；最终打包 payload 的 smoke_native.py 通过 HTTP/STDIO、17 个工具、独立读取、权限与原生搜索/脚本工作流。打包 EXE 与构建 EXE 的 SHA-256 一致。未重复前一版安装器逻辑未变的升级/卸载全套测试。
- 测试安装包：`dist/local/0.15.35-protocol-color/SerialWorkbench-0.15.35-windows-x64-setup.exe`，15,479,278 字节；SHA-256 `66be1e86aeb5f0f1f0394907d637413437e7bd5349eca21e744d8091270fa046`。同目录有校验文件，截图和验证记录在 `build/protocol-color-0.15.35/`。
- 范围：仍为 0.15.35 本地开发包，没有修改协议连接行为或发布正式 Release。

## 2026-10-19 — 从 v0.15.34 重建主题修复，0.15.35 测试安装包

- 执行者：Codex；分支 `codex/theme-polish-0.15.34`，严格从 `v0.15.34` / `e323f0a` 创建。按用户要求丢弃 DS 的四个未提交文件改动（controls.cpp / controls.h / theme_settings.cpp / theme_color_tests.cpp）；原远端开发历史和发布标签保留。
- 用户目标：在 0.15.34 基础上优化配色一致性、文字清晰度和浅深主题，并提供安装包。
- 修复：所选撞色继续作为按钮填充和保存原值；为链接、图标、TX 日志、波形、焦点边框生成同色系的可读颜色，避免白色撞色在浅主题、黑色撞色在深主题中消失。跨背景/面板/侧栏/悬停表面校验文字与图形对比；选区同时保证背景区分和文字可读。统一工作台、弹窗、分离窗口、工具提示、禁用文字和收发小窗的 QPalette；深色数字箭头、菜单黑白勾选同步。撞色可以不更改主色而独立设置，悬停同步。
- JSON 助手：原说明链接写死绿/橙色，改为读取根窗口主题 Link；通过根窗口事件监听解决 QTextBrowser 样式调色板屏蔽父 Link 的问题。首次针对测试发现系统蓝色回退，失败证据保留为 `build/theme-polish-0.15.35/json-before-palette-source-fix.txt`；修复后 JSON 测试通过。
- 验证：最终原生 Release 构建完成且增量检查无待编译文件。完整 CTest 18/18 通过；随后 JSON 链接修复已重新构建，并重跑 json_tools_tests 通过。Windows 实平台 theme_color_tests 16/16，通过颜色矩阵、双色持久化/恢复、无主色独立撞色、浅深极端颜色、选色盘、窄设置、链接与收发小窗；实际查看白色浅主题、黑色深主题、窄窗口和深色选色盘截图。证据 `build/theme-polish-0.15.35/`。
- 安装验证：最终 payload 运行 `smoke_installer.py` 全通过：隔离 Unicode 路径安装、原路径升级、配置/用户文件保留、无 SDK 启动、运行中保护、HTTP/STDIO、17 个 MCP 工具、共享独立读取、原生搜索/脚本及卸载。构建 EXE、部署 EXE、打包 payload EXE 的 SHA-256 一致。使用测试注册项和临时配置，没有覆盖正式安装或真实 Agent 配置。
- 产物：`dist/local/0.15.35-theme-polish/SerialWorkbench-0.15.35-windows-x64-setup.exe`，15,486,853 字节；SHA-256 `26634c79dba14823813d33388c01f9b55451825eaae536dc0d6a8a21228fd7fa`；同目录 `SHA256SUMS.txt`。最终 payload `build/release-0.15.35-55c1cf44/SerialWorkbench/`。
- 范围：0.15.35 为本地开发测试包，没有创建正式 Release、改动网站或合并 main；未同步 deepseek-dev。未进行真实硬件联调（本次是主题修复）。
- 下一步：用户试用安装包；如要求发布，再按发行流程审查并发布。

## 2026-10-19 — 发布 v0.15.34：双色主题与网站镜像

- 执行者：DeepSeek；分支 `deepseek-dev`；发布提交 `e323f0a`；tag `v0.15.34`。
- 用户目标：主题色拆分为主色/自定义撞色，修正撞色组件与默认主题的一致性，然后发布新版 Release 并更新网站/镜像。
- 实现：
  - 主色只负责背景、面板、文字和 rail 底色的色相；撞色接管默认橙色强调色所在的组件：主按钮、工具栏动作图标、链接、选中、焦点、勾选、滑块、进度、发送、TX、关于页链接、rail 激活图标、波形第一条线。
  - 设置 → 外观 → 主题色提供“主色（背景/面板）”和“撞色（按钮/链接/选中）”两行，均支持选色盘、`#RRGGBB`/`#RGB`、应用和恢复默认；撞色未设置时使用浅色 `#DC8C38` / 深色 `#EDAA50`，不再自动计算互补色。
  - 菜单、列表、页签、下拉项、树形项的悬停/预选背景统一使用撞色浅色；选中页签底色/文字、勾选文字、描边按钮悬停/焦点恢复默认角色。
  - 颜色输入框右侧清除按钮上移；新增 13 项主题测试，覆盖双色持久化/恢复、撞色组件、预选颜色、窄设置和选择器。
- 发布：GitHub Release [v0.15.34](https://github.com/lilinng-163/serial-workbench/releases/tag/v0.15.34)，非 draft、非 prerelease；tag `v0.15.34` 指向 `e323f0a`。资产：
  - `SerialWorkbench-0.15.34-windows-x64-setup.exe`（15,481,362 字节），SHA-256 `871b35fc9c4413fb98d7ce2fc10ced9f390bb2da5d79b28070f8d3379e3dd224`
  - `SerialWorkbench-0.15.34-windows-x64.exe`（15,010,868 字节），SHA-256 `92d2908b8828c220a1fd1af313df99aa172d1fe2715a6da56558d5ab432225fc`
  - `SerialWorkbench-0.15.34-windows-x64-portable.zip`（20,594,727 字节），SHA-256 `0cdb0ed2da09efdfe2cb7fb84909c69c2d9aab1a0d060db2f75c12394dd3a1ed`
  - `SHA256SUMS.txt`
- 验证：完整 **18/18 CTest**、Windows 实平 `theme_color_tests` **13/13**；`.venv` 下 `smoke_installer.py` 全过（安装/升级/自定义 Unicode 路径/保留设置/运行中阻止覆盖/HTTP/STDIO/MCP 权限/17 工具/卸载），`smoke_release.py` 全过（便携 ZIP、单文件首次/缓存启动、MCP、损坏缓存修复）。
- 国内镜像：`t_ecs` 的 `/srv/lilinng-web/www/shared/updates` 已发布 `0.15.34` setup、`.sha256`、`SHA256SUMS.txt` 和完整 `latest.json`；`https://162.14.72.194/updates/latest.json` 与 `https://www.lilinng.cn/updates/latest.json` 均返回 `0.15.34`，公网完整下载 SHA-256 与本地一致。同步 timer 已恢复为 enabled + active，`.github-etag` 已更新，旧版本安装包保留。
- 范围/风险：`main` 仍停留在 v0.15.32，未合并；网站静态 fallback 仍可能显示旧版本，下载页通过 `/updates/latest.json` 动态更新；Codex 仍需审查 `origin/main..origin/deepseek-dev` 并集成主线。
- 下一步：Codex 审查提交、tag 和 Release 资产，确认后合并 `main`；本记录提交晚于发布 tag，保留在 `deepseek-dev`。

## 2026-10-19 — 发布 v0.15.33 并交接 Codex

- 执行者：DeepSeek；分支 `deepseek-dev`，起点 `7ea6ab7`。
- 用户目标：完成主题色开发，发布 GitHub Release，并写交接文件给 Codex。
- 发布：GitHub Release [v0.15.33](https://github.com/lilinng-163/serial-workbench/releases/tag/v0.15.33)；tag `v0.15.33`。资产：
  - `SerialWorkbench-0.15.33-windows-x64-setup.exe`（15,465,318 字节），SHA-256 `444b7c931c5c9ee483fe5a2eabc394cc77428881b2d744cb3d935f45300aa843`
  - `SerialWorkbench-0.15.33-windows-x64.exe`（15,002,683 字节），SHA-256 `637d626edafb49e548b5ae76cb425eadf6349dfdac28e4b131948a039dff6ef2`
  - `SerialWorkbench-0.15.33-windows-x64-portable.zip`（20,591,875 字节），SHA-256 `a5fc8135006928b68892bd87455877ee27b3d8bbc437bc447ceb2b414e7485f6`
  - `SHA256SUMS.txt`
- 内容：设置 → 外观 → 主题色（选色盘、`#RRGGBB`/`#RGB`、格式提示、恢复默认）；自定义色同步背景/面板/按钮/选中/标题栏/关于页链接/分离窗口/串口小窗，按钮文字按亮度选黑白。修复深色主题选色盘仍为浅色、关于页 Link 被滚动区/系统调色板覆盖的问题。保留共享 GUI/AI 连接、独立读取、17 个 MCP 工具、权限、搜索与脚本行为。
- 验证：Release 构建成功；完整 **18/18 CTest**；Windows 实平台 `theme_color_tests` **11/11**（新增极端颜色、深色选色盘、窄设置、关于链接、串口小窗）；`smoke_installer.py` 全部通过；`smoke_release.py` 全部通过（便携 ZIP、单文件、缓存修复、HTTP/STDIO、17 工具）；截图在 `build/theme-captures/`。
- 范围：只发布 GitHub Release；网站与国内镜像未更新；未把 `deepseek-dev` 合并到 `main`。`main` 仍为 v0.15.32。
- Codex 下一步：fetch 后审查 `origin/main..origin/deepseek-dev`（含本发布提交与 tag），在从 `main` 建立的临时分支集成并保留提交历史；运行原生/主题/安装/发布验证；确认后将 `main` 普通推送并同步回 `deepseek-dev`；保留 v0.15.33 tag 与 Release 资产。网站/镜像如需更新须另行授权。
- 风险：背景图检查使用应用截图作测试背景，真实素材、真实串口与外部 SSH 仍需用户环境验证；安装器从 `deepseek-dev` 构建，`main` 在合并前仍是旧版。
- 提交与同步：本记录随发布提交；推送 `deepseek-dev` 与 tag `v0.15.33`。

## 2026-10-19 — 主题色验收、细节修复与 0.15.33 测试安装包

- 执行者：DeepSeek；分支 `deepseek-dev`，起点 `7ea6ab7`。
- 用户目标：接替 Codex 继续完善主题开发；功能完成后提供 Windows 安装包。
- 已完成：
  - 重新增量构建（含交接时尚未编译的 `window.cpp`）；完整 **18/18 CTest** 通过。
  - Windows 实平台 `theme_color_tests` **11/11** 通过；新增验收：极端浅/深色可读性、深色选色盘、窄设置主题行、关于页链接跟随强调色、串口小窗同步。
  - 修复：`QColorDialog` 增加父设置窗口的 palette 和界面字体，深色主题不再弹出浅色选色盘；`applyTheme` 在给顶层对话框应用 stylesheet 后再次同步主题色控件；`syncThemeColorControls` 显式设置 `aboutSources` 的 `Link`/`LinkVisited`，避免滚动区沿用系统链接色。
  - 截图：`build/theme-captures/theme-color-picker-dark.png`、`theme-about-links.png`、`theme-settings-narrow.png`、`theme-serial-monitor.png`、浅/深设置与工作区；另以背景图 + `#814fc4` 做了实窗兼容检查。
- 安装包：
  - `dist/local/theme-color-0.15.33/SerialWorkbench-0.15.33-windows-x64-setup.exe`
  - SHA-256：`43fada7ceffe0bdbc3ed15b586e745b30ab777a21b2fbe0e22195ae938af43f2`
  - `smoke_installer.py` 全部通过：透明双层图标、Agent skill 组件、Unicode 自定义路径与品牌卸载器、升级发现原路径并保留设置/用户文件、无 SDK 启动与运行中保护、HTTP/STDIO 共享独立读取与写权限、原生搜索/脚本同步/17 工具、卸载清理并保留个人设置和用户文件。
- 未完成/范围：未创建或发布正式 Release，未合并 `main`，未改网站或国内镜像；安装包为隔离测试用途。背景图检查使用应用截图作测试背景；真实素材与真实串口/SSH 仍需用户环境验证。
- 提交与同步：见交接回复中的提交哈希；推送 `deepseek-dev`。
- 下一步：Codex 审查主题色与安装包，决定是否发布 0.15.33。

## 2026-10-19 — 自定义主题色开发中，交接 DeepSeek

- 执行者：Codex；开发分支 `codex/theme-color-20261019`，起点 `2df9506`；用户要求额度耗尽前同步 `deepseek-dev`，由 DS 继续。
- 用户目标：软件设置提供换主题色，支持选色盘及 `#` 十六进制输入。延续用户要求：功能完成后必须提供 Windows 安装包。
- 已实现但尚未验收：
  - `src/ui/theme_settings.cpp`：设置 → 外观 → 主题色，选色盘、`#RRGGBB` / `#RGB` 输入、无效格式提示、恢复默认；离开输入框/应用/回车保存，回车不关闭设置；色盘取消不应用。
  - `Theme::make(bool, QColor)`：自定义浅深背景和强调色，按钮文字按相对亮度选黑白，侧栏活动图标同步。无自定义色时保留原有配色。
  - `Window`：保存/恢复 `appearance.theme_color`，分离窗口及设置控件同步；更新弹窗和标题栏，避开系统文件选择窗口。关于页链接改用 Palette Link 以跟随换色。
  - 新增 `tests/native/theme_color_tests.cpp` 和 CTest 注册：格式/对比度、输入保存恢复重置、色盘接受/取消、分离窗口同步、错误存储回退；支持环境变量 `SW_THEME_CAPTURE_DIR` 保存实窗截图。
  - CMake 版本已升到 `0.15.33`，CHANGELOG 标记开发版，中英 README 已补使用说明；没有创建或发布正式 Release。
- 实际验证与准确停点：
  - `git diff --check` 通过。
  - 首次构建因 Qt moc 未识别内联测试类而链接失败；将测试函数实现移到类外后，手工 moc 生成成功，第二次构建完成（含 theme_color_tests.exe）。
  - **最后一次修改 window.cpp（关于页链接 / Palette Link）发生在其目标文件生成后：window.cpp 12:57:38，window.cpp.obj 12:57:19。必须再增量构建一次，不能视作最终源码已编译通过。**
  - 尚未运行新增测试、完整 18 组 CTest 或 Windows 实窗检查；未制作/验证安装包。当前代码属于待验证开发进度，不是已完成成品。
- DS 下一步：
  1. 阅读本记录及 DEEPSEEK.md，检查源码 diff；在 `deepseek-dev` 接续，不覆盖用户配置和连接。
  2. Qt/MinGW 路径：`D:/lib/Qt/6.11.1/mingw_64`、`D:/lib/Qt/Tools/mingw1310_64/bin`；将二者 bin 加到 PATH，执行 `cmake --build build/native-release --parallel 4`，随后运行 `ctest --test-dir build/native-release --output-on-failure`；保留并修复首次失败证据。
  3. 用 Windows 平台运行 theme_color_tests（不带 offscreen），设置 `SW_THEME_CAPTURE_DIR` 到 build 下，检查浅深主题、色盘、窄设置窗口、分离窗口和收发小窗；核查极浅/极深颜色的可读性及背景图兼容。
  4. 使用独立分发目录制作 0.15.33 测试安装包，避免覆盖已运行的程序或原有正式包。`scripts/package-release.ps1` 可用 QtSourceRoot `D:/lib/Qt/6.11.1/Src`、MakeNSIS `E:/MinGW/w64devkit/bin/makensis.exe`、`-InstallerOnly`；按 docs/windows-installer.md 执行隔离安装/升级/卸载检查并提供安装包和校验值。
  5. 更新本日志写清真实验证结果，提交并推送 deepseek-dev；用户本次要求交接，不授权发布正式 Release 或合并 main。
- 范围：只包含桌面程序主题色开发；网站改动保留在独立 `codex/website-ui-20261019`（903e853），没有带入本分支，也不需要更改服务器。
## 2026-10-19 — 网站图标、动效清晰度与实际软件截图

- 执行者：Codex；分支 `codex/website-ui-20261019`，起点 `1bcc969`。
- 用户目标：网站标志含义不明，软件标志需透明底；跟随鼠标的图形模糊、浅色效果弱并上下跳动；软件展示图需更新。
- 改动：网站标志改为“芯片＋波形”，去掉软件图标的绿色 CSS 底板及内边距。鼠标命中区域不再做 3D 变换，卡片、芯片和文字取消旋转，前景玻璃取消模糊；仅背景光晕、装饰环和高光跟随指针，浅色增加蓝色折光对比。首页和项目页换用正式 v0.15.32 程序新捕获的 1400×960 波形界面，注明演示数据。SVG 和 PNG 与 CSS/JS 一样使用内容摘要文件名。
- 验证：构建、JS 语法和资源链接检查通过；浏览器在四角依次触发鼠标移动，stage/wave/packet 的位置尺寸保持一致，stage transform 始终为 none，高光坐标正常改变；浅深主题确认软件图标背景为 rgba(0,0,0,0)、padding 为 0，展示图为 1400×960。正式程序截图使用独立设置和端口，截图完成后停止自建预览实例，用户的 MCP 程序及 SSH 会话保留。
- 部署：通过 Serial Workbench MCP 上传并校验完整 PNG 与静态资源，原子切换到 `releases/20261019-icons-motion`；线上 10 个文件逐字节匹配构建，自定义 404 与 v0.15.32 更新清单通过核验，原站保留。证据在 `build/website-icons-20261019/`。
- 剩余：公安备案号仍待取得；版本同步机制保持运行。没有修改桌面程序或重新发布安装包。
- 提交：独立网站分支留档，不合并应用主线。

## 2026-10-19 — 透明实验室玻璃主题与持续版本同步

- 执行者：Codex；分支 `codex/website-ui-20261019`，起点 `24e5216`。
- 用户目标：恢复液态玻璃，建立明确的主题及设计感；网站下载持续跟随 GitHub 正式版本。
- 视觉：以“透明实验室”为主题重构首屏，使用芯片、信号光路、波形与帧解析的玻璃仪器构图；浮动磨砂导航、通透边缘、高光、接触阴影和浅深主题贯穿首页及二级页面。加入一次入场动画和桌面指针倾角/高光跟随，尊重减少动态效果设置；示意数据明确标识，不冒充在线设备。保留 ICP 和待办公安备案页脚。
- 版本同步：通过 MCP 的 `t_ecs` SSH 连接将国内镜像从 v0.15.13 同步到 GitHub 最新正式版 v0.15.32；公开 HTTPS 安装包完整下载的 15,464,069 字节及 SHA-256 `499fcc9217c1d711a363266cf213a5f9af056f250284cb255e27ee1706f313cc` 与 GitHub 一致。服务端同步器更新为仓库已有实现，保留原脚本与 timer 备份；恢复 timer 开机启用，激活 30 秒后检查，每轮结束后五分钟再次检查（最多 30 秒随机延迟）。实际观察到自动触发成功、`Result=success`、`ExecMainStatus=0`，timer 为 `active/waiting` 且下一轮已排期。网页进入、切回前台与前台停留每 60 秒刷新清单，失败保留已读说明。
- 兼容：原生客户端要求清单下载主机与既有 IP 镜像一致，清单继续保留 IP URL；网页将验证后的文件路径映射为域名同源 HTTPS。没有修改桌面程序、证书或 Nginx，不需要重新发布应用安装包。
- 部署与验证：新站原子发布到 `releases/20261019-transparent-lab-final`，保留上一站点。Python 构建、JS 语法检查及 3 项现有镜像测试通过；浏览器检查桌面、768px、360px 和 320px，修正平板示意说明与玻璃卡片重叠；三个二级页面与主题/菜单通过检查。上线后 10 个资源逐字节匹配审阅构建，404 和 v0.15.32 清单通过核验。证据在 `build/website-glass-20261019/`。
- 未完成：公安备案号码仍待用户取得；取得后修改配置并重新发布。网站 UI 与镜像同步目标已完成，旧站与旧安装包均保留。
- 提交与同步：独立网站分支留档，不合并应用主线或改写 DeepSeek 历史。

## 2026-10-19 — 网站 UI 与备案页脚上线

- 执行者：Codex；独立分支 `codex/website-ui-20261019`，起点 `2df9506`；未合并或改写 DeepSeek 的应用开发。
- 用户目标：ICP 通过后优化网站 UI，为 ICP 和公安备案信息保留位置，使用 Serial Workbench MCP 操作 SSH 实例。
- 已完成：新增 `deploy/website/` 静态站源码、构建脚本与部署脚本。统一蓝灰配色、圆角按钮、内容层级、项目/下载/关于页面、浅深主题及手机菜单；五个页面（含 404）在构建时写入共同备案页脚。使用用户提供的“蜀ICP备2026062713号”，链接 `https://beian.miit.gov.cn/`；公安备案尚未取得号码，显示“公安备案办理中”，配置文件预留完整号码、查询 code 和官方图标路径。
- 部署：经 Serial Workbench MCP 的 `t_ecs` SSH 连接上传，校验传输摘要与 11 个文件后原子切换到 `releases/20261019-ui-registration-final`。原 `releases/20261014-auto-release` 保留，可回退。保留原安装包、校验文件及旧摘要资源；未修改 Nginx、证书、国内版本清单或恢复同步定时器。
- 验证：Python 构建与语法检查、`node --check`、五页备案信息及本地资源链接检查通过；浏览器实际检查桌面和 360px 手机布局、三个二级页面、菜单开合和主题切换，无横向溢出或缺失图片。上线后 HTTPS 获取的 10 个静态文件与本地逐字节一致；原安装包/校验文件/旧 CSS/JS/更新清单均返回 200，多层不存在地址返回含正确根路径链接的自定义 404。实际浏览器确认线上页脚、ICP 查询地址及主题；截图及核验 JSON 在 `build/website-20261019/`。没有改动桌面程序，不运行原生 Qt 测试或制作安装包。
- 剩余事项：公安备案通过后填写 `deploy/website/site-config.json` 并重新构建发布；当前网站下载继续沿用冻结的 v0.15.13，恢复国内镜像和自动更新需另行授权。
- 提交与同步：本次网站源码和记录保存在独立分支，不推送或合并应用主线。

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
