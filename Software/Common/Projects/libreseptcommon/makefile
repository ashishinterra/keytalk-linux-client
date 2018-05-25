include ../../../mk/keytalk.common.mk


SRCS=util.cpp common.cpp computeruuid.cpp
GEN_FILE=common.h
DEPS:=$(GEN_FILE) util.h computeruuid.h

EXT_LIBS_DIR=../../../Import

SRCS_DIR=resept
OUT_DIR=../Export
INCLUDES = -I../../../SysInfra/Projects/libtacommon -I../../../SysInfra/Projects/libtalogger
INCLUDES += -isystem $(EXT_LIBS_DIR)/$(BOOST_INCLUDE_DIR)
INCLUDES += -I$(EXT_LIBS_DIR)/$(UTF8CPP_INCLUDE_DIR)

STATIC_LIB=libreseptcommon.a

POST_CLEAN=post_clean

include ../../../mk/linux.staticlib.mk

$(SRCS_DIR)/$(GEN_FILE): ../../../Tools/codegen/gen.py $(SRCS_DIR)/$(GEN_FILE).templ ../../common_config.py
	python ../../../Tools/codegen/gen.py --cxx $(SRCS_DIR)/$(GEN_FILE).templ ../../common_config.py > $@

post_clean:
	rm -f $(SRCS_DIR)/$(GEN_FILE)
