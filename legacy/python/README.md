# Python 历史实现

这里完整保留 PySide6 版源码、测试、依赖锁定文件及 PyInstaller 配置，供研究与回退使用。默认桌面入口为工程根目录的原生 Qt C++ 版。

在本目录运行 `setup.bat` 安装依赖，再双击 `start.bat` 启动；`build.bat` 打包至本目录的 `dist/`。

```powershell
# 在工程根目录使用已有开发环境运行
.\.venv\Scripts\python.exe legacy/python/run.py

# 根目录 pytest.ini 已配置历史源码与测试路径
.\.venv\Scripts\python.exe -m pytest -q
```

`serial_assistant/` 的相对路径和 STDIO 桥保持原有布局。新功能在 `src/` 的原生版本开发。
