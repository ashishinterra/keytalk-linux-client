include ../../../mk/keytalk.common.mk

SRCS=hwutils.cpp
DEPS:=$(SRCS:.cpp=.h)

EXT_LIBS_DIR=../../../Import

SRCS_DIR=.
OUT_DIR=../Export

DEP1=../Export/libtalogger.so
DEP1_DIR=../libtalogger
DEP2=../Export/libtacommon.a
DEP2_DIR=../libtacommon

INCLUDES += -I$(DEP1_DIR)
INCLUDES += -I$(DEP2_DIR)
INCLUDES += -isystem $(EXT_LIBS_DIR)/$(BOOST_INCLUDE_DIR)

LIBS += -ltalogger $(DEP2)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_DATETIME_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_REGEX_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_FILESYSTEM_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_SYSTEM_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_PROGRAMOPTIONS_LIB)

ifdef RESEPT_SERVER
LIBS += -lcurl
LIBS += -lconfig++
else
LIBS += $(EXT_LIBS_DIR)/$(LIBCONFIG_LIB)
LIBS += $(EXT_LIBS_DIR)/$(YAMLCPP_LIB)
LIBS += $(EXT_LIBS_DIR)/$(CURL_LIB)
endif
LIBS += -lssl -lcrypto -lpthread

# logger library location
EXTRA_LFLAGS=-L../Export -Wl,-R,/usr/local/lib/keytalk

PROG=hwutils


include ../../../mk/linux.prog.mk

install:
ifdef RESEPT_SERVER
	install -d -o keytalk -g keytalk /usr/local/bin/keytalk/ /usr/local/lib/keytalk/
	install -o keytalk -g keytalk $(OUT_DIR)/$(PROG) /usr/local/bin/keytalk/
	install -o keytalk -g keytalk -m 644 $(OUT_DIR)/libtalogger.so /usr/local/lib/keytalk/
else
	mkdir -p /usr/local/bin/keytalk/ /usr/local/lib/keytalk/
	cp $(OUT_DIR)/$(PROG) /usr/local/bin/keytalk/
	cp $(OUT_DIR)/libtalogger.so /usr/local/lib/keytalk/
endif
