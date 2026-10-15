"""Loopback-only SSH authentication fixture; excluded from all release payloads."""
# SPDX-License-Identifier: Apache-2.0
import argparse
import json
import socket
import threading
from pathlib import Path

import paramiko


class Login(paramiko.ServerInterface):
    def __init__(self):
        self.shell = threading.Event()

    def get_allowed_auths(self, username):
        return "password"

    def check_auth_password(self, username, password):
        return (paramiko.AUTH_SUCCESSFUL if username == "test" and password == "test-secret-42"
                else paramiko.AUTH_FAILED)

    def check_channel_request(self, kind, channel_id):
        return paramiko.OPEN_SUCCEEDED if kind == "session" else paramiko.OPEN_FAILED_ADMINISTRATIVELY_PROHIBITED

    def check_channel_pty_request(self, channel, term, width, height, pixelwidth, pixelheight, modes):
        return True

    def check_channel_shell_request(self, channel):
        self.shell.set()
        return True


def serve(connection, key):
    transport = paramiko.Transport(connection)
    try:
        transport.add_server_key(key)
        login = Login()
        transport.start_server(server=login)
        channel = transport.accept(10)
        if channel is None or not login.shell.wait(5):
            return
        channel.sendall(b"SSH_TEST_AUTHENTICATED\r\n")
        while transport.is_active():
            data = channel.recv(4096)
            if not data:
                break
            channel.sendall(data)
    except (EOFError, OSError, paramiko.SSHException):
        pass
    finally:
        transport.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ready-file", type=Path, required=True)
    parser.add_argument("--rotate-after", type=int, default=2)
    args = parser.parse_args()
    original = paramiko.RSAKey.generate(2048)
    replacement = paramiko.RSAKey.generate(2048)
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        listener.listen()
        args.ready_file.write_text(json.dumps({"port": listener.getsockname()[1]}), encoding="utf-8")
        count = 0
        while True:
            connection, _ = listener.accept()
            count += 1
            key = original if count <= args.rotate_after else replacement
            threading.Thread(target=serve, args=(connection, key), daemon=True).start()


if __name__ == "__main__":
    main()
