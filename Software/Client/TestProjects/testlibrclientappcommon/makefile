include ../../../mk/keytalk.common.mk


CXX_TEST_GENERATED_CPP=testlibrclientappcommon.cpp
SRCS=main.cpp ../ReseptClientAppTestConfig/ReseptClientAppTestConfig.cpp $(CXX_TEST_GENERATED_CPP)
CXX_TESTS=CommonUtilsTest.h ReseptClientApiTest.h ContentConfigInstallationTest.h

EXT_LIBS_DIR=../../../Import

CXX_TEST_GENERATOR=$(EXT_LIBS_DIR)/$(CXXTEST_GENERATOR)
DEPS=$(CXX_TESTS) ../ReseptClientAppTestConfig/ReseptClientAppTestConfig.h
SRCS_DIR=.
OUT_DIR=../Export

#@notice libreseptcommon stays above librclientcore which on its turn stays above librclientappcommon because of generated headers dependency
DEP1=../../../Common/Projects/Export/libreseptcommon.a
DEP1_DIR=../../../Common/Projects/libreseptcommon
DEP2=../../Projects/Export/librclientcore.a
DEP2_DIR=../../Projects/librclientcore
DEP3=../../Projects/Export/librclientappcommon.a
DEP3_DIR=../../Projects/librclientappcommon
DEP4=../../Projects/Export/libtaclientcommon.a
DEP4_DIR=../../Projects/libtaclientcommon
DEP7=../../../SysInfra/Projects/Export/libtalogger.so
DEP7_DIR=../../../SysInfra/Projects/libtalogger
DEP8=../../../SysInfra/Projects/Export/libtacommon.a
DEP8_DIR=../../../SysInfra/Projects/libtacommon

DEP10=../../../SysInfra/Projects/Export/hwutils
DEP10_DIR=../../../SysInfra/Projects/hwutils

INCLUDES += -I../ReseptClientAppTestConfig
INCLUDES += -I$(DEP1_DIR)
INCLUDES += -I$(DEP2_DIR)
INCLUDES += -I$(DEP3_DIR)
INCLUDES += -I$(DEP7_DIR)
INCLUDES += -I$(DEP8_DIR)
INCLUDES += -isystem $(EXT_LIBS_DIR)/$(BOOST_INCLUDE_DIR)
INCLUDES += -I$(EXT_LIBS_DIR)/$(CXXTEST_INCLUDE_DIR)
INCLUDES += -I$(EXT_LIBS_DIR)/$(LIBCONFIG_INCLUDE_DIR)

LIBS += $(DEP3) $(DEP2) $(DEP1) $(DEP4) -ltalogger $(DEP8)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_DATETIME_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_REGEX_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_FILESYSTEM_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_SYSTEM_LIB)
LIBS += $(EXT_LIBS_DIR)/$(LIBCONFIG_LIB)
LIBS += $(EXT_LIBS_DIR)/$(YAMLCPP_LIB)
LIBS += $(EXT_LIBS_DIR)/$(CURL_LIB)
LIBS += -lssl -lcrypto -lpthread

EXTRA_LFLAGS=-Wl,-R,../../../SysInfra/Projects/Export -L../../../SysInfra/Projects/Export

PROG=testlibrclientappcommon

POST_BUILD=post_build
POST_CLEAN=post_clean

include ../../../mk/linux.prog.mk

post_build:
	cp -f ../ReseptClientAppTestConfig/ReseptClientAppTest.conf log.conf ../../../SysInfra/Projects/Export/hwutils $(OUT_DIR)/

	@echo " Creating minimal KeyTalk installation..."
	mkdir -p ~/.keytalk/
	cp -f ../../../ContinuousIntegration/cc.py/rlinuxclient/user.ini ~/.keytalk/
	mkdir -p /etc/keytalk/
	cp -f ../../../ContinuousIntegration/cc.py/rlinuxclient/master.ini /etc/keytalk/
	cp -f ../../../ContinuousIntegration/cc.py/rlinuxclient/resept.ini /etc/keytalk/
	cat ../../../CertKeys/CommunicationAndSigning/commcacert.pem ../../../CertKeys/CommunicationAndSigning/pcacert.pem > /usr/local/share/ca-certificates/test-commcacert-pcacert.crt
	update-ca-certificates

post_clean:
	rm -f $(OUT_DIR)/ReseptClientAppTest.conf $(OUT_DIR)/log.conf $(OUT_DIR)/hwutils
	rm -f /usr/local/share/ca-certificates/test-commcacert-pcacert.crt
	update-ca-certificates --fresh > /dev/null 2>&1
