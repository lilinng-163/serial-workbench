"""One desktop window per Windows account and MCP port."""

import hashlib
from pathlib import Path
import tempfile

from PySide6.QtCore import QLockFile
from PySide6.QtNetwork import QLocalServer, QLocalSocket


class InstanceGuard:
    def __init__(self, username, port):
        identity = hashlib.sha256(f"{username}:{port}".encode()).hexdigest()[:20]
        self.name = f"serial-workbench-{identity}"
        self.lock = QLockFile(str(Path(tempfile.gettempdir()) / f"{self.name}.lock"))
        # A long-running data acquisition session is never stale just due to age.
        self.lock.setStaleLockTime(0)
        self.server = QLocalServer()
        self.server.setSocketOptions(QLocalServer.SocketOption.UserAccessOption)
        self.server.newConnection.connect(self.activate)
        self.window = None

    def acquire(self):
        if not self.lock.tryLock(0):
            if self.lock.error() != QLockFile.LockError.LockFailedError:
                raise RuntimeError("无法创建应用实例锁，请检查临时目录权限。")
            client = QLocalSocket()
            client.connectToServer(self.name)
            if client.waitForConnected(1000):
                client.disconnectFromServer()
            return False
        QLocalServer.removeServer(self.name)
        if not self.server.listen(self.name):
            self.lock.unlock()
            raise RuntimeError(f"无法建立窗口唤醒通道：{self.server.errorString()}")
        return True

    def activate(self):
        while self.server.hasPendingConnections():
            client = self.server.nextPendingConnection()
            client.close()
            client.deleteLater()
        if self.window is not None:
            self.window.showNormal()
            self.window.raise_()
            self.window.activateWindow()

    def close(self):
        self.server.close()
        self.lock.unlock()
