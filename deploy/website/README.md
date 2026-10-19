# lilinng 个人网站

“透明实验室”主题的四页静态站点：个人主页、项目、下载和关于；包含独立 404 页面。使用磨砂玻璃、细边高光与芯片/波形示意形成统一视觉。无需 Node.js、数据库或第三方字体服务。设计说明见 [DESIGN.md](DESIGN.md)。

## 构建

在仓库根目录运行：

```powershell
python deploy/website/build.py --output build/website-preview
python -m http.server 8026 --bind 127.0.0.1 --directory build/website-preview
```

打开 `http://127.0.0.1:8026/`。CSS、JavaScript、SVG 标志和软件展示 PNG 均按内容摘要生成文件名，避免旧缓存混入新页面。备案区域在构建时写入 HTML，关闭 JavaScript 后仍可见。

当前软件展示图来自正式 v0.15.32 程序，以独立设置、`--demo --view waveform --mcp-port 0 --screenshot ...` 捕获 1400×960 实际界面；页面明确标注演示数据。更新展示图时同步修改版本标注、图片尺寸和替代文本，再重新构建。

## 备案信息

修改 `site-config.json` 后重新构建：

- `icp_number`：用户提供的实际 ICP 备案号，查询链接为 `https://beian.miit.gov.cn/`。
- `public_security_number`：公安备案通过后填写完整展示文字。
- `public_security_code`：对应查询链接中的 14 位 code；必须与号码同时配置。
- `public_security_icon`：取得官方备案图标后，将文件放进 `site/assets/`，配置相对路径，例如 `assets/police.png`。

公安备案尚在办理时只显示办理状态，不生成虚构号码、图标或查询链接。

## 部署边界

通过 Serial Workbench MCP 的已确认 SSH 连接部署。先复制线上 release 至新目录，再覆盖本次构建文件；已有安装包、校验文件和旧摘要资源需保留，避免下载及缓存页面失效。核验上传文件摘要后，原子切换 `current` 软链接；旧 release 保留用于回退。

404 页面的资源及页脚使用根路径，保证不存在的多层 URL 也可正确显示。

下载页面读取 `/updates/latest.json`，只接受可信来源的有效版本、文件名、摘要和大小；安装包链接转换为同源 `/updates/`，沿用域名 HTTPS。进入页面、切回前台时立即刷新，前台停留期间每 60 秒刷新；失败时保留现有版本和已读取的说明。HTML 的当前后备版本为 v0.15.35；展示截图仍明确标注实际捕获的 v0.15.32。

2026-10-19 用户授权恢复自动同步：服务端每 5 分钟（随机延迟最多 30 秒）检查 GitHub 最新正式 Release，先下载并校验大小及 SHA-256，再发布清单。原生客户端要求清单 URL 保留既有 IP 主机，网站则将验证后的下载路径映射为自己的 HTTPS 域名；两者读取同一安装包。部署和故障保留行为见 [镜像说明](../../docs/update-mirror.md)。

页面不新增搜索、留言、注册、评论或聊天服务。主题切换只保存在浏览器本地。
