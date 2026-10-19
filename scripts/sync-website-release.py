#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Download the latest release locally, then publish via Serial Workbench MCP."""

import argparse
import asyncio
import base64
import gzip
import hashlib
import http.client
import importlib.util
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import socket
import subprocess
import time
from urllib.parse import urlsplit
from urllib.request import Request, urlopen
import uuid


ROOT = Path(__file__).resolve().parents[1]
DEPLOY = ROOT / 'deploy/update-mirror'
spec = importlib.util.spec_from_file_location('release_mirror', DEPLOY / 'sync_github_release.py')
mirror = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mirror)
ANSI = re.compile(r'\x1b\[[0-?]*[ -/]*[@-~]|\x1b\][^\x07]*(?:\x07|\x1b\\)|\x1b[=>]')


def latest_release():
    if shutil.which('gh'):
        data = subprocess.check_output(
            ['gh', 'api', f'repos/{mirror.REPOSITORY}/releases/latest'], timeout=60)
    else:
        request = Request(mirror.API, headers={'User-Agent': 'SerialWorkbench-Manual-Sync'})
        with urlopen(request, timeout=30) as response:
            data = response.read(1024 * 1024 + 1)
    if len(data) > 1024 * 1024:
        raise ValueError('Release metadata too large')
    return json.loads(data)


def unpack(result):
    if result.isError:
        raise RuntimeError('; '.join(item.text for item in result.content if hasattr(item, 'text')))
    return result.structuredContent or json.loads(result.content[0].text)


class Remote:
    def __init__(self, client, channel, status):
        self.client = client
        self.channel = channel
        self.cursor = status['latest_cursor']
        self.session = status['session_id']

    async def call(self, name, arguments):
        return unpack(await self.client.call_tool(name, arguments))

    async def send(self, command):
        await self.call('send_data', {'channel_id': self.channel,
                                     'payload': command, 'newline': 'lf'})

    async def run(self, source, sudo=False, timeout=60):
        marker = 'SW' + uuid.uuid4().hex[:8]
        wrapper = '''import base64,json
try:
 namespace={}
 exec(SOURCE,namespace)
 result={'ok':True,'value':namespace.get('result')}
except Exception as error:
 result={'ok':False,'error':type(error).__name__+': '+str(error)}
encoded=base64.b64encode(json.dumps(result,ensure_ascii=False).encode()).decode()
print('\\nMARKER:start',flush=True)
for offset in range(0,len(encoded),40):print('MARKER:'+encoded[offset:offset+40],flush=True)
print('MARKER:end',flush=True)
'''.replace('SOURCE', repr(source)).replace('MARKER', marker)
        encoded = base64.b64encode(gzip.compress(wrapper.encode())).decode()
        command = ('sudo -n ' if sudo else '') + 'python3 -c ' + shlex.quote(
            'import base64,gzip;exec(gzip.decompress(base64.b64decode(' + repr(encoded) + ')))')
        if len(command.encode()) > 3500:
            raise ValueError('Remote command exceeds terminal-safe limit; upload a helper file')
        await self.send(command)
        text = ''
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            batch = await self.call('read_events', {
                'channel_id': self.channel, 'after': self.cursor, 'limit': 200, 'wait_ms': 1000,
            })
            if (batch.get('session_id') != self.session or batch.get('gap')
                    or batch.get('cursor_reset_required')):
                raise RuntimeError('MCP journal changed or lost events; no other connection is used')
            self.cursor = batch['next_cursor']
            text += ''.join(event.get('text', '') for event in batch['events']
                            if event['direction'] == 'rx')
            clean = ANSI.sub('', text).replace('\r', '')
            if marker + ':end' in clean:
                chunks = re.findall(re.escape(marker) + r':([A-Za-z0-9+/=]+)\n', clean)
                chunks = [part for part in chunks if part not in ('start', 'end')]
                result = json.loads(base64.b64decode(''.join(chunks), validate=True))
                if not result['ok']:
                    raise RuntimeError(result['error'])
                return result['value']
        raise TimeoutError('MCP command timed out; inspect the exact SSH tab before retrying')

    async def upload(self, job, name, data):
        encoded = base64.b64encode(gzip.compress(data)).decode()
        path = job + '/' + name
        await self.send(': > ' + shlex.quote(path + '.b64'))
        for offset in range(0, len(encoded), 1400):
            await self.send("printf '%s' " + shlex.quote(encoded[offset:offset + 1400])
                            + ' >> ' + shlex.quote(path + '.b64'))
        await self.run('from pathlib import Path\nimport base64,gzip,hashlib\n'
                       + 'p=Path(' + repr(path) + ')\n'
                       + 'data=gzip.decompress(base64.b64decode(Path(str(p)+".b64").read_bytes(),validate=True))\n'
                       + 'assert hashlib.sha256(data).hexdigest()==' + repr(hashlib.sha256(data).hexdigest())
                       + '\np.write_bytes(data)\nPath(str(p)+".b64").unlink()')


