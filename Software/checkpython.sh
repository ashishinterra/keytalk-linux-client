#!/bin/bash

# check python scripts

FAILCOUNT=0
MSG_TEMPLATE='{abspath}:{line:3d},{column}: {obj}: [{msg_id}] {msg}'

function check_python_scripts()
{
    local filelist="$1"
    pylint --msg-template="${MSG_TEMPLATE}" ${filelist}
    if [ $? -ne 0 ]; then
        let "FAILCOUNT=FAILCOUNT+1"
    fi
}

function check_python3_scripts()
{
    local filelist="$1"
    local extra_options
    if [ $# -eq 2 ]; then
        extra_options+=" --disable=$2"
    fi
    pylint3 --msg-template="${MSG_TEMPLATE}" ${extra_options} ${filelist}
    if [ $? -ne 0 ]; then
        let "FAILCOUNT=FAILCOUNT+1"
    fi
}

# We check directories one-by-one (as separate projects) otherwise pylint gets confused by the same module names (e.g. common.py) and will not import them again
if [ -f /resept_server_dev ]; then
    check_python3_scripts "Server/Projects/settings/*.py"
    check_python3_scripts "Server/Projects/config/*.py"
    check_python3_scripts "Server/Projects/pykeytalk/*.py"
    check_python3_scripts "Server/TestProjects/testpykeytalk/*.py"
    # workaround pylint's false-positive "[E1101] Module 'ssl' has no 'PROTOCOL_TLSv1_2' member" erroneously reported in python-3.5.2
    check_python3_scripts "Client/Projects/ReseptPythonClient/*.py" "E1101"

    check_python_scripts "WebUI.Server/TestProjects/webuitests/*.py"
fi
if [ -f /resept_linux_client_dev ]; then
    check_python_scripts "Client/Projects/ReseptConsoleClient/*.py"
    check_python_scripts "Client/TestProjects/testReseptInstaller/linux/apache/*.py"
fi

exit $FAILCOUNT
