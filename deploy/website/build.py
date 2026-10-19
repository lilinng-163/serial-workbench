#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Build the static personal site with shared, server-rendered registration links."""

import argparse
import hashlib
import html
import json
from pathlib import Path
import re
import shutil


ROOT = Path(__file__).resolve().parent


def footer(base: str, config: dict) -> str:
    icp = html.escape(config['icp_number'])
    security_number = config.get('public_security_number', '').strip()
    security_code = config.get('public_security_code', '').strip()
    icon = config.get('public_security_icon', '').strip()

    if bool(security_number) != bool(security_code):
        raise ValueError('Public security number and query code must be configured together')

    security = '<span class="registration-pending">公安备案办理中</span>'
    if security_number:
        if not re.fullmatch(r'\d{14}', security_code):
            raise ValueError('Expected a 14-digit public security query code')
        image = ''
        if icon:
            if not re.fullmatch(r'assets/[a-zA-Z0-9._-]+\.(png|svg)', icon):
                raise ValueError('Expected a local public security icon asset')
            if not (ROOT / 'site' / icon).is_file():
                raise ValueError('Public security icon file is missing')
            image = f'<img src="{base}{icon}" width="18" height="20" alt="">'
        security = (
            f'<a href="https://beian.mps.gov.cn/#/query/webSearch?code={security_code}" '
            f'target="_blank" rel="noopener noreferrer">{image}{html.escape(security_number)}</a>'
        )

    return f'''<footer class="site-footer">
  <div class="container footer-inner">
    <span>© 2026 lilinng · 嵌入式开发与工具制作</span>
    <div class="footer-links"><a href="{base}about/index.html">关于这个小站</a><a href="https://github.com/lilinng-163" target="_blank" rel="noopener noreferrer">GitHub</a><a href="{base}downloads/index.html">软件下载</a></div>
  </div>
  <div class="container registration" aria-label="网站备案信息">
    <a href="https://beian.miit.gov.cn/" target="_blank" rel="noopener noreferrer">{icp}</a>
    {security}
  </div>
</footer>'''


def build(output: Path) -> None:
    source = ROOT / 'site'
    output = output.resolve()
    if output == ROOT or source == output or source in output.parents or output in source.parents:
        raise ValueError('Output must be separate from the website sources')
    output.mkdir(parents=True, exist_ok=True)
    config = json.loads((ROOT / 'site-config.json').read_text(encoding='utf-8'))
    if not config['icp_number'].strip():
        raise ValueError('ICP registration number must not be empty')
    assets = {}
    for filename in ('site.css', 'app.js', 'favicon.svg', 'serial-workbench.svg', 'workbench-preview.png'):
        data = (source / 'assets' / filename).read_bytes()
        stem, extension = filename.rsplit('.', 1)
        assets[filename] = f'{stem}.{hashlib.sha256(data).hexdigest()[:12]}.{extension}'

    for path in source.rglob('*'):
        if not path.is_file():
            continue
        relative = path.relative_to(source)
        if relative.parts[0] == 'assets' and path.name in assets:
            relative = Path('assets') / assets[path.name]
        target = output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        if path.suffix != '.html':
            shutil.copyfile(path, target)
            continue
        text = path.read_text(encoding='utf-8')
        base = '/' if relative.name == '404.html' else ('../' * (len(relative.parts) - 1) or './')
        if text.count('<!-- SITE_FOOTER -->') != 1:
            raise ValueError(f'{path}: expected exactly one footer marker')
        text = text.replace('<!-- SITE_FOOTER -->', footer(base, config))
        for name, hashed in assets.items():
            text = text.replace(f'assets/{name}', f'assets/{hashed}')
        target.write_text(text, encoding='utf-8', newline='\n')

    print(json.dumps({'output': str(output), 'assets': assets}, ensure_ascii=False))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    build(parser.parse_args().output)
