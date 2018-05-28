#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import site
import os
import re


def usage():
    print >>sys.stderr, "Usage: python %s --cxx|--objc|--java|--raw template-file config-file" % sys.argv[
        0]
    sys.exit(1)


def generate(templ_file):
    try:
        constants = [{'name': name, 'value': getattr(config_module, name)}
                     for name in dir(config_module) if not name.startswith("_")]
        templ_engine.substitute(templ_file, constants)
        return 0
    except BaseException, e:
        print >>sys.stderr, "%s: %s." % (type(e), str(e))
        return 1
    except:
        print >>sys.stderr, "Unexpected error occurred"
        return 1


if len(sys.argv) != 4:
    usage()
lang = sys.argv[1]
templ_file = sys.argv[2]
config_file = sys.argv[3]

if lang == "--cxx":
    import engine.cxx as templ_engine
elif lang == "--objc":
    import engine.objc as templ_engine
elif lang == "--java":
    import engine.java as templ_engine
elif lang == "--raw":
    import engine.default as templ_engine
else:
    usage()

config_module_dir = os.path.dirname(config_file)
config_module_name = re.sub(r"(.*)\.py\s*$", "\\1", os.path.basename(config_file))
site.addsitedir(config_module_dir)
config_module = __import__(config_module_name)

sys.exit(generate(templ_file))
