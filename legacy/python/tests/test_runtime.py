import os
import uuid

from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication, QWidget

from serial_assistant.instance import InstanceGuard
from serial_assistant.runtime import runtime_identity, serial_open_error


def test_permission_error_identifies_process_without_claiming_port_is_busy():
    error = "could not open port 'COM_TEST': PermissionError(13, '拒绝访问。', None, 5)"
    normal = {"username": "normal-user", "pid": 123, "sandbox_account": False}
    sandbox = {"username": "CodexSandboxOffline", "pid": 456, "sandbox_account": True}
    assert "normal-user" in serial_open_error(error, normal)
    assert "占用" not in serial_open_error(error, normal)
    assert error in serial_open_error(error, normal)
    assert "资源管理器" in serial_open_error(error, sandbox)
    assert serial_open_error("unsupported baudrate", normal) == "unsupported baudrate"
    actual = runtime_identity()
    assert actual["pid"] == os.getpid()
    assert actual["username"]


def test_second_instance_activates_first_and_lock_releases():
    app = QApplication.instance() or QApplication([])
    username = f"test-{uuid.uuid4()}"
    primary = InstanceGuard(username, 8765)
    secondary = InstanceGuard(username, 8765)
    other_port = InstanceGuard(username, 8877)
    window = QWidget()
    primary.window = window
    try:
        assert primary.acquire()
        assert not secondary.acquire()
        for _ in range(50):
            app.processEvents()
            if window.isVisible():
                break
            QTest.qWait(20)
        assert window.isVisible()
        assert other_port.acquire()
        primary.close()
        assert secondary.acquire()
    finally:
        primary.close()
        secondary.close()
        other_port.close()
        window.close()
