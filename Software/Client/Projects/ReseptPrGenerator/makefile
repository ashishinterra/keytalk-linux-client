include ../../../mk/keytalk.common.mk

SRCS=main.cpp LogInitializer.cpp ReseptPrGenerator.cpp
DEPS:=LogInitializer.h ReseptPrGenerator.h

EXT_LIBS_DIR=../../../Import

SRCS_DIR=.
OUT_DIR=../Export

#@notice libreseptcommon stays above librclientcore which on its turn stays above librclientappcommon because of generated headers dependency
DEP1=../../../Common/Projects/Export/libreseptcommon.a
DEP1_DIR=../../../Common/Projects/libreseptcommon
DEP2=../Export/librclientcore.a
DEP2_DIR=../librclientcore
DEP3=../Export/librclientappcommon.a
DEP3_DIR=../librclientappcommon
DEP4=../Export/libtaclientcommon.a
DEP4_DIR=../libtaclientcommon

DEP8=../../../SysInfra/Projects/Export/libtalogger.so
DEP8_DIR=../../../SysInfra/Projects/libtalogger
DEP9=../../../SysInfra/Projects/Export/libtacommon.a
DEP9_DIR=../../../SysInfra/Projects/libtacommon

INCLUDES += -I$(DEP1_DIR)
INCLUDES += -I$(DEP2_DIR)
INCLUDES += -I$(DEP3_DIR)
INCLUDES += -I$(DEP4_DIR)
INCLUDES += -I$(DEP8_DIR)
INCLUDES += -I$(DEP9_DIR)
INCLUDES += -I$(EXT_LIBS_DIR)/$(ZLIB_INCLUDE_DIR)
INCLUDES += -I$(EXT_LIBS_DIR)/$(CURL_INCLUDE_DIR)
INCLUDES += -isystem $(EXT_LIBS_DIR)/$(BOOST_INCLUDE_DIR)

LIBS += $(DEP3)
LIBS += $(DEP2)
LIBS += $(DEP1)
LIBS += $(DEP4)
LIBS += -ltalogger
LIBS += $(DEP9)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_DATETIME_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_REGEX_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_FILESYSTEM_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_SYSTEM_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_PROGRAMOPTIONS_LIB)
LIBS += $(EXT_LIBS_DIR)/$(LIBCONFIG_LIB)
LIBS += $(EXT_LIBS_DIR)/$(ZLIB_LIB)
LIBS += $(EXT_LIBS_DIR)/$(YAMLCPP_LIB)
LIBS += $(EXT_LIBS_DIR)/$(CURL_LIB)
LIBS += -lssl -lcrypto -lpthread

# logger library location
EXTRA_LFLAGS=-L../../../SysInfra/Projects/Export -Wl,-R,/usr/local/lib/keytalk

PROG=ktprgen

include ../../../mk/linux.prog.mk

install:
	mkdir -p /usr/local/bin/keytalk/
	cp $(OUT_DIR)/$(PROG) /usr/local/bin/keytalk/

	# copy logger
	mkdir -p /usr/local/lib/keytalk/
	cp ../../../SysInfra/Projects/Export/libtalogger.so /usr/local/lib/keytalk/

