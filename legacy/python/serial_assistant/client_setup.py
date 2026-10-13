# SPDX-License-Identifier: Apache-2.0

"""Merge only this server into client settings and check the actual MCP service."""

import asyncio
from datetime import datetime
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tomllib

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client
from mcp.client.streamable_http import streamable_http_client


SERVER = "serial-workbench"
CLIENTS = ("Codex", "Claude Code", "OpenCode CLI", "Claude Desktop")
TOKEN = re.compile(r'\s+|//[^\r\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|[^\s]', re.MULTILINE)


def json_tokens(text):
    return [match for match in TOKEN.finditer(text) if not match.group().isspace() and not match.group().startswith(("//", "/*"))]


def jsonc_load(text):
    tokens = [match.group() for match in json_tokens(text)]
    cleaned = "".join(token for index, token in enumerate(tokens) if not (token == "," and index + 1 < len(tokens) and tokens[index + 1] in ("}", "]")))
    return json.loads(cleaned)


def object_members(text, start=0):
    tokens = [token for token in json_tokens(text) if token.start() >= start]
    if not tokens or tokens[0].group() != "{":
        raise ValueError("配置节点必须为 JSON 对象")
    members = {}
    index = 1
    while index < len(tokens):
        token = tokens[index]
        if token.group() == "}":
            return members, token.start()
        if token.group() == ",":
            index += 1
            continue
        name = json.loads(token.group())
        if tokens[index + 1].group() != ":":
            raise ValueError("JSON 配置语法无效")
        index += 2
        begin = tokens[index].start()
        depth = 0
        while index < len(tokens):
            value = tokens[index].group()
            if value in ("{", "["):
                depth += 1
            elif value in ("}", "]"):
                if depth == 0:
                    break
                depth -= 1
            elif value == "," and depth == 0:
                break
            end = tokens[index].end()
            index += 1
        members[name] = (begin, end)
    raise ValueError("JSON 配置对象未闭合")


def merge_jsonc(text, section, name, value):
    parsed = jsonc_load(text)
    if not isinstance(parsed, dict):
        raise ValueError("配置根节点必须为对象")
    members, root_end = object_members(text)
    if section not in members:
        fragment = json.dumps({name: value}, ensure_ascii=False, indent=2)
        insert = f'{json.dumps(section)}: {fragment}'
        previous = [token for token in json_tokens(text) if token.start() < root_end][-1].group()
        separator = "" if not members or previous == "," else ","
        result = text[:root_end] + separator + "\n" + insert + "\n" + text[root_end:]
    else:
        begin, _ = members[section]
        entries, end = object_members(text, begin)
        fragment = json.dumps(value, ensure_ascii=False, indent=2)
        if name in entries:
            left, right = entries[name]
            result = text[:left] + fragment + text[right:]
        else:
            # Locate the final significant token, ignoring trailing comments.
            previous = [token for token in json_tokens(text) if begin <= token.start() < end][-1].group()
            separator = "" if not entries or previous == "," else ","
            result = text[:end] + separator + f'\n{json.dumps(name)}: {fragment}\n' + text[end:]
    jsonc_load(result)
    return result


def merge_codex(text, url):
    parsed = tomllib.loads(text)
    servers = parsed.get("mcp_servers", {})
    if not isinstance(servers, dict):
        raise ValueError("mcp_servers 必须是 TOML 表")
    if "mcp_servers" in parsed and re.search(r'^\s*mcp_servers\s*=', text, re.MULTILINE):
        raise ValueError("当前使用内联 mcp_servers，请改为表配置或使用复制配置手动合并")
    table = re.compile(r'^\s*\[mcp_servers\.(?:serial-workbench|"serial-workbench"|\'serial-workbench\')\]\s*(?:#[^\n]*)?$', re.MULTILINE)
    found = table.search(text)
    new = f'[mcp_servers.serial-workbench]\nurl = {json.dumps(url)}\nenabled = true\n'
    if found:
        tail = re.search(r'^\s*\[', text[found.end():], re.MULTILINE)
        end = found.end() + tail.start() if tail else len(text)
        # Remove nested settings of this server, such as old HTTP auth headers.
        while end < len(text) and re.match(r'\s*\[mcp_servers\.(?:serial-workbench|"serial-workbench"|\'serial-workbench\')\.', text[end:]):
            next_table = re.search(r'^\s*\[', text[end:].split("\n", 1)[1], re.MULTILINE)
            end = end + len(text[end:].split("\n", 1)[0]) + 1 + next_table.start() if next_table else len(text)
        result = text[:found.start()] + new + "\n" + text[end:]
    else:
        result = text.rstrip() + "\n\n" + new
    tomllib.loads(result)
    return result


