# lilinng 个人网站

四页静态站点：个人主页、项目、下载和关于；包含独立 404 页面。无需 Node.js、数据库或第三方字体服务。

## 构建

在仓库根目录运行：

```powershell
python deploy/website/build.py --output build/website-preview
python -m http.server 8026 --bind 127.0.0.1 --directory build/website-preview
```

打开 `http://127.0.0.1:8026/`。CSS 和 JavaScript 按内容摘要生成文件名，避免旧缓存混入新页面。备案区域在构建时写入 HTML，关闭 JavaScript 后仍可见。

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

下载页面继续读取现有 `/updates/latest.json`，只接受可信来源的有效版本、文件名、摘要和大小；安装包链接转换为同源 `/updates/`，沿用域名 HTTPS。读取失败时保留 HTML 中现有下载版本。本次页面更新不自动恢复更新镜像的定时任务，也不改写更新清单或替换安装包。

页面不新增搜索、留言、注册、评论或聊天服务。主题切换只保存在浏览器本地。
