#ifndef FileVersionInfoTest_H
#define FileVersionInfoTest_H

#include "ta/FileVersionInfo.h"
#include "cxxtest/TestSuite.h"

class FileVersionInfoTest : public CxxTest::TestSuite
{
public:
    void testVersion()
    {
        try
        {
            ta::FileVersionInfo myVerInfo("winhlp32.exe");
            TS_ASSERT_EQUALS(myVerInfo.getCompanyName(), "Microsoft Corporation");
            TS_ASSERT_EQUALS(myVerInfo.getFileVersion(), "6.0.6000.16386");
            TS_ASSERT_EQUALS(myVerInfo.getProductVersion(), "6.0.6000.16386");
            TS_ASSERT_EQUALS(myVerInfo.getProductName(), "Microsoft\x00ae Windows\x00ae Operating System");
            TS_ASSERT_EQUALS(myVerInfo.getFileDescription(), "Windows Winhlp32 Stub");
        }
        catch (std::exception& e)
        {
            TS_ASSERT(false);
            TS_TRACE(e.what());
        }
        catch (...)
        {
            TS_ASSERT(!"Unknown exception");
        }
    }
};

#endif
