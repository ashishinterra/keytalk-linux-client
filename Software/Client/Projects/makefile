#################################################
# RHEL/CentOS 6 & 7 support added on 8 May 2018 #
#################################################

PRJS=ReseptConsoleClient ReseptConfigManager ReseptConfigTool ReseptPrGenerator ReseptHwsigDump ReseptPythonClient ReseptInstaller/ConfigUpdater ReseptInstaller/linux/bin ReseptInstaller/linux/apache_remote_deployment
MAKE_FLAGS=-s

# use bash i.o. default sh shell to make use of some nifty features which are not present in sh on some platforms
SHELL=/bin/bash

all: release

check_devenv:
	@set -e; \
	if [ `uname` != "Linux" ]; then \
		echo "Linux platform required to build KeyTalk Linux client" 1>&2 ; \
		exit 1; \
	fi; \
	if [[ ! -f /etc/debian_version && ! -f /etc/centos-release && ! -f /etc/redhat-release ]]; then \
		echo "Linux Debian platform required to build KeyTalk Linux client" 1>&2 ; \
		exit 1; \
	fi; \
	if [ -f /etc/debian_version ]; then \
		if dpkg -s libcurl-dev > /dev/null 2>&1 ; then \
			echo "Curl developent package should NOT be installed to avoid possible build conflicts with local curl libs" 1>&2; \
			exit 1; \
		fi; \
	fi; 
	

release debug: check_devenv
	@set -e;			\
	for dir in $(PRJS); do		\
		echo " --- Building $$dir";	\
		make -C $$dir $(MAKE_FLAGS) $@;		\
	done;

clean: check_devenv
	@set -e;			\
	for dir in $(PRJS); do		\
		echo "--- Cleaning $$dir";	\
		make -C $$dir $(MAKE_FLAGS) clean;		\
	done;
	@rm -rf ./Export/*

install: check_devenv
	@set -e;			\
	for dir in $(PRJS); do \
		echo "--- Installing $$dir";	\
		make -C $$dir $(MAKE_FLAGS) install;		\
done;
