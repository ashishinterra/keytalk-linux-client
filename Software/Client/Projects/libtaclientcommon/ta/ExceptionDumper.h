//----------------------------------------------------------------------------
//
//  Description : ExceptionDumper utilites declaration
//                ExceptionDumper namespace contains API to catch and dump Win32 exceptions
//
//----------------------------------------------------------------------------
#ifndef	TA_EXCEPTIONDUMPER_H
#define	TA_EXCEPTIONDUMPER_H

#ifdef _WIN32

#include <string>
typedef struct _EXCEPTION_POINTERS *PEXCEPTION_POINTERS;

namespace ta
{
    namespace ExceptionDumper
    {
        static const std::string DumpExt  = ".dmp";
        static const std::string DumpReportExt  = ".dmp.txt";

        //
        // Abstract:   When Win32 exception occurs and caught inside the __except block, this function generates
        //             an appropriate error report and minidump and writes them to disk.
        //             Minidump file is written to %TMP%\<aModuleName><DumpExt>
        //             Dump report file is written to %TMP%\<aModuleName><DumpReportExt>
        //
        // Arguments: [in] exception info; this info is typically retrieved by calling GetExceptionInformation()
        //            [in]  aModuleName - name of this module to write to the error report
        //
        // Return:    EXCEPTION_CONTINUE_SEARCH
        //
        int __cdecl dump(PEXCEPTION_POINTERS data, const std::string& aModuleName);
    }
}
#endif

#endif
