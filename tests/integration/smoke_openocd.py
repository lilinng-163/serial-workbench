# SPDX-License-Identifier: Apache-2.0
"""Packaged MCP OpenOCD workflow using a process fixture, never hardware."""

import argparse
import asyncio
import json
from pathlib import Path
import socket
import subprocess
import tempfile
import time

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client
from mcp.client.streamable_http import streamable_http_client
from smoke_native import unpack


async def check(url, bridge, config):
    async with streamable_http_client(url) as (read, write, _):
        async with ClientSession(read, write) as client:
            await client.initialize()
            tools = {item.name: item for item in (await client.list_tools()).tools}
            assert len(tools) == 35
            assert all(name in tools for name in (
                "get_openocd_state", "list_openocd_configs", "configure_openocd",
                "preview_openocd", "start_openocd", "cancel_openocd",
                "list_openocd_views", "create_openocd_view", "close_openocd_view",
                "export_openocd_configuration", "import_openocd_configuration"))
            state = unpack(await client.call_tool("get_openocd_state"))
            view_id = state["view_id"]
            state = unpack(await client.call_tool("configure_openocd", {
                "view_id": view_id, "revision": state["revision"], "patch": config, "title": "Board A"}))
            catalog = unpack(await client.call_tool("list_openocd_configs", {"view_id": view_id}))
            assert not catalog["error"] and "board/fixture.cfg" in catalog["boards"]
            preview = unpack(await client.call_tool("preview_openocd", {"view_id": view_id, "operation": "flash"}))
            assert preview["writes_flash"] and preview["revision"] == state["revision"]
            second = unpack(await client.call_tool("create_openocd_view", {
                "title": "Board B", "configuration": {**config, "adapter_serial": "PROBE-B"}}))
            second_id = second["view_id"]
            assert second_id != view_id
            assert (await client.call_tool("get_openocd_state")).isError
            assert (await client.call_tool("configure_openocd", {
                "revision": state["revision"], "patch": {"verify": False}})).isError
            state = unpack(await client.call_tool("configure_openocd", {
                "view_id": view_id, "revision": state["revision"], "patch": {"adapter_serial": "PROBE-A"}}))
            same_second = unpack(await client.call_tool("get_openocd_state", {"view_id": second_id}))
            assert same_second["revision"] == second["revision"]
            assert same_second["configuration"]["adapter_serial"] == "PROBE-B"
            assert len(unpack(await client.call_tool("list_openocd_views"))["views"]) == 2
            params = StdioServerParameters(command=str(bridge), args=["--url", url])
            async with stdio_client(params) as (sr, sw):
                async with ClientSession(sr, sw) as other:
                    await other.initialize()
                    same = unpack(await other.call_tool("get_openocd_state", {"view_id": view_id}))
                    assert same["configuration"] == state["configuration"]
                    started = unpack(await other.call_tool("start_openocd", {
                        "view_id": view_id, "operation": "flash", "revision": state["revision"]}))
                    assert started["job_id"]
                    for _ in range(100):
                        finished = unpack(await client.call_tool("get_openocd_state", {"view_id": view_id}))
                        if not finished["running"]:
                            break
                        await asyncio.sleep(0.03)
                    assert finished["status"] == "succeeded", finished
                    independently = unpack(await other.call_tool("get_openocd_state", {"view_id": view_id}))
                    assert independently["records"] == finished["records"]
                    assert independently["next_cursor"] == finished["next_cursor"]
                    assert finished["exit_code"] == 0
                    idle_second = unpack(await other.call_tool("get_openocd_state", {"view_id": second_id}))
                    assert idle_second["status"] == "idle" and not idle_second["records"]
                    document = unpack(await other.call_tool("export_openocd_configuration", {"view_id": view_id}))
                    assert document["format"] == "serial-workbench.openocd" and document["version"] == 1
                    changed = unpack(await client.call_tool("configure_openocd", {
                        "view_id": view_id, "revision": finished["revision"], "patch": {"verify": False}}))
                    document["title"] = "Imported board"
                    imported = unpack(await other.call_tool("import_openocd_configuration", {
                        "view_id": view_id, "revision": changed["revision"], "document": document}))
                    assert imported["configuration"] == document["configuration"]
                    assert imported["title"] == "Imported board" and not imported["running"]
                    invalid = {**document, "version": 2}
                    assert (await client.call_tool("import_openocd_configuration", {
                        "view_id": view_id, "revision": imported["revision"], "document": invalid})).isError
                    unchanged = unpack(await client.call_tool("get_openocd_state", {"view_id": view_id}))
                    assert unchanged["revision"] == imported["revision"]
                    unpack(await other.call_tool("close_openocd_view", {"view_id": view_id}))
                    assert (await client.call_tool("get_openocd_state", {"view_id": view_id})).isError
                    remaining = unpack(await client.call_tool("list_openocd_views"))["views"]
                    assert len(remaining) == 1 and remaining[0]["view_id"] == second_id
                    assert unpack(await client.call_tool("get_openocd_state", {"view_id": second_id}))["configuration"] == second["configuration"]
                    unpack(await client.call_tool("close_openocd_view", {"view_id": second_id}))
                    assert not unpack(await client.call_tool("list_openocd_views"))["views"]
                    recreated = unpack(await client.call_tool("create_openocd_view", {"title": "Recreated"}))
                    assert recreated["view_id"] not in (view_id, second_id)
                    print("PASS packaged OpenOCD HTTP/STDIO: board pages, configuration export/import, scoped config, catalog, preview, fixture job, independent logs, isolated close/recreate")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--app-dir", type=Path, required=True)
    parser.add_argument("--fixture", type=Path, required=True)
    args = parser.parse_args()
    app = args.app_dir.resolve() / "SerialWorkbench.exe"
    bridge = app.with_name("SerialWorkbenchMCP.exe")
    assert args.fixture.is_file()
    with tempfile.TemporaryDirectory(prefix="openocd-http-", dir=Path("build")) as root:
        folder = Path(root).resolve()
        (folder / "scripts/board").mkdir(parents=True)
        (folder / "scripts/board/fixture.cfg").write_text("# test fixture\n", encoding="utf-8")
        firmware = folder / "firmware.bin"
        firmware.write_bytes(b"process fixture, not a real firmware image")
        settings = folder / "settings.json"
        settings.write_text(json.dumps({"allow_ai_control": True, "mcp_protocols": {"openocd": True}}), encoding="utf-8")
        config = {"executable": str(args.fixture.resolve()), "scripts_dir": str(folder / "scripts"),
                  "mode": "board", "board": "board/fixture.cfg",
                  "images": [{"path": str(firmware), "address": "0x08000000"}]}
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            port = sock.getsockname()[1]
        process = subprocess.Popen([str(app), "--headless", "--settings", str(settings), "--mcp-port", str(port)])
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
            asyncio.run(check(f"http://127.0.0.1:{port}/mcp", bridge, config))
        finally:
            try:
                subprocess.run([str(app.with_name("SerialWorkbenchCLI.exe")), "stop", "--settings", str(settings)],
                               check=True, capture_output=True, timeout=10)
                process.wait(timeout=10)
            finally:
                if process.poll() is None:
                    process.terminate()
                    process.wait(timeout=5)


if __name__ == "__main__":
    main()
