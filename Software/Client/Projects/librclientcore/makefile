include ../../../mk/keytalk.common.mk

SRCS=Common.cpp Settings.cpp
ifdef RESEPT_LINUX_CLIENT
SRCS+=NativeCertStore.cpp RcdpRequest.cpp RcdpResponse.cpp RcdpHandler.cpp
endif
GEN_VERSION_FILE=Version.h
GEN_SETTINGS_FILE=SettingsDefs.h
DEPS:=$(SRCS:.cpp=.h) SettingsImpl.hpp $(GEN_VERSION_FILE) $(GEN_SETTINGS_FILE)

INCLUDES += -I.
INCLUDES += -I../../../SysInfra/Projects/libtalogger
INCLUDES += -I../../../SysInfra/Projects/libtacommon
INCLUDES += -I../../../Common/Projects/libreseptcommon
ifdef RESEPT_LINUX_CLIENT
INCLUDES += -I../libtaclientcommon
INCLUDES += -I$(EXT_LIBS_DIR)/$(CURL_INCLUDE_DIR)
INCLUDES += -I$(EXT_LIBS_DIR)/$(LIBCONFIG_INCLUDE_DIR)
INCLUDES += -I$(EXT_LIBS_DIR)/$(YAMLCPP_INCLUDE_DIR)
endif
INCLUDES += -isystem $(EXT_LIBS_DIR)/$(BOOST_INCLUDE_DIR)

EXT_LIBS_DIR=../../../Import

SRCS_DIR=rclient
OUT_DIR=../Export

STATIC_LIB=librclientcore.a


POST_CLEAN=post_clean

include ../../../mk/linux.staticlib.mk

$(SRCS_DIR)/$(GEN_VERSION_FILE): VersionGen.pl ../../version
	@echo "Generating version..."
	perl $^ > $@

$(SRCS_DIR)/$(GEN_SETTINGS_FILE): ../../../Tools/codegen/gen.py $(SRCS_DIR)/$(GEN_SETTINGS_FILE).templ ../../settings_config.py
	@echo "Generating settings..."
	python ../../../Tools/codegen/gen.py --cxx $(SRCS_DIR)/$(GEN_SETTINGS_FILE).templ ../../settings_config.py > $@

post_clean:
	rm -f $(SRCS_DIR)/$(GEN_VERSION_FILE)
	rm -f $(SRCS_DIR)/$(GEN_SETTINGS_FILE)
