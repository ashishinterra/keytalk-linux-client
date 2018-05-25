#!/bin/bash

###############################################################################################################################
# Installation script for KeyTalk Linux client
###############################################################################################################################

set -o errexit
set -o nounset

VERBOSE=true
IS_CERT_RENEWAL_PREREQUISITES_OK=false

function usage()
{
    echo "Usage: $0 remove            uninstall KeyTalk"
    echo "Usage: $0                   install KeyTalk client without customizing."
    exit 2
}

function initialize()
{
    if [ "$(id -u)" != "0" ]; then
        echo "KeyTalk installation script must be run as root" 1>&2
        exit 1
    fi
}

function v_print()
{
    if ${VERBOSE} ; then
        echo "$1"
    fi
}

function need_unintstall_existing_keytalk()
{
    if [ -f /etc/keytalk/version ]; then
        if grep  -q 'version\=4\.' /etc/keytalk/version ; then
            return 0
        fi
    fi
    return 1
}

function uninstall_keytalk_with_self_installer()
{
    bash .uninstaller
}

function uninstall_keytalk()
{
    if [ -x "/usr/local/bin/keytalk/uninstall_keytalk" ] ; then
        /usr/local/bin/keytalk/uninstall_keytalk
    else
       uninstall_keytalk_with_self_installer
    fi
}

function install_keytalk()
{
    local platform=$(get_platform_info)

    echo "Installing KeyTalk on ${platform}..."

    mkdir -p /usr/local/bin/keytalk
    cp ktclient ktconfig ktconfupdater ktconfigtool ktprgen hwutils /usr/local/bin/keytalk/
    cp .uninstaller /usr/local/bin/keytalk/uninstall_keytalk
    chmod +x /usr/local/bin/keytalk/uninstall_keytalk
    # set setuid bit on ktconfig to let it alter /etc/keytalk
    chown root /usr/local/bin/keytalk/ktconfig
    chmod u+s /usr/local/bin/keytalk/ktconfig
    # set setuid bit on hwutils to let it read system serial
    chown root /usr/local/bin/keytalk/hwutils
    chmod u+s /usr/local/bin/keytalk/hwutils

    mkdir -p /usr/local/lib/keytalk
    cp libtalogger.so /usr/local/lib/keytalk/

    mkdir -p /etc/keytalk
    cp resept.ini apache.ini version devstage cr.conf /etc/keytalk/
    /usr/local/bin/keytalk/ktconfupdater --set-install-dir /usr/local/bin/keytalk

    if ${IS_CERT_RENEWAL_PREREQUISITES_OK} ; then
        echo "    Installing KeyTalk Apache certificate renewal..."
        cp renew_apache_ssl_cert util.py apache_util.py /usr/local/bin/keytalk/
        cp etc_cron.d_keytalk /etc/cron.d/keytalk
        chmod 644 /etc/cron.d/keytalk

        mkdir -p /etc/keytalk
        cp apache.ini /etc/keytalk/

        mkdir -p /usr/share/doc/keytalk/
        cp KeyTalk_LinuxClient_for_Apache.txt /usr/share/doc/keytalk/
        cp KeyTalk_LinuxClient_for_Apache.pdf /usr/share/doc/keytalk/
    fi


    echo "Installation complete. Please customize KeyTalk by calling /usr/local/bin/keytalk/ktconfig --rccd-path <rccd-url>"
}

# Check if an executable can be found in the path
function has_executable()
{
    if which "$1" > /dev/null && 2>&1 && [ -x "$(which $1)" ]; then
        return 0
    else
        return 1
    fi
}

