#!/bin/bash

distro_name=$(lsb_release --id --short)
distro_version=$(lsb_release --release --short)
distro_version_major=$(lsb_release --release --short | egrep -o [0-9]+ | sed -n '1p')
arch=$(uname -m)
#build_platform=$(distro_name)-$(distro_version_major)-$(arch)
if [ ! -d ../../Import -a ! -d ../../Documentation ]; then
     if [ x"${distro_name}" == x"Debian" ]; then
        
        if [ ${distro_version_major} -eq 8 ] ; then
            # KeyTalk is built on Debian 8
            if [ x"${distro_name}" == x"Debian" -a ${distro_version_major} -eq 8 ]; then
                mv ../../Import-debian ../../Import
		mv ../../Documentation-debian ../../Documentation
		/bin/cp -rf ../../Apache_Files_debian/apache_util.py ../../Apache_Files_debian/renew_apache_ssl_cert.py ../../Apache_Files_debian/util.py ./ReseptConsoleClient/
		/bin/cp -rf ../../Apache_Files_debian/ReseptPrGenerator.cpp ./ReseptPrGenerator/
		cp ./librclientcore/rclient/NativeCertStoreFiles/NativeCertStore-debian.cpp ./librclientcore/rclient/
		mv ./librclientcore/rclient/NativeCertStore-debian.cpp librclientcore/rclient/NativeCertStore.cpp
		mv ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk_others ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk
		/bin/cp -f ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk ./ReseptInstaller/linux/
		make clean && make && make install
                mv ../../Import ../../Import-debian
		mv ../../Documentation ../../Documentation-debian
		mv ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk_others
            elif [ x"${distro_name}" == x"Ubuntu" -a x"${distro_version}" == x"16.04" ]; then
                mv ../../Import-debian ../../Import
		mv ../../Documentation-debian ../../Documentation
		/bin/cp -rf ../../Apache_Files_debian/apache_util.py ../../Apache_Files_debian/renew_apache_ssl_cert.py ../../Apache_Files_debian/util.py ./ReseptConsoleClient/
		/bin/cp -rf ../../Apache_Files_debian/ReseptPrGenerator.cpp ./ReseptPrGenerator/
		cp ./librclientcore/rclient/NativeCertStoreFiles/NativeCertStore-debian.cpp ./librclientcore/rclient/
		mv ./librclientcore/rclient/NativeCertStore-debian.cpp librclientcore/rclient/NativeCertStore.cpp
		mv ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk_others ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk
		/bin/cp -f ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk ./ReseptInstaller/linux/
		make clean && make && make install
                mv ../../Import ../../Import-debian
		mv ../../Documentation ../../Documentation-debian
		mv ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk_others
            else
            	echo "Debian 8 or Ubuntu 16.04 (LTS) is required to install KeyTalk but ${distro_name} ${distro_version} found"
            
	    fi
        elif [ ${distro_version_major} -eq 9 ] ; then
            # KeyTalk is built on Debian 9
            if [ x"${distro_name}" == x"Debian" -a ${distro_version_major} -eq 9 ]; then
                mv ../../Import-debian ../../Import
		mv ../../Documentation-debian ../../Documentation
		/bin/cp -rf ../../Apache_Files_debian/apache_util.py ../../Apache_Files_debian/renew_apache_ssl_cert.py ../../Apache_Files_debian/util.py ./ReseptConsoleClient/
		/bin/cp -rf ../../Apache_Files_debian/ReseptPrGenerator.cpp ./ReseptPrGenerator/
		cp ./librclientcore/rclient/NativeCertStoreFiles/NativeCertStore-debian.cpp ./librclientcore/rclient/
		mv ./librclientcore/rclient/NativeCertStore-debian.cpp librclientcore/rclient/NativeCertStore.cpp
		mv ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk_others ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk
		/bin/cp -f ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk ./ReseptInstaller/linux/
		make clean && make && make install
                mv ../../Import ../../Import-debian
		mv ../../Documentation ../../Documentation-debian
		mv ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk_others
            else
            	echo "Debian 9 is required to install KeyTalk but ${distro_name} ${distro_version_major} found"
            
	    fi
        else
            echo "KeyTalk client is built on unsupported version ${distro_version_major} of ${distro_name}"
            
    	fi
    

    elif [ x"${distro_name}" == x"CentOS" -a ${distro_version_major} -eq 7 ]; then
                mv ../../Import-rhel7_centos7 ../../Import
		mv ../../Documentation-rhel_centos ../../Documentation
		/bin/cp -rf ../../Apache_Files_rhel_centos/apache_util.py ../../Apache_Files_rhel_centos/renew_apache_ssl_cert.py ../../Apache_Files_rhel_centos/util.py ./ReseptConsoleClient/
		/bin/cp -rf ../../Apache_Files_rhel_centos/ReseptPrGenerator.cpp ./ReseptPrGenerator/
		cp ./librclientcore/rclient/NativeCertStoreFiles/NativeCertStore-rhel_centos.cpp ./librclientcore/rclient/
		mv ./librclientcore/rclient/NativeCertStore-rhel_centos.cpp librclientcore/rclient/NativeCertStore.cpp
		mv ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk_others ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk
		/bin/cp -f ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk ./ReseptInstaller/linux/
		make clean && make && make install
                mv ../../Import ../../Import-rhel7_centos7
		mv ../../Documentation ../../Documentation-rhel_centos
		mv ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk_others
        
    elif [ x"${distro_name}" == x"RedHatEnterpriseServer" -a ${distro_version_major} -eq 7 ]; then
                mv ../../Import-rhel7_centos7 ../../Import
		mv ../../Documentation-rhel_centos ../../Documentation
		/bin/cp -rf ../../Apache_Files_rhel_centos/apache_util.py ../../Apache_Files_rhel_centos/renew_apache_ssl_cert.py ../../Apache_Files_rhel_centos/util.py ./ReseptConsoleClient/
		/bin/cp -rf ../../Apache_Files_rhel_centos/ReseptPrGenerator.cpp ./ReseptPrGenerator/
		cp ./librclientcore/rclient/NativeCertStoreFiles/NativeCertStore-rhel_centos.cpp ./librclientcore/rclient/
		mv ./librclientcore/rclient/NativeCertStore-rhel_centos.cpp librclientcore/rclient/NativeCertStore.cpp
		mv ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk_others ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk
		/bin/cp -f ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk ./ReseptInstaller/linux/
		make clean && make && make install
                mv ../../Import ../../Import-rhel7_centos7
		mv ../../Documentation ../../Documentation-rhel_centos
		mv ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk_others

    elif [ x"${distro_name}" == x"CentOS" -a ${distro_version_major} -eq 6 ]; then
		mv ../../Import-rhel6_centos6 ../../Import
		mv ../../Documentation-rhel_centos ../../Documentation
		/bin/cp -rf ../../Apache_Files_rhel_centos/apache_util.py ../../Apache_Files_rhel_centos/renew_apache_ssl_cert.py ../../Apache_Files_rhel_centos/util.py ./ReseptConsoleClient/
		/bin/cp -rf ../../Apache_Files_rhel_centos/ReseptPrGenerator.cpp ./ReseptPrGenerator/
		cp ./librclientcore/rclient/NativeCertStoreFiles/NativeCertStore-rhel_centos.cpp ./librclientcore/rclient/
		mv ./librclientcore/rclient/NativeCertStore-rhel_centos.cpp librclientcore/rclient/NativeCertStore.cpp
		mv ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk_rhel_centos_6 ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk
		/bin/cp -f ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk ./ReseptInstaller/linux/	
		make clean && make && make install
                mv ../../Import ../../Import-rhel6_centos6
		mv ../../Documentation ../../Documentation-rhel_centos
		mv ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk_rhel_centos_6

    elif [ x"${distro_name}" == x"RedHatEnterpriseServer" -a ${distro_version_major} -eq 6 ]; then
		mv ../../Import-rhel6_centos6 ../../Import
		mv ../../Documentation-rhel_centos ../../Documentation
		/bin/cp -rf ../../Apache_Files_rhel_centos/apache_util.py ../../Apache_Files_rhel_centos/renew_apache_ssl_cert.py ../../Apache_Files_rhel_centos/util.py ./ReseptConsoleClient/
		/bin/cp -rf ../../Apache_Files_rhel_centos/ReseptPrGenerator.cpp ./ReseptPrGenerator/
		cp ./librclientcore/rclient/NativeCertStoreFiles/NativeCertStore-rhel_centos.cpp ./librclientcore/rclient/
		mv ./librclientcore/rclient/NativeCertStore-rhel_centos.cpp librclientcore/rclient/NativeCertStore.cpp
		mv ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk_rhel_centos_6 ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk
		/bin/cp -f ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk ./ReseptInstaller/linux/	
		make clean && make && make install
                mv ../../Import ../../Import-rhel6_centos6
		mv ../../Documentation ../../Documentation-rhel_centos
		mv ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk ./ReseptInstaller/linux/apache_cron/etc_cron.d_keytalk_rhel_centos_6

    else
        echo "KeyTalk client is built on unsupported version ${distro_version_major} of ${distro_name}"
        
    fi
else
make clean && make && make install
fi
