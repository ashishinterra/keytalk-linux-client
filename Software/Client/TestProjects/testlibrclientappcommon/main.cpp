#ifndef CXXTEST_RUNNING
#define CXXTEST_RUNNING
#endif

#define _CXXTEST_HAVE_STD
#define _CXXTEST_HAVE_EH
#include "GlobFixture.h"
#include "ta/logconfiguration.h"
#include <cxxtest/TestListener.h>
#include <cxxtest/TestTracker.h>
#include <cxxtest/TestRunner.h>
#include <cxxtest/RealDescriptions.h>
#include <cxxtest/TestMain.h>
#include <cxxtest/ErrorPrinter.h>
#include <cxxtest/XUnitPrinter.h>
#include <fstream>

int main( int argc, char *argv[] ) {
    int status;
    static GlobFixture myGlobalFixture;
    if (!ta::LogConfiguration::instance().load("log.conf"))
        std::cerr << "WARNING: Failed to load log configuration\n";
#ifdef _WIN32
    std::ofstream ofstr("testlibrclientappcommon.log.xml");
    CxxTest::XUnitPrinter tmp(ofstr);
    status = CxxTest::Main<CxxTest::XUnitPrinter>( tmp, argc, argv );
#else
    CxxTest::ErrorPrinter tmp;
    status = CxxTest::Main<CxxTest::ErrorPrinter>( tmp, argc, argv );
#endif
    return status;
}
#include <cxxtest/Root.cpp>
const char* CxxTest::RealWorldDescription::_worldName = "cxxtest";
