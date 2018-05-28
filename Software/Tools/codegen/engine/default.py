#!/usr/bin/env python
# -*- coding: utf-8 -*-

import types
from string import Template

# 
# Type inspection
#
def _isString(val):
    return isinstance(val, types.StringType) or isinstance(val, types.UnicodeType)

def _isIntNum(val):
    return isinstance(val, types.IntType) or isinstance(val, types.LongType)
    
def _isArray(val):
    return isinstance(val, types.TupleType) or isinstance(val, types.ListType)
    
def _isDict(val):
    return isinstance(val, types.DictionaryType)

#
# Typed value formatters
#
def escape_string(x):
    return x
	
def escape_string_for_string_array(x):
    return '"%s"' % x

def escape_int(x):
    return x

def escape_string_array(x):
    return "{%s}" % ', '.join(map(escape_string_for_string_array, x))

def escape_int_array(x):
    return reduce(lambda cur, next: "%d, %d" % (cur, next), x)

def escape_dict(value):
    # sort by value so the resulted enum is easier to read
    lst = value.items()
    lst.sort(cmp = lambda x,y: cmp(x[1], y[1]))

    return ', '.join(["%s = %d" % (key, val) for (key, val) in lst])

def escape(value, module):
    if _isString(value):
        return module.escape_string(value)
        
    if _isIntNum(value):
        return module.escape_int(value)
        
    if _isArray(value) and len(value) > 0:
        if _isString(value[0]):
            return module.escape_string_array(value)
        if _isIntNum(value[0]):
            return module.escape_int_array(value)

    if _isDict(value) and len(value) > 0:
        return module.escape_dict(value)

def placeholders(s):
    import re

    return [m.group(1) for m in re.finditer('[^\$]\$\{(\w+)\}', s)]

#
# Public API
#    
def do_substitute(templ_file, constants, module):
    substs = {}
    
    for constant in constants:
    
        name = constant['name']
        value = constant['value']

        substs[name] = escape(value, module)

    templ_contents = open(templ_file).read()

    # Check if we have values for all placeholders (this protects against renames/typoes)
    open_placeholders = set(placeholders(templ_contents))
    available_values = set([constant['name'] for constant in constants])
    unfilled_placeholders = open_placeholders - available_values
    if unfilled_placeholders:
        raise Exception('%s: no values for the following variables: %s' % (templ_file, unfilled_placeholders))

    # Do substitution
    result = Template(templ_contents).safe_substitute(substs)
    print result
            
def substitute(templ, constants):
    import sys
    do_substitute(templ, constants, sys.modules[__name__])
