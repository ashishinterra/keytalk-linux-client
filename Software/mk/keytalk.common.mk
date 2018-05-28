OSNAME := $(shell uname -s | tr "[:upper:]" "[:lower:]")
OSSPEC := $(shell uname -s | tr "[:upper:]" "[:lower:]")-$(shell uname -r | cut -d '-' -f 1)-$(shell uname -m | cut -d '-' -f 1)


# We build KeyTalk Server if /resept_server_dev exists
ifneq ($(wildcard /resept_server_dev),)
  RESEPT_SERVER := 1
  EXTRA_CFLAGS +=-DRESEPT_SERVER
endif

# We build KeyTalk Client if /resept_linux_client_dev exists
ifneq ($(wildcard /resept_linux_client_dev),)
  RESEPT_LINUX_CLIENT := 1
  EXTRA_CFLAGS +=-DRESEPT_LINUX_CLIENT
endif

# We build KeyTalk HSM Proxy if /resept_hsm_proxy_dev exists
ifneq ($(wildcard /resept_hsm_proxy_dev),)
  RESEPT_HSM_PROXY := 1
  EXTRA_CFLAGS +=-DRESEPT_HSM_PROXY
endif
# Feed USE_SOFT_HSM to C compiler when it is defined as env variable
ifdef USE_SOFT_HSM
  EXTRA_CFLAGS +=-DUSE_SOFT_HSM
endif


#
# Location of libraries
#

# Boost libraries
BOOST_VERSION_DIR=boost-1.64
BOOST_LIB_DIR=$(BOOST_VERSION_DIR)/bin.v2/libs
BOOST_INCLUDE_DIR=$(BOOST_VERSION_DIR)
BOOST_SERIALIZE_LIB=$(BOOST_LIB_DIR)/serialization/$(OSSPEC)/libboost_serialization.a
BOOST_DATETIME_LIB=$(BOOST_LIB_DIR)/date_time/$(OSSPEC)/libboost_date_time.a
BOOST_PROGRAMOPTIONS_LIB=$(BOOST_LIB_DIR)/program_options/$(OSSPEC)/libboost_program_options.a
BOOST_REGEX_LIB=$(BOOST_LIB_DIR)/regex/$(OSSPEC)/libboost_regex.a
BOOST_FILESYSTEM_LIB=$(BOOST_LIB_DIR)/filesystem/$(OSSPEC)/libboost_filesystem.a
BOOST_SYSTEM_LIB=$(BOOST_LIB_DIR)/system/$(OSSPEC)/libboost_system.a
BOOST_THREAD_LIB=$(BOOST_LIB_DIR)/thread/$(OSSPEC)/libboost_thread.a

# cxxtest library
CXXTEST_VERSION_DIR=cxxtest
CXXTEST_INCLUDE_DIR=$(CXXTEST_VERSION_DIR)
CXXTEST_GENERATOR=$(CXXTEST_VERSION_DIR)/bin/cxxtestgen

# utf8cpp library
UTF8CPP_LIB_VERSION_DIR=utf8cpp
UTF8CPP_INCLUDE_DIR=$(UTF8CPP_LIB_VERSION_DIR)/source

# ulxmlrpccpp library
ULXMLRPCCPP_LIB_VERSION_DIR=ulxmlrpcpp
ULXMLRPCCPP_INCLUDE_DIR=$(ULXMLRPCCPP_LIB_VERSION_DIR)
ULXMLRPCCPP_LIB=$(ULXMLRPCCPP_LIB_VERSION_DIR)/lib/libulxmlrpcpp.a

# SoftHsm library
SOFT_HSM_LIB_VERSION_DIR=softhsm-2.1
SOFT_HSM_INCLUDE_DIR=$(SOFT_HSM_LIB_VERSION_DIR)/include

# WPA supplicant library
WPA_SUPPLICANT_LIB_VERSION_DIR=wpa_supplicant-2.0
WPA_SUPPLICANT_INCLUDE_DIR=$(WPA_SUPPLICANT_LIB_VERSION_DIR)/include
WPA_SUPPLICANT_BIN_DIR=$(WPA_SUPPLICANT_LIB_VERSION_DIR)/bin/$(OSSPEC)


ifndef RESEPT_SERVER

# sqlite3cpp library
SQLITE3CPP_LIB_VERSION_DIR=sqlite3cpp
SQLITE3CPP_INCLUDE_DIR=$(SQLITE3CPP_LIB_VERSION_DIR)
SQLITE3CPP_LIB=$(SQLITE3CPP_LIB_VERSION_DIR)/lib/libsqlite3cpp.a


# Zlib static library
ZLIB_LIB_VERSION_DIR=zlib-1.2.8
ZLIB_INCLUDE_DIR=$(ZLIB_LIB_VERSION_DIR)
ZLIB_LIB=$(ZLIB_LIB_VERSION_DIR)/lib/$(OSSPEC)/libzwapi.a

# Curl static library
CURL_LIB_VERSION_DIR=curl-7.54
CURL_INCLUDE_DIR=$(CURL_LIB_VERSION_DIR)/include/$(OSSPEC)
CURL_LIB=$(CURL_LIB_VERSION_DIR)/lib/$(OSSPEC)/libcurl.a

ifdef RESEPT_LINUX_CLIENT

# libconfig library
LIBCONFIG_LIB_VERSION_DIR=libconfig-1.5
LIBCONFIG_INCLUDE_DIR=$(LIBCONFIG_LIB_VERSION_DIR)
LIBCONFIG_LIB=$(LIBCONFIG_LIB_VERSION_DIR)/lib/$(OSSPEC)/libconfig++.a

# YAML-cpp library
YAMLCPP_LIB_VERSION_DIR=yaml-cpp-0.5.3
YAMLCPP_INCLUDE_DIR=$(YAMLCPP_LIB_VERSION_DIR)/include
YAMLCPP_LIB=$(YAMLCPP_LIB_VERSION_DIR)/lib/$(OSSPEC)/libyaml-cpp.a

endif

endif