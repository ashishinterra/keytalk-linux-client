#!/bin/bash

# Build guest docker images used in multi-platform client tests'

set -e
set -u

docker build --no-cache --file=guest_dockerfiles/debian_jessie_keytalk_test --tag=debian-jessie-keytalk-test .
docker build --no-cache --file=guest_dockerfiles/debian_stretch_keytalk_test --tag=debian-stretch-keytalk-test .
docker build --no-cache --file=guest_dockerfiles/ubuntu_16.04_keytalk_test --tag=ubuntu-16.04-keytalk-test .