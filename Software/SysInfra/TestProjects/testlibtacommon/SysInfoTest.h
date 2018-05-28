#pragma once

#include "ta/sysinfo.h"
#include "ta/timeutils.h"
#include "cxxtest/TestSuite.h"
#include <string>

class SysInfoTest : public CxxTest::TestSuite
{
public:
    void testIsWow64()
    {
#ifdef _WIN32
        TS_ASSERT(!ta::SysInfo::isWow64());
#else
        TS_SKIP("This test is for Windows only");
#endif
    }

    void testWinProductId()
    {
#ifdef _WIN32
        const std::string myWinProductId = ta::SysInfo::getWinProductId();
        TS_TRACE("Windows Product ID is " + myWinProductId);
        TS_ASSERT(!myWinProductId.empty());
#else
        TS_SKIP("This test is for Windows only");
#endif
    }

    void testWinRegisteredOwner()
    {
#ifdef _WIN32
        const std::string myWinRegisteredOwner = ta::SysInfo::getWinRegisteredOwner();
        TS_TRACE("Windows Registered owner is " + myWinRegisteredOwner);
        TS_ASSERT(!myWinRegisteredOwner.empty());
#else
        TS_SKIP("This test is for Windows only");
#endif
    }

    void testSerialNumber()
    {
        const std::string serialNumber = ta::SysInfo::getSerialNumber();
        TS_TRACE("Serial number is " + serialNumber);
        TS_ASSERT(!serialNumber.empty());
    }

    void testGetHardwareDescription()
    {
        const std::string hwDescription = ta::SysInfo::getHardwareDescription();
        TS_TRACE("HW description is " + hwDescription);
        TS_ASSERT(!hwDescription.empty());
    }

    void testIsUserPasswordExpiring()
    {
        TS_ASSERT(ta::SysInfo::isUserPasswordExpiring(0));
        TS_ASSERT(ta::SysInfo::isUserPasswordExpiring(1));
        TS_ASSERT(!ta::SysInfo::isUserPasswordExpiring(365 * ta::TimeUtils::SecondsInDay));
    }
};
