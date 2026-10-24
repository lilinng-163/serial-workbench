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

当前已配置 **川公网安备51162302000307号**，链接到[官方备案查询页](https://beian.mps.gov.cn/#/query/webSearch?code=51162302000307)。号码前使用用户提供的备案图标 `site/assets/police.png`（36×40 原图，显示为 18×20），图标与号码同属一个链接。五个页面共用构建页脚，包括关闭 JavaScript 时和 404 页面。

## 部署边界

通过 Serial Workbench MCP 的已确认 SSH 连接部署。先复制线上 release 至新目录，再覆盖本次构建文件；已有安装包、校验文件和旧摘要资源需保留，避免下载及缓存页面失效。核验上传文件摘要后，原子切换 `current` 软链接；旧 release 保留用于回退。

404 页面的资源及页脚使用根路径，保证不存在的多层 URL 也可正确显示。

下载页面读取 `/updates/latest.json`，只接受可信来源的有效版本、文件名、摘要和大小；安装包链接转换为同源 `/updates/`，沿用域名 HTTPS。进入页面、切回前台时立即刷新，前台停留期间每 60 秒刷新；失败时保留现有版本和已读取的说明。HTML 的当前后备版本为 v0.15.35；展示截图仍明确标注实际捕获的 v0.15.32。

网站版本由维护者通过本机私有工具发布，仓库不包含专用同步脚本。远端操作通过 Serial Workbench MCP 完成；服务器定时任务仅检查已有本地文件，不再请求 GitHub。网站进入、回到前台和每 60 秒读取清单的行为保留。

页面不新增搜索、留言、注册、评论或聊天服务。主题切换只保存在浏览器本地。

## 校庆视频

页脚右侧以小字显示“校庆视频链接 001 002”，不占用主导航或主页内容区。两个链接分别打开 `anniversary/001.html` 和 `anniversary/002.html`，支持切换编号、播放、全屏和下载；默认不自动播放，使用 `preload="metadata"`。下载按钮明确显示“下载 4K 原片”及大小，指向 `media/original-001.mp4` / `original-002.mp4`，分别为 1,706,330,343 / 1,715,342,420 字节，与用户提供的 4K / 60fps HEVC 原文件逐字节一致；下载保存名称沿用原文件名。

在线播放使用 1080p / 30fps H.264 + AAC MP4，时长约 04:01 和 04:03，moov 位于文件前部，可边加载边播放。播放器通过相对路径 `anniversary/media/001.mp4`、`002.mp4` 及同名 JPG 封面读取媒体。在线播放与原片下载使用独立文件，原片只做原样上传、校验，不重新编码。视频和专用上传工具不提交 Git，本机原文件保留。

线上媒体独立于页面 release 保存，通过 release 内的相对软链接提供访问；后续发布须保留现有软链接及媒体文件。通过 MCP 核验媒体大小和 SHA-256 后再启用入口，并验证 `video/mp4` 和 HTTP 206 Range，避免只能整文件下载或无法拖动进度。本地预览时可在构建输出的 `anniversary/media/` 放置播放版和封面。

发布校验按 1 MiB 分块计算 SHA-256，避免把 GB 级原片读入服务器内存。运行 `python -m unittest discover -s deploy/website -p "test_*.py"` 可检查大文件摘要及内存上限回归。
