# SPDX-License-Identifier: Apache-2.0
"""Exercise the production installer logic with isolated registration names.

The test build changes only the product/registry names. It never installs over
an existing user installation or updates real AI-client configuration files.
"""
import argparse
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import time
import winreg
import zlib

ROOT = Path(__file__).resolve().parents[2]
PRODUCT = "Serial Workbench Installer Test"
REGISTRY = r"Software\Microsoft\Windows\CurrentVersion\Uninstall\SerialWorkbench.InstallerTest"


def registration():
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, REGISTRY, 0, winreg.KEY_READ | winreg.KEY_WOW64_64KEY) as key:
            return {name: winreg.QueryValueEx(key, name)[0] for name in
                    ("DisplayName", "DisplayVersion", "InstallLocation", "UninstallString", "QuietUninstallString")}
    except FileNotFoundError:
        return None


def wait_for(predicate, timeout=30):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        if predicate():
            return
        time.sleep(.05)
    raise AssertionError("Timed out waiting for installer state")


def icon_resources(executable):
    """Read PE resources without executing application or automating the UI."""
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.LoadLibraryExW.argtypes = [wintypes.LPCWSTR, wintypes.HANDLE, wintypes.DWORD]
    kernel.LoadLibraryExW.restype = wintypes.HMODULE
    kernel.FindResourceW.argtypes = [wintypes.HMODULE, ctypes.c_void_p, ctypes.c_void_p]
    kernel.FindResourceW.restype = wintypes.HANDLE
    kernel.LoadResource.argtypes = [wintypes.HMODULE, wintypes.HANDLE]
    kernel.LoadResource.restype = wintypes.HANDLE
    kernel.LockResource.argtypes = [wintypes.HANDLE]
    kernel.LockResource.restype = ctypes.c_void_p
    kernel.SizeofResource.argtypes = [wintypes.HMODULE, wintypes.HANDLE]
    kernel.SizeofResource.restype = wintypes.DWORD
    kernel.FreeLibrary.argtypes = [wintypes.HMODULE]
    module = kernel.LoadLibraryExW(str(executable), None, 2)
    if not module:
        raise ctypes.WinError(ctypes.get_last_error())
    resources = []
    try:
        for identifier in range(1, 64):
            handle = kernel.FindResourceW(module, identifier, 3)  # RT_ICON
            if not handle:
                continue
            pointer = kernel.LockResource(kernel.LoadResource(module, handle))
            resources.append(ctypes.string_at(pointer, kernel.SizeofResource(module, handle)))
    finally:
        kernel.FreeLibrary(module)
    return resources


def expected_icons():
    data = (ROOT / "resources/brand/lightning.ico").read_bytes()
    count = struct.unpack_from("<H", data, 4)[0]
    assert count == 9
    result = []
    for index in range(count):
        size, offset = struct.unpack_from("<II", data, 6 + index * 16 + 8)
        bitmap = data[offset:offset + size]
        width = struct.unpack_from("<i", bitmap, 4)[0]
        alpha = bitmap[40:40 + width * width * 4][3::4]
        assert 0 in alpha and 255 in alpha, "Icon must contain transparent and opaque pixels"
        result.append(bitmap)
    return result


def compile_fixture(args, target, version=None):
    payload = args.payload.resolve()
    size = sum(p.stat().st_size for p in payload.rglob("*") if p.is_file()) // 1024 + 1
    subprocess.run([
        args.makensis, "/V2", "/INPUTCHARSET", "UTF8",
        f"/DVERSION={version or args.expected_version}", f"/DPAYLOAD_DIR={payload}",
        f"/DOUTPUT_EXE={target}", f"/DBRAND_DIR={ROOT / 'resources/brand'}",
        f"/DREMOVE_MANIFEST={args.remove_manifest.resolve()}", f"/DSIZE_KB={size}",
        f"/DPRODUCT_NAME={PRODUCT}", f"/DREGISTRY_KEY={REGISTRY}",
        str(ROOT / "scripts/windows-installer.nsi")
    ], check=True)


