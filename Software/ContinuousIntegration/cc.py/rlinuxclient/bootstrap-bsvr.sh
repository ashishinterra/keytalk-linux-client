#!/bin/bash

cp -f ccpy.conf /etc/

DISTRO_NAME=$(lsb_release --id --short | tr '[:upper:]' '[:lower:]')
DISTRO_VERSION_MAJOR=$(lsb_release --release --short | egrep -o [0-9]+ | sed -n '1p')

install -m 644 cron.d/ccpy.${DISTRO_NAME}${DISTRO_VERSION_MAJOR} /etc/cron.d/ccpy