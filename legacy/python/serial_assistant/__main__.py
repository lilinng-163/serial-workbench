import argparse
from pathlib import Path
import sys


def main():
    parser = argparse.ArgumentParser(description="Serial Workbench / 串口与网络调试助手")
    parser.add_argument("--mcp-port", type=int, default=8765)
    parser.add_argument("--demo", action="store_true")
    parser.add_argument("--screenshot", help="保存界面截图后退出，用于视觉验证")
    parser.add_argument("--settings", default=None)
    parser.add_argument("--stdio", action="store_true", help="作为 stdio MCP 桥连接已经运行的桌面程序")
    parser.add_argument("--url", default="http://127.0.0.1:8765/mcp")
    args = parser.parse_args()
    if args.stdio:
        import asyncio
        from .bridge import run_bridge
        asyncio.run(run_bridge(args.url))
        return
    if not 1 <= args.mcp_port <= 65535:
        parser.error("--mcp-port must be 1..65535")

    # Windowed Windows builds have no console streams; the server logger
    # still needs a real stream with isatty().
    if sys.stderr is None:
        sys.stderr = open(Path(sys.executable).parent / "workbench.log", "w", encoding="utf-8")
    if sys.stdout is None:
        sys.stdout = sys.stderr

    from PySide6.QtCore import QTimer
    from PySide6.QtWidgets import QApplication
    from .engine import Engine
    from .mcp_server import MCPService
    from .ui import MainWindow
    from .instance import InstanceGuard
    from .runtime import runtime_identity

    app = QApplication(sys.argv[:1])
    app.setApplicationName("Serial Workbench")
    app.setStyle("Fusion")
    guard = InstanceGuard(runtime_identity()["username"], args.mcp_port)
    if not guard.acquire():
        return
    app.aboutToQuit.connect(guard.close)
    engine = Engine()
    mcp = MCPService(engine, args.mcp_port)
    mcp.start()
    base = Path(sys.executable).parent if getattr(sys, "frozen", False) else Path(__file__).resolve().parent.parent
    window = MainWindow(engine, mcp, args.settings or base / "settings.json")
    guard.window = window
    window.show()
    if args.demo:
        window.start_demo()
    if args.screenshot:
        def capture():
            window.grab().save(str(Path(args.screenshot).resolve()))
            window.close()
        QTimer.singleShot(5000, capture)
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