def install(setup, destination, env):
    # NSIS requires /D last, without quoting the directory itself. No shell is
    # involved; the destination is generated and verified inside this workspace.
    assert destination.resolve().is_relative_to((ROOT / "build").resolve())
    command = subprocess.list2cmdline([str(setup), "/S"]) + " /D=" + str(destination)
    subprocess.run(command, env=env, check=True, timeout=40, creationflags=subprocess.CREATE_NO_WINDOW)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--payload", type=Path, required=True)
    parser.add_argument("--remove-manifest", type=Path, required=True)
    parser.add_argument("--installer", type=Path, required=True)
    parser.add_argument("--expected-version", required=True)
    parser.add_argument("--makensis", default="makensis")
    args = parser.parse_args()
    if registration() is not None:
        raise RuntimeError("An installer-test registration already exists; refusing to overwrite it")

    frames = expected_icons()
    for binary in (args.installer, args.payload / "SerialWorkbench.exe"):
        embedded = icon_resources(binary.resolve())
        assert all(frame in embedded for frame in frames), f"Incorrect transparent icon in {binary}"
    print("PASS transparent double-fold icon: 9 sizes in installer and application", flush=True)

    with tempfile.TemporaryDirectory(prefix="installer-验证-", dir=ROOT / "build") as temporary:
        directory = Path(temporary).resolve()
        assert directory.is_relative_to((ROOT / "build").resolve())
        setup = directory / "SerialWorkbenchInstallerTest.exe"
        destination = directory / "自定义目录 Serial Workbench"
        profile = directory / "profile"
        settings = profile / "SerialWorkbench/settings.json"
        settings.parent.mkdir(parents=True)
        background = settings.parent / "backgrounds/preserved.png"
        background.parent.mkdir()
        def png_chunk(kind, data):
            return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
        image = (b"\x89PNG\r\n\x1a\n"
                 + png_chunk(b"IHDR", struct.pack(">IIBBBBB", 2, 2, 8, 6, 0, 0, 0))
                 + png_chunk(b"IDAT", zlib.compress((b"\x00" + b"\x20\x80\x40\xff" * 2) * 2))
                 + png_chunk(b"IEND", b""))
        background.write_bytes(image)
        control_page = {"kind": "control", "title": "串口控制", "source_key": "serial:loop://",
                        "serial_controls": {"auto_send": False, "controls": [{
                            "type": "number", "title": "目标转速", "key": "speed",
                            "minimum": 0, "maximum": 3000, "step": 10, "decimals": 0,
                            "value": "1200", "format": "text", "pattern": "SET {key} {value}",
                            "encoding": "utf-8", "newline": "crlf", "options": "低=0\n高=1"}]}}
        canvas_page = {"kind": "control", "title": "自由布局测试", "source_key": "serial:loop://",
                       "serial_controls": {"layout_version": 2, "columns": 0, "active_group": "canvas-test",
                           "groups": [{"id": "canvas-test", "name": "电机参数", "controls": [{
                               **control_page["serial_controls"]["controls"][0], "auto_send": False,
                               "rect": {"x": 87, "y": 64, "width": 420, "height": 170}}]}]}}
        settings.write_text(json.dumps({"appearance": {"theme": "dark", "window_background": {
            "file": "backgrounds/preserved.png", "opacity": 25, "fit": "cover",
            "position_x": 20, "position_y": 80,
        }}, "mcp_port": 8765, "widget_pages": [control_page, canvas_page]}), encoding="utf-8")
        env = os.environ.copy()
        env["LOCALAPPDATA"] = str(profile)
        windows = Path(os.environ["SystemRoot"])
        env["PATH"] = os.pathsep.join((str(windows / "System32"), str(windows)))
        app = None
        try:
            baseline_version = "0.15.7"
            compile_fixture(args, setup, baseline_version)
            install(setup, destination, env)
            info = registration()
            assert info is not None, f"Registration missing: installed={destination.exists()}, marker={(destination / 'installation.json').exists()}, files={len(list(destination.rglob('*')))}"
            assert info["DisplayVersion"] == baseline_version
            assert Path(info["InstallLocation"]) == destination
            assert info["UninstallString"] == f'"{destination / "Uninstall.exe"}"'
            assert info["QuietUninstallString"].endswith('" /S')
            assert json.loads((destination / "installation.json").read_text(encoding="utf-8"))["distribution"] == "installed"
            assert (destination / "SerialWorkbenchMCP.exe").is_file()
            assert settings.is_file()
            assert not (destination / "settings.json").exists()
            assert all(frame in icon_resources(destination / "Uninstall.exe") for frame in frames)
            print("PASS custom Unicode installation path, registration and branded uninstaller", flush=True)

            note = destination / "keep-user-file.txt"
            plugin_note = destination / "platforms/keep-user-file.txt"
            note.write_text("user data", encoding="utf-8")
            plugin_note.write_text("user data", encoding="utf-8")
            compile_fixture(args, setup)
            # No /D: the new installer must discover the previous custom path
            # using its per-user installation registration.
            subprocess.run([str(setup), "/S"], env=env, check=True, timeout=40,
                           creationflags=subprocess.CREATE_NO_WINDOW)
            assert Path(registration()["InstallLocation"]) == destination
            assert registration()["DisplayVersion"] == args.expected_version
            assert json.loads((destination / "installation.json").read_text(encoding="utf-8"))["version"] == args.expected_version
            assert note.read_text(encoding="utf-8") == "user data"
            assert json.loads(settings.read_text(encoding="utf-8"))["appearance"]["theme"] == "dark"
            assert json.loads(settings.read_text(encoding="utf-8"))["mcp_port"] == 8765
            assert json.loads(settings.read_text(encoding="utf-8"))["appearance"]["window_background"]["file"] == "backgrounds/preserved.png"
            assert json.loads(settings.read_text(encoding="utf-8"))["appearance"]["window_background"]["position_x"] == 20
            assert json.loads(settings.read_text(encoding="utf-8"))["appearance"]["window_background"]["position_y"] == 80
            assert json.loads(settings.read_text(encoding="utf-8"))["widget_pages"] == [control_page, canvas_page]
            assert background.read_bytes() == image
            assert not (destination / "FileTransferDemo.exe").exists()
            print("PASS upgrade discovers custom path without /D and preserves settings/user files; test demo excluded", flush=True)

            ready = directory / "ready.json"
            app = subprocess.Popen([
                str(destination / "SerialWorkbench.exe"), "--mcp-port", "0",
                "--ready-file", str(ready), "--quit-after", "6000"
            ], env=env, creationflags=subprocess.CREATE_NO_WINDOW)
            wait_for(ready.exists)
            assert json.loads(ready.read_text(encoding="utf-8"))["version"] == args.expected_version
            assert not (destination / "settings.json").exists()
            blocked = subprocess.run(subprocess.list2cmdline([str(setup), "/S"]) + " /D=" + str(destination),
                                     env=env, timeout=20, creationflags=subprocess.CREATE_NO_WINDOW)
            assert blocked.returncode == 2, "Running application must block install before changing files"
            app.wait(timeout=12)
            assert json.loads(settings.read_text(encoding="utf-8"))["appearance"]["theme"] == "dark"
            assert json.loads(settings.read_text(encoding="utf-8"))["appearance"]["window_background"]["position_x"] == 20
            assert json.loads(settings.read_text(encoding="utf-8"))["appearance"]["window_background"]["position_y"] == 80
            assert json.loads(settings.read_text(encoding="utf-8"))["widget_pages"][0]["serial_controls"]["groups"][0]["controls"][0]["value"] == "1200"
            canvas = json.loads(settings.read_text(encoding="utf-8"))["widget_pages"][1]["serial_controls"]
            assert canvas["columns"] == 0 and canvas["active_group"] == "canvas-test"
            assert canvas["groups"][0]["controls"][0]["rect"] == {"x": 87, "y": 64, "width": 420, "height": 170}
            print("PASS installed launch without SDK, per-user settings and running-app protection", flush=True)

            subprocess.run([sys.executable, str(ROOT / "tests/integration/smoke_native.py"),
                            "--app-dir", str(destination), "--expected-version", args.expected_version],
                           env=env, check=True, timeout=60)

            subprocess.run([str(destination / "Uninstall.exe"), "/S"], env=env, check=True,
                           timeout=30, creationflags=subprocess.CREATE_NO_WINDOW)
            wait_for(lambda: registration() is None and not (destination / "SerialWorkbench.exe").exists())
            wait_for(lambda: not (destination / "Uninstall.exe").exists())
            assert settings.exists(), "Personal settings must be retained by default"
            assert background.read_bytes() == image, "Personal background images must be retained by default"
            assert note.read_text(encoding="utf-8") == "user data"
            assert plugin_note.read_text(encoding="utf-8") == "user data"
            remaining = sorted(p.relative_to(destination).as_posix() for p in destination.rglob("*") if p.is_file())
            assert remaining == ["keep-user-file.txt", "platforms/keep-user-file.txt"], remaining
            print("PASS uninstall removes owned files/registration and retains personal settings and other files", flush=True)
        finally:
            if app is not None and app.poll() is None:
                app.terminate()
                app.wait(timeout=10)
            uninstall = destination / "Uninstall.exe"
            if uninstall.is_file() and (destination / "installation.json").is_file():
                subprocess.run([str(uninstall), "/S"], env=env, timeout=30,
                               creationflags=subprocess.CREATE_NO_WINDOW)
                wait_for(lambda: registration() is None and not (destination / "SerialWorkbench.exe").exists())
                wait_for(lambda: not uninstall.exists())



if __name__ == "__main__":
    main()
