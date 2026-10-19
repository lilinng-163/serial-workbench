#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""One-shot loopback upload and verified publication, invoked through GUI MCP."""

import argparse
import base64
from datetime import datetime
import gzip
import html
from http.server import BaseHTTPRequestHandler, HTTPServer
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


ROOT = Path('/srv/lilinng-web/www')


def load_module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def job_path(value):
    job = Path(value).resolve(strict=True)
    if job.parent != Path('/tmp') or not re.fullmatch(r'sw-release-sync-[0-9a-f]{32}', job.name):
        raise ValueError('Unexpected upload job directory')
    return job


def receive(job):
    mirror = load_module(job / 'mirror.py', 'mirror')
    release = json.loads((job / 'release.json').read_text(encoding='utf-8'))
    info = mirror.release_info(release, 'https://162.14.72.194')
    incoming = job / 'installer.exe'

    class Handler(BaseHTTPRequestHandler):
        def do_POST(self):
            if (self.path != '/' + job.name
                    or self.headers.get('Content-Length') != str(info['size'])):
                self.send_error(400)
                return
            self.connection.settimeout(60)
            partial = job / 'installer.partial'
            try:
                remaining = info['size']
                with partial.open('xb') as target:
                    while remaining:
                        chunk = self.rfile.read(min(262144, remaining))
                        if not chunk:
                            raise ValueError('Incomplete upload')
                        target.write(chunk)
                        remaining -= len(chunk)
                    target.flush()
                    os.fsync(target.fileno())
                if not mirror.verified_file(partial, info):
                    raise ValueError('Uploaded installer SHA-256 mismatch')
                os.replace(partial, incoming)
                data = json.dumps({'size': info['size'], 'sha256': info['sha256']}).encode()
                self.send_response(200)
                self.send_header('Content-Length', str(len(data)))
                self.end_headers()
                self.wfile.write(data)
            except Exception as error:
                partial.unlink(missing_ok=True)
                self.send_error(500, type(error).__name__)
                raise

        def log_message(self, *args):
            pass

    server = HTTPServer(('127.0.0.1', 0), Handler)
    server.timeout = 180
    mirror.atomic_write(job / 'ready.json', json.dumps({'port': server.server_port}).encode())
    try:
        server.handle_request()
    finally:
        server.server_close()


def configure(job):
    """Keep the timer, replacing its network work with a local integrity check."""
    mirror = load_module(job / 'mirror.py', 'mirror')
    mirror.atomic_write(Path('/srv/lilinng-web/sync_github_release.py'),
                        (job / 'mirror.py').read_bytes())
    mirror.atomic_write(Path('/etc/systemd/system/lilinng-release-sync.service'),
                        (job / 'sync.service').read_bytes())
    subprocess.run(['systemctl', 'daemon-reload'], check=True, timeout=20)
    return {'mode': 'local-integrity-only', 'timer_preserved': True}


def update_html(text, info, downloads=False):
    version = info['version']
    text = re.sub(r'(data-release-version(?=[\s=>])[^>]*>)[^<]*(<)',
                  lambda match: match[1] + 'v' + version + match[2], text)
    if not downloads:
        return text
    text = re.sub(r'SerialWorkbench-\d+\.\d+\.\d+-windows-x64-setup\.exe',
                  info['filename'], text)
    text = re.sub(r'(/releases/(?:download|tag)/)v\d+\.\d+\.\d+',
                  lambda match: match[1] + 'v' + version, text)
    text = re.sub(r'下载 Serial Workbench \d+\.\d+\.\d+',
                  '下载 Serial Workbench ' + version, text)
    fields = {
        'size': f'{info["size"] / 1048576:.1f} MiB',
        'date': '更新于 ' + info['published_at'][:10],
        'checksum': info['sha256'],
    }
    for name, value in fields.items():
        text = re.sub(r'(data-release-' + name + r'(?=[\s=>])[^>]*>)[^<]*(<)',
                      lambda match: match[1] + html.escape(value) + match[2], text)
    # Recover the label if a previous publisher matched the checksum-file prefix.
    text = re.sub(r'(<a\b[^>]*\bdata-release-checksum-file(?=[\s=>])[^>]*>)[0-9a-fA-F]{64}(</a>)',
                  lambda match: match[1] + '校验文件' + match[2], text)
    summary = html.escape(info['title'] + ' · 完整更新说明自动加载。')
    return re.sub(r'(<div data-release-notes>).*?(</div>)',
                  lambda match: match[1] + summary + match[2], text, flags=re.S)


