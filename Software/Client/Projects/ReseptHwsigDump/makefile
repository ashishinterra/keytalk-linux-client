include ../../../mk/keytalk.common.mk
SRCS=ReseptHwsigDump.cpp
DEPS:=

EXT_LIBS_DIR=../../../Import

SRCS_DIR=.
OUT_DIR=../Export

DEP1=../../../Common/Projects/Export/libreseptcommon.a
DEP1_DIR=../../../Common/Projects/libreseptcommon
DEP4=../Export/libtaclientcommon.a
DEP4_DIR=../libtaclientcommon
DEP8=../../../SysInfra/Projects/Export/libtalogger.so
DEP8_DIR=../../../SysInfra/Projects/libtalogger
DEP9=../../../SysInfra/Projects/Export/libtacommon.a
DEP9_DIR=../../../SysInfra/Projects/libtacommon

INCLUDES += -I$(DEP1_DIR)
INCLUDES += -I$(DEP4_DIR)
INCLUDES += -I$(DEP8_DIR)
INCLUDES += -I$(DEP9_DIR)
INCLUDES += -isystem $(EXT_LIBS_DIR)/$(BOOST_INCLUDE_DIR)

LIBS += $(DEP3) $(DEP2) $(DEP1) $(DEP4) -ltalogger $(DEP9)

LIBS += $(EXT_LIBS_DIR)/$(BOOST_DATETIME_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_REGEX_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_FILESYSTEM_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_SYSTEM_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_PROGRAMOPTIONS_LIB)
LIBS += $(EXT_LIBS_DIR)/$(LIBCONFIG_LIB)
LIBS += -lssl -lcrypto -lpthread

# logger library location
EXTRA_LFLAGS=-L../../../SysInfra/Projects/Export -Wl,-R,/usr/local/lib/keytalk

PROG=kthwsigdump

include ../../../mk/linux.prog.mk

install:
