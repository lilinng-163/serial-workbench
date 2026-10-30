# SPDX-License-Identifier: Apache-2.0
"""Exercise the packaged 3D tools over HTTP and STDIO, in an isolated runtime."""
import argparse
import asyncio
import base64
import json
import os
from pathlib import Path
import subprocess
import struct
import zlib
import tempfile
import time

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client
from mcp.client.streamable_http import streamable_http_client

from smoke_cli import unpack, visible_windows

ROOT = Path(__file__).resolve().parents[2]


async def workflow(directory, profile, status, cli, model_path, captures):
    async with streamable_http_client(status["mcp_url"]) as (read, write, _):
        async with ClientSession(read, write) as client:
            await client.initialize()
            names = {tool.name for tool in (await client.list_tools()).tools}
            assert {"list_model_views", "open_model_view", "get_model_state",
                    "configure_model_view", "capture_model_view"} <= names
            for topic in ("model3d", "agents"):
                guide = unpack(await client.call_tool("read_usage_guide", {"topic": topic}))
                assert guide["guide"] and len(guide["guide"]) > 500

            channel = unpack(await client.call_tool("open_connection", {
                "config": {"kind": "serial", "port": "loop://", "baudrate": 115200},
            }))
            cid = channel["id"]
            opened = unpack(await client.call_tool("open_model_view", {
                "file_path": str(model_path), "channel_id": cid,
            }))
            view_id = opened["view_id"]

            async def state():
                return unpack(await client.call_tool("get_model_state", {"view_id": view_id}))

            for _ in range(100):
                current = await state()
                if not current["loading"]:
                    break
                await asyncio.sleep(.05)
            assert not current["loading"] and not current["load_error"], current
            assert current["model"]["triangles"] == 4 and not current["using_example_mesh"]
            assert view_id in [v["view_id"] for v in unpack(
                await client.call_tool("list_model_views", {"channel_id": cid}))["views"]]

            async def configure(settings, **extra):
                current = await state()
                return unpack(await client.call_tool("configure_model_view", {
                    "view_id": view_id, "expected_revision": current["revision"],
                    "settings": settings, **extra,
                }))

            await configure({
                "stream": "imu", "centered": False, "follow_theme": False, "color": "#34B4D0",
                "manual_position": [3, 4, 5],
                "mapping": {"position": [{"field": key} for key in ("x", "y", "z")]},
            })
            unpack(await client.call_tool("send_data", {
                "channel_id": cid,
                "payload": "@wave[imu] x=1,y=2,z=3,roll=0,pitch=0,yaw=90\n"
                           "@wave[motor] x=99,y=99,z=99,roll=0,pitch=0,yaw=0\n",
            }))
            for _ in range(100):
                current = await state()
                if current["telemetry"]["rotation_sequence"] > 0:
                    break
                await asyncio.sleep(.05)
            assert current["model"]["position"] == [4, 6, 8], current
            rows = current["model"]["model_matrix_rows"]
            assert abs(rows[0][0]) < .0001 and abs(rows[1][0] - 1) < .0001, rows
            assert current["telemetry"]["rotation_sequence"] == current["telemetry"]["position_sequence"]
            adopted = current["telemetry"]["rotation_sequence"]
            assert current["telemetry"]["observed_sequence"] > adopted
            assert not current["telemetry"]["errors"]
            assert current["model"]["effective_color"] == "#34b4d0"

            params = StdioServerParameters(
                command=str(directory / "SerialWorkbenchMCP.exe"),
                args=["--settings", str(profile)],
            )
            async with stdio_client(params) as (r2, w2):
                async with ClientSession(r2, w2) as second:
                    await second.initialize()
                    other = unpack(await second.call_tool("get_model_state", {"view_id": view_id}))
                    assert other == current, (other, current)
                    old_revision = current["revision"]
                    current = await configure({}, fit=True)
                    assert current["revision"] != old_revision
                    rejected = await second.call_tool("configure_model_view", {
                        "view_id": view_id, "expected_revision": old_revision,
                        "settings": {"scale": 99},
                    })
                    assert rejected.isError
                    # Both transports preserve native MCP image content and
                    # keep base64 out of structured/text state metadata.
                    for label, session in (("http", client), ("stdio", second)):
                        result = await session.call_tool("capture_model_view", {
                            "view_id": view_id, "max_size": 640,
                        })
                        metadata = unpack(result)
                        images = [item for item in result.content if item.type == "image"]
                        assert len(images) == 1 and images[0].mimeType == "image/png"
                        assert "image_png_base64" not in metadata
                        assert metadata["model"] == current["model"]
                        raw = base64.b64decode(images[0].data, validate=True)
                        assert raw[:8] == b"\x89PNG\r\n\x1a\n"
                        width, height = struct.unpack_from(">II", raw, 16)
                        assert 0 < max(width, height) <= 640
                        assert (width, height) == (metadata["image_width"], metadata["image_height"])
                        offset = 8
                        compressed = bytearray()
                        while offset < len(raw):
                            size = struct.unpack_from(">I", raw, offset)[0]
                            kind = raw[offset + 4:offset + 8]
                            data = raw[offset + 8:offset + 8 + size]
                            checksum = struct.unpack_from(">I", raw, offset + 8 + size)[0]
                            assert zlib.crc32(kind + data) == checksum
                            if kind == b"IDAT":
                                compressed.extend(data)
                            offset += size + 12
                        assert offset == len(raw)
                        pixels = zlib.decompress(compressed)
                        assert len(pixels) >= height * (width * 3 + 1)
                        assert len(set(pixels)) > 16
                        if captures:
                            (captures / f"integration-{label}.png").write_bytes(raw)
                    events_a = unpack(await client.call_tool("read_events", {"channel_id": cid, "after": 0}))
                    events_b = unpack(await second.call_tool("read_events", {"channel_id": cid, "after": 0}))
                    assert events_a == events_b, "3D reads must not consume shared history"

                    # Invalid quaternion retains the preceding valid orientation
                    # and reports its provenance/error instead of inventing data.
                    await configure({"mapping": {"mode": "quaternion"}})
                    unpack(await client.call_tool("send_data", {
                        "channel_id": cid, "payload": "@wave[imu] qw=1,qx=0,qy=0,qz=0\n",
                    }))
                    for _ in range(100):
                        current = await state()
                        if current["telemetry"]["rotation_sequence"] > adopted:
                            break
                        await asyncio.sleep(.05)
                    quaternion_sequence = current["telemetry"]["rotation_sequence"]
                    assert quaternion_sequence > adopted
                    rows = current["model"]["model_matrix_rows"]
                    assert abs(rows[0][0] - 1) < .0001 and abs(rows[1][0]) < .0001
                    unpack(await client.call_tool("send_data", {
                        "channel_id": cid, "payload": "@wave[imu] qw=0,qx=0,qy=0,qz=0\n",
                    }))
                    for _ in range(100):
                        current = await state()
                        if current["telemetry"]["observed_sequence"] > quaternion_sequence:
                            break
                        await asyncio.sleep(.05)
                    assert current["telemetry"]["rotation_sequence"] == quaternion_sequence
                    assert current["telemetry"]["errors"]
                    retained_errors = current["telemetry"]["errors"]
                    current = await configure({"color": "#d08542", "paused": True})
                    assert current["telemetry"]["errors"] == retained_errors
                    assert current["telemetry"]["rotation_sequence"] == quaternion_sequence
                    assert not visible_windows(status["pid"]), "Background render opened a desktop window"
                    shown = cli("show")
                    assert shown["pid"] == status["pid"] and shown["visible"]
                    assert visible_windows(status["pid"])
                    after = await state()
                    assert after["settings"] == current["settings"]
                    assert after["model"] == current["model"]
                    assert after["telemetry"] == current["telemetry"]
    print("PASS 3D HTTP/STDIO import, stream isolation, transforms, revisions, PNG images and GUI handover", flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--app-dir", required=True, type=Path)
    parser.add_argument("--captures", type=Path)
    args = parser.parse_args()
    directory = args.app_dir.resolve()
    if args.captures:
        args.captures.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="mcp-3d-测试-", dir=ROOT / "build") as temporary:
        profile = Path(temporary) / "settings.json"
        profile.write_text('{"mcp_port":0}', encoding="utf-8")
        mesh = Path(temporary) / "姿态模型.obj"
        mesh.write_text("v 0 0 0\nv 2 0 0\nv 0 1 0\nv 0 0 1\n"
                        "f 1 2 3\nf 1 4 2\nf 1 3 4\nf 2 4 3\n", encoding="utf-8")

        def cli(*arguments, expected=0):
            process = subprocess.run([
                str(directory / "SerialWorkbenchCLI.exe"), *arguments, "--settings", str(profile),
            ], capture_output=True, text=True, encoding="utf-8", timeout=25,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
            assert process.returncode == expected, (arguments, process.returncode, process.stdout, process.stderr)
            return json.loads(process.stdout)

        clients = cli("agents")
        assert len(clients["agents"]) == 26 and len(clients["skill_targets"]) == 17
        for agent, root in (("cursor", "mcpServers"), ("vscode", "servers"),
                            ("kilo", "mcp"), ("zed", "context_servers"), ("generic", "mcpServers")):
            exported = cli("export-agent", agent)
            assert not exported["files_changed"]
            assert "serial-workbench" in json.loads(exported["configuration"])[root]
        assert not profile.exists() or json.loads(profile.read_text(encoding="utf-8")) == {"mcp_port": 0}
        print("PASS CLI client discovery and configuration export without starting a runtime", flush=True)
        try:
            status = cli("start", "--allow-control")
            assert not status["visible"] and status["allow_ai_control"]
            asyncio.run(workflow(directory, profile, status, cli, mesh, args.captures))
        finally:
            cli("stop")
            for _ in range(50):
                process = subprocess.run([
                    str(directory / "SerialWorkbenchCLI.exe"), "status", "--settings", str(profile),
                ], capture_output=True, timeout=10,
                    creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
                if process.returncode == 3:
                    break
                time.sleep(.1)
            else:
                raise AssertionError("Isolated runtime failed to stop")


if __name__ == "__main__":
    main()
