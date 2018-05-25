#!/bin/bash

echo -n $1 | openssl sha1 | sed 's/.*\([a-z0-9]\{40\}\).*/\1/'
