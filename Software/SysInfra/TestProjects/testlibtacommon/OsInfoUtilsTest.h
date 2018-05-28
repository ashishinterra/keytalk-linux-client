#pragma once

#include "ta/osinfoutils.h"
#include "cxxtest/TestSuite.h"
#include <string>

class OsInfoUtilsTest : public CxxTest::TestSuite
{
public:
    void testOsVersion()
    {
        ta::OsInfoUtils::Version myVersion = ta::OsInfoUtils::getVersion();
        TS_ASSERT(!myVersion.name.empty());
        TS_ASSERT(!myVersion.ver.empty());
        TS_TRACE(("OS Name: " + myVersion.name + "\nOS Version: " + myVersion.ver).c_str());
    }
};
