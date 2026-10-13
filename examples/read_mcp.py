# SPDX-License-Identifier: Apache-2.0

"""Run with the project Python while the desktop application is open."""

import asyncio
import json

from mcp import ClientSession
from mcp.client.streamable_http import streamable_http_client


async def main():
    cursor = 0
    session_id = None
    async with streamable_http_client("http://127.0.0.1:8765/mcp") as (read, write, _):
        async with ClientSession(read, write) as client:
            await client.initialize()
            connections = await client.call_tool("list_connections")
            print(connections.model_dump_json(indent=2))
            while True:
                result = await client.call_tool("read_events", {"after": cursor, "limit": 200, "wait_ms": 5000})
                if result.isError:
                    raise RuntimeError(result.content)
                data = result.structuredContent or json.loads(result.content[0].text)
                if session_id is not None and session_id != data["session_id"]:
                    cursor = 0
                    session_id = data["session_id"]
                    print("Application restarted; resetting cursor.")
                    continue
                session_id = data["session_id"]
                if data["gap"]:
                    print("Some older events have expired from the bounded buffer.")
                for event in data["events"]:
                    print(event["time"], event["channel_id"], event["direction"], event["hex"], event["text"])
                cursor = data["next_cursor"]


if __name__ == "__main__":
    asyncio.run(main())