# Edited on May 07, 2018
# Supported Linux distros: Ubuntu 16.04 (LTS), Debian 8 and Debian 9, Rhel 6, CentOS 6, Rhel 7, CentOS 7
function check_platform_compatibility()
{
    local os=$(uname)
    if [ x"${os}" != x"Linux" ]; then
        echo "KeyTalk Linux client requires Linux to install, but '${os}' detected"
        return 1
    fi

    local arch=$(uname -m)
    if [ x"${arch}" != x"x86_64" ]; then
        echo "KeyTalk Linux client requires 64-bit Linux to install"
        return 1
    fi

    local PKG_MANAGER=$( command -v yum || command -v apt-get ) || (echo "Neither yum nor apt-get found" && return 1)

    if ! has_executable lsb_release ; then
	if [ -f /etc/debian_version ]; then
		apt-get -qq -y update
		apt-get -qq -y install lsb-release
	elif [ -f /etc/centos-release ]; then
		yum -y update
		yum -y install redhat-lsb-core
	elif [ -f /etc/redhat-release ]; then
		yum -y update
		yum -y install redhat-lsb-core
	fi
    fi

    local distro_name=$(lsb_release --id --short)
    local distro_version=$(lsb_release --release --short)
    # 'build-platform' file contains <distro-name>-<distro-version-major>-<distro-arch>
    local build_distro_name=$(cat ./build-platform | cut -d '-' -f 1)
    local build_distro_version_major=$(cat ./build-platform | cut -d '-' -f 2)
    local build_arch=$(cat ./build-platform | cut -d '-' -f 3)
    local distro_version_major=$(echo ${distro_version} | egrep -o [0-9]+ | sed -n '1p')
    if [ x"${arch}" != x"${build_arch}" ]; then
        echo "KeyTalk Linux client requires Linux with ${build_arch} architecture to install"
        return 1
    fi

    # KeyTalk built on Debian 8 can be installed on Debian 8 and on Ubuntu-16.04
    # KeyTalk built on Debian 9 can be installed on Debian 9 only
    # Edited May 07, 2018
    # KeyTalk built on RHEL/CentOS 6,RHEL/CentOS 7 can be installed on RHEL/CentOS 6, RHEL/CentOS 7 respectively
    
    if [ x"${build_distro_name}" == x"Debian" ]; then
        
        if [ ${build_distro_version_major} -eq 8 ] ; then
            # KeyTalk is built on Debian 8
            if [ x"${distro_name}" == x"Debian" -a ${distro_version_major} -eq 8 ]; then
                return 0 # ok
            elif [ x"${distro_name}" == x"Ubuntu" -a x"${distro_version}" == x"16.04" ]; then
                return 0 # ok
            else
            	echo "Debian 8 or Ubuntu 16.04 (LTS) is required to install KeyTalk but ${distro_name} ${distro_version} found"
            	return 1
	    fi
        elif [ ${build_distro_version_major} -eq 9 ] ; then
            # KeyTalk is built on Debian 9
            if [ x"${distro_name}" == x"Debian" -a ${distro_version_major} -eq 9 ]; then
                return 0 # ok
            else
            echo "Debian 9 is required to install KeyTalk but ${distro_name} ${distro_version} found"
            return 1
	    fi
        else
            echo "KeyTalk client is built on unsupported version ${build_distro_version_major} of ${build_distro_name}"
            return 1
    	fi
    

    elif [ x"${build_distro_name}" == x"CentOS" -a ${distro_version_major} -eq 7 ]; then
                return 0 # ok
        
    elif [ x"${build_distro_name}" == x"RedHatEnterpriseServer" -a ${distro_version_major} -eq 7 ]; then
                return 0 # ok

    elif [ x"${build_distro_name}" == x"CentOS" -a ${distro_version_major} -eq 6 ]; then
		update-ca-trust enable 
                return 0 # ok
        
    elif [ x"${build_distro_name}" == x"RedHatEnterpriseServer" -a ${distro_version_major} -eq 6 ]; then
		update-ca-trust enable
                return 0 # ok

    else
        echo "KeyTalk client is built on unsupported version ${build_distro_version_major} of ${build_distro_name}"
        return 1
    fi
}

