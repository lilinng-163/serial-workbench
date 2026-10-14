#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
set -eu

docker run --rm \
    -v /srv/lilinng-web/certbot/etc:/etc/letsencrypt \
    -v /srv/lilinng-web/certbot/lib:/var/lib/letsencrypt \
    -v /srv/lilinng-web/www:/var/www/html \
    certbot/certbot:latest renew --non-interactive

docker exec lilinng-https nginx -s reload
