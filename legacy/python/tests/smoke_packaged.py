# SPDX-License-Identifier: Apache-2.0

"""Manual release smoke check; requires the two built Windows executables."""

import asyncio
import argparse
import json
from pathlib import Path
import socket
import subprocess
import sys
import time

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client
from mcp.client.streamable_http import streamable_http_client


ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

from serial_assistant import __version__


def unpack(result):
    assert not result.isError, result
    return result.structuredContent or json.loads(result.content[0].text)


async def check(url, bridge):
    async with streamable_http_client(url) as (read, write, _):
        async with ClientSession(read, write) as client:
            await client.initialize()
            assert len((await client.list_tools()).tools) == 9
            status = unpack(await client.call_tool("get_status"))
            assert status["version"] == __version__
            assert status["process"]["username"]
            events = unpack(await client.call_tool("read_events", {}))
            assert any(e["direction"] == "rx" for e in events["events"])
            print("Packaged GUI: HTTP MCP initialized, 9 tools, real demo RX data.")
            params = StdioServerParameters(command=str(bridge), args=["--url", url])
            async with stdio_client(params) as (sr, sw):
                async with ClientSession(sr, sw) as second:
                    await second.initialize()
                    assert len((await second.list_tools()).tools) == 9
                    other = unpack(await second.call_tool("read_events", {}))
                    assert other["session_id"] == events["session_id"]
                    assert other["events"][:len(events["events"])] == events["events"]
                    print("Packaged STDIO bridge: reads the same session and retained events.")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--app-dir", type=Path, default=ROOT / "dist" / "SerialWorkbench")
    args = parser.parse_args()
    executable = args.app_dir.resolve() / "SerialWorkbench.exe"
    bridge = executable.with_name("SerialWorkbenchMCP.exe")
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]
    process = subprocess.Popen([
        str(executable), "--demo", "--mcp-port", str(port),
        "--settings", str(ROOT / "build" / "smoke-settings.json"),
    ])
    try:
        for _ in range(30):
            if process.poll() is not None:
                raise RuntimeError(f"GUI exited early: {process.returncode}")
            try:
                with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                    break
            except OSError:
                time.sleep(0.2)
        else:
            raise TimeoutError("Packaged MCP startup timed out")
        time.sleep(1)
        asyncio.run(check(f"http://127.0.0.1:{port}/mcp", bridge))
    finally:
        # Ask only the window owned by this test process to close normally.
        import ctypes
        from ctypes import wintypes
        user32 = ctypes.windll.user32
        callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

        @callback_type
        def close_own_window(hwnd, _):
            pid = wintypes.DWORD()
            user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
            if pid.value == process.pid:
                user32.PostMessageW(hwnd, 0x0010, 0, 0)
            return True

        user32.EnumWindows(close_own_window, 0)
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.terminate()
            process.wait(timeout=5)
    assert process.returncode == 0, process.returncode
    print("Packaged GUI closed cleanly.")


if __name__ == "__main__":
    main()

