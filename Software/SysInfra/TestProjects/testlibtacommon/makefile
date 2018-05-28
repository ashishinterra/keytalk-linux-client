include ../../../mk/keytalk.common.mk

CXX_TEST_GENERATED_CPP = libtacommontests.cpp
SRCS                   = main.cpp SingletonTest.cpp $(CXX_TEST_GENERATED_CPP)
CXX_TESTS              = CertUtilsTest.h \
                         CommonTest.h \
                         DnsUtilsClientTest.h \
                         DynLibLoaderTest.h \
                         EcUtilsTest.h \
                         EncodingUtilsTest.h \
                         HashUtilsTest.h \
                         HostUtilsTest.h \
                         LibConfigWrapperTest.h \
                         LinuxHwUtilsTest.h \
                         LoggerTest.h \
                         NetUtilsTest.h \
                         OpenSSLWrappersTest.h \
                         OsInfoUtilsTest.h \
                         PngUtilsTest.h \
                         ProcessTest.h \
                         RsaUtilsTest.h \
                         ScopedResourceTest.h \
                         SessionContainerTest.h \
                         SignUtilsTest.h \
                         SingletonTest.h \
                         StringsTest.h \
                         SysInfoTest.h \
                         TcpClientTest.h \
                         TcpServerTest.h \
                         TimeUtilsTest.h \
                         URLTest.h \
                         UtilsTest.h \
                         VersionTest.h

ifdef RESEPT_SERVER
CXX_TESTS +=             AesUtilsTest.h \
                         DhUtilsTest.h \
                         DnsUtilsServerTest.h \
                         EcDhUtilsTest.h
endif

EXT_LIBS_DIR=../../../Import

CXX_TEST_GENERATOR=$(EXT_LIBS_DIR)/$(CXXTEST_GENERATOR)

DEPS=$(CXX_TESTS) GlobFixture.h
SRCS_DIR=.
OUT_DIR=../Export

DEP1=../../Projects/Export/libtalogger.so
DEP1_DIR=../../Projects/libtalogger
DEP2=../../Projects/Export/libtacommon.a
DEP2_DIR=../../Projects/libtacommon
DEP3=../../Projects/Export/hwutils
DEP3_DIR=../../Projects/hwutils
INCLUDES = -I$(DEP1_DIR) -I$(DEP2_DIR)
INCLUDES += -I$(EXT_LIBS_DIR)/$(CXXTEST_INCLUDE_DIR)
INCLUDES += -isystem $(EXT_LIBS_DIR)/$(BOOST_INCLUDE_DIR)

LIBS = -ltalogger $(DEP2)

ifdef RESEPT_LINUX_CLIENT
INCLUDES += -I$(EXT_LIBS_DIR)/$(LIBCONFIG_INCLUDE_DIR)
LIBS += $(EXT_LIBS_DIR)/$(LIBCONFIG_LIB)
LIBS += $(EXT_LIBS_DIR)/$(CURL_LIB)
else
LIBS += -lcurl
LIBS += -lconfig++
endif
LIBS += $(EXT_LIBS_DIR)/$(BOOST_DATETIME_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_REGEX_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_FILESYSTEM_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_SYSTEM_LIB)
LIBS += $(EXT_LIBS_DIR)/$(BOOST_THREAD_LIB)
LIBS += -lssl -lcrypto -lpthread

EXTRA_LFLAGS=-Wl,-R,../../Projects/Export -L../../Projects/Export

PROG=testlibtacommon

POST_BUILD=post_build
POST_CLEAN=post_clean

include ../../../mk/linux.prog.mk

post_build:
	cp -rf ../CA/ $(OUT_DIR)/
	cp -f orig.data $(OUT_DIR)/CA/
	cp -f naive_http_server.py test.html blob.tst *.bmp *.png $(OUT_DIR)
	cp -f log.conf  $(OUT_DIR)/
	cp -f ../../Projects/Export/hwutils  $(OUT_DIR)/
ifdef RESEPT_SERVER
	@../../../Server/Projects/config/setup-logging.sh $(PROG)
endif

post_clean:
	rm -rf $(OUT_DIR)/
