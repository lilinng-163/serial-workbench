# SPDX-License-Identifier: Apache-2.0

import argparse
import asyncio

from serial_assistant.bridge import run_bridge

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="STDIO bridge to the running Serial Workbench")
    parser.add_argument("--url", default="http://127.0.0.1:8765/mcp")
    args = parser.parse_args()
    asyncio.run(run_bridge(args.url))
