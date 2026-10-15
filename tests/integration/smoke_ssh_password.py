"""Exercise Windows OpenSSH against an isolated loopback password server."""
# SPDX-License-Identifier: Apache-2.0
import argparse
import json
import os
import subprocess
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--paramiko-dir", type=Path)
    parser.add_argument("--test-executable", type=Path, default=ROOT / "build/native-release/bin/ssh_auth_tests.exe")
    parser.add_argument("--qt-bin", type=Path)
    parser.add_argument("--compiler-bin", type=Path)
    args = parser.parse_args()
    environment = dict(os.environ)
    runtime_paths = [str(path.resolve()) for path in (args.qt_bin, args.compiler_bin) if path]
    if runtime_paths:
        environment["PATH"] = ";".join(runtime_paths) + ";" + environment["PATH"]
    if args.paramiko_dir:
        environment["PYTHONPATH"] = str(args.paramiko_dir.resolve())
    with tempfile.TemporaryDirectory(prefix="ssh-password-", dir=ROOT / "build") as temporary:
        ready = Path(temporary) / "ready.json"
        server = subprocess.Popen([os.sys.executable, str(ROOT / "tests/integration/ssh_password_server.py"),
                                   "--ready-file", str(ready)], env=environment,
                                  stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            for _ in range(150):
                if ready.exists():
                    break
                if server.poll() is not None:
                    raise RuntimeError("Loopback SSH fixture failed to start")
                time.sleep(0.1)
            else:
                raise TimeoutError("Loopback SSH fixture did not start")
            environment["SW_SSH_LOCAL_TEST_PORT"] = str(json.loads(ready.read_text())["port"])
            report = ROOT / "build/ssh-real-auth-tests.txt"
            subprocess.run([str(args.test_executable.resolve()), "realOpenSshPasswordAndChangedHostKey",
                            "-platform", "offscreen", "-o", f"{report},txt"],
                           env=environment, check=True, timeout=50)
            print(report.read_text(encoding="utf-8"))
        finally:
            server.terminate()
            server.wait(timeout=10)


if __name__ == "__main__":
    main()
