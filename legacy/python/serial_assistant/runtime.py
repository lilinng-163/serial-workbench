"""Identify the account that owns the actual process, not inherited env vars."""

import getpass
import os


def runtime_identity():
    username = getpass.getuser()
    if os.name == "nt":
        import ctypes
        from ctypes import wintypes

        buffer = ctypes.create_unicode_buffer(256)
        size = wintypes.DWORD(len(buffer))
        if ctypes.windll.advapi32.GetUserNameW(buffer, ctypes.byref(size)):
            username = buffer.value
    return {
        "username": username,
        "pid": os.getpid(),
        "sandbox_account": username.casefold().startswith("codexsandbox"),
    }


def serial_open_error(error, identity=None):
    info = identity or runtime_identity()
    context = f'运行账户：{info["username"]}，PID：{info["pid"]}'
    detail = str(error)
    if "PermissionError" in detail or "WinError 5" in detail:
        if info["sandbox_account"]:
            return f"当前窗口由 Codex 沙箱账户启动，可能没有串口设备访问权限。请关闭此窗口，从资源管理器双击程序，以自己的 Windows 账户运行。{context}。原始错误：{detail}"
        return f"Windows 拒绝当前程序打开串口。{context}。原始错误：{detail}"
    return detail
