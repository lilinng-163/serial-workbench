# SPDX-License-Identifier: Apache-2.0

"""Exercise packaged native HTTP/stdio transports with the official MCP SDK."""

import argparse
import asyncio
import ctypes
from ctypes import wintypes
import json
import re
from pathlib import Path
import socket
import subprocess
import tempfile
import time

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client
from mcp.client.streamable_http import streamable_http_client

ROOT = Path(__file__).resolve().parents[2]


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


async def check(url, bridge, allow, expected_version=None):
    async with streamable_http_client(url) as (read, write, _):
        async with ClientSession(read, write) as client:
            initialized = await client.initialize()
            if expected_version:
                assert initialized.serverInfo.version == expected_version
            assert len((await client.list_tools()).tools) == 24
            assert "read_usage_guide" in initialized.instructions
            guide = unpack(await client.call_tool("read_usage_guide", {}))
            assert guide["guide"] and guide["skill_path_scope"] == "server_host"
            assert guide["skill_path"] and Path(guide["skill_path"]).is_file()
            parser_guide = unpack(await client.call_tool("read_usage_guide", {"topic": "frame_script"}))
            assert "void main(const uint8_t data[], int length)" in parser_guide["guide"]
            code = re.search(r"```c\n(.*?)\n```", parser_guide["guide"], re.S).group(1)
            validation = unpack(await client.call_tool("validate_frame_script", {
                "code": code, "sample_chunks": ["AA55C4", "09E40CBC0D"],
            }))
            assert validation["valid"] and validation["frames"][0]["温度"] == 25
            missing = await client.call_tool("read_events", {})
            assert missing.isError and "channel_id" in missing.content[0].text
            status = unpack(await client.call_tool("get_status"))
            assert status["version"] == initialized.serverInfo.version
            assert status["implementation"] == "Qt C++"
            assert status["allow_ai_control"] is allow
            assert status["process"]["username"]
            events = unpack(await client.call_tool("read_all_events", {}))
            assert any(e["direction"] == "rx" for e in events["events"])
            params = StdioServerParameters(command=str(bridge), args=["--url", url])
            async with stdio_client(params) as (sr, sw):
                async with ClientSession(sr, sw) as second:
                    await second.initialize()
                    assert len((await second.list_tools()).tools) == 24
                    other = unpack(await second.call_tool("read_all_events", {}))
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
                    start = unpack(await client.call_tool("read_all_events", {}))["next_cursor"]
                    # Unrelated events advance the global cursor but must not end a channel-filtered wait.
                    unrelated = unpack(await second.call_tool("open_connection", {"config": {
                        "kind": "udp", "host": "127.0.0.1", "network_port": 9,
                        "bind_host": "127.0.0.1", "bind_port": 0,
                    }}))
                    unpack(await second.call_tool("close_connection", {"channel_id": unrelated["id"]}))
                    # A pending read must leave the GUI/service free to accept a second reader's write.
                    waiter = asyncio.create_task(client.call_tool("read_events", {
                        "after": start, "channel_id": channel_id, "wait_ms": 2000,
                    }))
                    await asyncio.sleep(0.1)
                    unpack(await second.call_tool("send_data", {
                        "channel_id": channel_id,
                        "payload": "@wave temperature=25.5,voltage=3.3,count=4294967295,precision=1.2345678901234567\n",
                    }))
                    assert unpack(await waiter)["events"]
                    await asyncio.sleep(0.1)
                    retained = unpack(await second.call_tool("read_events", {"after": start, "channel_id": channel_id}))
                    assert any(e["direction"] == "rx" and "temperature=25.5" in e["text"] for e in retained["events"])
                    wave = unpack(await client.call_tool("read_waveform", {"channel_id": channel_id}))
                    assert wave["protocol"] == "wave/1"
                    assert wave["sources"]["default"]["samples"]
                    values = wave["sources"]["default"]["samples"][-1]["values"]
                    assert values["count"] == 4294967295
                    assert values["precision"] == 1.2345678901234567
                    sample_count = wave["sources"]["default"]["total_samples"]
                    unpack(await second.call_tool("send_data", {
                        "channel_id": channel_id,
                        "payload": 'temperature=999\n{"temperature":888}\n1,2,3\n',
                    }))
                    await asyncio.sleep(0.1)
                    unchanged = unpack(await client.call_tool("read_waveform", {"channel_id": channel_id}))
                    assert unchanged["sources"]["default"]["total_samples"] == sample_count
                    assert unchanged["sources"]["default"]["rejected_lines"] == 0
                    unpack(await second.call_tool("send_data", {
                        "channel_id": channel_id,
                        "payload": "@wave[imu] yaw=10\n@wave[motor] yaw=100\n@wave[imu] yaw=20\n",
                    }))
                    for _ in range(50):
                        named = unpack(await client.call_tool("read_waveform", {
                            "channel_id": channel_id, "stream": "imu", "limit": 1,
                        }))
                        selected = named["sources"]["default"]["samples"]
                        if selected and selected[-1]["values"]["yaw"] == 20:
                            break
                        await asyncio.sleep(.02)
                    else:
                        raise TimeoutError("Named wave stream did not reach MCP")
                    assert len(selected) == 1 and selected[0]["stream"] == "imu"
                    assert named["sources"]["default"]["available_streams"] == ["", "imu", "motor"]
                    motor = unpack(await second.call_tool("read_waveform", {
                        "channel_id": channel_id, "stream": "motor", "limit": 1,
                    }))
                    assert motor["sources"]["default"]["samples"][0]["values"]["yaw"] == 100
                    legacy = unpack(await client.call_tool("read_waveform", {
                        "channel_id": channel_id, "stream": "", "limit": 1,
                    }))
                    assert legacy["sources"]["default"]["samples"][0]["values"] == values
                    assert named == unpack(await second.call_tool("read_waveform", {
                        "channel_id": channel_id, "stream": "imu", "limit": 1,
                    }))
                    print("PASS named wave streams over HTTP/STDIO, filtered limits and legacy compatibility")
                    views = unpack(await client.call_tool("list_search_views", {"channel_id": channel_id}))["views"]
                    log_view = next(v for v in views if v["kind"] == "log")
                    found = unpack(await client.call_tool("search_logs", {
                        "channel_id": channel_id, "view_id": log_view["view_id"],
                        "filter": {"text": "temperature && 999", "logical": True,
                                   "regex": False, "whole_word": False, "case_sensitive": False},
                    }))
                    assert found["total_matches"] > 0
                    assert all("temperature=999" in r["text"] for r in found["rows"])
                    live_script = unpack(await second.call_tool("get_frame_script", {"channel_id": channel_id}))
                    applied = unpack(await second.call_tool("set_frame_script", {
                        "channel_id": channel_id, "code": code, "enabled": True,
                        "expected_revision": live_script["revision"], "sample_chunks": ["AA55C4", "09E40CBC0D"],
                    }))
                    assert applied["enabled"] and applied["code"] == code
                    assert applied["sample_chunks"] == ["AA 55 C4", "09 E4 0C BC 0D"]
                    unpack(await second.call_tool("send_data", {
                        "channel_id": channel_id, "hex_mode": True, "payload": "AA55C409E40CBC0D",
                    }))
                    records = []
                    for _ in range(50):
                        parsed = unpack(await client.call_tool("read_script_results", {"channel_id": channel_id}))
                        records = parsed.get("sources", {}).get("default", {}).get("records", [])
                        if records:
                            break
                        await asyncio.sleep(.02)
                    assert records[-1]["value"]["温度"] == 25
                    print("PASS native GUI search and synchronized script validate/apply/results over HTTP/STDIO")
                    unpack(await second.call_tool("close_connection", {"channel_id": channel_id}))
                    reopened = unpack(await second.call_tool("open_connection", config))
                    assert reopened["id"] == channel_id
                    assert reopened["rx_bytes"] > 0
                    denied = await client.call_tool("send_data", {"channel_id": channel_id, "payload": "GG", "hex_mode": True})
                    assert denied.isError
                    invalid = await client.call_tool("read_events", {"channel_id": channel_id, "after": -1})
                    assert invalid.isError
                    status = unpack(await client.call_tool("get_status"))
                    assert status["session_id"] == events["session_id"]
                    print("PASS HTTP/stdio, 24 tools, shared history, baud reconfiguration, long poll, loopback, strict wave/1, integer/double values, reconnect, validation")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--app-dir", type=Path, default=ROOT / "dist" / "SerialWorkbenchNative")
    parser.add_argument("--expected-version")
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
                asyncio.run(check(f"http://127.0.0.1:{port}/mcp", executable.with_name("SerialWorkbenchMCP.exe"), allow, args.expected_version))
            finally:
                close_window(process)


if __name__ == "__main__":
    main()
