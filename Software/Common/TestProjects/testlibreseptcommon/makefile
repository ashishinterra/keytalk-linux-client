include ../../../mk/keytalk.common.mk

CXX_TEST_GENERATED_CPP=ReseptCommonTests.cpp
SRCS=main.cpp $(CXX_TEST_GENERATED_CPP)
CXX_TESTS=ReseptCommonTest.h ReseptUtilTest.h ComputerUuidTest.h

EXT_LIBS_DIR=../../../Import

CXX_TEST_GENERATOR=$(EXT_LIBS_DIR)/$(CXXTEST_GENERATOR)
DEPS=$(CXX_TESTS)
SRCS_DIR=.
OUT_DIR=../Export

DEP1=../../Projects/Export/libreseptcommon.a
DEP1_DIR=../../Projects/libreseptcommon
DEP2=../../../SysInfra/Projects/Export/libtalogger.so
DEP2_DIR=../../../SysInfra/Projects/libtalogger
DEP3=../../../SysInfra/Projects/Export/libtacommon.a
DEP3_DIR=../../../SysInfra/Projects/libtacommon

INCLUDES = -I$(DEP1_DIR) -I$(DEP2_DIR) -I$(DEP3_DIR)
INCLUDES += -isystem $(EXT_LIBS_DIR)/$(BOOST_INCLUDE_DIR)
INCLUDES += -I$(EXT_LIBS_DIR)/$(UTF8CPP_INCLUDE_DIR)
INCLUDES += -I$(EXT_LIBS_DIR)/$(CXXTEST_INCLUDE_DIR)

LIBS += $(DEP1) -ltalogger $(DEP3)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_DATETIME_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_REGEX_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_FILESYSTEM_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_SYSTEM_LIB)
ifdef RESEPT_LINUX_CLIENT
LIBS += $(EXT_LIBS_DIR)/$(CURL_LIB)
else
LIBS += -lcurl
endif

LIBS += -lssl -lcrypto -lpthread

# logger
EXTRA_LFLAGS=-Wl,-R,../../../SysInfra/Projects/Export -L../../../SysInfra/Projects/Export

PROG=testlibreseptcommon

POST_BUILD=post_build
POST_CLEAN=post_clean

include ../../../mk/linux.prog.mk

post_build:
	cp -f *.pem $(OUT_DIR)/

post_clean:
	rm -f $(OUT_DIR)/*.pem
