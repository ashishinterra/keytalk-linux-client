//----------------------------------------------------------------------------
//
//  Description : ExceptionDumper API implementation
//                Adapted from http://www.codeproject.com/debug/XCrashReportPt4.asp
//
//----------------------------------------------------------------------------

#ifdef _WIN32
#pragma message("automatically link to DbgHelp.lib")
#pragma comment(lib, "DbgHelp.lib")
#pragma warning (disable: 4312)

#include "ExceptionDumper.h"
#include "FileVersionInfo.h"
#include "ta/osinfoutils.h"
#include "ta/process.h"
#include "ta/common.h"
#include <windows.h>
#include <tchar.h>
#include <dbghelp.h>
#include <stdexcept>

using std::string;

const int NumCodeBytes = 16;	// Number of code bytes to record.
const int MaxStackDump = 3072;	// Maximum number of DWORDS in stack dumps.
const int StackColumns = 4;		// Number of columns in stack dump.

#define	ONEK			1024
#define	SIXTYFOURK		(64*ONEK)
#define	ONEM			(ONEK*ONEK)
#define	ONEG			(ONEK*ONEK*ONEK)

namespace ta
{
    namespace ExceptionDumper
    {
        bool getModuleVerInfo(const char* aPath, char* aCompanyName, char* aProductName, char* aFileDescription, char* aFileVersion, char* aProductVersion);
        ///////////////////////////////////////////////////////////////////////////////
        // lstrrchr (avoid the C Runtime )
        static TCHAR* lstrrchr(LPCTSTR string, int ch)
        {

            TCHAR* start = (TCHAR*)string;

            while (*string++)                       /* find end of string */
                ;
            /* search towards front */
            while (--string != start && *string != (TCHAR) ch)
                ;

            if (*string == (TCHAR) ch)                /* char found ? */
                return (TCHAR*)string;

            return NULL;
        }


        // hprintf behaves similarly to printf, with a few vital differences.
        // It uses wvsprintf to do the formatting, which is a system routine,
        // thus avoiding C run time interactions. For similar reasons it
        // uses WriteFile rather than fwrite.
        // The one limitation that this imposes is that wvsprintf, and
        // therefore hprintf, cannot handle floating point numbers.

        // Too many calls to WriteFile can take a long time, causing
        // confusing delays when programs crash. Therefore I implemented
        // a simple buffering scheme for hprintf

#define HPRINTF_BUFFER_SIZE (8*1024)				// must be at least 2048
        static TCHAR hprintf_buffer[HPRINTF_BUFFER_SIZE];	// wvsprintf never prints more than one K.
        static int  hprintf_index = 0;

        ///////////////////////////////////////////////////////////////////////////////
        // hflush
        static void hflush(HANDLE LogFile)
        {
            if (hprintf_index > 0)
            {
                DWORD NumBytes;
                WriteFile(LogFile, hprintf_buffer, lstrlen(hprintf_buffer), &NumBytes, 0);
                hprintf_index = 0;
            }
        }

        ///////////////////////////////////////////////////////////////////////////////
        // hprintf
        static void hprintf(HANDLE LogFile, LPCTSTR Format, ...)
        {
            if (hprintf_index > (HPRINTF_BUFFER_SIZE-1024))
            {
                DWORD NumBytes;
                WriteFile(LogFile, hprintf_buffer, lstrlen(hprintf_buffer), &NumBytes, 0);
                hprintf_index = 0;
            }

            va_list arglist;
            va_start( arglist, Format);
            hprintf_index += wvsprintf(&hprintf_buffer[hprintf_index], Format, arglist);
            va_end( arglist);
        }

        ///////////////////////////////////////////////////////////////////////////////
        // DumpMiniDump
        static void DumpMiniDump(HANDLE hFile, PEXCEPTION_POINTERS excpInfo)
        {
            if (excpInfo == NULL)
            {
                // Generate exception to get proper context in dump
                __try
                {
                    RaiseException(EXCEPTION_BREAKPOINT, 0, 0, NULL);
                }
                __except(DumpMiniDump(hFile, GetExceptionInformation()), EXCEPTION_CONTINUE_EXECUTION)
                {}
            }
            else
            {
                MINIDUMP_EXCEPTION_INFORMATION eInfo;
                eInfo.ThreadId = GetCurrentThreadId();
                eInfo.ExceptionPointers = excpInfo;
                eInfo.ClientPointers = FALSE;

                MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal, excpInfo ? &eInfo : NULL, NULL, NULL);
            }
        }

