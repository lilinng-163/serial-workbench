"""Loopback virtual SSH device for the developer-only interactive demo."""
# SPDX-License-Identifier: Apache-2.0
import base64
import hashlib
import json
import logging
import socket
import sys
import threading
import time

import paramiko

logging.getLogger("paramiko").setLevel(logging.CRITICAL)


class Device:
    def __init__(self):
        self.lock = threading.RLock()
        self.output_lock = threading.Lock()
        self.key = paramiko.RSAKey.generate(2048)
        self.password = "demo-password-42"
        self.transports = set()
        self.started = time.monotonic()
        self.port = 0

    def emit(self, kind, **fields):
        with self.output_lock:
            print(json.dumps({"type": kind, **fields}), flush=True)

    def describe(self, kind):
        with self.lock:
            digest = base64.b64encode(hashlib.sha256(self.key.asbytes()).digest()).decode().rstrip("=")
            self.emit(kind, port=self.port, user="demo", password=self.password, fingerprint="SHA256:" + digest)

    def disconnect(self):
        with self.lock:
            active = list(self.transports)
        for transport in active:
            transport.close()

    def controls(self):
        for line in sys.stdin:
            try:
                command = json.loads(line).get("command")
                if command == "replace_key":
                    key = paramiko.RSAKey.generate(2048)
                    with self.lock:
                        self.key = key
                    self.disconnect()
                    self.describe("key_changed")
                elif command == "change_password":
                    with self.lock:
                        self.password = ("demo-password-99" if self.password == "demo-password-42"
                                         else "demo-password-42")
                    self.disconnect()
                    self.describe("password_changed")
                elif command == "quit":
                    self.disconnect()
                    return
            except (ValueError, OSError) as error:
                self.emit("error", message=str(error))


class Login(paramiko.ServerInterface):
    def __init__(self, device):
        self.device = device
        self.shell = threading.Event()

    def get_allowed_auths(self, username):
        return "password"

    def check_auth_password(self, username, password):
        with self.device.lock:
            accepted = username == "demo" and password == self.device.password
        self.device.emit("authentication", user=username, accepted=accepted)
        return paramiko.AUTH_SUCCESSFUL if accepted else paramiko.AUTH_FAILED

    def check_channel_request(self, kind, channel_id):
        return paramiko.OPEN_SUCCEEDED if kind == "session" else paramiko.OPEN_FAILED_ADMINISTRATIVELY_PROHIBITED

    def check_channel_pty_request(self, channel, term, width, height, pixelwidth, pixelheight, modes):
        return True

    def check_channel_shell_request(self, channel):
        self.shell.set()
        return True


def shell(channel, device):
    channel.sendall(b"\x1b[2J\x1b[HSSH_DEMO_READY\r\n"
                    b"Developer-only virtual device. No host commands are executed.\r\n"
                    b"Type help, status, uptime, echo <text>, clear or exit.\r\n\r\n")
    prompt = b"\x1b[32mdemo@new-device\x1b[0m:~$ "
    channel.sendall(prompt)
    line = bytearray()
    escape = False
    while True:
        data = channel.recv(4096)
        if not data:
            return
        for byte in data:
            if escape:
                if 0x40 <= byte <= 0x7E and byte != ord('['):
                    escape = False
                continue
            if byte == 0x1B:
                escape = True
            elif byte in (10, 13):
                channel.sendall(b"\r\n")
                command = line.decode("utf-8", errors="replace").strip()
                line.clear()
                if command == "exit":
                    channel.sendall(b"Demo session closed.\r\n")
                    return
                if command == "clear":
                    channel.sendall(b"\x1b[2J\x1b[H")
                elif command == "help":
                    channel.sendall(b"help | status | uptime | echo <text> | clear | exit\r\n")
                elif command == "status":
                    channel.sendall(b"device=new-device-demo  online=true  transport=SSH  bind=127.0.0.1\r\n")
                elif command == "uptime":
                    channel.sendall(f"up {int(time.monotonic() - device.started)} seconds\r\n".encode())
                elif command.startswith("echo "):
                    channel.sendall(command[5:].encode("utf-8") + b"\r\n")
                elif command:
                    channel.sendall(b"Unknown demo command. Type help.\r\n")
                channel.sendall(prompt)
            elif byte in (3, 4):
                channel.sendall(b"^C\r\n" + prompt)
                line.clear()
            elif byte in (8, 127):
                if line:
                    line.pop()
                    channel.sendall(b"\b \b")
            elif byte >= 32 and len(line) < 4096:
                line.append(byte)
                channel.sendall(bytes([byte]))


def serve(connection, device):
    transport = paramiko.Transport(connection)
    with device.lock:
        transport.add_server_key(device.key)
        device.transports.add(transport)
    try:
        login = Login(device)
        transport.start_server(server=login)
        channel = transport.accept(120)
        if channel is not None and login.shell.wait(10):
            shell(channel, device)
    except (EOFError, OSError, paramiko.SSHException):
        pass
    finally:
        transport.close()
        with device.lock:
            device.transports.discard(transport)


def main():
    device = Device()
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        listener.listen()
        device.port = listener.getsockname()[1]
        threading.Thread(target=device.controls, daemon=True).start()
        device.describe("ready")
        while True:
            connection, _ = listener.accept()
            threading.Thread(target=serve, args=(connection, device), daemon=True).start()


if __name__ == "__main__":
    main()
