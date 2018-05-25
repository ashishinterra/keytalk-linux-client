#pragma once

#include "ta/ExceptionDumper.h"
#include "ta/process.h"
#include "ta/utils.h"

#include "cxxtest/TestSuite.h"
#include <string>
#include <sstream>
#include <string.h>
#include <windows.h>
#include <fstream>

static bool fireAV(const std::string& aModuleName)
{
    bool myIsExceptionFired = false;
    __try
    {
        memcpy(NULL, "12345", 5);
    }
    __except(ta::ExceptionDumper::dump(GetExceptionInformation(), aModuleName), EXCEPTION_EXECUTE_HANDLER)
    {
        myIsExceptionFired = true;
    }
    return myIsExceptionFired;
}

class ExceptionDumperTest : public CxxTest::TestSuite
{
public:
    void testAV()
    {
        const std::string myModuleName = "ExceptionDumperTest.testAV";
        const std::string myMiniDumpPath = ta::Process::getTempDir() + myModuleName + ta::ExceptionDumper::DumpExt;
        const std::string myMiniDumpReportPath = ta::Process::getTempDir() + myModuleName + ta::ExceptionDumper::DumpReportExt;
        remove(myMiniDumpPath.c_str());
        remove(myMiniDumpReportPath.c_str());

        TS_ASSERT(!ta::isFileExist(myMiniDumpPath));
        TS_ASSERT(!ta::isFileExist(myMiniDumpReportPath));

        bool myIsExceptionFired = fireAV(myModuleName);
        TS_ASSERT(myIsExceptionFired);
        TS_ASSERT(ta::isFileExist(myMiniDumpPath));
        TS_ASSERT(ta::isFileExist(myMiniDumpReportPath));

        const std::string myReportContent = ta::readData(myMiniDumpReportPath);
		TS_ASSERT(myReportContent.find(myModuleName) != std::string::npos);

        remove(myMiniDumpPath.c_str());
        remove(myMiniDumpReportPath.c_str());
    }
};
