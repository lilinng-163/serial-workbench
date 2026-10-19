# SPDX-License-Identifier: Apache-2.0
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('mirror', ROOT / 'deploy/update-mirror/sync_github_release.py')
mirror = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mirror)
manual_spec = importlib.util.spec_from_file_location('manual', ROOT / 'deploy/update-mirror/manual_receive.py')
manual = importlib.util.module_from_spec(manual_spec)
manual_spec.loader.exec_module(manual)
PAYLOAD = b'MZ verified installer fixture'


def release(version='0.15.14', notes='## Fixes\n\nSSH wrapping fixed.'):
    filename = f'SerialWorkbench-{version}-windows-x64-setup.exe'
    return {
        'tag_name': 'v' + version,
        'name': 'Serial Workbench ' + version,
        'body': notes,
        'published_at': '2026-10-14T15:00:00Z',
        'assets': [{
            'name': filename, 'state': 'uploaded', 'size': len(PAYLOAD),
            'digest': 'sha256:' + hashlib.sha256(PAYLOAD).hexdigest(),
            'browser_download_url': f'https://github.com/{mirror.REPOSITORY}/releases/download/v{version}/{filename}',
        }],
    }


class MirrorTests(unittest.TestCase):
    def test_local_only_check_never_uses_network(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            info = mirror.publish(release(), root, 'https://162.14.72.194', lambda *a, **kw: io.BytesIO(PAYLOAD))
            with patch.object(mirror, 'urlopen', side_effect=AssertionError('No network allowed')):
                with patch('sys.argv', ['mirror', '--check-local', '--root', str(root)]):
                    mirror.main()
                self.assertEqual(mirror.check_local(root), info)
            (root / info['filename']).write_bytes(b'corrupt')
            with self.assertRaises(ValueError):
                mirror.check_local(root)

    def test_local_check_rejects_bad_checksum_file(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            mirror.publish(release(), root, 'https://162.14.72.194', lambda *a, **kw: io.BytesIO(PAYLOAD))
            (root / 'SHA256SUMS.txt').write_text('incorrect')
            with self.assertRaises(ValueError):
                mirror.check_local(root)

    def test_html_update_keeps_screenshot_version_and_registration(self):
        text = '''<span data-release-version>v0.15.32</span>
<p>v0.15.32 实际界面 · 演示数据</p>
<a href="https://beian.miit.gov.cn/">蜀ICP备2026062713号</a>
<a data-release-download href="/updates/SerialWorkbench-0.15.32-windows-x64-setup.exe">下载</a>
<a data-release-page href="https://github.com/lilinng-163/serial-workbench/releases/tag/v0.15.32">Release</a>
<span data-release-size>old</span><span data-release-date>old</span>
<code data-release-checksum>old</code><div data-release-notes>old</div>'''
        info = mirror.release_info(release('0.15.35'), 'https://162.14.72.194')
        updated = manual.update_html(text, info, True)
        self.assertIn('data-release-version>v0.15.35', updated)
        self.assertIn('v0.15.32 实际界面', updated)
        self.assertIn('https://beian.miit.gov.cn/', updated)
        self.assertIn(info['filename'], updated)
        self.assertIn('/tag/v0.15.35', updated)
        self.assertIn(info['sha256'], updated)
        self.assertNotIn('>old<', updated)
        self.assertEqual(manual.update_html(updated, info, True), updated)

    def test_job_path_rejects_other_directories(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(ValueError):
                manual.job_path(directory)

    def test_verified_download_and_notes_update(self):
        calls = []
        def opener(request, timeout):
            calls.append(request.full_url)
            return io.BytesIO(PAYLOAD)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            info = mirror.publish(release(), root, 'https://162.14.72.194', opener)
            self.assertEqual((root / info['filename']).read_bytes(), PAYLOAD)
            self.assertEqual(json.loads((root / 'latest.json').read_text())['notes'], release()['body'])
            mirror.publish(release(notes='New notes'), root, 'https://162.14.72.194', opener)
            self.assertEqual(len(calls), 1)
            self.assertEqual(json.loads((root / 'latest.json').read_text())['notes'], 'New notes')

    def test_corrupt_download_preserves_previous_version(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            mirror.publish(release(), root, 'https://162.14.72.194', lambda *a, **kw: io.BytesIO(PAYLOAD))
            before = (root / 'latest.json').read_bytes()
            with self.assertRaises(ValueError):
                mirror.publish(release('0.15.15'), root, 'https://162.14.72.194', lambda *a, **kw: io.BytesIO(b'broken'))
            self.assertEqual((root / 'latest.json').read_bytes(), before)
            self.assertFalse(list(root.glob('.installer-*')))
            with self.assertRaises(ValueError):
                mirror.publish(release('0.15.13'), root, 'https://162.14.72.194')

    def test_rejects_untrusted_asset(self):
        data = release()
        data['assets'][0]['browser_download_url'] = 'http://github.com/installer.exe'
        with self.assertRaises(ValueError):
            mirror.release_info(data, 'https://162.14.72.194')
        data = release()
        data['assets'][0]['digest'] = ''
        with self.assertRaises(ValueError):
            mirror.release_info(data, 'https://162.14.72.194')


if __name__ == '__main__':
    unittest.main()
