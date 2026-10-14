# 国内更新镜像

从 0.15.13 起，工作台先请求 `https://162.14.72.194/updates/latest.json`。清单不可用时回退 GitHub Releases。安装包必须位于同一 HTTPS IP 的 `/updates/` 路径，文件名、版本、大小和 SHA-256 都要与清单一致；客户端在启动安装器前重新校验完整文件。

服务器 `t_ecs` 的网站容器继续提供 80 端口，`lilinng-https` 容器通过 443 端口代理现有网站。更新文件单独存放在 `/srv/lilinng-web/www/shared/updates`，不受网站 `current` 发布目录切换影响。服务器配置在 `deploy/update-mirror/`，本地发布命令为：

```powershell
./scripts/publish-update-mirror.ps1 `
    -Installer dist/releases/<版本>/SerialWorkbench-<版本>-windows-x64-setup.exe
```

脚本先上传安装包，在服务器上复核 SHA-256，再以原子重命名更新 `latest.json`。发布顺序应为：构建和测试安装包、发布 GitHub Release、发布镜像、从外部网络验证 HTTPS 清单及安装包。不要先更新清单再上传安装包。

目前域名仍在备案，HTTPS 使用 Let's Encrypt 的 IP 证书。证书约六天有效，服务器的 `lilinng-cert-renew.timer` 每天检查两次，到期前自动续期并重载 `lilinng-https`。可用 `sudo systemctl status lilinng-cert-renew.timer` 和 `sudo systemctl start lilinng-cert-renew.service` 检查续期。备案域名可用后，先配置该域名的 HTTPS，再在后续客户端版本中切换固定更新地址。

0.15.12 及更早版本的更新器仍使用 GitHub 地址，无法远程修改其内置 URL；国内用户需要手动安装一次 0.15.13，之后才能走国内镜像。
