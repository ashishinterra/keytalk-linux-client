include ../../../mk/keytalk.common.mk

CXX_TEST_GENERATED_CPP=testlibrclientcore.cpp
SRCS=main.cpp $(CXX_TEST_GENERATED_CPP)
CXX_TESTS=SettingsTest.h
ifdef RESEPT_LINUX_CLIENT
CXX_TESTS += SettingsInstallationTest.h RcdpTest.h RcdpServerStressTest.h NativeCertStoreTest.h
endif

EXT_LIBS_DIR=../../../Import

CXX_TEST_GENERATOR=$(EXT_LIBS_DIR)/$(CXXTEST_GENERATOR)
DEPS=$(CXX_TESTS)
SRCS_DIR=.
OUT_DIR=../Export

# notice libreseptcommon stays above librclientcore because it should be build first to generate include files needed by librclientcore.a
# at the same time LIBS variable below correctly specifies librclientcore.a before libreseptcommon.a
DEP1=../../../Common/Projects/Export/libreseptcommon.a
DEP1_DIR=../../../Common/Projects/libreseptcommon
DEP2=../../Projects/Export/librclientcore.a
DEP2_DIR=../../Projects/librclientcore
ifdef RESEPT_LINUX_CLIENT
DEP3=../../Projects/Export/libtaclientcommon.a
DEP3_DIR=../../Projects/libtaclientcommon
endif
DEP8=../../../SysInfra/Projects/Export/libtalogger.so
DEP8_DIR=../../../SysInfra/Projects/libtalogger
DEP9=../../../SysInfra/Projects/Export/libtacommon.a
DEP9_DIR=../../../SysInfra/Projects/libtacommon

DEP10=../../../SysInfra/Projects/Export/hwutils
DEP10_DIR=../../../SysInfra/Projects/hwutils

INCLUDES += -I$(DEP1_DIR)
INCLUDES += -I$(DEP2_DIR)
ifdef RESEPT_LINUX_CLIENT
INCLUDES += -I$(DEP3_DIR)
endif
INCLUDES += -I$(DEP8_DIR)
INCLUDES += -I$(DEP9_DIR)
INCLUDES += -isystem $(EXT_LIBS_DIR)/$(BOOST_INCLUDE_DIR)
INCLUDES += -I$(EXT_LIBS_DIR)/$(CXXTEST_INCLUDE_DIR)

LIBS += $(DEP2)
LIBS += $(DEP1)
ifdef RESEPT_LINUX_CLIENT
LIBS += $(DEP3)
endif
LIBS += -ltalogger
LIBS += $(DEP9)

LIBS += $(EXT_LIBS_DIR)/$(BOOST_DATETIME_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_REGEX_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_FILESYSTEM_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_SYSTEM_LIB)
ifdef RESEPT_LINUX_CLIENT
LIBS += $(EXT_LIBS_DIR)/$(LIBCONFIG_LIB)
LIBS += $(EXT_LIBS_DIR)/$(YAMLCPP_LIB)
LIBS += $(EXT_LIBS_DIR)/$(CURL_LIB)
endif
ifdef RESEPT_SERVER
LIBS += -lconfig++ -lyaml-cpp
endif
LIBS += -lssl -lcrypto -lpthread

# logger
EXTRA_LFLAGS=-Wl,-R,../../../SysInfra/Projects/Export -L../../../SysInfra/Projects/Export

PROG=testlibrclientcore


POST_BUILD=post_build
POST_CLEAN=post_clean

include ../../../mk/linux.prog.mk

post_build:
	cp -f *.der *.pem *.p12 *.pfx.pass.txt winhlp32.exe log.conf *.ini $(OUT_DIR)/
	cp -f user.ini $(OUT_DIR)/user.ini.orig
	cp -f master.ini $(OUT_DIR)/master.ini.orig
	cp -f resept.ini.linux $(OUT_DIR)/resept.ini
ifdef RESEPT_LINUX_CLIENT
	cp -f ../../../SysInfra/Projects/Export/hwutils $(OUT_DIR)/
	@echo " Creating minimal KeyTalk client installation..."
	mkdir -p ~/.keytalk/
	cp -f ../../../ContinuousIntegration/cc.py/rlinuxclient/user.ini ~/.keytalk/
	mkdir -p /etc/keytalk/
	cp -f ../../../ContinuousIntegration/cc.py/rlinuxclient/master.ini /etc/keytalk/
	cp -f ../../../ContinuousIntegration/cc.py/rlinuxclient/resept.ini /etc/keytalk/
	cat ../../../CertKeys/CommunicationAndSigning/commcacert.pem ../../../CertKeys/CommunicationAndSigning/pcacert.pem > /usr/local/share/ca-certificates/test-commcacert-pcacert.crt
	update-ca-certificates

endif

post_clean:
	rm -f $(OUT_DIR)/*.der $(OUT_DIR)/*.pem $(OUT_DIR)/*.p12 $(OUT_DIR)/*.pfx.pass.txt $(OUT_DIR)/winhlp32.exe $(OUT_DIR)/log.conf $(OUT_DIR)/*.ini $(OUT_DIR)/user.ini.orig $(OUT_DIR)/master.ini.orig
	rm -rf $(OUT_DIR)/CA
ifdef RESEPT_LINUX_CLIENT
	rm -f $(OUT_DIR)/hwutils
	rm -f /usr/local/share/ca-certificates/test-commcacert-pcacert.crt
	update-ca-certificates --fresh > /dev/null 2>&1
endif