def atomic_save(path, content):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        stamp = datetime.now().strftime("%Y%m%d-%H%M%S-%f")
        shutil.copy2(path, path.with_name(path.name + f".{stamp}.bak"))
    temporary = path.with_name(path.name + ".workbench.tmp")
    temporary.write_text(content, encoding="utf-8")
    temporary.replace(path)


def bridge_command():
    if getattr(sys, "frozen", False):
        return str(Path(sys.executable).with_name("SerialWorkbenchMCP.exe")), []
    root = Path(__file__).resolve().parent.parent
    return sys.executable, [str(root / "mcp_bridge.py")]


class ClientSetup:
    def __init__(self, url, home=None, appdata=None):
        self.url = url
        self.home = Path(home or Path.home())
        self.appdata = Path(appdata or os.environ.get("APPDATA", self.home / "AppData" / "Roaming"))

    def path(self, index):
        if index == 0:
            return Path(os.environ.get("CODEX_HOME", self.home / ".codex")) / "config.toml"
        if index == 1:
            return self.home / ".claude.json"
        if index == 2:
            base = Path(os.environ.get("XDG_CONFIG_HOME", self.home / ".config")) / "opencode"
            return base / ("opencode.jsonc" if (base / "opencode.jsonc").exists() else "opencode.json")
        return self.appdata / "Claude" / "claude_desktop_config.json"

    def entry(self, index):
        if index == 1:
            return {"type": "http", "url": self.url}
        if index == 2:
            return {"type": "remote", "url": self.url, "enabled": True, "oauth": False, "timeout": 15000}
        executable, arguments = bridge_command()
        return {"command": executable, "args": [*arguments, "--url", self.url]}

    def preview(self, index):
        if index == 0:
            return f'[mcp_servers.serial-workbench]\nurl = "{self.url}"\nenabled = true'
        return json.dumps({"mcp" if index == 2 else "mcpServers": {SERVER: self.entry(index)}}, ensure_ascii=False, indent=2)

    def install(self, index):
        path = self.path(index)
        text = path.read_text(encoding="utf-8-sig") if path.exists() else "" if index == 0 else "{}"
        content = merge_codex(text, self.url) if index == 0 else merge_jsonc(text, "mcp" if index == 2 else "mcpServers", SERVER, self.entry(index))
        atomic_save(path, content)
        return path

    async def probe(self, index):
        async def inspect_session(session):
            await session.initialize()
            tools = (await session.list_tools()).tools
            result = await session.call_tool("get_status")
            if result.isError:
                raise RuntimeError("get_status 调用失败")
            data = result.structuredContent or json.loads(result.content[0].text)
            if not {"get_status", "read_events", "list_connections"} <= {tool.name for tool in tools}:
                raise RuntimeError("MCP 服务缺少必要工具")
            return f'{len(tools)} 个工具 · {data.get("connected_count", 0)} 个连接 · v{data["version"]}'

        if index == 3:
            executable, arguments = bridge_command()
            async with stdio_client(StdioServerParameters(command=executable, args=[*arguments, "--url", self.url])) as (read, write):
                async with ClientSession(read, write) as session:
                    return "STDIO 检查通过 · " + await inspect_session(session)
        async with streamable_http_client(self.url) as (read, write, _):
            async with ClientSession(read, write) as session:
                return "HTTP 检查通过 · " + await inspect_session(session)

    def configure_and_check(self, index):
        path = self.install(index)
        try:
            result = asyncio.run(asyncio.wait_for(self.probe(index), timeout=15))
        except Exception as exc:
            return f"配置已写入 {path}；服务检查失败：{type(exc).__name__}。请确认本程序 MCP 已就绪。"
        cli = ("codex", "claude", "opencode", "")[index]
        installed = bool(shutil.which(cli)) if cli else True
        suffix = "重启客户端后生效" if installed else "客户端未在 PATH 中找到；安装后重启生效"
        return f"已配置 {path}\n{result}\n{suffix}"
