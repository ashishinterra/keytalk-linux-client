PrettyPython
============

PEP-8 python code checker and formatter


This script is capable of checking and fixing formatting in python scripts:
 - Checks and fixes python shebang to /usr/bin/env python
 - Checks and fixes python coding to utf-8
 - Checks and fixes PEP8 formatting (requires autopep8)

Compatibility: Python 2.7+ or Python 3.

Requires autopep8. Install it with:
```bash
# pip install argparse autopep8
```
or by calling this script with --install-deps argument

Usage:
```python
from PrettyPython import check_shebang, check_coding, check_pep8
from PrettyPython import fix_shebang, fix_coding, fix_pep8

dirs = ["./"]

# check formatting
formatting_ok = check_shebang(dirs)
formatting_ok = check_coding(dirs) and formatting_ok
formatting_ok = check_pep8(dirs) and formatting_ok

# fix formatting
fix_shebang(dirs)
fix_coding(dirs)
fix_pep8(dirs)
```
