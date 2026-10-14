# 工程结构与构建

```text
serial/
├── CMakeLists.txt           # 项目、版本、依赖、子目录与安装规则
├── CMakePresets.json        # Windows Release / Debug 预设
├── LICENSE                 # Apache-2.0 项目许可证
├── cmake/                  # 编译规则及版本头模板
├── src/
│   ├── app/                # 桌面程序入口
│   ├── core/               # 串口、TCP、UDP、SSH、历史与数据解析
│   ├── mcp/                # MCP HTTP 服务与工具
│   ├── ui/                 # 工作台、控件、终端和窗口停靠
│   └── bridge/             # MCP STDIO 桥入口
├── third_party/            # 固定版本 TOML / YAML 配置校验库与许可证
├── resources/              # 字体、图标和许可证
├── scripts/                # 构建、启动、Release 打包与启动耗时测量
├── tests/
│   ├── native/             # QtTest 回归测试
│   └── integration/        # 打包版 HTTP / STDIO 测试
├── legacy/python/          # 历史 Python 子项目及其测试
├── docs/                   # 使用与设计文档、实图
├── examples/               # 报文模板与客户端配置
├── build/                  # 编译、测试报告、日志与缓存（忽略）
├── dist/                   # 可分发程序（忽略）
└── start.bat               # 转发至 scripts/start.bat
```

## CMake 目标

| 目标 | 职责与依赖 |
| --- | --- |
| `workbench_options` | C++17、编译器警告规则 |
| `workbench_core` | 静态库，Qt Network / SerialPort / Core5Compat |
| `workbench_mcp` | 静态库，依赖 core |
| `workbench_ui` | 对象库，依赖 mcp / Widgets / Svg；界面和资源编译一次，供应用和测试使用 |
| `SerialWorkbench` | 桌面程序 |
| `SerialWorkbenchMCP` | 独立 STDIO 桥，依赖 Qt Network |
| `native_tests` | BUILD_TESTING 开启时构建，依赖 ui / Qt Test |
| `wave_benchmark` | 非 CTest 的波形性能诊断程序，测量 16 通道 × 5000 点的同步与绘制耗时 |
| `serial_transfer_tests` | 文件传输协议、原始字节、CRC 校验、重传、取消、隔离与中键关页测试 |
| `FileTransferDemo` | 独立虚拟串口 Demo，仅 BUILD_TESTING 构建，无安装规则 |
| `frame_script_tests` | 脚本语法、分帧、执行限制、后台处理与编辑器联动测试 |

版本仅在根 CMake 项目中定义，通过生成头提供给界面和 MCP。工具链路径由环境或配置参数传入。
禁止在源码目录直接构建。`BUILD_TESTING=OFF` 时不查找 Qt Test，不构建测试程序。

## 构建与测试

将 CMake、Ninja 加入 PATH，设置 Qt 与对应 MinGW 工具链：

```powershell
$env:QTDIR = 'D:/lib/Qt/6.11.1/mingw_64'
$env:MINGW_BIN = 'D:/lib/Qt/Tools/mingw1310_64/bin'
cmake --preset windows-release
cmake --build --preset windows-release --parallel 4
ctest --preset windows-release
# 调试版使用 windows-debug
```

也可以显式配置任意工具链和构建目录：

```powershell
cmake -S . -B build/custom -G Ninja -DCMAKE_BUILD_TYPE=Release `
  "-DCMAKE_PREFIX_PATH=$env:QTDIR" "-DCMAKE_CXX_COMPILER=$env:MINGW_BIN/g++.exe" `
  -DBUILD_TESTING=OFF
cmake --build build/custom --parallel 4
cmake --install build/custom --prefix dist/install
```

项目 `LICENSE` 同时安装至分发目录根部。

`scripts/package-release.ps1` 从已部署的运行目录收集可分发文件，用 NSIS 的 `scripts/windows-installer.nsi` 生成可选择目录的安装包及卸载程序，同时可用 `scripts/release-launcher.nsi` 生成单文件启动版和完整便携 ZIP；所有交付文件提供 SHA-256 清单。`-InstallerOnly` 仅生成安装包。阶段目录放入 `build/release-*`，最终附件放入 `dist/releases/`；个人设置不进入附件。`tests/integration/smoke_release.py` 验证实际下载产物的运行行为。

常规 CMake 安装将程序放入 `bin/`，字体许可证放入 `licenses/`；Qt 运行库需使用 `windeployqt` 部署。
一键构建脚本 `scripts/build.ps1` 会完成配置、编译、测试、安装和 Qt 部署，并使用 `CMAKE_INSTALL_BINDIR=.` 生成可直接启动的便携目录：

```powershell
.\scripts\build.ps1
# 工具不在 PATH 时可指定 -CMake / -Ninja；可选 -Jobs / -BuildDir / -PackageDir
# 只编译测试：-SkipPackage；仅构建生产目标：-SkipTests
.\.venv\Scripts\python.exe tests/integration/smoke_native.py --expected-version 0.15.9
```

脚本可从任何工作目录调用，结束时恢复原工作目录和 PATH。替换分发包前会检查其中的程序是否仍运行。
个人工具链配置可放入被 Git 忽略的 `CMakeUserPresets.json`，不必修改共享构建文件。

波形绘制位于 `src/ui/wave_plot.cpp`，包含增量同步、图像缓存、屏幕列极值归并和原始采样读数。`src/ui/window.cpp` 中的 `Instrument` 管理可见波形页的 16 ms 刷新定时器，冻结或隐藏时停止定时器。原始解析和 MCP 历史接口仍由 core / mcp 管理。

品牌源文件为 `resources/brand/lightning.svg`，透明双折闪电顺时针旋转 45°。`scripts/generate-icon.py` 使用构建环境的 PySide6 生成九种分辨率的 Windows ICO 和安装器 BMP。`cmake/app.rc.in` 为主程序嵌入图标及版本信息；部署程序不依赖 Python。

文件传输协议位于 `src/core/serial_transfer.*`，异步处理文件读写、分块和确认；`src/ui/file_transfer_dialog.*` 管理进度界面和当前串口的独占绑定。`tests/native/file_transfer_demo.cpp` 提供独立虚拟串口测试，正式安装仅包含桌面程序和 MCP 桥。
