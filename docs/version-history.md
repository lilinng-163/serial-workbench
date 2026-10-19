<!-- SPDX-License-Identifier: Apache-2.0 -->
# 版本回溯

打开 **设置 → 版本回溯**，选择最近三个正式版本之一，查看发布日期、特性说明及 Windows x64 安装包大小。草稿和预发布版本不出现在列表中。

软件附带最近发布记录的快照，离线也能查看。首次打开此页会向 GitHub 刷新；也可点击「刷新列表」。网络错误时保留现有记录，不阻塞串口或 MCP。随包快照来自 `resources/releases/history.json`，构建后发布的新版本需联网刷新才能看到。

点击「下载安装包」在后台下载所选版本，离开设置后仍会继续。下载期间可取消；完成后校验文件大小和 SHA-256，通过后才提供「关闭工作台并安装」。程序不会自动安装旧版本。安装器沿用原安装目录并保留本地配置。

主窗口及分离出的工作台窗口底部始终显示当前运行版本，例如 `v0.15.34`；临时状态消息不会覆盖版本号。它表示正在运行的程序，与历史列表中选择的版本无关。

## English

Open **Settings → Version history** to review the latest three stable releases, their release notes, publication dates and Windows x64 installer sizes. Drafts and prereleases are excluded.

The bundled snapshot is available offline. Opening the page first refreshes it from GitHub; the refresh button retries manually. Failed requests retain the existing list. New releases published after the build require an online refresh.

Download the selected installer in the background, with cancellation available. Size and SHA-256 must match before the explicit install action becomes available. Installation reuses the existing location and retains local settings. No automatic downgrade occurs.

The permanent status-bar version identifies the running build (for example `v0.15.34`), independently of the release selected in Settings. Temporary status messages do not hide it.
