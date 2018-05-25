//----------------------------------------------------------------------------
//
//  Description : RESEPT desktop client application entry point
//
//----------------------------------------------------------------------------
#include "ReseptDesktopClientApp.h"
#ifdef _WIN32
#include "rclient/Common.h"
#include "ta/ExceptionDumper.h"
#include "ta/process.h"

#include <windows.h>
#include <string>
#include <stdio.h>
#endif

static int doMain(int argc, char* argv[])
{
    try
    {
        ReseptDesktopClientApp myApp(argc, argv);
        myApp.execute();
    }
    catch (ReseptDesktopClientAppError&)
    {
        return -1;
    }
    catch (...)
    {
        return -2;
    }
    return 0;
}

#ifdef _WIN32
static int main_SEH_Wrapper(int argc, char* argv[], const std::string& aModuleName, const std::string& aMiniDumpPath, const std::string& aMiniDumpReportPath)
{
    __try
    {
        return doMain(argc, argv);
    }
    __except (ta::ExceptionDumper::dump(GetExceptionInformation(), aModuleName), EXCEPTION_EXECUTE_HANDLER)
    {
        char myMsg[1024*10] = {};
        if (!aMiniDumpPath.empty() && !aMiniDumpReportPath.empty())
        {
            _snprintf(myMsg, sizeof(myMsg)-1, "%s has encountered a problem and needs to close. We are sorry for the inconvenience."
                      " The following files are included in the error report:\n%s\n%s",
                      aModuleName.c_str(), aMiniDumpReportPath.c_str(), aMiniDumpPath.c_str());
        }
        else
        {
            _snprintf(myMsg, sizeof(myMsg)-1, "%s has encountered a problem and needs to close. We are sorry for the inconvenience.",
                      aModuleName.c_str());

        }
        ::MessageBox(NULL, myMsg, "ReseptClient error", MB_ICONEXCLAMATION);
        return -3;
    }
}
#endif


// Entry point
//
// Return 0 on success,
//        -1 if expected C++ exception was caught
//        -2 if unexpected C++ exception was caught
//        Windows only: -3 if unexpected Windows Exception was caught
int main(int argc, char* argv[])
{
#ifdef _WIN32
    std::string myMiniDumpPath, myMiniDumpReportPath;
    try
    {
        myMiniDumpPath = ta::Process::getTempDir() + rclient::ReseptDesktopClient + ta::ExceptionDumper::DumpExt;
        myMiniDumpReportPath = ta::Process::getTempDir() + rclient::ReseptDesktopClient + ta::ExceptionDumper::DumpReportExt;
    }
    catch (...)
    {}
    return main_SEH_Wrapper(argc, argv, rclient::ReseptDesktopClient, myMiniDumpPath, myMiniDumpReportPath);
#else
    return doMain(argc, argv);
#endif
}
