"""Build/launch the internal SSH new-device demo without installing it."""
# SPDX-License-Identifier: Apache-2.0
import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def cmake_cache(path):
    values = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if line and not line.startswith(("//", "#")) and "=" in line:
            name, value = line.split("=", 1)
            values[name.split(":", 1)[0]] = value
    return values


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--detached", action="store_true")
    args = parser.parse_args()
    build = ROOT / "build/native-release"
    cache_file = build / "CMakeCache.txt"
    if not cache_file.exists():
        raise SystemExit("Configure the native build with scripts/build.ps1 first.")
    cache = cmake_cache(cache_file)
    qt_root = Path(cache["Qt6_DIR"]).parents[2]
    compiler_bin = Path(cache["CMAKE_CXX_COMPILER"]).parent
    environment = dict(os.environ)
    environment["PATH"] = f"{qt_root / 'bin'};{compiler_bin};" + environment["PATH"]
    dependencies = ROOT / "build/ssh-test-deps"
    if not (dependencies / "paramiko/__init__.py").exists():
        subprocess.run([sys.executable, "-m", "pip", "install", "--target", str(dependencies),
                        "-r", str(ROOT / "tests/demo/requirements.txt")], check=True)
    if not args.no_build:
        subprocess.run([cache["CMAKE_COMMAND"], "--build", str(build), "--target", "SshNewDeviceDemo",
                        "--parallel", "4"], env=environment, check=True)
    command = [str(build / "bin/SshNewDeviceDemo.exe"), "--root", str(ROOT),
               "--python", sys.executable, "--dependencies", str(dependencies)]
    if args.self_test:
        report = ROOT / "build/ssh-new-device-demo-report.json"
        command += ["--self-test", "--report", str(report), "--screenshot", str(ROOT / "build/ssh-new-device-demo.png")]
        subprocess.run(command, env=environment, check=True, timeout=45)
        result = json.loads(report.read_text(encoding="utf-8"))
        if not result["passed"] or not all(check["passed"] for check in result["checks"]):
            raise SystemExit("SSH demo self-test failed")
        print(json.dumps(result, indent=2))
    elif args.detached:
        ready = ROOT / "build/ssh-new-device-demo-ready.json"
        ready.unlink(missing_ok=True)
        command += ["--ready-file", str(ready)]
        log_path = ROOT / "build/ssh-new-device-demo.log"
        with log_path.open("wb") as log:
            process = subprocess.Popen(command, env=environment, stdout=log, stderr=log,
                                       creationflags=subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP)
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise SystemExit(f"SSH demo exited before startup (code {process.returncode}). See {log_path}")
            if ready.exists():
                break
            time.sleep(0.1)
        else:
            raise SystemExit(f"SSH demo startup was not confirmed. See the demo window and {log_path}")
        print(f"SSH developer demo opened (PID {process.pid}).")
    else:
        subprocess.run(command, env=environment, check=True)


if __name__ == "__main__":
    main()
