"""STDIO adapter: proxies tools to the running desktop, never opens serial ports."""

import sys

import anyio
from mcp import ClientSession, types
from mcp.client.streamable_http import streamable_http_client
from mcp.server import Server
from mcp.server.stdio import stdio_server


async def run_bridge(url):
    server = Server("Serial Workbench Bridge")

    @server.list_tools()
    async def list_tools():
        async with streamable_http_client(url) as (read, write, _):
            async with ClientSession(read, write) as session:
                await session.initialize()
                return (await session.list_tools()).tools

    @server.call_tool()
    async def call_tool(name, arguments):
        try:
            async with streamable_http_client(url) as (read, write, _):
                client_info = server.request_context.session.client_params.clientInfo
                async with ClientSession(read, write, client_info=client_info) as session:
                    await session.initialize()
                    return await session.call_tool(name, arguments)
        except Exception as exc:
            return types.CallToolResult(
                isError=True,
                content=[types.TextContent(type="text", text=f"无法调用桌面程序，请确认 Serial Workbench 已启动：{exc}")],
            )

    # Keep ownership of process streams: replacing TextIOWrapper around the
    # same buffer can close stdout during shutdown in frozen Windows builds.
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8")
    async with stdio_server(
        stdin=anyio.wrap_file(sys.stdin), stdout=anyio.wrap_file(sys.stdout)
    ) as (read, write):
        await server.run(read, write, server.create_initialization_options())
