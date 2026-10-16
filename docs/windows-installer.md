# Windows 安装与卸载

安装包：`SerialWorkbench-<版本>-windows-x64-setup.exe`，Windows x64。

双击后按安装向导选择目录及桌面快捷方式。默认安装到当前用户的 `%LOCALAPPDATA%/Programs/SerialWorkbench`；可选其他可写目录。安装后程序和原生 MCP 桥直接位于所选目录，不再需要每次从单文件缓存启动。

Windows「已安装的应用」、开始菜单和安装目录均提供卸载入口。卸载界面可选择删除当前用户的设置，默认保留。程序或 MCP 桥正在运行时会停止安装/卸载并提示关闭。

安装版设置存放在 `%LOCALAPPDATA%/SerialWorkbench/settings.json`；便携版继续使用程序目录的配置。`--settings` 可指定独立配置文件。安装版通过 `installation.json` 识别分发方式，该标记不包含用户名和绝对路径。

同一 Windows 用户启动新版安装包时，自动从卸载注册项读取原安装目录，安装向导直接使用该路径并覆盖升级，无需重新选择。若需要更换目录，先卸载并保留设置，再安装到新目录。安装后使用 AI/MCP 的一键更新重新同步 Agent 的 URL 或 STDIO 桥路径。

卸载清单在打包阶段从分发文件生成。只删除清单中的文件和空目录，不递归删除用户选择的安装目录；自行添加的文件会保留。Windows 卸载注册项和快捷方式均为当前用户创建。

## 构建

先执行 `scripts/build.ps1`，再执行：

```powershell
./scripts/package-release.ps1 -OutputDir dist/local/0.15.29 `
    -QtSourceRoot D:/lib/Qt/6.11.1/Src `
    -MakeNSIS E:/MinGW/w64devkit/bin/makensis.exe -InstallerOnly
```

NSIS Modern UI 提供目录、组件、安装和卸载页面，中文字体与浅绿色透明闪电图标统一用于程序、安装器、卸载器和快捷方式。原始 SVG 和 ICO 在 `resources/brand/`，图标九种分辨率从 16 到 256 像素，源文件可通过 `scripts/generate-icon.py` 重新生成。

## 验证

`tests/integration/smoke_installer.py` 使用独立的 Installer Test 注册项及快捷方式，安装到工作区中文临时目录，先注册旧版本，再不传入 `/D` 运行新版本，检查原自定义路径自动识别、设置保留、安装、升级、启动、HTTP/STDIO、运行程序保护和卸载。测试结束会卸载清理，不更改正式应用或真实 Agent 配置。注册项/快捷方式测试需要当前用户的普通 Windows 写权限。

传入打包脚本返回的 payload、uninstall manifest 和 installer 路径：

```powershell
./.venv/Scripts/python.exe tests/integration/smoke_installer.py `
    --payload build/release-<版本>-<阶段ID>/SerialWorkbench `
    --remove-manifest build/release-<版本>-<阶段ID>/remove-files.nsh `
    --installer dist/local/0.15.29/SerialWorkbench-0.15.29-windows-x64-setup.exe `
    --expected-version 0.15.29 --makensis E:/MinGW/w64devkit/bin/makensis.exe
```

参考：[NSIS Modern UI 文档](https://nsis.sourceforge.io/Docs/Modern%20UI%202/Readme.html)。
