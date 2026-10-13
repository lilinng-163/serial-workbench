# SPDX-License-Identifier: Apache-2.0

"""Verify clean ZIP and single-file releases without relying on the Qt SDK PATH."""

import argparse
import asyncio
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import zipfile

from mcp import ClientSession
from mcp.client.streamable_http import streamable_http_client

from smoke_native import ROOT, check, unpack


def verify_payload(directory):
    assert not list(directory.rglob("settings*.json"))
    for name in (
        "SerialWorkbench.exe",
        "SerialWorkbenchMCP.exe",
        "platforms/qwindows.dll",
        "LICENSE",
        "licenses/Qt/qtbase/LICENSES/LGPL-3.0-only.txt",
    ):
        assert (directory / name).is_file(), name
    for line in (directory / "SHA256SUMS.txt").read_text(encoding="utf-8").splitlines():
        digest, name = line.split("  ", 1)
        path = (directory / name).resolve()
        assert path.is_relative_to(directory.resolve()), name
        assert hashlib.sha256(path.read_bytes()).hexdigest() == digest, name


async def identify(url):
    async with streamable_http_client(url) as (read, write, _):
        async with ClientSession(read, write) as client:
            await client.initialize()
            return unpack(await client.call_tool("get_status"))


def close_pid(pid):
    user32 = ctypes.windll.user32
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

    @callback_type
    def visit(hwnd, _):
        owner = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid:
            user32.PostMessageW(hwnd, 0x0010, 0, 0)
        return True

    user32.EnumWindows(visit, 0)


def check_launcher(executable, runtime, settings, allow, version, environment):
    settings.write_text(json.dumps({"allow_ai_control": allow}), encoding="utf-8")
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]
    process = subprocess.Popen(
        [str(executable), "--demo", "--settings", str(settings), "--mcp-port", str(port), f"/D={runtime}"],
        env=environment,
    )
    native_pid = None
    try:
        for _ in range(200):
            assert process.poll() is None, process.returncode
            try:
                with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                    break
            except OSError:
                time.sleep(0.1)
        else:
            raise TimeoutError("Single-file release did not start")
        url = f"http://127.0.0.1:{port}/mcp"
        status = asyncio.run(identify(url))
        native_pid = status["process"]["pid"]
        assert native_pid != process.pid
        assert settings.exists()
        assert (runtime / ".complete").is_file()
        time.sleep(0.3)
        asyncio.run(check(url, runtime / "SerialWorkbenchMCP.exe", allow, version))
    finally:
        if native_pid:
            close_pid(native_pid)
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"], capture_output=True)
            process.wait(timeout=10)
            raise
    assert process.returncode == 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--release-dir", type=Path, default=ROOT / "dist/releases")
    parser.add_argument("--expected-version", required=True)
    args = parser.parse_args()
    base = f"SerialWorkbench-{args.expected_version}-windows-x64"
    release = args.release_dir.resolve()
    environment = os.environ.copy()
    windows = Path(os.environ["SystemRoot"])
    environment["PATH"] = os.pathsep.join([str(windows / "System32"), str(windows)])

    with tempfile.TemporaryDirectory(prefix="release-smoke-", dir=ROOT / "build") as temporary:
        directory = Path(temporary)
        with zipfile.ZipFile(release / f"{base}-portable.zip") as archive:
            for name in archive.namelist():
                assert (directory / name).resolve().is_relative_to(directory.resolve()), name
            archive.extractall(directory)
        portable = directory / "SerialWorkbench"
        verify_payload(portable)
        subprocess.run(
            [sys.executable, str(ROOT / "tests/integration/smoke_native.py"),
             "--app-dir", str(portable), "--expected-version", args.expected_version],
            env=environment, check=True,
        )
        print("PASS portable ZIP: clean payload, full manifest and isolated runtime")

        launcher = directory / "Serial Workbench 测试.exe"
        shutil.copy2(release / f"{base}.exe", launcher)
        runtime = directory / "runtime"
        settings = directory / "launcher-settings.json"
        check_launcher(launcher, runtime, settings, False, args.expected_version, environment)
        marker = runtime / ".complete"
        first_time = marker.stat().st_mtime_ns
        assert (runtime / "SerialWorkbench.exe").read_bytes() == (portable / "SerialWorkbench.exe").read_bytes()
        check_launcher(launcher, runtime, settings, True, args.expected_version, environment)
        assert marker.stat().st_mtime_ns == first_time
        print("PASS single EXE: Unicode path, first launch, cached launch, CLI forwarding and shared MCP")

        (runtime / "platforms/qwindows.dll").unlink()
        check_launcher(launcher, runtime, settings, False, args.expected_version, environment)
        assert marker.stat().st_mtime_ns != first_time
        assert (runtime / "platforms/qwindows.dll").is_file()
        print("PASS single EXE: incomplete runtime cache is repaired")


if __name__ == "__main__":
    main()
