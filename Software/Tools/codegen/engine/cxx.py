#!/usr/bin/env python
# -*- coding: utf-8 -*-

import default

def escape_string(x):
    return '\"' + x.replace('\\', '\\\\').replace('"', '\\"') + '\"'

def escape_int(x):
    return x

def escape_string_array(x):
    return "{%s}" % ', '.join(map(escape_string, x))

def escape_int_array(x):
    return reduce(lambda cur, next: "%d, %d" % (cur, next), x)
    
def escape_dict(xs):
    return default.escape_dict(xs)

def substitute(templ, constants):
    import sys
    default.do_substitute(templ, constants, sys.modules[__name__])
