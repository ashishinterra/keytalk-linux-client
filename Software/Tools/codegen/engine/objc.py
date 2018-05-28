#!/usr/bin/env python
# -*- coding: utf-8 -*-

import cxx, default

def escape_string(x):
    """
    Return a string as a raw constant.
    
    This is the default serialization because sometimes we want to be able
    to use strings inside the C preprocessor, and then we don't need the
    ObjC runtime instantiation. So if you do need NSStrings, prepend the
    constant with @ (like @${MY_STRING}).
    """
    return cxx.escape_string(x)

def objc_string(x):
    """
    Return a string as an ObjC string literal
    """
    return "@" + escape_string(x)

def escape_int(x):
    return x

def escape_string_array(xs):
    """
    Output as XCode4 array literal ( @[ @"foo", @"bar" ] )
    """
    return "@[ %s ]" % ', '.join(map(objc_string, xs))

def escape_int_array(x):
    return reduce(lambda cur, next: "%d, %d" % (cur, next), x)

def escape(x):
    if default._isString(x):
        return objc_string(x)
    if default._isIntNum(x):
        return escape_int(x)

def escape_dict(xs):
    return default.escape_dict(xs)

def substitute(templ, constants):
    import sys
    default.do_substitute(templ, constants, sys.modules[__name__])