async def connect(client, server, channel_id):
    status = unpack(await client.call_tool('get_status', {}))
    if not status['allow_ai_control'] or not status['protocol_access'].get('ssh'):
        raise RuntimeError('Enable SSH access and AI control in Serial Workbench Settings')
    matches = [connection for connection in status['connections']
               if connection.get('config', {}).get('kind') == 'ssh'
               and connection['config'].get('ssh_host') == server
               and (not channel_id or connection['id'] == channel_id)]
    if channel_id and not matches:
        raise ValueError('Selected MCP channel does not match the requested SSH host')
    if len(matches) > 1:
        raise ValueError('Several matching SSH channels; specify --channel-id')
    if matches and matches[0]['state'] == 'connected':
        selected = matches[0]['id']
    else:
        selected = unpack(await client.call_tool('open_connection', {
            'config': {'kind': 'ssh', 'ssh_host': server},
        }))['id']
        for _ in range(40):
            await asyncio.sleep(0.5)
            status = unpack(await client.call_tool('get_status', {}))
            connection = next(item for item in status['connections'] if item['id'] == selected)
            if connection['state'] == 'connected':
                break
            if connection.get('error'):
                raise RuntimeError('SSH authentication failed; resolve it in Serial Workbench')
        else:
            raise TimeoutError('SSH did not connect; resolve authentication in the workbench')
    return Remote(client, selected, status)


def upload_installer(server, port, job, installer, info, log_path):
    with socket.socket() as probe:
        probe.bind(('127.0.0.1', 0))
        local_port = probe.getsockname()[1]
    flags = subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0
    with log_path.open('wb') as log:
        process = subprocess.Popen([
            'ssh', '-N', '-T', '-o', 'BatchMode=yes', '-o', 'ExitOnForwardFailure=yes',
            '-o', 'ServerAliveInterval=15', '-L', f'127.0.0.1:{local_port}:127.0.0.1:{port}',
            server,
        ], stdout=log, stderr=subprocess.STDOUT, creationflags=flags)
        connection = http.client.HTTPConnection('127.0.0.1', local_port, timeout=120)
        try:
            deadline = time.monotonic() + 20
            while True:
                if process.poll() is not None:
                    raise RuntimeError(f'SSH forwarding failed; inspect {log_path}')
                try:
                    connection.connect()
                    break
                except ConnectionRefusedError:
                    if time.monotonic() >= deadline:
                        raise
                    time.sleep(0.2)
            connection.putrequest('POST', '/' + Path(job).name)
            connection.putheader('Content-Length', str(info['size']))
            connection.putheader('Content-Type', 'application/octet-stream')
            connection.endheaders()
            with installer.open('rb') as source:
                while chunk := source.read(262144):
                    connection.send(chunk)
            response = connection.getresponse()
            if response.status != 200:
                raise RuntimeError(f'Upload rejected: HTTP {response.status}')
            result = json.loads(response.read())
            if result != {key: info[key] for key in ('size', 'sha256')}:
                raise ValueError('Upload acknowledgement does not match local release')
        finally:
            connection.close()
            process.terminate()
            process.wait(timeout=15)


def verify_public(website, info):
    def fetch(path):
        request = Request(website.rstrip('/') + path, headers={'Cache-Control': 'no-cache'})
        return urlopen(request, timeout=60)

    with fetch('/updates/latest.json') as response:
        actual = json.load(response)
    for key in ('version', 'filename', 'size', 'sha256'):
        if actual[key] != info[key]:
            raise ValueError(f'Public manifest mismatch: {key}')
    total = 0
    digest = hashlib.sha256()
    with fetch('/updates/' + info['filename']) as response:
        while chunk := response.read(262144):
            total += len(chunk)
            if total > info['size']:
                raise ValueError('Public installer exceeds expected size')
            digest.update(chunk)
    if total != info['size'] or digest.hexdigest() != info['sha256']:
        raise ValueError('Public installer failed complete-download verification')
    with fetch('/downloads/index.html') as response:
        page = response.read().decode()
    if ('v' + info['version']) not in page or info['sha256'] not in page:
        raise ValueError('Website HTML fallback was not updated')
    return {'version': info['version'], 'size': total, 'sha256': digest.hexdigest()}


