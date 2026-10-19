# SPDX-License-Identifier: Apache-2.0
"""Connection isolation and real loopback upload failure/success checks."""

from contextlib import redirect_stderr
import http.client
import importlib.util
import io
import json
from pathlib import Path
import shutil
import tempfile
import threading
import time
from types import SimpleNamespace
import unittest

from test_update_mirror import manual, PAYLOAD, release, ROOT

spec = importlib.util.spec_from_file_location('sync', ROOT / 'scripts/sync-website-release.py')
sync = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sync)


def connection(identifier, host):
    return {'id': identifier, 'state': 'connected', 'config': {'kind': 'ssh', 'ssh_host': host}}


class Client:
    def __init__(self, connections, allowed=True):
        self.status = {'connections': connections, 'allow_ai_control': allowed,
                       'protocol_access': {'ssh': True}, 'latest_cursor': 29, 'session_id': 'session'}
        self.calls = []

    async def call_tool(self, name, arguments):
        self.calls.append(name)
        if name != 'get_status':
            raise AssertionError('Unexpected write or connection fallback')
        return SimpleNamespace(isError=False, structuredContent=self.status)


class ConnectionTests(unittest.IsolatedAsyncioTestCase):
    async def test_uses_exact_host_instead_of_first_channel(self):
        client = Client([connection('other', 'another-host'), connection('correct', 't_ecs')])
        remote = await sync.connect(client, 't_ecs', '')
        self.assertEqual(remote.channel, 'correct')
        self.assertEqual(remote.cursor, 29)

    async def test_control_permission_is_required(self):
        with self.assertRaises(RuntimeError):
            await sync.connect(Client([], allowed=False), 't_ecs', '')

    async def test_wrong_channel_does_not_fall_back(self):
        client = Client([connection('other', 'another-host')])
        with self.assertRaises(ValueError):
            await sync.connect(client, 't_ecs', 'other')
        self.assertEqual(client.calls, ['get_status'])

    async def test_ambiguous_channels_require_explicit_selection(self):
        with self.assertRaises(ValueError):
            await sync.connect(Client([connection('one', 't_ecs'), connection('two', 't_ecs')]), 't_ecs', '')


class UploadTests(unittest.TestCase):
    def upload(self, payload, expected_status):
        with tempfile.TemporaryDirectory() as directory:
            job = Path(directory)
            shutil.copyfile(ROOT / 'deploy/update-mirror/sync_github_release.py', job / 'mirror.py')
            (job / 'release.json').write_text(json.dumps(release()))
            with redirect_stderr(io.StringIO()):
                worker = threading.Thread(target=manual.receive, args=(job,), daemon=True)
                worker.start()
                deadline = time.monotonic() + 5
                while not (job / 'ready.json').exists():
                    if time.monotonic() > deadline:
                        self.fail('Receiver readiness timed out')
                    time.sleep(0.01)
                port = json.loads((job / 'ready.json').read_text())['port']
                client = http.client.HTTPConnection('127.0.0.1', port, timeout=5)
                try:
                    client.request('POST', '/' + job.name, payload)
                    response = client.getresponse()
                    self.assertEqual(response.status, expected_status)
                    response.read()
                finally:
                    client.close()
                    worker.join(timeout=5)
                self.assertFalse(worker.is_alive(), 'One-shot listener must exit')
            self.assertFalse((job / 'installer.partial').exists())
            if expected_status == 200:
                self.assertEqual((job / 'installer.exe').read_bytes(), PAYLOAD)
            else:
                self.assertFalse((job / 'installer.exe').exists())

    def test_verified_upload_exits_and_preserves_bytes(self):
        self.upload(PAYLOAD, 200)

    def test_corrupt_upload_is_not_publishable(self):
        self.upload(b'X' * len(PAYLOAD), 500)


if __name__ == '__main__':
    unittest.main()
