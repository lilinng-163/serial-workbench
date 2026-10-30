# SPDX-License-Identifier: Apache-2.0
"""Packaged CLI/STDIO auto-start checks with isolated settings and loopback."""
import argparse
import asyncio
import concurrent.futures
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import re
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


def visible_windows(pid):
    if os.name != "nt":
        return []
    user32 = ctypes.windll.user32
    windows = []

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def visit(hwnd, _):
        owner = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid and user32.IsWindowVisible(hwnd):
            windows.append(hwnd)
        return True

    user32.EnumWindows(visit, 0)
    return windows


async def workflow(bridge, profile, status, version, cli):
    params = StdioServerParameters(command=str(bridge), args=["--settings", str(profile)])
    async with stdio_client(params) as (read, write):
        async with ClientSession(read, write) as client:
            initialized = await client.initialize()
            assert initialized.serverInfo.version == version
            assert len((await client.list_tools()).tools) == 24
            first = unpack(await client.call_tool("get_status", {}))
            assert first["process"]["pid"] == status["pid"]
            assert first["background"] and first["allow_ai_control"]
            channel = unpack(await client.call_tool("open_connection", {
                "config": {"kind": "serial", "port": "loop://", "baudrate": 115200},
            }))
            cid = channel["id"]
            unpack(await client.call_tool("send_data", {"channel_id": cid, "payload": "ALARM 42\nnormal 7\n"}))
            await asyncio.sleep(.15)
            async with streamable_http_client(status["mcp_url"]) as (r2, w2, _):
                async with ClientSession(r2, w2) as second:
                    await second.initialize()
                    a = unpack(await client.call_tool("read_events", {"channel_id": cid, "after": 0}))
                    b = unpack(await second.call_tool("read_events", {"channel_id": cid, "after": 0}))
                    assert a["events"] == b["events"] and a["session_id"] == b["session_id"]
                    views = unpack(await second.call_tool("list_search_views", {"channel_id": cid}))["views"]
                    view = next(v for v in views if v["kind"] == "log")
                    found = unpack(await second.call_tool("search_logs", {
                        "channel_id": cid, "view_id": view["view_id"],
                        "filter": {"text": "ALARM && 4[0-9]", "logical": True, "regex": True,
                                   "case_sensitive": True, "whole_word": False},
                    }))
                    assert found["rows"] and all("ALARM 42" in row["text"] for row in found["rows"])
                    guide = unpack(await client.call_tool("read_usage_guide", {"topic": "frame_script"}))
                    code = re.search(r"```c\n(.*?)\n```", guide["guide"], re.S).group(1)
                    samples = ["AA55C4", "09E40CBC0D"]
                    validated = unpack(await client.call_tool("validate_frame_script", {"code": code, "sample_chunks": samples}))
                    assert validated["valid"] and validated["frames"][0]["温度"] == 25
                    before = unpack(await client.call_tool("get_frame_script", {"channel_id": cid}))
                    applied = unpack(await client.call_tool("set_frame_script", {
                        "channel_id": cid, "code": code, "enabled": True,
                        "sample_chunks": samples, "expected_revision": before["revision"],
                    }))
                    assert applied["enabled"] and applied["sample_chunks"] == ["AA 55 C4", "09 E4 0C BC 0D"]
                    unpack(await client.call_tool("send_data", {"channel_id": cid, "payload": "AA55C409E40CBC0D", "hex_mode": True}))
                    records = []
                    for _ in range(60):
                        parsed = unpack(await client.call_tool("read_script_results", {"channel_id": cid}))
                        records = parsed.get("sources", {}).get("default", {}).get("records", [])
                        if records:
                            break
                        await asyncio.sleep(.05)
                    assert records[-1]["value"]["温度"] == 25
                    assert not visible_windows(status["pid"]), "Background MCP must not open a window/editor"
                    shown = cli("show")
                    assert shown["pid"] == status["pid"] and shown["visible"]
                    assert visible_windows(status["pid"])
                    after = unpack(await second.call_tool("get_status", {}))
                    assert after["session_id"] == first["session_id"] and not after["background"]
                    assert cid in [c["id"] for c in after["connections"]]
                    kept = unpack(await second.call_tool("get_frame_script", {"channel_id": cid}))
                    assert kept["revision"] == applied["revision"]
                    same = unpack(await second.call_tool("search_logs", {"channel_id": cid, "view_id": view["view_id"]}))
                    assert same["filter"] == found["filter"]
    print("PASS HTTP/STDIO cursors, loopback, native search, live script and GUI handover", flush=True)


