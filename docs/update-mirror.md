# 国内更新镜像与本机手动发布

客户端读取 `https://162.14.72.194/updates/latest.json`，网站读取同一清单并使用同源 HTTPS 下载。清单包含版本、安装包 URL、大小、SHA-256、发布日期和完整更新说明。客户端下载完成并校验后才允许安装。

## 当前发布方式

**2026-10-19 起：在本机拉取 GitHub 最新正式 Release，校验后经 MCP 手动传到网站。服务器不再拉取 GitHub 安装包。** 国内服务器原同步下载曾占用发布锁十分钟后超时；不再依赖这一流程。

`lilinng-release-sync.timer` 保持启用，每五分钟左右运行一次本地完整性检查。对应 service 使用 `sync_github_release.py --check-local`，只核验既有清单、安装包、单包校验文件及 `SHA256SUMS.txt`，不访问 GitHub，也不更新版本。旧脚本不加该参数仍保留历史拉取入口，生产定时任务不使用它。

## 一条命令更新网站

在仓库根目录运行：

```powershell
.\scripts\publish-update-mirror.ps1
```

默认获取 `lilinng-163/serial-workbench` 的最新正式版本，不需要手填版本、安装包路径或更新说明。也可以使用已下载的安装包，但必须与最新 GitHub Release 的大小和摘要一致：

```powershell
.\scripts\publish-update-mirror.ps1 -Installer .\dist\local\0.15.35\SerialWorkbench-0.15.35-windows-x64-setup.exe
```

只在本机下载校验、不操作服务器：

```powershell
.\scripts\publish-update-mirror.ps1 -DownloadOnly
```

首次使用需要 Python 和 MCP SDK（与现有集成测试共用依赖），以及已配置 `t_ecs` 的 Windows OpenSSH。推荐安装并登录 GitHub CLI，脚本优先用 `gh api` 获取最新正式版本元数据；没有 `gh` 时尝试匿名 GitHub API，受 API 限流时会失败并保留网站版本。

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r tests/integration/requirements.txt
```

打开 Serial Workbench，在设置中启用 SSH 协议与 AI 控制。脚本直接使用工作台 MCP；已连接的 `t_ecs` 会话会复用，未连接时通过 MCP 打开。认证问题在工作台处理。运行脚本期间不要在同一 SSH 终端输入其他命令。工作台 MCP 地址变化时传入 `-McpUrl http://127.0.0.1:端口/mcp`。

## 实际执行流程

1. 本机读取最新稳定 Release，校验官方仓库、版本、文件名、上传状态、大小和 GitHub SHA-256；下载在本机完成，服务器不持锁等待此步骤。
2. 通过 MCP 选择精确的 SSH channel，保持协议权限、AI 控制和独立读取游标；辅助脚本分块传输，避免终端输入行长度限制。
3. 将生产同步 service 调整为 `--check-local` 后 reload 配置，不 stop、disable 或 restart 生产定时器与服务。
4. MCP 在服务器创建只监听 `127.0.0.1` 的一次性接收端。本机 SSH 只建立端口转发，原样上传安装包；不通过本地 SSH 执行网站命令，也不开放公网上传入口。接收端校验字节数和摘要后退出。
5. 获取相同发布锁，已有任务占锁时立即失败，不等待其 GitHub 超时。不降级现有版本。先把通过校验的安装包放到镜像目录，再原子更新完整 `latest.json` 与两个校验文件；手动发布函数禁止网络下载。
6. 复制当前网站到新的 release，更新版本、链接、大小、日期和摘要；备案页脚、资源及实际截图版本标注保留。校验后原子切换 `current`，旧目录和安装包保留用于回退。
7. 从公开 HTTPS 链接完整下载安装包，再校验大小和 SHA-256，核对清单及 HTML 后备版本。仅清单显示新版本不算完成。

本机缓存和验证记录位于 `dist/website-sync/<版本>/`：安装包、完整 Release 元数据、`publication.json`、`verification.json` 和 SSH 转发日志。已校验的缓存可复用。服务器发布记录位于 `/srv/lilinng-web/deployments/`。失败的临时目录保留排查，成功任务会清理自己的临时文件。

## 网站与客户端兼容

网站进入页面、回到前台，以及前台每 60 秒刷新清单；发布后不必等服务器抓取 GitHub。页面源文件中的后备版本也由脚本更新。实际软件截图继续按捕获版本标注，不能仅修改图片版本号。

既有原生客户端要求清单安装包主机匹配 IP 镜像，因此 `-BaseUrl` 保持 `https://162.14.72.194`；网站将验证过的路径映射为 `https://www.lilinng.cn/updates/`。镜像文件与网站 `current` 发布目录独立。域名及原 IP 证书自动续期保留，本流程不改证书。

0.15.12 及更早版本使用旧更新地址，首次升级可能需要手动下载较新安装包。0.15.14 曾只发布 GitHub、未更新镜像，这是历史记录，不代表当前发布方式。
