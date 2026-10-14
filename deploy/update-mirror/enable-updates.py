#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0

from pathlib import Path


CONFIG = Path('/srv/lilinng-web/config/default.conf')
MARKER = '    location /updates/ {'
BLOCK = '''    location = /updates/latest.json {
        alias /usr/share/nginx/html/shared/updates/latest.json;
        default_type application/json;
        add_header Cache-Control "no-store";
        add_header X-Content-Type-Options nosniff;
    }

    location /updates/ {
        alias /usr/share/nginx/html/shared/updates/;
        add_header Content-Disposition "attachment";
        add_header X-Content-Type-Options nosniff;
    }

'''


def main() -> None:
    contents = CONFIG.read_text(encoding='utf-8')
    if MARKER in contents:
        return

    anchor = '    location / {\n'
    if contents.count(anchor) != 1:
        raise RuntimeError('Unexpected nginx site configuration')

    CONFIG.write_text(contents.replace(anchor, BLOCK + anchor), encoding='utf-8')


if __name__ == '__main__':
    main()