async def bridge_boot(bridge, profile, cli):
    params = StdioServerParameters(command=str(bridge), args=["--settings", str(profile)])
    async with stdio_client(params) as (read, write):
        async with ClientSession(read, write) as client:
            await client.initialize()
            state = unpack(await client.call_tool("get_status", {}))
            assert state["background"] and not state["connections"]
            assert not visible_windows(state["process"]["pid"])
    assert cli("status")["running"], "Agent exit must not stop the shared service"
    print("PASS STDIO auto-start and shared service lifetime", flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--app-dir", type=Path, required=True)
    parser.add_argument("--expected-version", required=True)
    args = parser.parse_args()
    directory = args.app_dir.resolve()
    with tempfile.TemporaryDirectory(prefix="cli-测试-", dir=ROOT / "build") as temporary:
        profile = Path(temporary) / "settings.json"
        occupied = socket.socket()
        occupied.bind(("127.0.0.1", 0))
        occupied.listen()
        profile.write_text(json.dumps({"mcp_port": occupied.getsockname()[1]}), encoding="utf-8")

        def cli(*arguments, expected=0):
            process = subprocess.run([str(directory / "SerialWorkbenchCLI.exe"), *arguments,
                "--settings", str(profile)], capture_output=True, text=True, encoding="utf-8", timeout=25,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
            assert process.returncode == expected, (arguments, process.returncode, process.stdout, process.stderr)
            return json.loads(process.stdout)

        # CLI setup actions must target the same isolated Agent directories as
        # the installer, preserve unrelated MCP entries and install CLI docs.
        agent_home = Path(temporary) / "agent-codex"
        agent_home.mkdir()
        (agent_home / "config.toml").write_text("model = 'keep'\n[mcp_servers.other]\nurl = 'http://other.invalid/mcp'\n", encoding="utf-8")
        environment = os.environ.copy()
        environment["CODEX_HOME"] = str(agent_home)
        fake_bin = Path(temporary) / "bin"
        fake_bin.mkdir()
        (fake_bin / "codex.cmd").write_text("@exit /b 0\n", encoding="ascii")
        environment["PATH"] = str(fake_bin) + os.pathsep + environment.get("PATH", "")
        for action in ("install-skill", "configure-agent"):
            result = subprocess.run([str(directory / "SerialWorkbenchCLI.exe"), action, "codex", "--settings", str(profile)],
                capture_output=True, text=True, encoding="utf-8", env=environment, timeout=25,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
            assert result.returncode == 0, (result.stdout, result.stderr)
        assert (agent_home / "skills/serial-workbench/references/cli.md").is_file()
        text = (agent_home / "config.toml").read_text(encoding="utf-8")
        assert "--settings" in text and "model = 'keep'" in text and "http://other.invalid/mcp" in text
        print("PASS CLI skill installation and Agent configuration in an isolated profile", flush=True)

        def stop():
            cli("stop")
            for _ in range(40):
                process = subprocess.run([str(directory / "SerialWorkbenchCLI.exe"), "status", "--settings", str(profile)],
                    capture_output=True, timeout=10, creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
                if process.returncode == 3:
                    return
                time.sleep(.1)
            raise AssertionError("Runtime did not shut down")

        try:
            assert not cli("status", expected=3)["running"]
            with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
                states = list(pool.map(lambda _: cli("start"), range(3)))
            assert len({s["pid"] for s in states}) == 1
            first = states[0]
            assert not first["visible"] and not first["allow_ai_control"]
            assert not visible_windows(first["pid"])
            assert first["mcp_url"] != f"http://127.0.0.1:{occupied.getsockname()[1]}/mcp"
            assert cli("call", "open_connection", "--args", '{"config":{"kind":"serial","port":"loop://"}}', expected=4)["isError"]
            assert "error" in cli("start", "--allow-control", expected=2)
            assert not cli("status")["allow_ai_control"]
            assert "error" in cli("call", "get_status", "--args", "[]", expected=2)
            assert len(cli("tools")["tools"]) == 24
            print("PASS concurrent start, no windows, dynamic port, JSON errors and permission gate", flush=True)
            stop()
            status = cli("start", "--allow-control")
            assert status["allow_ai_control"]
            asyncio.run(workflow(directory / "SerialWorkbenchMCP.exe", profile, status, args.expected_version, cli))
            stop()
            asyncio.run(bridge_boot(directory / "SerialWorkbenchMCP.exe", profile, cli))
            stop()
            assert json.loads(profile.read_text(encoding="utf-8"))["allow_ai_control"]
            print("PASS graceful stop, restart and explicit permission persistence", flush=True)
        finally:
            stop()
            occupied.close()


if __name__ == "__main__":
    main()
