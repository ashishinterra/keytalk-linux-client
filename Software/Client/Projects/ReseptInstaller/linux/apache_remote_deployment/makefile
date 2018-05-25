CLIENT_VERSION:=$(shell cut -d '=' -f 2 ../../../../version)
CLIENT_VERSION:=$(CLIENT_VERSION)$(shell grep -m 1 '^Hardware' /proc/cpuinfo | cut -d ':' -f 2 | grep -q BCM2708 && echo '.RaspberryPi')
PACKAGE_DIR_NAME=keytalkclient-$(CLIENT_VERSION)
OUT_FILE_PATH=$(shell pwd)/../../../Export/KeyTalkClient-$(CLIENT_VERSION).linux.apache-remote-deployment.tgz
OUT_DIR=../../../Export/installer.apache-remote-deployment/$(PACKAGE_DIR_NAME)


.PHONY : release debug clean install

release debug: Documentation
	mkdir -p $(OUT_DIR)/

	# remove .py extension to not advertise implementation details
	cp -f ../../../ReseptConsoleClient/ktclient_remote_deploy.py $(OUT_DIR)/ktclient_remote_deploy
	cp -f ../../../../version  $(OUT_DIR)/

	cp -f ../../../../../Documentation/KeyTalk_LinuxClient_for_Apache_remote_deployment.txt $(OUT_DIR)/
	cp -f ../../../../../Documentation/KeyTalk_LinuxClient_for_Apache_remote_deployment.pdf $(OUT_DIR)/

	@echo "Packaging KeyTalk Apache Remote Deployment script"
	cd $(OUT_DIR)/.. && tar -czf $(OUT_FILE_PATH) $(PACKAGE_DIR_NAME)

clean: Documentation
	rm -rf $(OUT_FILE_PATH) $(OUT_DIR)

Documentation:
	cd ../../../../../$@ && $(MAKE) $(MAKECMDGOALS)

install:


.SUFFIXES :
