include ../../../mk/keytalk.common.mk

CXX_TEST_GENERATED_CPP=taclientcommontests.cpp
SRCS=main.cpp $(CXX_TEST_GENERATED_CPP)
CXX_TESTS=ZipTest.h

EXT_LIBS_DIR=../../../Import

CXX_TEST_GENERATOR=$(EXT_LIBS_DIR)/$(CXXTEST_GENERATOR)
DEPS=$(CXX_TESTS)
SRCS_DIR=.
OUT_DIR=../Export

DEP1=../../Projects/Export/libtaclientcommon.a
DEP1_DIR=../../Projects/libtaclientcommon
DEP2=../../../SysInfra/Projects/Export/libtalogger.so
DEP2_DIR=../../../SysInfra/Projects/libtalogger
DEP3=../../../SysInfra/Projects/Export/libtacommon.a
DEP3_DIR=../../../SysInfra/Projects/libtacommon

INCLUDES += -I$(DEP1_DIR)
INCLUDES += -I$(DEP2_DIR)
INCLUDES += -I$(DEP3_DIR)
INCLUDES += -I../../../Common/Projects/libreseptcommon
INCLUDES += -isystem $(EXT_LIBS_DIR)/$(BOOST_INCLUDE_DIR)
INCLUDES += -I$(EXT_LIBS_DIR)/$(CXXTEST_INCLUDE_DIR)

LIBS += $(DEP1)
LIBS += $(DEP2)
LIBS += -ltalogger
LIBS += $(DEP3)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_DATETIME_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_REGEX_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_FILESYSTEM_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_SYSTEM_LIB)
LIBS += $(EXT_LIBS_DIR)/$(ZLIB_LIB)
LIBS += -lssl -lcrypto -lpthread

EXTRA_LFLAGS=-Wl,-R,../../../SysInfra/Projects/Export -L../../../SysInfra/Projects/Export

PROG=testlibtaclientcommon


POST_BUILD=post_build
POST_CLEAN=post_clean

include ../../../mk/linux.prog.mk

post_build:
	cp -f test.zip log.conf $(OUT_DIR)/

post_clean:
	rm -f $(OUT_DIR)/log.conf $(OUT_DIR)/test.zip



