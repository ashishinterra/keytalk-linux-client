#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Executes all unit tests in the current directory
To run tests from a specific test module run
python test_module_name
To run specific test case:
python -m unittest test_module_name.TestClass.test_method
"""

import sys
import os
import re
import unittest


def testAll():
    myDir = os.path.abspath(os.path.dirname(sys.argv[0]))
    myFiles = os.listdir(myDir)
    myUnitTestFileRegex = re.compile(r"test\.py$", re.IGNORECASE)
    myUnitTestFiles = list(filter(myUnitTestFileRegex.search, myFiles))
    myUnitTestModuleNames = [os.path.splitext(f)[0] for f in myUnitTestFiles]
    myUnitTestModules = list(map(__import__, myUnitTestModuleNames))
    return unittest.TestSuite(list(map(unittest.defaultTestLoader.loadTestsFromModule,
                                       myUnitTestModules)))


if __name__ == '__main__':
    unittest.main(defaultTest='testAll')
