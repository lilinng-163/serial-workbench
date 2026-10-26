# 开发交接记录

每项任务完成或代理交接前，在本文件顶部新增记录。保持简洁，写事实与准确停点，不把聊天全文放进仓库。

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
