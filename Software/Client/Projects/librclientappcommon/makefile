include ../../../mk/keytalk.common.mk

SRCS=CommonUtils.cpp ContentConfig.cpp ReseptClientApp.cpp CRFile.cpp
DEPS:=$(SRCS:.cpp=.h)

EXT_LIBS_DIR=../../../Import

SRCS_DIR=rclient
OUT_DIR=../Export

INCLUDES += -I.
INCLUDES += -I../../../SysInfra/Projects/libtalogger
INCLUDES += -I../../../SysInfra/Projects/libtacommon
INCLUDES += -I../../../Common/Projects/libreseptcommon
INCLUDES += -I../librclientcore
INCLUDES += -I../libtaclientcommon
INCLUDES += -I$(EXT_LIBS_DIR)/$(LIBCONFIG_INCLUDE_DIR)
INCLUDES += -isystem $(EXT_LIBS_DIR)/$(BOOST_INCLUDE_DIR)

STATIC_LIB=librclientappcommon.a

include ../../../mk/linux.staticlib.mk