async def synchronize(args, release, installer, info, cache):
    from mcp import ClientSession
    from mcp.client.streamable_http import streamable_http_client

    async with streamable_http_client(args.mcp_url) as (read, write, _):
        async with ClientSession(read, write) as client:
            await client.initialize()
            remote = await connect(client, args.server, args.channel_id)
            job = '/tmp/sw-release-sync-' + uuid.uuid4().hex
            print('Connected through MCP:', args.server, remote.channel, flush=True)
            await remote.send('stty -echo')
            try:
                await remote.run('from pathlib import Path\nPath(' + repr(job) + ').mkdir(mode=0o700)')
                sources = {
                    'manual_receive.py': DEPLOY / 'manual_receive.py',
                    'mirror.py': DEPLOY / 'sync_github_release.py',
                    'sync.service': DEPLOY / 'lilinng-release-sync.service',
                    'website_deploy.py': ROOT / 'deploy/website/remote_deploy.py',
                }
                for name, path in sources.items():
                    await remote.upload(job, name, path.read_bytes())
                await remote.upload(job, 'release.json', json.dumps(release).encode())
                bootstrap = ('import importlib.util\nspec=importlib.util.spec_from_file_location'
                             + '("manual",' + repr(job + '/manual_receive.py') + ')\n'
                             + 'manual=importlib.util.module_from_spec(spec);spec.loader.exec_module(manual)\n'
                             + 'job=manual.job_path(' + repr(job) + ')\n')
                # No stop/disable/restart: future timer rounds only verify local files.
                await remote.run(bootstrap + 'result=manual.configure(job)', sudo=True)
                await remote.run('import subprocess,sys\n'
                                 + 'subprocess.Popen([sys.executable,' + repr(job + '/manual_receive.py')
                                 + ',"receive",' + repr(job) + '],stdin=subprocess.DEVNULL,'
                                 + 'stdout=open(' + repr(job + '/receiver.log')
                                 + ',"w"),stderr=subprocess.STDOUT,start_new_session=True)')
                ready = None
                for _ in range(20):
                    ready = await remote.run('from pathlib import Path\nimport json\np=Path('
                                             + repr(job + '/ready.json') + ')\n'
                                             + 'result=json.loads(p.read_text()) if p.exists() else None')
                    if ready:
                        break
                    await asyncio.sleep(0.2)
                if not ready:
                    raise RuntimeError('One-shot upload receiver did not start')
                print('Uploading verified local installer through SSH forwarding...', flush=True)
                await asyncio.to_thread(upload_installer, args.server, ready['port'], job,
                                        installer, info, cache / 'ssh-forward.log')
                result = await remote.run(bootstrap + 'result=manual.publish(job,'
                                          + repr(args.base_url) + ')', sudo=True)
                (cache / 'publication.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n',
                                                       encoding='utf-8')
                await remote.run(bootstrap + 'import shutil\nshutil.rmtree(job)', sudo=True)
            finally:
                await remote.send('stty echo')
    print('Published. Verifying a complete public HTTPS download...', flush=True)
    report = await asyncio.to_thread(verify_public, args.website_url, info)
    (cache / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('PASS', json.dumps(report), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', default='t_ecs', help='SSH alias used by both the GUI and forwarding')
    parser.add_argument('--mcp-url', default='http://127.0.0.1:8765/mcp')
    parser.add_argument('--channel-id', default='')
    parser.add_argument('--base-url', default='https://162.14.72.194')
    parser.add_argument('--website-url', default='https://www.lilinng.cn')
    parser.add_argument('--installer', type=Path, help='Existing file that matches the latest release')
    parser.add_argument('--download-only', action='store_true')
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9_][A-Za-z0-9_.@:-]*', args.server):
        parser.error('Expected an SSH alias or host, not an SSH command')
    for value in (args.base_url, args.website_url):
        parsed = urlsplit(value)
        if (parsed.scheme != 'https' or not parsed.hostname or parsed.username
                or parsed.password or parsed.query or parsed.fragment or parsed.path not in ('', '/')):
            parser.error('Website and mirror URLs must be plain HTTPS origins')
    release = latest_release()
    info = mirror.release_info(release, args.base_url)
    cache = ROOT / 'dist/website-sync' / info['version']
    cache.mkdir(parents=True, exist_ok=True)
    installer = args.installer.resolve() if args.installer else cache / info['filename']
    print('Latest stable release:', info['version'], flush=True)
    if args.installer:
        if not mirror.verified_file(installer, info):
            raise ValueError('Provided installer does not match the latest GitHub release')
    elif not mirror.verified_file(installer, info):
        print('Downloading on this computer:', info['github_url'], flush=True)
        mirror.download(info, installer)
    (cache / 'release.json').write_text(json.dumps(release, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print('Verified local installer:', installer, info['sha256'], flush=True)
    if not args.download_only:
        try:
            import mcp
        except ImportError as error:
            raise RuntimeError('Install the MCP SDK: python -m pip install -r tests/integration/requirements.txt') from error
        asyncio.run(synchronize(args, release, installer, info, cache))


if __name__ == '__main__':
    main()
