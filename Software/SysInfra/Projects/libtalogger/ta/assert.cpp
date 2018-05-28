#include "assert.h"
#include "logger.h"
#include "ta/process.h"
#include "ta/common.h"
#include <string>
#include <iostream>
#include "boost/format.hpp"

#ifdef _WIN32
# include <windows.h>
#endif

namespace ta
{
    // Private stuff
    namespace
    {
        void writeLog(const std::string& myMsg)
        {
            ERRORDEVLOG(myMsg);
        }

        void writeToConsole(const std::string& aMessage)
        {
            std::cerr << aMessage << std::endl;
        }
    }

    // Public stuff
    void assertion_failed(const std::string& anExpr, const std::string& aFunc, const std::string& aFile, unsigned int aLine)
    {
#if defined(_WIN32) && !defined(NDEBUG)
        if (::IsDebuggerPresent())
        {
            ::DebugBreak();
            return;
        }
#endif
        std::string myMsg = str(boost::format("Assertion failed! \nExpression: %1% \nFunction: %2%\nFile: %3%\nLine: %4%\nThe program will be terminated.") % anExpr % aFunc % aFile % aLine);
        writeLog(myMsg);
        try
        {
            Process::Subsystem mySelfSubsystem = Process::getSelfSubsystem();
            {
                switch (mySelfSubsystem)
                {
                case Process::Console:
                    writeToConsole(myMsg);
                    break;
                case Process::Window:
#ifdef _WIN32
                    ::MessageBox(NULL, myMsg.c_str(), "Error", MB_ICONERROR);
#endif
                    break;
                default:
                    break;
                }
            }
        }
        catch (...)
        {}
#if defined(_WIN32) && defined(_MSC_VER) && defined(NDEBUG)
        // VC8 allows supressing annoying abort message box, since the user has just seen our error report
        _set_abort_behavior( 0, _WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
        abort();
    }
}

