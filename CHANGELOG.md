<!-- SPDX-License-Identifier: Apache-2.0 -->
# 更新记录

## 0.15.30 — 2026-10-17

### 新增

- 收发日志、串口收发小窗和 JSON/XML 记录列表增加统一搜索开关：区分大小写、全词匹配、正则表达式和逻辑匹配。
- 逻辑匹配支持 `&&` 与 `||`（`&&` 优先），可与正则组合；双引号可保护包含运算符的搜索项。查询与开关随页面设置保存。

### 改进与修复

- 暂停显示时仍可重新筛选现有日志；分段收到的文本按完整行重新判断，筛选保留共享 MCP 历史和独立读取游标。
- 无效搜索表达式显示警示边框与悬停说明；JSON/XML 详情与当前可见记录同步，无匹配时清空详情。
- SSH 草稿切回串口时恢复收发日志，已连接串口仍保留其显式终端偏好。
- 默认窗口适度增高并限制在可用屏幕高度内；标签重命名下划线贴近文字，并避开图标和关闭按钮。

### English

- Add shared search controls for receive logs, serial monitor windows and JSON/XML records: case sensitivity, whole-word, regular expressions and logical matching.
- Support `&&` before `||`, quoted terms and combined regex/logical queries; persist search settings per page/window.
- Refilter paused and partially received log lines without consuming shared MCP history. Explain invalid expressions and keep structured details aligned with visible records.
- Restore serial log mode when leaving an SSH draft, preserve explicit connected-serial preferences, improve default window height and refine inline tab renaming.

## 0.15.29 — 2026-10-16

### 新增

- SSH 专用交互终端保留一万行滚动历史，支持滚轮回看、历史选择复制、阅读位置保持，以及全屏程序退出后的主屏恢复。
- 串口控制页支持控件组、自由拖动缩放、常用预设、各控件独立自动发送，以及独立收发小窗。
- MCP 增加使用指南工具与随附 skill，说明连接选择、增量读取和 C 风格帧脚本；串口、TCP、UDP、SSH 可分别设置 AI 访问范围。
- 主界面与收发区域支持独立背景图片、位置与透明度设置，窗口标题栏跟随主题和背景。
- 全新英文与简体中文 README，加入功能概览、实际界面截图、下载说明、快捷操作与 MCP 接入示例。

### 改进与修复

- SSH 固定使用终端，取消日志模式切换按钮和串口式发送栏；旧配置自动进入终端。
- 波形实时读数采用定宽卡片，默认前四路，可自行更换通道，避免数值位数变化导致布局跳动。
- 波形、JSON、XML 页面增加本地清空操作，保留连接和 MCP 共享历史。
- 重命名标签仅使用右键菜单或 F2，输入框避开图标和关闭按钮；JSON、XML、波形使用对应特征图标。
- 优化按钮与勾选动画、下拉菜单圆角、标签与内容区衔接、设置布局及串口参数可读性。

### English

- Add a dedicated SSH terminal with 10,000 scrollback lines, history selection/copy, stable reading position and alternate-screen restoration.
- Add grouped serial-control dashboards, presets, free positioning/resizing, per-control automatic sending and a separate receive monitor.
- Add MCP usage guides and a bundled skill, plus protocol-level AI access controls.
- Add custom workspace backgrounds and coordinated window chrome.
- Replace the README with English and Simplified Chinese editions, fresh screenshots and concise setup guides.
- Improve waveform readouts, per-page clear actions, tab renaming, data-page icons, rounded transitions and input feedback.

## 0.15.15 — 2026-10-15

- SSH 终端支持按字符、单词和行扩展键盘选区；Ctrl+C 有选区时复制，无选区时发送中断，长按复制不会误发中断。
- 从当前光标用键盘选择同一行输入后，Backspace 或 Delete 可整段删除；历史输出和鼠标选区仍用于复制。
- 设置和各类二级窗口支持缩小、滚动查看内容，底部操作按钮保持可用；设置窗口大小与位置会保存。
- Agent 接入页更新透明背景的真实项目图标，适配浅色、深色主题和高 DPI，并补齐第三方许可说明。
- 手动选择波形颜色会自动关闭自动配色和默认样式继承；修改线宽、线型时自动为当前通道单独保存。

安装包与完整说明见 [0.15.15 Release](https://github.com/lilinng-163/serial-workbench/releases/tag/v0.15.15)。本次只发布 GitHub，备案期间网站及国内镜像保持冻结。

## 0.15.14 — 2026-10-15

- 波形页内嵌可开关的实时数值面板，支持通道独立颜色、线宽、线型与自定义坐标单位；设置保存后可继续编辑。
- JSON 与 XML 拆分页面，旧配置自动迁移。新增 JSON 助手，支持格式化、语法定位及按字段命名的 cJSON 解析和组装示例。
- MCP 增加串口、TCP、UDP、SSH 独立开放开关；SSH 支持新设备指纹确认、密码与私钥口令输入和认证缓存清除。
- 更新详情使用独立窗口，下载可在后台进行；修复 SSH 终端换行、字体保存与中文字体对话框，并统一暂停/继续图标。

安装包与完整说明见 [0.15.14 Release](https://github.com/lilinng-163/serial-workbench/releases/tag/v0.15.14)。本次只发布 GitHub，备案期间网站及国内镜像保持冻结。

## 0.15.10 — 2026-10-14

对调收发工具栏的日志 / 终端切换图标：日志页显示终端图标，进入终端后显示日志图标。图标与鼠标悬停提示一致，表示点击后将切换到的页面。

## 0.15.9 — 2026-10-14

串口文件传输、波形交互、Agent 接入和 Windows 安装流程更新。空白收发页的闪电图标移除背景与边框，并适当放大。

- 串口文件传输支持原始字节和 XMODEM-CRC 的发送、接收，显示进度、速率、剩余时间及重试次数，支持取消。XMODEM 进度按对端确认更新，接收失败不会覆盖原文件。设备端需要对应协议的收发程序。
- 文件传输期间独占串口写入，暂时绕过脚本、波形和 JSON/XML 解析。共享 MCP 历史保留原始字节，连接状态可查询是否在传输。
- 波形按可见区域缓存和刷新，保留原始采样读数；跟随最新保持当前显示范围，支持分别隐藏 X/Y 轴。波形与 JSON/XML 使用独立 Demo 数据源。
- AI / MCP 设置以两列展示常用 Agent 图标和安装检测，支持修改 MCP 端口、批量更新配置及配置检查。STDIO 桥使用安装目录中的可执行文件。
- 收发和控件标签支持鼠标中键关闭；透明浅绿色双折闪电统一用于应用、侧栏、菜单和空白页。
- Windows 安装器支持自选目录、快捷方式和卸载。升级自动读取当前用户的原安装路径，保留个人设置；卸载仅移除应用自己的文件。
- 独立虚拟串口 Demo 验证实际文件传输及 SHA-256，不进入正式安装包。

QtTest 共 175 项通过；原始字节和 XMODEM 的发送、接收模拟验证一致。正式安装包通过无 Qt SDK 环境启动、旧目录升级、HTTP/STDIO MCP 和卸载测试。未进行真实 COM 设备的文件传输验证。

下载附件 `SerialWorkbench-0.15.9-windows-x64-setup.exe`，双击安装；`SHA256SUMS.txt` 用于校验下载内容。覆盖升级前请关闭工作台和正在使用的 MCP 桥。