function install_apache_cert_renewal_prerequisities()
{
    # Apache version should be 2.2 - 2.4
	
	if [ -f /etc/debian_version ]; then
	    if ! has_executable apache2 ; then
		echo "WARNING: KeyTalk Apache SSL certificate renewal feature will not be installed because Apache 2 is required by KeyTalk client certificate renewal."
		return 0
	    fi

	    local apache_version=( $(apache2 -v | grep "Server version" | egrep -o [0-9]+) )

	    if [ ${apache_version[0]} -ne 2 ] ; then
		echo "WARNING: KeyTalk Apache SSL certificate renewal feature will not be installed because Apache ${apache_version} is not supported by KeyTalk client certificate renewal. Apache 2.2-2.4 is required."
		return 0
	    fi
	    if [ ${apache_version[1]} -ne 2 -a ${apache_version[1]} -ne 4 ] ; then
		echo "WARNING: KeyTalk Apache SSL certificate renewal feature will not be installed because Apache ${apache_version} is not supported by KeyTalk client certificate renewal. Apache 2.2-2.4 is required."
		return 0
	    fi

	elif [ -f /etc/centos-release ]; then
	    if ! has_executable httpd ; then
		echo "WARNING: KeyTalk Apache SSL certificate renewal feature will not be installed because Apache 2 is required by KeyTalk client certificate renewal."
		return 0
	    fi

	    local apache_version=( $(httpd -v | grep "Server version" | egrep -o [0-9]+) )

	    if [ ${apache_version[0]} -ne 2 ] ; then
		echo "WARNING: KeyTalk Apache SSL certificate renewal feature will not be installed because Apache ${apache_version} is not supported by KeyTalk client certificate renewal. Apache 2.2-2.4 is required."
		return 0
	    fi
	    if [ ${apache_version[1]} -ne 2 -a ${apache_version[1]} -ne 4 ] ; then
		echo "WARNING: KeyTalk Apache SSL certificate renewal feature will not be installed because Apache ${apache_version} is not supported by KeyTalk client certificate renewal. Apache 2.2-2.4 is required."
		return 0
	    fi

	elif [ -f /etc/redhat-release ]; then
	    if ! has_executable httpd ; then
		echo "WARNING: KeyTalk Apache SSL certificate renewal feature will not be installed because Apache 2 is required by KeyTalk client certificate renewal."
		return 0
	    fi

	    local apache_version=( $(httpd -v | grep "Server version" | egrep -o [0-9]+) )

	    if [ ${apache_version[0]} -ne 2 ] ; then
		echo "WARNING: KeyTalk Apache SSL certificate renewal feature will not be installed because Apache ${apache_version} is not supported by KeyTalk client certificate renewal. Apache 2.2-2.4 is required."
		return 0
	    fi
	    if [ ${apache_version[1]} -ne 2 -a ${apache_version[1]} -ne 4 ] ; then
		echo "WARNING: KeyTalk Apache SSL certificate renewal feature will not be installed because Apache ${apache_version} is not supported by KeyTalk client certificate renewal. Apache 2.2-2.4 is required."
		return 0
	    fi
	fi

	
    if [ -f /etc/debian_version ]; then
	apt-get -qq -y install cron python-lxml python-openssl
	IS_CERT_RENEWAL_PREREQUISITES_OK=true
	return 0
    elif [ -f /etc/centos-release ]; then
	yum -y install cronie python-lxml pyOpenSSL
	IS_CERT_RENEWAL_PREREQUISITES_OK=true
	return 0		
	
    elif [ -f /etc/redhat-release ]; then
	yum -y install cronie python-lxml pyOpenSSL
	IS_CERT_RENEWAL_PREREQUISITES_OK=true
	return 0		
    fi

}

function get_platform_info()
{
    local platform_hw_name=$(uname -m)
    local platform_description=$(lsb_release --description --short)
    echo ${platform_description} ${platform_hw_name}
}

# Check and install prerequisites
function install_prerequisites()
{
    check_platform_compatibility

    v_print "Installing prerequisites for KeyTalk on $(get_platform_info)"
	if [ -f /etc/debian_version ]; then
		apt-get -qq -y update
		apt-get -qq -y install ca-certificates hdparm psmisc
		install_apache_cert_renewal_prerequisities
	elif  [ -f /etc/centos-release ]; then
		yum -y update
		yum -y install ca-certificates hdparm psmisc
		install_apache_cert_renewal_prerequisities
	
	elif  [ -f /etc/redhat-release ]; then
		yum -y update
		yum -y install ca-certificates hdparm psmisc
		install_apache_cert_renewal_prerequisities
	fi
}


function on_keytalk_installation_error()
{
    echo "KeyTalk installation failed, rolling back"
    uninstall_keytalk_with_self_installer
    exit 1
}

#
# Entry point
#

cd `dirname "$0"` # Go to current script directory

initialize

if [ $# -eq 0 ]; then
    if need_unintstall_existing_keytalk; then
        echo "Existing KeyTalk installation is not compatible with the current. Uninstalling it first ..."
        uninstall_keytalk
    fi
    install_prerequisites
    trap on_keytalk_installation_error EXIT
    install_keytalk
    trap "" EXIT
elif [ $# -eq 1 -a x"$1" == x"remove" ]; then
    uninstall_keytalk
else
    usage
fi