        ///////////////////////////////////////////////////////////////////////////////
        // FormatTime
        // Format the specified FILETIME to output in a human readable format,
        // without using the C run time.
        static void FormatTime(LPTSTR output, FILETIME TimeToPrint)
        {
            output[0] = _T('\0');
            WORD Date, Time;
            if (FileTimeToLocalFileTime(&TimeToPrint, &TimeToPrint) &&
                    FileTimeToDosDateTime(&TimeToPrint, &Date, &Time))
            {
                wsprintf(output, _T("%d/%d/%d %02d:%02d:%02d"),
                         (Date / 32) & 15, Date & 31, (Date / 512) + 1980,
                         (Time >> 11), (Time >> 5) & 0x3F, (Time & 0x1F) * 2);
            }
        }

        ///////////////////////////////////////////////////////////////////////////////
        // DumpModuleInfo
        // Print information about a code module (DLL or EXE) such as its size,
        // location, time stamp, etc.
        static bool DumpModuleInfo(HANDLE LogFile, HINSTANCE ModuleHandle, int nModuleNo)
        {
            char szModName[MAX_PATH*2] = {};
            __try
            {
                if (!::GetModuleFileName(ModuleHandle, szModName, sizeof(szModName)-2))
                    return false;
                // If GetModuleFileName returns greater than zero then this must
                // be a valid code module address. Therefore we can try to walk
                // our way through its structures to find the link time stamp.
                IMAGE_DOS_HEADER* DosHeader = (IMAGE_DOS_HEADER*)ModuleHandle;
                if (IMAGE_DOS_SIGNATURE != DosHeader->e_magic)
                    return false;

                IMAGE_NT_HEADERS* NTHeader = (IMAGE_NT_HEADERS*)((TCHAR*)DosHeader
                                             + DosHeader->e_lfanew);
                if (IMAGE_NT_SIGNATURE != NTHeader->Signature)
                    return false;

                // open the code module file so that we can get its file date and size
                HANDLE ModuleFile = CreateFile(szModName, GENERIC_READ,
                                               FILE_SHARE_READ, 0, OPEN_EXISTING,
                                               FILE_ATTRIBUTE_NORMAL, 0);

                TCHAR TimeBuffer[100];
                TimeBuffer[0] = _T('\0');

                DWORD FileSize = 0;
                if (ModuleFile != INVALID_HANDLE_VALUE)
                {
                    FileSize = GetFileSize(ModuleFile, 0);
                    FILETIME LastWriteTime;
                    if (GetFileTime(ModuleFile, 0, 0, &LastWriteTime))
                        FormatTime(TimeBuffer, LastWriteTime);
                    CloseHandle(ModuleFile);
                }
                hprintf(LogFile, _T("Module %d\r\n"), nModuleNo);
                hprintf(LogFile, _T("%s\r\n"), szModName);
                hprintf(LogFile, _T("Image Base: 0x%08x  Image Size: 0x%08x\r\n"), NTHeader->OptionalHeader.ImageBase, NTHeader->OptionalHeader.SizeOfImage),
                        hprintf(LogFile, _T("Checksum:   0x%08x  Time Stamp: 0x%08x\r\n"), NTHeader->OptionalHeader.CheckSum, NTHeader->FileHeader.TimeDateStamp);
                hprintf(LogFile, _T("File Size:  %-10d  File Time:  %s\r\n"),  FileSize, TimeBuffer);

                char myCompanyName[256], myProductName[256], myFileDescription[1024], myFileVersion[256], myProductVersion[256];
                if (getModuleVerInfo(szModName, myCompanyName, myProductName, myFileDescription, myFileVersion, myProductVersion))
                {
                    hprintf(LogFile, _T("Version Information:\r\n"));
                    hprintf(LogFile, _T("   Company:    %s\r\n"), myCompanyName);
                    hprintf(LogFile, _T("   Product:    %s\r\n"), myProductName);
                    hprintf(LogFile, _T("   FileDesc:   %s\r\n"), myFileDescription);
                    hprintf(LogFile, _T("   FileVer:    %s\r\n"), myFileVersion);
                    hprintf(LogFile, _T("   ProdVer:    %s\r\n"), myProductVersion);
                }
                hprintf(LogFile, _T("\r\n"));
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
            return true;
        }

        bool getModuleVerInfo(const char* aPath, char* aCompanyName, char* aProductName, char* aFileDescription, char* aFileVersion, char* aProductVersion)
        {
            try
            {
                FileVersionInfo myModuleVersionInfo(aPath);
                strcpy(aCompanyName, myModuleVersionInfo.getCompanyName().c_str());
                strcpy(aProductName, myModuleVersionInfo.getProductName().c_str());
                strcpy(aFileDescription, myModuleVersionInfo.getFileDescription().c_str());
                strcpy(aFileVersion, myModuleVersionInfo.getFileVersion().c_str());
                strcpy(aProductVersion, myModuleVersionInfo.getProductVersion().c_str());
            }
            catch (const std::exception&)
            {
                return false;
            }
            return true;
        }

        ///////////////////////////////////////////////////////////////////////////////
        // DumpModuleList
        // Scan memory looking for code modules (DLLs or EXEs). VirtualQuery is used
        // to find all the blocks of address space that were reserved or committed,
        // and ShowModuleInfo will display module information if they are code
        // modules.
        static void DumpModuleList(HANDLE LogFile)
        {
            SYSTEM_INFO	SystemInfo;
            GetSystemInfo(&SystemInfo);

            const size_t PageSize = SystemInfo.dwPageSize;

            // Set NumPages to the number of pages in the 4GByte address space,
            // while being careful to avoid overflowing ints
            const size_t NumPages = 4 * size_t(ONEG / PageSize);
            size_t pageNum = 0;
            void* LastAllocationBase = 0;

            int nModuleNo = 1;

            while (pageNum < NumPages)
            {
                MEMORY_BASIC_INFORMATION MemInfo;
                if (VirtualQuery((void*)(pageNum * PageSize), &MemInfo,  sizeof(MemInfo)))
                {
                    if (MemInfo.RegionSize > 0)
                    {
                        // Adjust the page number to skip over this block of memory
                        pageNum += MemInfo.RegionSize / PageSize;
                        if (MemInfo.State == MEM_COMMIT && MemInfo.AllocationBase > LastAllocationBase)
                        {
                            // Look for new blocks of committed memory, and try
                            // recording their module names - this will fail
                            // gracefully if they aren't code modules
                            LastAllocationBase = MemInfo.AllocationBase;
                            if (DumpModuleInfo(LogFile, (HINSTANCE)LastAllocationBase, nModuleNo))
                                nModuleNo++;
                        }
                    }
                    else
                        pageNum += SIXTYFOURK / PageSize;
                }
                else
                    pageNum += SIXTYFOURK / PageSize;

                // If VirtualQuery fails we advance by 64K because that is the
                // granularity of address space doled out by VirtualAlloc()
            }
        }

        ///////////////////////////////////////////////////////////////////////////////
        // DumpSystemInformation
        // Record information about the user's system, such as processor type, amount
        // of memory, etc.
        static void DumpSystemInformation(HANDLE LogFile)
        {
            FILETIME CurrentTime;
            GetSystemTimeAsFileTime(&CurrentTime);
            TCHAR szTimeBuffer[100];
            FormatTime(szTimeBuffer, CurrentTime);

            hprintf(LogFile, _T("Error occurred at %s.\r\n"), szTimeBuffer);

            char szModuleName[MAX_PATH*2] = {};
            if (::GetModuleFileName(0, szModuleName, sizeof(szModuleName)-2) <= 0)
                lstrcpy(szModuleName, _T("Unknown"));

            char szUserName[200];
            ::ZeroMemory(szUserName, sizeof(szUserName));
            DWORD UserNameSize = sizeof(szUserName) - 2;
            if (!GetUserName(szUserName, &UserNameSize))
                lstrcpy(szUserName, _T("Unknown"));

            hprintf(LogFile, _T("%s, run by %s.\r\n"), szModuleName, szUserName);

            try
            {
                OsInfoUtils::Version myVer = OsInfoUtils::getVersion();
                hprintf(LogFile, _T("Operating system:  %s (%s).\r\n"), myVer.name.c_str(), myVer.ver.c_str());
            }
            catch(std::runtime_error&)
            {}

            SYSTEM_INFO	SystemInfo;
            GetSystemInfo(&SystemInfo);
            hprintf(LogFile, _T("%d processor(s), type %d.\r\n"),  SystemInfo.dwNumberOfProcessors, SystemInfo.dwProcessorType);

            MEMORYSTATUS MemInfo;
            MemInfo.dwLength = sizeof(MemInfo);
            GlobalMemoryStatus(&MemInfo);

            // Print out info on memory, rounded up.
            hprintf(LogFile, _T("%d%% memory in use.\r\n"), MemInfo.dwMemoryLoad);
            hprintf(LogFile, _T("%d MBytes physical memory.\r\n"), (MemInfo.dwTotalPhys + ONEM - 1) / ONEM);
            hprintf(LogFile, _T("%d MBytes physical memory free.\r\n"), (MemInfo.dwAvailPhys + ONEM - 1) / ONEM);
            hprintf(LogFile, _T("%d MBytes paging file.\r\n"), (MemInfo.dwTotalPageFile + ONEM - 1) / ONEM);
            hprintf(LogFile, _T("%d MBytes paging file free.\r\n"), (MemInfo.dwAvailPageFile + ONEM - 1) / ONEM);
            hprintf(LogFile, _T("%d MBytes user address space.\r\n"), (MemInfo.dwTotalVirtual + ONEM - 1) / ONEM);
            hprintf(LogFile, _T("%d MBytes user address space free.\r\n"), (MemInfo.dwAvailVirtual + ONEM - 1) / ONEM);
        }

        ///////////////////////////////////////////////////////////////////////////////
        // GetExceptionDescription
        // Translate the exception code into something human readable
        static const TCHAR* GetExceptionDescription(DWORD ExceptionCode)
        {
            struct ExceptionNames
            {
                DWORD	ExceptionCode;
                TCHAR* 	ExceptionName;
            };

            ExceptionNames ExceptionMap[] =
            {
                {0x40010005, _T("a Control-C")},
                {0x40010008, _T("a Control-Break")},
                {0x80000002, _T("a Datatype Misalignment")},
                {0x80000003, _T("a Breakpoint")},
                {0xc0000005, _T("an Access Violation")},
                {0xc0000006, _T("an In Page Error")},
                {0xc0000017, _T("a No Memory")},
                {0xc000001d, _T("an Illegal Instruction")},
                {0xc0000025, _T("a Noncontinuable Exception")},
                {0xc0000026, _T("an Invalid Disposition")},
                {0xc000008c, _T("a Array Bounds Exceeded")},
                {0xc000008d, _T("a Float Denormal Operand")},
                {0xc000008e, _T("a Float Divide by Zero")},
                {0xc000008f, _T("a Float Inexact Result")},
                {0xc0000090, _T("a Float Invalid Operation")},
                {0xc0000091, _T("a Float Overflow")},
                {0xc0000092, _T("a Float Stack Check")},
                {0xc0000093, _T("a Float Underflow")},
                {0xc0000094, _T("an Integer Divide by Zero")},
                {0xc0000095, _T("an Integer Overflow")},
                {0xc0000096, _T("a Privileged Instruction")},
                {0xc00000fD, _T("a Stack Overflow")},
                {0xc0000142, _T("a DLL Initialization Failed")},
                {0xe06d7363, _T("a Microsoft C++ Exception")},
            };

            for (int i = 0; i < sizeof(ExceptionMap) / sizeof(ExceptionMap[0]); i++)
                if (ExceptionCode == ExceptionMap[i].ExceptionCode)
                    return ExceptionMap[i].ExceptionName;

            return _T("an Unknown exception type");
        }

        ///////////////////////////////////////////////////////////////////////////////
        // GetFilePart
        static TCHAR* GetFilePart(LPCTSTR source)
        {
            TCHAR* result = lstrrchr(source, _T('\\'));
            if (result)
                result++;
            else
                result = (TCHAR*)source;
            return result;
        }

        ///////////////////////////////////////////////////////////////////////////////
        // DumpStack
        static void DumpStack(HANDLE LogFile, PCONTEXT Context)
        {
            DWORD* pStack = &Context->Esp;
            hprintf(LogFile, _T("\r\n\r\nStack:\r\n"));

            __try
            {
                // Esp contains the bottom of the stack, or at least the bottom of
                // the currently used area.
                DWORD* pStackTop;

                __asm
                {
                    // Load the top (highest address) of the stack from the
                    // thread information block. It will be found there in
                    // Win9x and Windows NT.
                    mov	eax, fs:[4]
                    mov pStackTop, eax
                }

                if (pStackTop > pStack + MaxStackDump)
                    pStackTop = pStack + MaxStackDump;

                int Count = 0;

                DWORD* pStackStart = pStack;

                int nDwordsPrinted = 0;

                while (pStack + 1 <= pStackTop)
                {
                    if ((Count % StackColumns) == 0)
                    {
                        pStackStart = pStack;
                        nDwordsPrinted = 0;
                        hprintf(LogFile, _T("0x%08x: "), pStack);
                    }

                    if ((++Count % StackColumns) == 0 || pStack + 2 > pStackTop)
                    {
                        hprintf(LogFile, _T("%08x "), *pStack);
                        nDwordsPrinted++;

                        int n = nDwordsPrinted;
                        while (n < 4)
                        {
                            hprintf(LogFile, _T("         "));
                            n++;
                        }

                        for (int i = 0; i < nDwordsPrinted; i++)
                        {
                            DWORD dwStack = *pStackStart;
                            for (int j = 0; j < 4; j++)
                            {
                                char c = (char)(dwStack & 0xFF);
                                if (c < 0x20 || c > 0x7E)
                                    c = '.';
#ifdef _UNICODE
                                WCHAR w = (WCHAR)c;
                                hprintf(LogFile, _T("%c"), w);
#else
                                hprintf(LogFile, _T("%c"), c);
#endif
                                dwStack = dwStack >> 8;
                            }
                            pStackStart++;
                        }

                        hprintf(LogFile, _T("\r\n"));
                    }
                    else
                    {
                        hprintf(LogFile, _T("%08x "), *pStack);
                        nDwordsPrinted++;
                    }
                    pStack++;
                }
                hprintf(LogFile, _T("\r\n"));
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                hprintf(LogFile, _T("Exception encountered during stack dump.\r\n"));
            }
        }

        static void DumpInstructinPointer(HANDLE LogFile, PCONTEXT Context)
        {
            // Since the crash may have been caused by an instruction pointer that was bad,
            // this code needs to be wrapped in an exception handler, in case there
            // is no memory to read. If the dereferencing of code[] fails, the
            // exception handler will print '??'.
            hprintf(LogFile, _T("\r\nBytes at CS:EIP:\r\n"));
            BYTE* code = (BYTE*)Context->Eip;
            for (int codebyte = 0; codebyte < NumCodeBytes; codebyte++)
            {
                __try
                {
                    hprintf(LogFile, _T("%02x "), code[codebyte]);

                }
                __except(EXCEPTION_EXECUTE_HANDLER)
                {
                    hprintf(LogFile, _T("?? "));
                }
            }
        }

        ///////////////////////////////////////////////////////////////////////////////
        // DumpRegisters
        static void DumpRegisters(HANDLE LogFile, PCONTEXT Context)
        {
            // Print out the register values in an XP error window compatible format.
            hprintf(LogFile, _T("\r\n"));
            hprintf(LogFile, _T("Context:\r\n"));
            hprintf(LogFile, _T("EDI:    0x%08x  ESI: 0x%08x  EAX:   0x%08x\r\n"),
                    Context->Edi, Context->Esi, Context->Eax);
            hprintf(LogFile, _T("EBX:    0x%08x  ECX: 0x%08x  EDX:   0x%08x\r\n"),
                    Context->Ebx, Context->Ecx, Context->Edx);
            hprintf(LogFile, _T("EIP:    0x%08x  EBP: 0x%08x  SegCs: 0x%08x\r\n"),
                    Context->Eip, Context->Ebp, Context->SegCs);
            hprintf(LogFile, _T("EFlags: 0x%08x  ESP: 0x%08x  SegSs: 0x%08x\r\n"),
                    Context->EFlags, Context->Esp, Context->SegSs);
        }



        int __cdecl dump(PEXCEPTION_POINTERS pExceptPtrs, const string& aModuleName)
        {
            static bool bFirstTime = true;
            if (!bFirstTime)	// Going recursive! That must mean this routine crashed!
                return EXCEPTION_CONTINUE_SEARCH;
            bFirstTime = false;

            char myTempDir[MAX_PATH+1];
            if (!::GetTempPath (sizeof(myTempDir)-1, myTempDir))
                return EXCEPTION_CONTINUE_SEARCH;
            const string myMiniDumpPath = myTempDir + aModuleName + DumpExt;
            const string myDumpReportPath = myTempDir + aModuleName + DumpReportExt;

            string myShortName = "<unknown>";
            unsigned long myPid = 0;
            try
            {
                myShortName = Process::getSelfShortName();
                myPid = Process::getSelfPid();
            }
            catch (ProcessGetNameError&)
            {}

            HANDLE hLogFile = ::CreateFile(myDumpReportPath.c_str(), GENERIC_WRITE, 0, 0,
                                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, 0);

            if (hLogFile == INVALID_HANDLE_VALUE)
                return EXCEPTION_CONTINUE_SEARCH;

            ::SetFilePointer(hLogFile, 0, 0, FILE_END);

            PEXCEPTION_RECORD Exception = pExceptPtrs->ExceptionRecord;
            PCONTEXT          Context   = pExceptPtrs->ContextRecord;

            char szCrashModulePathName[MAX_PATH*2] = {};
            TCHAR* pszCrashModuleFileName = _T("Unknown");

            MEMORY_BASIC_INFORMATION MemInfo;

            // VirtualQuery can be used to get the allocation base associated with a
            // code address, which is the same as the ModuleHandle. This can be used
            // to get the filename of the module that the crash happened in.
            if (::VirtualQuery((void*)Context->Eip, &MemInfo, sizeof(MemInfo)) &&
                    (::GetModuleFileName((HINSTANCE)MemInfo.AllocationBase,  szCrashModulePathName, sizeof(szCrashModulePathName)-2) > 0))
            {
                pszCrashModuleFileName = GetFilePart(szCrashModulePathName);
            }

            hprintf(hLogFile, _T("%s (PID %d) caused %s (0x%08x) \r\nin module %s at %04x:%08x.\r\n\r\n"),
                    myShortName.c_str(), myPid, GetExceptionDescription(Exception->ExceptionCode),
                    Exception->ExceptionCode,
                    pszCrashModuleFileName, Context->SegCs, Context->Eip);
            hprintf(hLogFile, _T("Exception handler called in %s.\r\n"), aModuleName.c_str());

            DumpSystemInformation(hLogFile);

            // If the exception was an access violation, print out some additional
            // information, to the error log and the debugger.
            if (Exception->ExceptionCode == STATUS_ACCESS_VIOLATION &&	Exception->NumberParameters >= 2)
            {
                TCHAR szDebugMessage[1000];
                const TCHAR* readwrite = _T("Read from");
                if (Exception->ExceptionInformation[0])
                    readwrite = _T("Write to");
                wsprintf(szDebugMessage, _T("%s location %08x caused an access violation.\r\n"), readwrite, Exception->ExceptionInformation[1]);
                hprintf(hLogFile, _T("%s"), szDebugMessage);
            }

            DumpRegisters(hLogFile, Context);
            DumpInstructinPointer(hLogFile, Context);
            DumpStack(hLogFile, Context);
            DumpModuleList(hLogFile);

            hflush(hLogFile);
            CloseHandle(hLogFile);


            HANDLE hMiniDumpFile = ::CreateFile(myMiniDumpPath.c_str(),
                                                GENERIC_WRITE,
                                                0,
                                                NULL,
                                                CREATE_ALWAYS,
                                                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
                                                NULL);

            // Write the minidump to the file
            if (hMiniDumpFile != INVALID_HANDLE_VALUE)
            {
                DumpMiniDump(hMiniDumpFile, pExceptPtrs);
                CloseHandle(hMiniDumpFile);
            }
            return EXCEPTION_CONTINUE_SEARCH;
        }
    }
}
#endif
