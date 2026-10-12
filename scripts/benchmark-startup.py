"""Measure process launch to first visible HWND, using isolated settings and ports."""
import ctypes
from ctypes import wintypes
import json
from pathlib import Path
import socket
import statistics
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests" / "integration"))
from smoke_native import close_window
user32 = ctypes.windll.user32
callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)


def visible_window(pid):
    found = False

    @callback_type
    def visit(hwnd, _):
        nonlocal found
        owner = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid and user32.IsWindowVisible(hwnd):
            length = user32.GetWindowTextLengthW(hwnd)
            if length:
                found = True
        return True

    user32.EnumWindows(visit, 0)
    return found


results = {}
for name, folder in (("Python legacy", "SerialWorkbench"), ("Qt C++", "SerialWorkbenchNative")):
    samples = []
    for run in range(3):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as temporary:
            with socket.socket() as sock:
                sock.bind(("127.0.0.1", 0))
                port = sock.getsockname()[1]
            start = time.perf_counter()
            process = subprocess.Popen([
                str(ROOT / "dist" / folder / "SerialWorkbench.exe"),
                "--mcp-port", str(port), "--settings", str(Path(temporary) / "settings.json"),
            ])
            try:
                while not visible_window(process.pid):
                    assert process.poll() is None, process.returncode
                    if time.perf_counter() - start > 30:
                        raise TimeoutError("No visible workbench window")
                    time.sleep(0.005)
                elapsed = round((time.perf_counter() - start) * 1000, 1)
                samples.append(elapsed)
                print(f"{name}, run {run + 1}: {elapsed} ms", flush=True)
                time.sleep(0.3)
            finally:
                close_window(process)
    results[name] = {"samples_ms": samples, "median_ms": statistics.median(samples)}
report = {"metric": "process spawn to first visible titled window", "runs": 3,
          "cache": "normal Windows cache; not a controlled cold-start benchmark", "results": results}
(ROOT / "build" / "native-startup.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
print(json.dumps(report, indent=2))
