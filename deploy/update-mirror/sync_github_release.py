#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Mirror a verified public GitHub release; publish metadata only after the file."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import tempfile
from urllib.error import HTTPError
from urllib.parse import urlsplit
from urllib.request import Request, urlopen


REPOSITORY = 'lilinng-163/serial-workbench'
API = f'https://api.github.com/repos/{REPOSITORY}/releases/latest'
MAXIMUM_BYTES = 300 * 1024 * 1024


def release_info(release: dict, base_url: str) -> dict:
    tag = release.get('tag_name', '')
    version = tag.removeprefix('v')
    if release.get('draft') or release.get('prerelease') or not re.fullmatch(r'\d+\.\d+\.\d+', version):
        raise ValueError('Expected a stable release with a numeric version')

    filename = f'SerialWorkbench-{version}-windows-x64-setup.exe'
    assets = [asset for asset in release.get('assets', []) if asset.get('name') == filename]
    if len(assets) != 1:
        raise ValueError('Release must contain exactly one Windows installer')

    asset = assets[0]
    url = urlsplit(asset.get('browser_download_url', ''))
    expected_path = f'/{REPOSITORY}/releases/download/{tag}/{filename}'
    digest = asset.get('digest', '')
    size = asset.get('size', 0)
    if (asset.get('state') != 'uploaded' or url.scheme != 'https' or url.netloc != 'github.com'
            or url.path != expected_path or url.query or url.fragment
            or not re.fullmatch(r'sha256:[0-9a-fA-F]{64}', digest)
            or type(size) is not int or not 0 < size <= MAXIMUM_BYTES):
        raise ValueError('Invalid installer URL, digest, state or size')

    return {
        'version': version,
        'filename': filename,
        'url': f'{base_url.rstrip("/")}/updates/{filename}',
        'sha256': digest[7:].lower(),
        'size': size,
        'title': str(release.get('name') or f'Serial Workbench {version}')[:256],
        'notes': str(release.get('body') or '')[:128 * 1024],
        'published_at': str(release.get('published_at') or '')[:64],
        'release_url': f'https://github.com/{REPOSITORY}/releases/tag/{tag}',
        'github_url': asset['browser_download_url'],
    }


def atomic_write(path: Path, data: bytes) -> None:
    with tempfile.NamedTemporaryFile(dir=path.parent, prefix='.publish-', delete=False) as output:
        temporary = Path(output.name)
        try:
            output.write(data)
            output.flush()
            os.fsync(output.fileno())
        except BaseException:
            temporary.unlink(missing_ok=True)
            raise
    try:
        temporary.chmod(0o644)
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def verified_file(path: Path, info: dict) -> bool:
    if not path.is_file() or path.stat().st_size != info['size']:
        return False
    with path.open('rb') as stream:
        digest = hashlib.sha256()
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
        return digest.hexdigest() == info['sha256']


def download(info: dict, target: Path, opener=urlopen) -> None:
    temporary = None
    try:
        request = Request(info['github_url'], headers={'User-Agent': 'SerialWorkbench-Mirror'})
        with opener(request, timeout=120) as source, tempfile.NamedTemporaryFile(
                dir=target.parent, prefix='.installer-', delete=False) as output:
            temporary = Path(output.name)
            digest = hashlib.sha256()
            total = 0
            while chunk := source.read(1024 * 1024):
                total += len(chunk)
                if total > info['size']:
                    raise ValueError('Installer is larger than the release metadata')
                digest.update(chunk)
                output.write(chunk)
            if total != info['size'] or digest.hexdigest() != info['sha256']:
                raise ValueError('Installer size or SHA-256 verification failed')
            output.flush()
            os.fsync(output.fileno())
        temporary.chmod(0o644)
        os.replace(temporary, target)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def publish(release: dict, root: Path, base_url: str, opener=urlopen) -> dict:
    info = release_info(release, base_url)
    root.mkdir(parents=True, exist_ok=True)
    manifest = root / 'latest.json'
    if manifest.exists():
        try:
            previous = json.loads(manifest.read_text(encoding='utf-8'))
            if tuple(map(int, previous['version'].split('.'))) > tuple(map(int, info['version'].split('.'))):
                raise ValueError('Refusing to replace the mirror with an older release')
        except (KeyError, json.JSONDecodeError):
            pass

    target = root / info['filename']
    if not verified_file(target, info):
        download(info, target, opener)

    checksum = f'{info["sha256"]}  {info["filename"]}\n'.encode()
    atomic_write(root / 'SHA256SUMS.txt', checksum)
    atomic_write(root / f'{info["filename"]}.sha256', checksum)
    atomic_write(manifest, (json.dumps(info, ensure_ascii=False, indent=2) + '\n').encode())
    return info


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, default=Path('/srv/lilinng-web/www/shared/updates'))
    parser.add_argument('--base-url', default='https://162.14.72.194')
    args = parser.parse_args()
    args.root.mkdir(parents=True, exist_ok=True)
    etag_file = args.root / '.github-etag'
    headers = {'User-Agent': 'SerialWorkbench-Mirror', 'Accept': 'application/vnd.github+json'}
    if etag_file.exists() and (args.root / 'latest.json').exists():
        try:
            current = json.loads((args.root / 'latest.json').read_text(encoding='utf-8'))
            filename = f'SerialWorkbench-{current["version"]}-windows-x64-setup.exe'
            if current['filename'] == filename and verified_file(args.root / filename, current):
                headers['If-None-Match'] = etag_file.read_text().strip()
        except (ValueError, KeyError, OSError):
            pass
    try:
        with urlopen(Request(API, headers=headers), timeout=30) as source:
            data = source.read(1024 * 1024 + 1)
            if len(data) > 1024 * 1024:
                raise ValueError('Release metadata exceeds its size limit')
            etag = source.headers.get('ETag', '')
            release = json.loads(data)
    except HTTPError as error:
        if error.code == 304:
            print('GitHub release unchanged; mirror retained')
            return
        raise

    info = publish(release, args.root, args.base_url)
    if etag:
        atomic_write(etag_file, etag.encode())
    print(f'Published verified release {info["version"]}; {info["size"]} bytes')


if __name__ == '__main__':
    main()
