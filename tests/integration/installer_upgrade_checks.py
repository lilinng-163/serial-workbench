# SPDX-License-Identifier: Apache-2.0
"""Busy-installation checks, used only with smoke_installer's isolated paths."""
from contextlib import contextmanager
import ctypes
from ctypes import wintypes
import hashlib
import json
import queue
from pathlib import Path
import shutil
import subprocess
import time
import threading


def cli(folder, env, command, profile):
    return subprocess.run([str(folder / "SerialWorkbenchCLI.exe"), command,
                           "--settings", str(profile)], env=env, capture_output=True,
                          encoding="utf-8", timeout=25, creationflags=subprocess.CREATE_NO_WINDOW)


def initialize_bridge(bridge):
    replies = queue.Queue()
    threading.Thread(target=lambda: replies.put(bridge.stdout.readline()), daemon=True).start()
    request = {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {
        "protocolVersion": "2025-03-26", "capabilities": {},
        "clientInfo": {"name": "installer-test", "version": "1"}}}
    bridge.stdin.write(json.dumps(request).encode() + b"\n")
    bridge.stdin.flush()
    assert b'"result"' in replies.get(timeout=20)


@contextmanager
def file_lease(path, create=False):
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                                  ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
    kernel.CreateFileW.restype = wintypes.HANDLE
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    handle = kernel.CreateFileW(str(path), 0xC0010000 if create else 0x80000000,
                               0 if create else 1, None, 4 if create else 3,
                               0x04000000 if create else 0x80, None)
    assert handle != ctypes.c_void_p(-1).value, ctypes.WinError(ctypes.get_last_error())
    try:
        yield
    finally:
        kernel.CloseHandle(handle)


def check_busy_upgrade(setup, destination, directory, env, settings, app):
    guard = destination / ".workbench-maintenance"
    assert not guard.exists(), "Cancelled setup left a maintenance lease"
    assert app.poll() is None, "Silent setup must not close an app without consent"
    custom = directory / "自定义 profile with spaces.json"
    custom.write_text(json.dumps({"appearance": {"theme": "dark"}}), encoding="utf-8")
    with file_lease(guard, create=True):
        rejected = cli(destination, env, "start", custom)
        assert rejected.returncode == 2 and "updated" in json.loads(rejected.stdout)["error"], rejected
        headless = subprocess.run([str(destination / "SerialWorkbench.exe"), "--headless",
                                   "--settings", str(custom)], env=env, timeout=20,
                                  creationflags=subprocess.CREATE_NO_WINDOW)
        assert headless.returncode == 5
        bridge = subprocess.run([str(destination / "SerialWorkbenchMCP.exe"), "--url",
                                 "http://127.0.0.1:1/mcp"], input="", env=env,
                                capture_output=True, text=True, timeout=10,
                                creationflags=subprocess.CREATE_NO_WINDOW)
        assert bridge.returncode == 2 and "updated" in bridge.stderr
    assert not guard.exists()
    print("PASS active maintenance lease blocks GUI/headless/STDIO startup; lease auto-releases", flush=True)

    # A second installation with the same EXE names must survive this upgrade.
    outsider = directory / "其他目录 Workbench"
    shutil.copytree(destination, outsider)
    other_settings = directory / "outsider.json"
    companions = []
    try:
        result = cli(outsider, env, "start", other_settings)
        assert result.returncode == 0, result.stderr
        outsider_pid = json.loads(result.stdout)["pid"]
        result = cli(destination, env, "start", custom)
        assert result.returncode == 0, result.stderr
        for program, options, profile in (("SerialWorkbenchMCP.exe", [], settings),
                                          ("SerialWorkbenchMCP.exe", [], custom),
                                          ("SerialWorkbenchCLI.exe", ["mcp"], custom)):
            bridge = subprocess.Popen([str(destination / program), *options, "--settings", str(profile)],
                                      stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                      env=env, creationflags=subprocess.CREATE_NO_WINDOW)
            companions.append(bridge)
            initialize_bridge(bridge)
        command = subprocess.list2cmdline([str(setup), "/S", "/CLOSEAPPLICATIONS"]) + " /D=" + str(destination)
        subprocess.run(command, env=env, check=True, timeout=70, creationflags=subprocess.CREATE_NO_WINDOW)
        assert app.wait(timeout=10) == 0, "GUI/runtime must close normally"
        for bridge in companions:
            bridge.wait(timeout=10)
        assert not json.loads(cli(destination, env, "status", custom).stdout)["running"]
        assert json.loads(cli(outsider, env, "status", other_settings).stdout)["pid"] == outsider_pid
        assert json.loads(custom.read_text(encoding="utf-8"))["appearance"]["theme"] == "dark"
        assert not guard.exists()
        print("PASS acknowledged upgrade closes GUI + custom headless profile + two MCP bridges + CLI mcp; other installation survives", flush=True)

        digest = hashlib.sha256((destination / "SerialWorkbench.exe").read_bytes()).hexdigest()
        with file_lease(destination / "Qt6Core.dll"):
            blocked = subprocess.run(command, env=env, timeout=30, creationflags=subprocess.CREATE_NO_WINDOW)
        assert blocked.returncode == 2
        assert hashlib.sha256((destination / "SerialWorkbench.exe").read_bytes()).hexdigest() == digest
        assert not guard.exists()
        subprocess.run(command, env=env, check=True, timeout=40, creationflags=subprocess.CREATE_NO_WINDOW)
        print("PASS locked DLL stops before replacement; releasing lock allows retry", flush=True)
    finally:
        for folder, profile in ((destination, settings), (destination, custom), (outsider, other_settings)):
            if folder.exists():
                cli(folder, env, "stop", profile)
        for bridge in companions:
            if bridge.poll() is None:
                bridge.terminate()
                bridge.wait(timeout=10)
            for stream in (bridge.stdin, bridge.stdout, bridge.stderr):
                stream.close()
        # Windows can keep recently exited images mapped briefly.
        time.sleep(.2)


def start_for_uninstall(destination, env, settings):
    result = cli(destination, env, "start", settings)
    assert result.returncode == 0, result.stderr
    bridge = subprocess.Popen([str(destination / "SerialWorkbenchMCP.exe"), "--settings", str(settings)],
                              stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              env=env, creationflags=subprocess.CREATE_NO_WINDOW)
    initialize_bridge(bridge)
    return bridge
