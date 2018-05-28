#!/usr/bin/env python

from __future__ import with_statement
import sys
import os
import os.path
import re
from subprocess import Popen, PIPE
from difflib import unified_diff

WIN32 = sys.platform == 'win32'

# Configuration
inputDirectories = [os.path.join(x, "Projects") for x in (
    "Common", "SysInfra", "Server", "WebUI.Server", "Client")]
astyleOptions = "--indent-namespaces --keep-one-line-blocks --keep-one-line-statements --indent=spaces=4 --lineend=linux --mode=c --formatted"
testFilePatterns = re.compile(".*\.(c|h|cpp|hpp)$")
if WIN32:
    astyleExecutable = "Import\\astyle\\bin\\win32\\Astyle.exe"
else:
    import platform
    os_, _, _, _, arch, _ = platform.uname()
    astyleExecutable = "Import/astyle/bin/{}-{}/astyle".format(os_.lower(), arch.lower())

ignoredDirs = [os.path.join(*x) for x in (
    # ignore iOS dirs
    ("Client", "Projects", "iOSDemoClient"),
    ("Client", "Projects", "ReseptiOSClient"),
    ("Client", "Projects", "libKeyTalkiOS"),
    ("Client", "Projects", "libKMCrypto"),
    ("Client", "Projects", "Export"),
    # ignore most client app dirs because they contain a lot of Qt generated stuff
    ("Client", "Projects", "ReseptResponseCalculator", "generatedfiles"),
    ("Client", "Projects", "ReseptConfigManager", "generatedfiles"),
    ("Client", "Projects", "ReseptPrGenerator", "generatedfiles"),
    ("Client", "Projects", "ReseptInstaller", "win", "ReseptActiveXClient", "generatedfiles"),
)]

# ignored files (mostly generated)
ignoredFiles = [os.path.join(*x) for x in (
    ("Common", "Projects", "libreseptcommon", "resept", "common.h"),
    ("Server", "Projects", "libmodmysql", "createtables.h"),
    ("Server", "Projects", "librsvrcommon", "create_admin_db_tables.h"),
    ("Server", "Projects", "librsvrcommon", "create_main_db_tables.h"),
    ("Server", "Projects", "kt4verifysettings", "certs.h"),
    ("Client", "Projects", "librclientcore", "rclient", "Version.h"),
    ("Client", "Projects", "librclientappcommon", "rclient", "moc_AddUserDialog.cpp"),
    ("Client", "Projects", "librclientappcommon", "rclient", "moc_AuthenticatePage.cpp"),
    ("Client", "Projects", "librclientappcommon", "rclient", "moc_AuthenticationWizard.cpp"),
    ("Client", "Projects", "librclientappcommon", "rclient", "moc_ChangePasswordDialog.cpp"),
    ("Client", "Projects", "librclientappcommon",
     "rclient", "moc_ChooseProviderServicePage.cpp"),
    ("Client", "Projects", "librclientappcommon", "rclient", "moc_ConfigUsersDialog.cpp"),
    ("Client", "Projects", "librclientappcommon",
     "rclient", "moc_ProxyUserPassAuthDialog.cpp"),
    ("Client", "Projects", "librclientappcommon", "rclient", "moc_WaitDialog.cpp"),
    ("Client", "Projects", "librclientappcommon", "rclient", "moc_ChooseProviderPage.cpp"),
    ("Client", "Projects", "librclientappcommon", "rclient", "moc_ChooseServicePage.cpp"),
    ("Client", "Projects", "librclientappcommon", "rclient", "qrc_RClientAppCommon.cpp"),
    ("Client", "Projects", "librclientappcommon", "rclient", "ui_AddUserDialog.h"),
    ("Client", "Projects", "librclientappcommon", "rclient", "ui_ChangePasswordDialog.h"),
    ("Client", "Projects", "librclientappcommon", "rclient", "ui_ConfigUsersDialog.h"),
    ("Client", "Projects", "librclientappcommon", "rclient", "ui_ProxyUserPassAuthDialog.h"),
    ("Client", "Projects", "librclientappcommon", "rclient", "ui_WaitDialog.h"),
    ("Client", "Projects", "ReseptIeClient", "dlldata.c"),
    ("Client", "Projects", "ReseptIeClient", "resource.h"),
    ("Client", "Projects", "ReseptIeClient", "vc120.h"),
    ("Client", "Projects", "ReseptIeClient", "vc120_i.c"),
    ("Client", "Projects", "ReseptIeClient", "vc120_p.c"),
)]

# Change "$ {" to "${" for template files (formatting template files
# introduces this space, making the template invalid)
templateFixFiles = [os.path.join(*x) for x in (
    ("Common", "Projects", "libreseptcommon", "resept", "common.h.templ"),
)]


def elem_exist(pred, seq):
    for elem in seq:
        if pred(elem):
            return True
    return False


def filter_out_ignored_files(files):
    filtered_files = []
    if WIN32:  # take into a consideration that filenames on Windows are case-insensitive
        for file in files:
            if elem_exist(lambda f: f.upper() == file.upper(), ignoredFiles):
                continue
            if elem_exist(lambda dir: file.upper().startswith(dir.upper()), ignoredDirs):
                continue
            filtered_files.append(file)
    else:
        filtered_files = filter(lambda f: f not in ignoredFiles, files)
        for ignoredDir in ignoredDirs:
            filtered_files = filter(lambda f: not f.startswith(ignoredDir), filtered_files)
    return filtered_files


def apply_template_fix(string):
    return re.sub("\$ {", "${", string)

#
# Here we go
#


files = [os.path.join(dirpath, filename)
         for directory in inputDirectories
         for dirpath, dirnames, dirfiles in os.walk(directory)
         for filename in dirfiles if testFilePatterns.match(filename)]

files = filter_out_ignored_files(files)

if len(sys.argv) == 1:
    print("Checking %d files..." % len(files))
    returnval = 0

    for filename in files:
        with open(filename) as f:
            content = [line.rstrip() for line in f]

        p = Popen('%s %s < "%s"' %
                  (astyleExecutable, astyleOptions, filename), stdout=PIPE, shell=True)
        indentedContent, err = p.communicate()
        if filename in templateFixFiles:
            indentedContent = apply_template_fix(indentedContent)
        indented = [line.rstrip() for line in indentedContent.splitlines()]

        errors = "\n".join(unified_diff(content, indented,
                                        fromfile=filename + " before", tofile=filename + " after"))

        if errors:
            print >> sys.stderr, errors
            print >> sys.stderr, 80 * '='
            returnval = 1

    sys.exit(returnval)

elif len(sys.argv) == 2 and sys.argv[1] == "--fix":
    # fix source files
    for filename in files:
        p = Popen('%s %s -n "%s"' %
                  (astyleExecutable, astyleOptions, filename),
                  shell=True)
        p.communicate()

    # fix template files
    for filename in templateFixFiles:
        with open(filename, 'rb') as f:  # binary, to prevent conversion to platform line endings
            content = f.read()
        fixed_content = apply_template_fix(content)
        if fixed_content != content:
            with open(filename, 'wb') as f:  # binary, to prevent conversion to platform line endings
                f.write(fixed_content)

else:
    print("Usage: %s [--fix]")
    sys.exit(1)
