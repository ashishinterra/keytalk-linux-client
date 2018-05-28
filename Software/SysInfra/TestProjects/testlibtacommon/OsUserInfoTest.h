#pragma once

#include "ta/OsUserInfo.h"
#include "cxxtest/TestSuite.h"
#include <string>

class OsUserInfoTest : public CxxTest::TestSuite
{
public:
    void testGetUserSID()
    {
        std::string myUserSID = ta::OsUserInfo::getCurentUserSID();
        TS_ASSERT(!myUserSID.empty());
    }
};
