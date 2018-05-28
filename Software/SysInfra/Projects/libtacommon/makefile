include ../../../mk/keytalk.common.mk

SRCS=signutils.cpp certutils.cpp hashutils.cpp pngutils.cpp libconfigwrapper.cpp \
     strings.cpp utils.cpp dynlibloader.cpp encodingutils.cpp version.cpp process.cpp url.cpp thread.cpp \
     netutils.cpp dnsutils.cpp hostutils.cpp tcpsocketutils.cpp tcpclient.cpp tcpserver.cpp osinfoutils.cpp rsautils.cpp opensslapp.cpp \
     timeutils.cpp ecutils.cpp opensslwrappers.cpp sysinfo.cpp linuxhwutils.cpp

ifdef RESEPT_SERVER
SRCS += aesutils.cpp dhutils.cpp ecdhutils.cpp
endif

DEPS:=$(SRCS:.cpp=.h) common.h utilsimpl.hpp stringsimpl.hpp scopedresource.hpp singletonholder.hpp proto.hpp \
       sessioncontainer.h sessioncontainerimpl.hpp boost_serialize_helper.hpp

SRCS_DIR=ta
OUT_DIR=../Export
EXT_LIBS_DIR=../../../Import

INCLUDES += -I.
INCLUDES +=-I../hwutils
INCLUDES +=-I../libtalogger
INCLUDES += -isystem $(EXT_LIBS_DIR)/$(BOOST_INCLUDE_DIR)
ifdef RESEPT_LINUX_CLIENT
INCLUDES += -I$(EXT_LIBS_DIR)/$(LIBCONFIG_INCLUDE_DIR)
INCLUDES += -I$(EXT_LIBS_DIR)/$(CURL_INCLUDE_DIR)
endif

LBITS:=$(shell getconf LONG_BIT)
ifeq ($(LBITS),64)
# please libtalogger.so
EXTRA_CFLAGS+=-fPIC
endif


STATIC_LIB=libtacommon.a

include ../../../mk/linux.staticlib.mk
