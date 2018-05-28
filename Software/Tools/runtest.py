#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import sys


if len(sys.argv) > 1:
    test_args = "-v " + " ".join(sys.argv[1:])
else:
    test_args = "-v"
dirname = os.path.split(os.getcwd())[1]

if os.system('make') != 0:
    sys.exit(-1)

if os.system('cd ../Export && ./{} {}'.format(dirname, test_args)) != 0:
    sys.exit(-1)
