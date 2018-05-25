#################################################
# RHEL/CentOS 6 & 7 support added on 8 May 2018 #
#################################################
CLIENT_VERSION:=$(shell cut -d '=' -f 2 ../../../../version)
CLIENT_VERSION:=$(CLIENT_VERSION)$(shell grep -m 1 '^Hardware' /proc/cpuinfo | cut -d ':' -f 2 | grep -q BCM2708 && echo '.RaspberryPi')
DISTRO_NAME:=$(shell lsb_release --id --short)
DISTRO_VERSION_MAJOR:=$(shell lsb_release --release --short | egrep -o [0-9]+ | sed -n '1p')
ARCH:=$(shell uname -m)
BUILD_PLATFORM:=$(DISTRO_NAME)-$(DISTRO_VERSION_MAJOR)-$(ARCH)
ifeq ($(BUILD_PLATFORM),Debian-8-x86_64)
  OUT_FILE_PLATFORM:=debian8_ubuntu16.04-x64
else ifeq ($(BUILD_PLATFORM),Debian-9-x86_64)
  OUT_FILE_PLATFORM:=debian9-x64
else ifeq ($(BUILD_PLATFORM),CentOS-7-x86_64)
  OUT_FILE_PLATFORM:=centos7-x64
else ifeq ($(BUILD_PLATFORM),RedHatEnterpriseServer-7-x86_64)
  OUT_FILE_PLATFORM:=RedHatEnterpriseServer7-x64
else ifeq ($(BUILD_PLATFORM),RedHatEnterpriseServer-6-x86_64)
  OUT_FILE_PLATFORM:=RedHatEnterpriseServer6-x64
else ifeq ($(BUILD_PLATFORM),CentOS-6-x86_64)
  OUT_FILE_PLATFORM:=centos6-x64
else
  $(error $(BUILD_PLATFORM) platform is not supported)
endif
PACKAGE_DIR_NAME=keytalkclient-$(CLIENT_VERSION)
OUT_FILE_PATH=$(shell pwd)/../../../Export/KeyTalkClient-$(CLIENT_VERSION)-$(OUT_FILE_PLATFORM).tgz
OUT_DIR=../../../Export/installer.bin/$(PACKAGE_DIR_NAME)


.PHONY : release debug clean install

release debug: hwutils ReseptConsoleClient ReseptConfigManager ReseptConfigTool ReseptPrGenerator Documentation
	mkdir -p $(OUT_DIR)/

	cp -f ../install.sh ../apache.ini ../etc_cron.d_keytalk ../../resept.ini $(OUT_DIR)/
	echo $(BUILD_PLATFORM) > $(OUT_DIR)/build-platform

	cp -f ../../../Export/ktclient ../../../Export/ktconfig ../../../Export/ktconfupdater ../../../Export/ktconfigtool ../../../Export/ktprgen $(OUT_DIR)/
	cp -f ../../../ReseptConsoleClient/util.py ../../../ReseptConsoleClient/apache_util.py ../../../ReseptConsoleClient/cr.conf $(OUT_DIR)/

	cp -f ../../../../../SysInfra/Projects/Export/libtalogger.so ../../../../../SysInfra/Projects/Export/hwutils $(OUT_DIR)/
	cp -f ../../../../version ../../../../devstage  $(OUT_DIR)/


	# drop file extensions to not advertise implementation details
	cp -f ../uninstaller.sh $(OUT_DIR)/.uninstaller
	cp -f ../../../ReseptConsoleClient/renew_apache_ssl_cert.py $(OUT_DIR)/renew_apache_ssl_cert

	cp -f ../../../../../Documentation/KeyTalk_LinuxClient_for_Apache.txt $(OUT_DIR)/
	cp -f ../../../../../Documentation/KeyTalk_LinuxClient_for_Apache.pdf $(OUT_DIR)/

	@echo "Packaging KeyTalk client to $(OUT_FILE_PATH)"
	cd $(OUT_DIR)/.. && tar -czf $(OUT_FILE_PATH) $(PACKAGE_DIR_NAME)

clean: hwutils ReseptConsoleClient ReseptConfigManager ReseptConfigTool ReseptPrGenerator Documentation
	rm -rf $(OUT_FILE_PATH) $(OUT_DIR)

ReseptConsoleClient ReseptConfigManager ReseptConfigTool ReseptPrGenerator:
	$(MAKE) -C ../../../$@ $(MAKECMDGOALS)

hwutils:
	$(MAKE) -C ../../../../../SysInfra/Projects/$@ $(MAKECMDGOALS)

Documentation:
	$(MAKE) -C ../../../../../$@ $(MAKECMDGOALS)

install:


.SUFFIXES :
