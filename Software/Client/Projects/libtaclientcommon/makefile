include ../../../mk/keytalk.common.mk

SRCS=Zip.cpp
DEPS:=$(SRCS:.cpp=.h)

EXT_LIBS_DIR=../../../Import

SRCS_DIR=ta
OUT_DIR=../Export
INCLUDES += -I.
INCLUDES += -I../../../SysInfra/Projects/libtalogger
INCLUDES += -I../../../SysInfra/Projects/libtacommon
INCLUDES += -isystem $(EXT_LIBS_DIR)/$(BOOST_INCLUDE_DIR)
INCLUDES += -I$(EXT_LIBS_DIR)/$(ZLIB_INCLUDE_DIR)

STATIC_LIB=libtaclientcommon.a

include ../../../mk/linux.staticlib.mk
