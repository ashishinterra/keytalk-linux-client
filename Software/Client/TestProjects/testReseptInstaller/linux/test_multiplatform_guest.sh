#!/bin/bash

# Script executed by guest buildserver
# Dependency: KeyTalk Linux client build server output
#             BSVR_ADDRESS environment variable should be set

set -e
set -u
set -x

INITIAL_PWD=`pwd`
INSTALLER_DIR=${INITIAL_PWD}/installer
TEST_SCRIPTS_DIR=${INITIAL_PWD}/test_scripts
RCCDS_DIR=${INITIAL_PWD}/rccds
CAS_DIR=${INITIAL_PWD}/cas
LOG_DIR=${INITIAL_PWD}/log
CLIENT_VERSION="unknown-version"

function download_test_data()
{
    echo "Downloading KeyTalk test data"
    rm -rf ${INSTALLER_DIR}/ ${RCCDS_DIR}/ ${CAS_DIR}/ ${TEST_SCRIPTS_DIR}/
    mkdir ${INSTALLER_DIR} ${RCCDS_DIR} ${CAS_DIR} ${TEST_SCRIPTS_DIR}

    scp root@${BSVR_ADDRESS}:/builds/Software/Client/version /tmp/
    CLIENT_VERSION=$(cut -d '=' -f 2 /tmp/version)

    scp root@${BSVR_ADDRESS}:/builds/Software/Client/Projects/Export/KeyTalkClient-${CLIENT_VERSION}-*.tgz ${INSTALLER_DIR}/
    scp root@${BSVR_ADDRESS}:/builds/Software/WebUI.Server/Projects/Export/*.rccd ${RCCDS_DIR}/
    scp root@${BSVR_ADDRESS}:/builds/Software/CertKeys/CommunicationAndSigning/*.pem ${CAS_DIR}/
    scp -r root@${BSVR_ADDRESS}:/builds/Software/Client/TestProjects/testReseptInstaller/linux/* ${TEST_SCRIPTS_DIR}/
}

function start_test()
{
    echo "Starting tests"

    pushd ${TEST_SCRIPTS_DIR} > /dev/null

    # make sure 'tee' does not mask failed tests
    set -o pipefail

    ./test_install_bin.sh ${CLIENT_VERSION} ${INSTALLER_DIR} ${RCCDS_DIR} ${CAS_DIR} ${LOG_DIR} 2>&1 | tee -a ${LOG_DIR}/test.log

    popd > /dev/null
}

#
# Here we go!
#

download_test_data
start_test
