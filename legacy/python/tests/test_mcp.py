import asyncio
import json
import os
from pathlib import Path
import socket
import sys

from mcp import ClientSession, StdioServerParameters, types
from mcp.client.stdio import stdio_client
from mcp.client.streamable_http import streamable_http_client
import pytest

from serial_assistant import __version__
from serial_assistant.engine import ConnectionConfig
from serial_assistant.mcp_server import MCPService
from conftest import eventually


def result_data(result):
    assert not result.isError, result
    return result.structuredContent or json.loads(result.content[0].text)


@pytest.fixture
def service(engine):
    with socket.socket() as reserve:
        reserve.bind(("127.0.0.1", 0))
        port = reserve.getsockname()[1]
    service = MCPService(engine, port)
    service.start()
    assert service.wait_ready(), service.error
    yield service
    service.stop()


def test_mcp_two_clients_read_same_serial_stream_and_permissions(engine, service):
    connection = engine.connect(ConnectionConfig(port="loop://"))
    channel_id = connection["id"]
    eventually(lambda: engine.get(channel_id).state == "connected")
    engine.send(channel_id, "hello AI\n1.5,2.5\n")
    eventually(lambda: engine.get(channel_id).rx_bytes > 0)

    async def exercise():
        async with streamable_http_client(service.url) as (ra, wa, _):
            async with ClientSession(ra, wa) as a:
                await a.initialize()
                tools = (await a.list_tools()).tools
                assert {t.name for t in tools} >= {"get_status", "read_events", "send_data", "read_waveform", "open_connection"}
                async with streamable_http_client(service.url) as (rb, wb, _):
                    async with ClientSession(rb, wb) as b:
                        await b.initialize()
                        results = await asyncio.gather(a.call_tool("read_events", {"channel_id": channel_id}), b.call_tool("read_events", {"channel_id": channel_id}))
                        first, second = map(result_data, results)
                        assert first == second
                        assert b"hello AI" in b"".join(bytes.fromhex(e["hex"]) for e in first["events"] if e["direction"] == "rx")
                        denied = await a.call_tool("send_data", {"channel_id": channel_id, "payload": "blocked"})
                        assert denied.isError
                        assert (await a.call_tool("open_connection", {"config": {"kind": "demo"}})).isError
                        engine.allow_ai_control = True
                        sent = result_data(await a.call_tool("send_data", {"channel_id": channel_id, "payload": "02 ff", "hex_mode": True}))
                        assert sent["sent_bytes"] == 2
                        wave = result_data(await b.call_tool("read_waveform", {"channel_id": channel_id}))
                        assert wave["sources"]["default"]["total_samples"] == 1
                        status = result_data(await b.call_tool("get_status"))
                        assert status["session_id"] == engine.journal.session_id
                        assert status["process"]["pid"] == os.getpid()
                        assert status["process"]["username"]
                        assert status["version"] == __version__
                        assert (await a.call_tool("read_events", {"limit": 1001})).isError
                        assert len(engine.journal.snapshot(channel_id)) >= len(first["events"])
    asyncio.run(exercise())


def test_mcp_port_conflict_is_visible_and_can_retry(engine):
    with socket.socket() as occupied:
        occupied.bind(("127.0.0.1", 0))
        occupied.listen()
        port = occupied.getsockname()[1]
        service = MCPService(engine, port)
        service.start()
        assert not service.running
        assert not service.wait_ready()
        assert str(port) in service.error
        assert "旧窗口" in service.error
    try:
        service.start()
        assert service.wait_ready(), service.error
        assert not service.error
    finally:
        service.stop()


def test_stdio_bridge_reads_same_desktop_engine(engine, service):
    engine.journal.append("fixture", "rx", b"stdio bridge")

    async def exercise():
        params = StdioServerParameters(command=sys.executable, args=[str(Path(__file__).resolve().parent.parent / "mcp_bridge.py"), "--url", service.url])
        async with stdio_client(params) as (read, write):
            async with ClientSession(read, write) as session:
                await session.initialize()
                assert len((await session.list_tools()).tools) == 9
                result = result_data(await session.call_tool("read_events", {}))
                assert result["events"][0]["text"] == "stdio bridge"
                assert result["session_id"] == engine.journal.session_id
                denied = await session.call_tool("send_data", {"channel_id": "fixture", "payload": "blocked"})
                assert denied.isError
    asyncio.run(exercise())


def test_long_poll_does_not_block_other_mcp_tools(engine, service):
    async def exercise():
        async with streamable_http_client(service.url) as (read, write, _):
            async with ClientSession(read, write) as client:
                await client.initialize()
                pending = asyncio.create_task(client.call_tool("read_events", {"wait_ms": 1500}))
                await asyncio.sleep(0.1)
                status = await asyncio.wait_for(client.call_tool("get_status"), timeout=1)
                assert result_data(status)["session_id"] == engine.journal.session_id
                engine.journal.append("test", "rx", b"wake both readers")
                events = result_data(await pending)
                assert events["events"][0]["text"] == "wake both readers"
    asyncio.run(exercise())


def test_mcp_can_open_send_and_close_when_enabled(engine, service):
    engine.allow_ai_control = True

    async def exercise():
        async with streamable_http_client(service.url) as (read, write, _):
            async with ClientSession(read, write) as client:
                await client.initialize()
                opened = result_data(await client.call_tool("open_connection", {"config": {"kind": "serial", "port": "loop://", "baudrate": 460800}}))
                channel_id = opened["id"]
                for _ in range(100):
                    if engine.get(channel_id).state == "connected":
                        break
                    await asyncio.sleep(0.02)
                assert engine.get(channel_id).state == "connected"
                assert result_data(await client.call_tool("send_data", {"channel_id": channel_id, "payload": "AI test", "newline": "lf"}))["sent_bytes"] == 8
                closed = result_data(await client.call_tool("close_connection", {"channel_id": channel_id}))
                assert closed["state"] == "closed"
    asyncio.run(exercise())


def test_call_logs_distinguish_http_clients_and_stdio_bridge_preserves_identity(engine, service):
    async def exercise():
        for name in ("agent-A", "agent-B"):
            async with streamable_http_client(service.url) as (read, write, _):
                async with ClientSession(read, write, client_info=types.Implementation(name=name, version="1")) as client:
                    await client.initialize()
                    await client.call_tool("get_status")
        params = StdioServerParameters(command=sys.executable, args=[str(Path(__file__).resolve().parent.parent / "mcp_bridge.py"), "--url", service.url])
        async with stdio_client(params) as (read, write):
            async with ClientSession(read, write, client_info=types.Implementation(name="agent-stdio", version="1")) as client:
                await client.initialize()
                await client.call_tool("get_status")
    asyncio.run(exercise())
    assert {entry["client"] for entry in service.audit.snapshot()} == {"agent-A", "agent-B", "agent-stdio"}
    assert all(entry["status"] == "ok" for entry in service.audit.snapshot())
