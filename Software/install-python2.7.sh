#!/bin/bash

#############################################################################################################################
# Installation Script for installing Python 2.7.6 and making it as the default Python
#############################################################################################################################


# Install extra libraries before installing Python
	yum -y groupinstall "Development tools"
	yum -y install zlib-devel bzip2-devel openssl-devel ncurses-devel sqlite-devel


# Download, compile and install Python
	cd /opt
	wget --no-check-certificate https://www.python.org/ftp/python/2.7.6/Python-2.7.6.tar.xz
	tar xf Python-2.7.6.tar.xz
	cd Python-2.7.6
	./configure --prefix=/usr/local
	make && make altinstall


# Create a symbolic link to /usr/local/bin
	ln -s /usr/local/bin/python2.7 /usr/local/bin/python


# Enter the path of Python 2.7.6 in $PATH
	echo 'pathmunge /usr/local/bin' > /etc/profile.d/ree.sh
	chmod +x /etc/profile.d/ree.sh
	. /etc/profile


# Installing Setup tools of Python
	wget --no-check-certificate https://bootstrap.pypa.io/ez_setup.py
	sudo /usr/local/bin/python2.7 ez_setup.py
	sudo /usr/local/bin/easy_install-2.7 pip
	sudo /usr/local/bin/easy_install-2.7 requests
	sudo /usr/local/bin/easy_install-2.7 psutil


# Creating Symbolic link of easy_install and pip
	cd /usr/local/bin
	mv pip pip2.6
	mv easy_install easy_install-2.6
	ln -s pip2.7 pip
	ln -s easy_install-2.7 easy_install


#############################################################################################################################
# Python 2.7.6 is installed and will now act as the default Python
#############################################################################################################################
