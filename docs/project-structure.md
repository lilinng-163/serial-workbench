# 工程结构与构建

```text
serial/
├── CMakeLists.txt           # 项目、版本、依赖、子目录与安装规则
├── CMakePresets.json        # Windows Release / Debug 预设
├── cmake/                  # 编译规则及版本头模板
├── src/
│   ├── app/                # 桌面程序入口
│   ├── core/               # 串口、TCP、UDP、SSH、历史与数据解析
│   ├── mcp/                # MCP HTTP 服务与工具
│   ├── ui/                 # 工作台、控件、终端和窗口停靠
│   └── bridge/             # MCP STDIO 桥入口
├── resources/              # 字体、图标和许可证
├── scripts/                # 构建、启动、启动耗时测量
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
| `workbench_ui` | 对象库，依赖 mcp / Widgets；界面和资源编译一次，供应用和测试使用 |
| `SerialWorkbench` | 桌面程序 |
| `SerialWorkbenchMCP` | 独立 STDIO 桥，依赖 Qt Network |
| `native_tests` | BUILD_TESTING 开启时构建，依赖 ui / Qt Test |

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

常规 CMake 安装将程序放入 `bin/`，字体许可证放入 `licenses/`；Qt 运行库需使用 `windeployqt` 部署。
一键构建脚本 `scripts/build.ps1` 会完成配置、编译、测试、安装和 Qt 部署，并使用 `CMAKE_INSTALL_BINDIR=.` 生成可直接启动的便携目录：

```powershell
.\scripts\build.ps1
# 工具不在 PATH 时可指定 -CMake / -Ninja；可选 -Jobs / -BuildDir / -PackageDir
# 只编译测试：-SkipPackage；仅构建生产目标：-SkipTests
.\.venv\Scripts\python.exe tests/integration/smoke_native.py --expected-version 0.12.0
```

脚本可从任何工作目录调用，结束时恢复原工作目录和 PATH。替换分发包前会检查其中的程序是否仍运行。
个人工具链配置可放入被 Git 忽略的 `CMakeUserPresets.json`，不必修改共享构建文件。
