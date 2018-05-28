#!/usr/bin/env python
# -*- coding: utf-8 -*-

# This script checks and fixes formatting in python scripts
# - Checks and fixes PEP8 formatting
#
# Usage: formatpython.py [--fix]
# Return values: 0 - success
# 1 - indicates invalid formatting when called to check formatting;
# indicates error when called to fix formatting

import os
import sys
sys.path.append("Import")
import PrettyPython as fmt

# Configuration
DIRS = [
    os.path.join("Client", "TestProjects", "testReseptInstaller", "linux", "apache"),
    os.path.join("Client", "Projects", "ReseptConsoleClient"),
    os.path.join("Client", "Projects", "ReseptPythonClient", "keytalk-client.py"),
    os.path.join("WebUI.Server", "Projects"),
    os.path.join("WebUI.Server", "TestProjects", "webuitests"),
    os.path.join("Server", "Projects", "settings"),
    os.path.join("Server", "Projects", "config"),
    os.path.join("Server", "Projects", "pykeytalk", "*.py"),
    os.path.join("Server", "TestProjects", "testpykeytalk"),
]

if not fmt.install_deps():
    print('Failed to install dependencies')
    sys.exit(1)

# Check
if len(sys.argv) == 1:
    success = fmt.check_pep8(DIRS)
    sys.exit(0 if success else 1)

# Fix
elif len(sys.argv) == 2 and sys.argv[1] == "--fix":
    success = fmt.fix_pep8(DIRS)
    sys.exit(0 if success else 1)

else:
    prog = sys.argv[0]
    print("Usage: %s [--fix]" % prog)
    sys.exit(1)
