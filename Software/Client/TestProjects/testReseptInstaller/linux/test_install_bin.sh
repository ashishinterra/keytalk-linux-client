#!/bin/bash

set -e
set -u
set -x

# usage: ./$0 [client-version installer-dir rccds-dir cas-dir logs-backup-dir | test-name ]

. ./test_install_functions.inc.sh

CLIENT_VERSION=
INSTALLER_DIR=../../../Projects/Export/
RCCDS_DIR=
LOGS_BACKUP_DIR=/var/log/keytalk
TEST_NAME=
CAS_DIR=../../../../CertKeys/CommunicationAndSigning

if [ $# -eq 0 ]; then
    CLIENT_VERSION=$(cut -d '=' -f 2 ../../../version)
elif [ $# -eq 1 ]; then
    CLIENT_VERSION=$(cut -d '=' -f 2 ../../../version)
    TEST_NAME="$1"
elif [ $# -eq 5 ]; then
    CLIENT_VERSION="$1"
    INSTALLER_DIR="$2"
    RCCDS_DIR="$3"
    CAS_DIR="$4"
    LOGS_BACKUP_DIR="$5"
fi

KT_SVR_WEBUI_IP="192.168.33.111"
INSTALLER_TGZ_PATH=${INSTALLER_DIR}/KeyTalkClient-${CLIENT_VERSION}-$(get_client_platform_file_suffix).tgz
SUCCEEDED_TESTS=0
PID=$$
INSTALLER_TEMP_DIR=/tmp/installer.bin_${PID}
PACKAGE_DIR_NAME=keytalkclient-${CLIENT_VERSION}


# usage: _test_install_customize $rccd1_path [$rccd2_path]
# rccdX_path is either URL or path to RCCD file with filename part formatted as settings.<ProviderName>.<rccd-type>[.<content-version>].rccd
function _test_install_customize()
{
    local rccd1_path="$1"
    local rccd1_provider=''
    local rccd1_type=''
    local rccd1_content_version=''
    local rccd2_path=''
    local rccd2_provider=''
    local rccd2_type=''
    local rccd2_content_version=''

    # Parse provider name and RCCD type from RCCD filename
    parse_rccd_props ${rccd1_path} rccd1_provider rccd1_type rccd1_content_version
    if [ $# -eq 2 ] ; then
        rccd2_path="$2"
        parse_rccd_props ${rccd2_path} rccd2_provider rccd2_type rccd2_content_version
    fi

    # Install and customize
    pushd ${INSTALLER_TEMP_DIR}/${PACKAGE_DIR_NAME} >/dev/null
    ./install.sh
    /usr/local/bin/keytalk/ktconfig --rccd-path ${rccd1_path}
    if [ -n "${rccd2_path}" ]; then
        echo "Customizing KeyTalk with RCCD from ${rccd2_path}"
        if [ "${rccd2_content_version}" -ge "${rccd1_content_version}" ]; then
            /usr/local/bin/keytalk/ktconfig --rccd-path ${rccd2_path}
        else
            # Downgrade should fail when run with default arguments
            if /usr/local/bin/keytalk/ktconfig --rccd-path ${rccd2_path} ; then
                echo "Customization failed. RCCD downgrade from content version ${rccd1_content_version} to ${rccd2_content_version} expected to fail but it was not" 2>&1
                return 1
            fi
            # Downgrade should now succeed when run supplying special arguments
            /usr/local/bin/keytalk/ktconfig --allow-downgrade --rccd-path ${rccd2_path}
        fi
    fi
    popd >/dev/null


    # Checks
    verify_installation
    verify_customization ${rccd1_path} ${rccd1_provider} ${rccd1_type} ${rccd2_path} ${rccd2_provider} ${rccd2_type}
    verify_provisioning ${rccd1_provider} ${rccd2_provider}
    verify_pr_generation
}


function _test_install()
{
    pushd ${INSTALLER_TEMP_DIR}/${PACKAGE_DIR_NAME} >/dev/null
    ./install.sh
    popd >/dev/null

    verify_installation

    for f in ~/.keytalk/user.ini /etc/keytalk/master.ini
    do
        if [ -f ${f} ] ; then
            echo "Installation failed. ${f} is not expected because no customization performed" 2>&1
            return 1
        fi
    done

    for d in /etc/keytalk/DemoProvider ~/.keytalk/keystore /etc/keytalk/.keystore
    do
        if [ -d ${d} ] ; then
            echo "Installation failed. ${d} is not expected because no customization performed" 2>&1
            return 1
        fi
    done

    if /usr/local/bin/keytalk/ktclient --service CUST_ANO_INTERNAL_TESTUI --user DemoUser ; then
        echo "Installation failed. Installed KeyTalk client should not authenticate because it is not customized" 2>&1
        return 1
    fi
}

function _test_uninstall()
{
    pushd ${INSTALLER_TEMP_DIR}/${PACKAGE_DIR_NAME} >/dev/null
    ./install.sh remove
    popd >/dev/null

    for f in ~/.keytalk ${INSTALLATION_FILES_REQUIRED}
    do
        if [ -f ${f} ] ; then
            echo "Uninstallation failed. ${f} still exists" 2>&1
            return 1
        fi
    done

    for d in ~/.keytalk ${INSTALLATION_DIRS_REQUIRED}
    do
        if [ -d ${d} ] ; then
            echo "Uninstallation failed. ${d} directory still exists" 2>&1
            return 1
        fi
    done
}

function _cleanup_keytalk_installation()
{
    # Since all KeyTalk logs are removed during uninstallation tests, we backup logs to assist future troubleshooting
    cleanup_keytalk_installation ${LOGS_BACKUP_DIR}
}

#
# Test global setup and teardown routines
#

function setup_test()
{
    echo "Setting up test"
    teardown_test

    mkdir -p ${INSTALLER_TEMP_DIR}
    tar -xzf ${INSTALLER_TGZ_PATH} -C ${INSTALLER_TEMP_DIR}

    # generate or grab test RCCDs
    if [ "${RCCDS_DIR}" ]; then
        cp -f "${RCCDS_DIR}"/*.rccd ${INSTALLER_TEMP_DIR}/${PACKAGE_DIR_NAME}/
    else
        pushd ../../../../WebUI.Server/Projects/ > /dev/null
        ./create_test_rccds.py ${KT_SVR_WEBUI_IP}
        cp -f Export/*.rccd ${INSTALLER_TEMP_DIR}/${PACKAGE_DIR_NAME}/
        popd > /dev/null
    fi
}

function teardown_test()
{
    _cleanup_keytalk_installation
    rm -rf ${INSTALLER_TEMP_DIR}
}

#
# Test cases
#


# Test installation wihout customization and then uninstallation
function test_install()
{
    echo "--- Running test_install ..."
    _cleanup_keytalk_installation
    _test_install
    _test_uninstall
    _cleanup_keytalk_installation
    SUCCEEDED_TESTS=$((SUCCEEDED_TESTS+1))
}


# Test installation, then customization from URL with HTTP redirection with user RCCDv1 and then uninstallation
function test_install_customize_from_url_with_user_rccd_v1()
{
    echo "--- Running test_install_customize_from_url_with_user_rccd_v1 ..."
    _cleanup_keytalk_installation
    _test_install_customize "http://r4webdemo.gotdns.com/rccds/v1/settings.DemoProvider.user.redirect.me.rccd"
    _test_uninstall
    _cleanup_keytalk_installation
    SUCCEEDED_TESTS=$((SUCCEEDED_TESTS+1))
}

# Test installation, then customization from URL with HTTP redirection with user RCCDv2 and then uninstallation
function test_install_customize_from_url_with_user_rccd_v2()
{
    echo "--- Running test_install_customize_from_url_with_user_rccd_v2 ..."
    _cleanup_keytalk_installation
    _test_install_customize "http://r4webdemo.gotdns.com/rccds/v2/settings.DemoProvider.user.redirect.me.rccd"
    _test_uninstall
    _cleanup_keytalk_installation
    SUCCEEDED_TESTS=$((SUCCEEDED_TESTS+1))
}


# Test installation, then customization from file with user RCCD and then uninstallation
function test_install_customize_from_file_with_user_rccd()
{
    echo "--- Running test_install_customize_from_file_with_user_rccd ..."
    _cleanup_keytalk_installation
    _test_install_customize "settings.DemoProvider.user.rccd"
    _test_uninstall
    _cleanup_keytalk_installation
    SUCCEEDED_TESTS=$((SUCCEEDED_TESTS+1))
}

# Test installation, then customization from file with admin RCCD and then uninstallation
function test_install_customize_from_file_with_admin_rccd()
{
    echo "--- Running test_install_customize_from_file_with_admin_rccd ..."
    _cleanup_keytalk_installation
    _test_install_customize "settings.DemoProvider.admin.rccd"
    _test_uninstall
    _cleanup_keytalk_installation
    SUCCEEDED_TESTS=$((SUCCEEDED_TESTS+1))
}


# Test installation, then customization from file with user RCCD, then customization from file with admin RCCD with the same provider and finally uninstallation
function test_install_customize_from_file_with_user_admin_rccds_same_provider()
{
    echo "--- Running test_install_customize_from_file_with_user_admin_rccds_same_provider ..."
    _cleanup_keytalk_installation
    _test_install_customize "settings.DemoProvider.user.rccd" 2 "settings.DemoProvider.admin.rccd"
    _test_uninstall
    _cleanup_keytalk_installation
    SUCCEEDED_TESTS=$((SUCCEEDED_TESTS+1))
}

# Test installation, then customization from file with user RCCD, then customization from file with user RCCD with different provider and finally uninstallation
function test_install_customize_from_file_with_user_rccds_different_providers()
{
    echo "--- Running test_install_customize_from_file_with_user_rccds_different_providers ..."
    _cleanup_keytalk_installation
    _test_install_customize "settings.DemoProvider.user.rccd" 2 "settings.DemoProvider2.user.rccd"
    _test_uninstall
    _cleanup_keytalk_installation
    SUCCEEDED_TESTS=$((SUCCEEDED_TESTS+1))
}

# Test installation, then customization from file with user RCCD, then customization from file with admin RCCD with different provider and finally uninstallation
function test_install_customize_from_file_with_admin_user_rccds_different_providers()
{
    echo "--- Running test_install_customize_from_file_with_admin_user_rccds_different_providers ..."
    _cleanup_keytalk_installation
    _test_install_customize "settings.DemoProvider2.user.rccd" 2 "settings.DemoProvider.admin.rccd"
    _test_uninstall
    _cleanup_keytalk_installation
    SUCCEEDED_TESTS=$((SUCCEEDED_TESTS+1))
}

# Test installation, then customization, then customization with a lower content version (downgrade) in non-interactive mode and finally uninstallation
function test_downgrade_rccd_content_version_non_interactively()
{
    echo "--- Running test_downgrade_rccd_content_version_non_interactively ..."
    _cleanup_keytalk_installation
    _test_install_customize "settings.DemoProvider.user.12.rccd" 2 "settings.DemoProvider.user.11.rccd"
    _test_uninstall
    _cleanup_keytalk_installation
    SUCCEEDED_TESTS=$((SUCCEEDED_TESTS+1))
}

# Install, customize, test apache SSL certificate renewal feature and finally uninstall
function test_apache_ssl_cert_renewal()
{
    echo "--- Running test_apache_ssl_cert_renewal ..."
    _cleanup_keytalk_installation
    _test_install_customize "http://r4webdemo.gotdns.com/rccds/v2/settings.DemoProvider.ApacheSslTest.user.rccd"
    configure_apache ${CAS_DIR}
    pushd apache/ > /dev/null
    ./run_tests.py
    popd > /dev/null
    _test_uninstall
    _cleanup_keytalk_installation
    SUCCEEDED_TESTS=$((SUCCEEDED_TESTS+1))
}




#
# Here we go
#

trap 'teardown_test' EXIT

setup_test

if [ -z ${TEST_NAME} ] ; then
    # invoked without arguments (default), run all tests
    test_install
    test_install_customize_from_url_with_user_rccd_v1
    test_install_customize_from_url_with_user_rccd_v2
    test_install_customize_from_file_with_user_rccd
    test_install_customize_from_file_with_admin_rccd
    test_install_customize_from_file_with_user_admin_rccds_same_provider
    test_install_customize_from_file_with_user_rccds_different_providers
    test_install_customize_from_file_with_admin_user_rccds_different_providers
    test_downgrade_rccd_content_version_non_interactively
    test_apache_ssl_cert_renewal
else
    # run specific test
    eval ${TEST_NAME}
fi

trap '' EXIT
teardown_test

echo "ALL ${SUCCEEDED_TESTS} TESTS SUCCEEDED!"
