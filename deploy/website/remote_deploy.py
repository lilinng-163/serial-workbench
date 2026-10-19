#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Stage or activate a verified site bundle on the existing web server."""

import argparse
import base64
import gzip
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil


ROOT = Path('/srv/lilinng-web/www')
RECORDS = Path('/srv/lilinng-web/deployments')


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def record_path(release: str) -> Path:
    if not re.fullmatch(r'\d{8}-[a-z0-9-]+', release):
        raise ValueError('Invalid release directory name')
    return RECORDS / f'{release}.json'


def prepare(payload: Path) -> None:
    bundle = json.loads(gzip.decompress(base64.b64decode(payload.read_bytes(), validate=True)))
    release = bundle['release']
    record = record_path(release)
    previous = os.readlink(ROOT / 'current')
    if previous != bundle['expected_current']:
        raise RuntimeError('Current website changed since inspection; inspect before proceeding')
    source = (ROOT / 'current').resolve(strict=True)
    if source.parent != (ROOT / 'releases').resolve():
        raise ValueError('Current release must be inside the existing release directory')
    destination = ROOT / 'releases' / release
    if destination.exists() or record.exists():
        raise FileExistsError('Release already exists; never overwrite a previous deployment')

    files = {}
    for name, encoded in bundle['files'].items():
        path = PurePosixPath(name)
        if path.is_absolute() or '..' in path.parts or path.suffix not in {'.html', '.css', '.js', '.svg', '.txt'}:
            raise ValueError(f'Invalid static asset path: {name}')
        files[name] = base64.b64decode(encoded, validate=True)
    for name in ('index.html', 'projects/index.html', 'downloads/index.html', 'about/index.html', '404.html'):
        text = files[name].decode('utf-8')
        if 'https://beian.miit.gov.cn/' not in text or '公安备案办理中' not in text:
            raise ValueError(f'Missing registration footer: {name}')

    image = source / 'assets/workbench-preview.9c2a8cfd07.png'
    if digest(image) != bundle['image_sha256']:
        raise ValueError('Existing preview image differs from the reviewed source')

    # Copy downloads and old hashed assets as well, so cached pages remain usable.
    shutil.copytree(source, destination, symlinks=True)
    for name, data in files.items():
        target = destination / name
        if target.is_symlink() or not target.resolve().is_relative_to(destination.resolve()):
            raise ValueError(f'Refusing to overwrite a symlink outside the staged release: {name}')
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        target.chmod(0o644)
    shutil.copyfile(image, destination / 'assets/workbench-preview.png')
    (destination / 'assets/workbench-preview.png').chmod(0o644)
    hashes = {name: digest(destination / name) for name in files}
    hashes['assets/workbench-preview.png'] = bundle['image_sha256']
    data = {'release': release, 'previous': previous, 'hashes': hashes, 'activated': False}
    RECORDS.mkdir(exist_ok=True)
    record.write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'staged': str(destination), 'previous': previous, 'verified_files': len(hashes)}))


def activate(release: str) -> None:
    record = record_path(release)
    data = json.loads(record.read_text(encoding='utf-8'))
    if os.readlink(ROOT / 'current') != data['previous']:
        raise RuntimeError('Current website changed before activation')
    destination = ROOT / 'releases' / release
    for name, expected in data['hashes'].items():
        if digest(destination / name) != expected:
            raise ValueError(f'Staged file changed: {name}')
    pending = ROOT / f'.current-{release}'
    pending.symlink_to(f'releases/{release}')
    os.replace(pending, ROOT / 'current')
    data['activated'] = True
    record.write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'active': os.readlink(ROOT / 'current'), 'rollback': data['previous']}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--prepare', type=Path)
    mode.add_argument('--activate')
    args = parser.parse_args()
    if args.prepare:
        prepare(args.prepare)
    else:
        activate(args.activate)
