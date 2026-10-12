"""Exercise packaged native HTTP/stdio transports with the official MCP SDK."""

import argparse
import asyncio
import ctypes
from ctypes import wintypes
import json
from pathlib import Path
import socket
import subprocess
import tempfile
import time

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client
from mcp.client.streamable_http import streamable_http_client

ROOT = Path(__file__).resolve().parents[1]


def unpack(result):
    assert not result.isError, result
    return result.structuredContent or json.loads(result.content[0].text)


def close_window(process):
    user32 = ctypes.windll.user32
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

    @callback_type
    def visit(hwnd, _):
        pid = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        if pid.value == process.pid:
            user32.PostMessageW(hwnd, 0x0010, 0, 0)
        return True

    user32.EnumWindows(visit, 0)
    process.wait(timeout=10)


async def check(url, bridge, allow):
    async with streamable_http_client(url) as (read, write, _):
        async with ClientSession(read, write) as client:
            initialized = await client.initialize()
            assert initialized.serverInfo.version == "0.8.0"
            assert len((await client.list_tools()).tools) == 9
            status = unpack(await client.call_tool("get_status"))
            assert status["implementation"] == "Qt C++"
            assert status["allow_ai_control"] is allow
            assert status["process"]["username"]
            events = unpack(await client.call_tool("read_events", {}))
            assert any(e["direction"] == "rx" for e in events["events"])
            params = StdioServerParameters(command=str(bridge), args=["--url", url])
            async with stdio_client(params) as (sr, sw):
                async with ClientSession(sr, sw) as second:
                    await second.initialize()
                    assert len((await second.list_tools()).tools) == 9
                    other = unpack(await second.call_tool("read_events", {}))
                    assert other["session_id"] == events["session_id"]
                    assert other["events"][:len(events["events"])] == events["events"]
                    config = {"config": {"kind": "serial", "port": "loop://", "baudrate": 115200}}
                    opened = await second.call_tool("open_connection", config)
                    if not allow:
                        assert opened.isError
                        print("PASS HTTP/stdio, shared independent reads, write permission denied")
                        return
                    channel = unpack(opened)
                    channel_id = channel["id"]
                    assert channel["applied_baudrate"] == 115200
                    config["config"]["baudrate"] = 1200
                    updated = unpack(await second.call_tool("open_connection", config))
                    assert updated["id"] == channel_id
                    assert updated["applied_baudrate"] == 1200
                    start = unpack(await client.call_tool("read_events", {}))["next_cursor"]
                    # A pending read must leave the GUI/service free to accept a second reader's write.
                    waiter = asyncio.create_task(client.call_tool("read_events", {
                        "after": start, "channel_id": channel_id, "wait_ms": 2000,
                    }))
                    await asyncio.sleep(0.1)
                    unpack(await second.call_tool("send_data", {
                        "channel_id": channel_id, "payload": "temperature=25.5,voltage=3.3\n",
                    }))
                    assert unpack(await waiter)["events"]
                    await asyncio.sleep(0.1)
                    retained = unpack(await second.call_tool("read_events", {"after": start, "channel_id": channel_id}))
                    assert any(e["direction"] == "rx" and "temperature=25.5" in e["text"] for e in retained["events"])
                    wave = unpack(await client.call_tool("read_waveform", {"channel_id": channel_id}))
                    assert wave["sources"]["default"]["samples"]
                    unpack(await second.call_tool("close_connection", {"channel_id": channel_id}))
                    reopened = unpack(await second.call_tool("open_connection", config))
                    assert reopened["id"] == channel_id
                    assert reopened["rx_bytes"] > 0
                    denied = await client.call_tool("send_data", {"channel_id": channel_id, "payload": "GG", "hex_mode": True})
                    assert denied.isError
                    invalid = await client.call_tool("read_events", {"after": -1})
                    assert invalid.isError
                    status = unpack(await client.call_tool("get_status"))
                    assert status["session_id"] == events["session_id"]
                    print("PASS HTTP/stdio, 9 tools, shared history, baud reconfiguration, long poll, loopback, waveform, reconnect, validation")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--app-dir", type=Path, default=ROOT / "dist" / "SerialWorkbenchNative")
    args = parser.parse_args()
    executable = args.app_dir.resolve() / "SerialWorkbench.exe"
    for allow in (False, True):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as temporary:
            settings = Path(temporary) / "settings.json"
            settings.write_text(json.dumps({"allow_ai_control": allow}), encoding="utf-8")
            with socket.socket() as sock:
                sock.bind(("127.0.0.1", 0))
                port = sock.getsockname()[1]
            process = subprocess.Popen([str(executable), "--demo", "--settings", str(settings), "--mcp-port", str(port)])
            try:
                for _ in range(50):
                    assert process.poll() is None, process.returncode
                    try:
                        with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                            break
                    except OSError:
                        time.sleep(0.1)
                else:
                    raise TimeoutError("Native MCP did not start")
                time.sleep(0.3)
                asyncio.run(check(f"http://127.0.0.1:{port}/mcp", executable.with_name("SerialWorkbenchMCP.exe"), allow))
            finally:
                close_window(process)


if __name__ == "__main__":
    main()
