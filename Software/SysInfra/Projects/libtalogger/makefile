include ../../../mk/keytalk.common.mk


SRCS=logger.cpp logappender.cpp syslogappender.cpp consoleappender.cpp fileappender.cpp logconfiguration.cpp assert.cpp
DEPS:=$(SRCS:.cpp=.h)

EXT_LIBS_DIR=../../../Import

SRCS_DIR=ta
OUT_DIR=../Export

DEP1=../Export/libtacommon.a
DEP1_DIR=../libtacommon

INCLUDES += -I.
INCLUDES += -I$(DEP1_DIR)
INCLUDES += -isystem $(EXT_LIBS_DIR)/$(BOOST_INCLUDE_DIR)

LIBS = $(DEP1)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_DATETIME_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_REGEX_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_FILESYSTEM_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_SYSTEM_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_THREAD_LIB)

ifdef RESEPT_LINUX_CLIENT
INCLUDES += -I$(EXT_LIBS_DIR)/$(LIBCONFIG_INCLUDE_DIR)
LIBS += $(EXT_LIBS_DIR)/$(LIBCONFIG_LIB)
LIBS += $(EXT_LIBS_DIR)/$(CURL_LIB)
else
LIBS += -lcurl -lconfig++
endif
LIBS += -lssl -lcrypto -lpthread -lrt

LBITS:=$(shell getconf LONG_BIT)
ifeq ($(LBITS),64)
EXTRA_CFLAGS+=-fPIC
endif

EXTRA_LFLAGS+=-shared
PROG=libtalogger.so

include ../../../mk/linux.prog.mk