def publish(job, base_url):
    import fcntl

    mirror = load_module(job / 'mirror.py', 'mirror')
    release = json.loads((job / 'release.json').read_text(encoding='utf-8'))
    info = mirror.release_info(release, base_url)
    incoming = job / 'installer.exe'
    if not mirror.verified_file(incoming, info):
        raise ValueError('Manual upload missing or corrupt')

    def no_download(*args, **kwargs):
        raise RuntimeError('Server-side GitHub downloading is forbidden in manual mode')

    with open('/srv/lilinng-web/release-sync.lock', 'a+') as lock:
        # Fail immediately instead of waiting behind an old network job.
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        updates = ROOT / 'shared/updates'
        old = json.loads((updates / 'latest.json').read_text())
        if tuple(map(int, old['version'].split('.'))) > tuple(map(int, info['version'].split('.'))):
            raise ValueError('Refusing a mirror downgrade')
        with tempfile.NamedTemporaryFile(dir=updates, prefix='.manual-', delete=False) as target:
            pending = Path(target.name)
            try:
                with incoming.open('rb') as source:
                    shutil.copyfileobj(source, target)
                target.flush()
                os.fsync(target.fileno())
            except BaseException:
                pending.unlink(missing_ok=True)
                raise
        try:
            pending.chmod(0o644)
            os.replace(pending, updates / info['filename'])
        finally:
            pending.unlink(missing_ok=True)
        result = mirror.publish(release, updates, base_url, opener=no_download)
        mirror.check_local(updates)

        site = load_module(job / 'website_deploy.py', 'website_deploy')
        previous = os.readlink(ROOT / 'current')
        source = (ROOT / 'current').resolve(strict=True)
        names = ['index.html', 'projects/index.html', 'downloads/index.html',
                 'about/index.html', '404.html']
        files = {}
        changed = False
        for name in names:
            text = (source / name).read_text(encoding='utf-8')
            updated = update_html(text, info, name == 'downloads/index.html')
            changed = changed or updated != text
            files[name] = base64.b64encode(updated.encode()).decode()
        stamp = datetime.now().strftime('%Y%m%d')
        release_name = stamp + '-manual-' + version_slug(info['version']) + '-' + job.name[-8:]
        if changed:
            bundle = {'release': release_name, 'expected_current': previous, 'files': files}
            payload = job / 'website.bundle'
            payload.write_bytes(base64.b64encode(gzip.compress(json.dumps(bundle).encode())))
            site.prepare(payload)
            site.activate(release_name)
        result['source'] = 'manual-local-upload'
        result['website'] = os.readlink(ROOT / 'current')
        result['previous_website'] = previous
        result['server_network_download'] = False
        record = Path('/srv/lilinng-web/deployments') / (release_name + '-upload.json')
        mirror.atomic_write(record, (json.dumps(result, ensure_ascii=False, indent=2) + '\n').encode())
        return result


def version_slug(version):
    return version.replace('.', '-')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=['receive', 'configure', 'publish'])
    parser.add_argument('job')
    parser.add_argument('--base-url', default='https://162.14.72.194')
    args = parser.parse_args()
    job = job_path(args.job)
    if args.mode == 'receive':
        receive(job)
    elif args.mode == 'configure':
        print(json.dumps(configure(job)))
    else:
        print(json.dumps(publish(job, args.base_url), ensure_ascii=False))
