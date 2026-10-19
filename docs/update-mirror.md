# 国内更新镜像与自动同步

客户端优先读取 `https://162.14.72.194/updates/latest.json`，镜像不可用时回退 GitHub Releases。清单包含版本、安装包 URL、大小、SHA-256、发布日期和更新说明。下载结束后校验完整文件，校验通过才允许安装。

0.15.14 增加独立更新窗口。点击右下角提醒可查看特性与修复说明，窗口支持自由调整大小、最小化及隐藏。下载期间工作台继续运行，主窗口保留进度。用户选择“关闭工作台并安装”后才退出并启动安装向导。

## 自动发布与备案期间的冻结

服务器同步器 `deploy/update-mirror/sync_github_release.py` 每十分钟读取 GitHub 最新正式 Release，使用 ETag 避免重复读取；下载并校验安装包后，以原子重命名更新清单。错误或校验失败时保留上一个可用版本。网站读取相同清单，自动显示版本、下载地址和说明。

**2026-10-19：用户确认 ICP 已通过，并授权更新网站 UI 与备案页脚，页面更新已完成。国内版本清单和安装包的自动同步仍保持冻结：`lilinng-release-sync.timer` 未恢复，本次没有运行同步器、修改清单或替换安装包。未经用户明确允许，不恢复自动同步。**

0.15.14 仅发布至 GitHub Release，不上传国内镜像、不更新网站和版本清单。客户端从可用镜像读到旧版本时不会再查询 GitHub，因此此次升级需从 GitHub Release 手动下载安装。

恢复后，只需发布 `v<版本>` 正式 Release，附上 `SerialWorkbench-<版本>-windows-x64-setup.exe`，并在 Release 正文填写更新说明，服务器和网站即可自动更新。同步器不需要 GitHub 密钥。

检查冻结状态：

```powershell
ssh t_ecs 'sudo systemctl is-enabled lilinng-release-sync.timer'
ssh t_ecs 'sudo systemctl is-active lilinng-release-sync.timer'
```

安装包位于 `/srv/lilinng-web/www/shared/updates`，与网站 `current` 发布目录独立。HTTPS 证书的自动续期继续运行，续期不修改网站内容。域名备案期间使用约六天有效的 IP 证书，`lilinng-cert-renew.timer` 每天检查两次并在到期前续期。

0.15.12 及更早版本内置 GitHub 更新地址，需要手动安装一次较新版本；0.15.13 已支持国内镜像。
