# SPDX-License-Identifier: Apache-2.0
"""Compare shipped, embedded and installed guides against the source bundle."""
import ctypes
from ctypes import wintypes
import hashlib
import os
import json
import re
import subprocess
from pathlib import Path
from urllib.parse import unquote, urlsplit


def source_files(root):
    directory = root / "skills/serial-workbench"
    result = {p.relative_to(directory).as_posix(): p.read_bytes() for p in directory.rglob("*.md")}
    assert "SKILL.md" in result and "references/openocd.md" in result
    for relative, content in result.items():
        for link in re.findall(r"\]\(([^)]+)\)", content.decode("utf-8")):
            parsed = urlsplit(link)
            if parsed.scheme or link.startswith("#"):
                continue
            target = (directory / relative).parent / unquote(parsed.path)
            assert target.resolve().is_relative_to(directory.resolve()), (relative, link)
            assert target.is_file(), (relative, link)
    return result


def call(cli, settings, env, *args):
    completed = subprocess.run([str(cli), *args, "--settings", str(settings)],
                               capture_output=True, text=True, encoding="utf-8",
                               env=env, timeout=25, check=True)
    return json.loads(completed.stdout)


def check_guides(directory, expected, exact=False):
    for name, content in expected.items():
        assert (directory / name).read_bytes() == content, (directory, name)
    assert not list(directory.rglob("*.bak")), directory
    if exact:
        assert {p.relative_to(directory).as_posix() for p in directory.rglob("*") if p.is_file()} == set(expected)


def check_manifest(report, expected):
    actual = report["skill_manifest"]["files"]
    assert actual == {name: hashlib.sha256(data).hexdigest() for name, data in expected.items()}


def check_cli_updates(app_dir, settings, env, expected, profile):
    cli = app_dir / "SerialWorkbenchCLI.exe"
    report = call(cli, settings, env, "agents")
    check_manifest(report, expected)
    for target in report["skill_targets"]:
        directory = Path(target["path"])
        assert directory.resolve().is_relative_to(profile.resolve())
        result = call(cli, settings, env, "install-skill", target["agent"])
        assert len(result["files"]) == len(expected)
        check_guides(directory, expected)
    codex = next(target for target in report["skill_targets"] if target["agent"] == "codex")
    directory = Path(codex["path"])
    guide = directory / "references/openocd.md"
    guide.unlink()
    report = call(cli, settings, env, "agents")
    assert next(t for t in report["skill_targets"] if t["agent"] == "codex")["skill_state"] == "可更新 / 修复"
    call(cli, settings, env, "install-skill", "codex")
    check_guides(directory, expected)
    changed = directory / "SKILL.md"
    changed.write_bytes(b"user customized guide")
    legacy = directory / "references/agents.md.20261017-094030-238-51c9ad73.bak"
    legacy.write_bytes(b"previous customized guide")
    extra = directory / "personal-note.txt"
    extra.write_bytes(b"user note")
    result = call(cli, settings, env, "install-skill", "codex")
    archive = Path(result["backup_directory"])
    assert not archive.resolve().is_relative_to(directory.parent.resolve())
    manifest = json.loads((archive / "manifest.json").read_text(encoding="utf-8"))
    assert Path(manifest["directory"]).resolve() == directory.resolve()
    assert (archive / "files/SKILL.md").read_bytes() == b"user customized guide"
    assert (archive / "files" / legacy.relative_to(directory)).read_bytes() == b"previous customized guide"
    for name, digest in manifest["files"].items():
        assert hashlib.sha256((archive / "files" / name).read_bytes()).hexdigest() == digest
    assert extra.read_bytes() == b"user note"
    check_guides(directory, expected)
    repeated = call(cli, settings, env, "install-skill", "codex")
    assert repeated["backups"] == []
    assert all(t["skill_state"] == "已是最新" for t in call(cli, settings, env, "agents")["skill_targets"])
    if os.name == "nt":
        changed.write_bytes(b"custom guide before failed update")
        locked = directory / "references/model3d.md"
        locked.write_bytes(b"custom reference before failed update")
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
                                      wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
        kernel.CreateFileW.restype = wintypes.HANDLE
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        handle = kernel.CreateFileW(str(locked), 0x80000000, 1, None, 3, 0x80, None)
        assert handle != ctypes.c_void_p(-1).value
        try:
            failed = subprocess.run([str(cli), "install-skill", "codex", "--settings", str(settings)],
                                    capture_output=True, env=env, timeout=25)
            assert failed.returncode != 0, "The locked reference should block replacement"
            assert changed.read_bytes() == b"custom guide before failed update"
            assert locked.read_bytes() == b"custom reference before failed update"
        finally:
            kernel.CloseHandle(handle)
        call(cli, settings, env, "install-skill", "codex")
        check_guides(directory, expected)
    print("PASS CLI complete guides for all targets, reference repair, external backups, idempotence and rollback", flush=True)
