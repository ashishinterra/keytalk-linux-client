#ifndef _WIN32
#error "Only Windows platform is supported"
#endif
#include "InternetExplorer.h"
#include "ta/registry.h"
#include "ta/process.h"
#include "ta/encodingutils.h"
#include "ta/strings.h"
#include "ta/utils.h"
#include "ta/dynlibloader.h"
#include "ta/ScopedResource.hpp"
#include "ta/common.h"
#include "boost/algorithm/string.hpp"
#include <stdexcept>
#include <windows.h>
#include <vector>
#include <Psapi.h>

using std::string;

namespace ta
{
    namespace
    {
        // throws std::runtime_error on error
        string getProgramFilesDir()
        {
            foreach (string myEnvVar, Process::getEnvVars())
            {
                string::size_type pos = myEnvVar.find("=");
                if ( pos != string::npos && boost::algorithm::iequals(myEnvVar.substr(0, pos), string("ProgramFiles")) )
                {
                    string myDir = myEnvVar.substr(pos+1);
                    if (boost::iends_with(myDir, "\\"))
                        myDir = myDir.erase(myDir.size()-1);
                    return myDir;
                }
            }
            TA_THROW_MSG(std::runtime_error, "ProgramFiles environment variable not found");
        }

        bool getProcInfo(unsigned int aProcId, std::string& aCmd, std::string& aCurDir)
        {
            // some stuff from Windows headers
#define ProcessBasicInformation 0
            typedef struct
            {
                USHORT Length;
                USHORT MaximumLength;
                PWSTR  Buffer;
            } UNICODE_STRING, *PUNICODE_STRING;

            typedef struct
            {
                ULONG          AllocationSize;
                ULONG          ActualSize;
                ULONG          Flags;
                ULONG          Unknown1;
                UNICODE_STRING Unknown2;
                HANDLE         InputHandle;
                HANDLE         OutputHandle;
                HANDLE         ErrorHandle;
                UNICODE_STRING CurrentDirectory;
                HANDLE         CurrentDirectoryHandle;
                UNICODE_STRING SearchPaths;
                UNICODE_STRING ApplicationName;
                UNICODE_STRING CommandLine;
                PVOID          EnvironmentBlock;
                ULONG          Unknown[9];
                UNICODE_STRING Unknown3;
                UNICODE_STRING Unknown4;
                UNICODE_STRING Unknown5;
                UNICODE_STRING Unknown6;
            } PROCESS_PARAMETERS, *PPROCESS_PARAMETERS;

            typedef struct
            {
                ULONG               AllocationSize;
                ULONG               Unknown1;
                HINSTANCE           ProcessHinstance;
                PVOID               ListDlls;
                PPROCESS_PARAMETERS ProcessParameters;
                ULONG               Unknown2;
                HANDLE              Heap;
            } PEB, *PPEB;

            typedef struct
            {
                DWORD ExitStatus;
                PPEB  PebBaseAddress;
                DWORD AffinityMask;
                DWORD BasePriority;
                ULONG UniqueProcessId;
                ULONG InheritedFromUniqueProcessId;
            }   PROCESS_BASIC_INFORMATION;

            typedef LONG (WINAPI *PROCNTQSIP)(HANDLE,UINT,PVOID,ULONG,PULONG);
            PROCNTQSIP  NtQueryInformationProcess = (PROCNTQSIP)::GetProcAddress(::GetModuleHandle("NTDLL"), "NtQueryInformationProcess");
            if (!NtQueryInformationProcess)
                return false;
            ta::ScopedResource<HANDLE> myProcess (::OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, aProcId ), ::CloseHandle);
            if (!myProcess)
                return false;

            PROCESS_BASIC_INFORMATION pbi = {0};
            if (NtQueryInformationProcess(myProcess, ProcessBasicInformation, &pbi, sizeof(pbi), 0))
                return false;

            PEB      Peb = {0};
            DWORD    dwDummy;
            if (!::ReadProcessMemory( myProcess, pbi.PebBaseAddress, &Peb, sizeof(PEB), &dwDummy))
                return false;
            PROCESS_PARAMETERS        ProcParam = {0};
            if (!::ReadProcessMemory( myProcess, Peb.ProcessParameters, &ProcParam, sizeof(PROCESS_PARAMETERS),&dwDummy))
                return false;

            LPVOID lpAddress = ProcParam.CommandLine.Buffer;
            DWORD dwSize = ProcParam.CommandLine.Length;
            wchar_t* wszBuf = new wchar_t[dwSize+1];
            memset(wszBuf, '\0', dwSize+1);
            if (!::ReadProcessMemory(myProcess, lpAddress, wszBuf, dwSize, &dwDummy) )
            {
                delete []wszBuf;
                return false;
            }
            aCmd = ta::EncodingUtils::toMbyte(wszBuf);
            delete []wszBuf;

            lpAddress = ProcParam.CurrentDirectory.Buffer;
            dwSize = ProcParam.CurrentDirectory.Length;
            wszBuf = new wchar_t[dwSize+1];
            memset(wszBuf, '\0', dwSize+1);
            if (!::ReadProcessMemory(myProcess, lpAddress, wszBuf, dwSize, &dwDummy) )
            {
                delete []wszBuf;
                return false;
            }
            aCurDir = ta::EncodingUtils::toMbyte(wszBuf);
            delete []wszBuf;

            return true;
        }

        bool isImageName(unsigned int aProcId, const std::string& aShortName, std::string& aFullName)
        {
            ta::ScopedResource<HANDLE> myProcess (::OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, aProcId ), ::CloseHandle);
            if (!myProcess)
                return false;
            HMODULE myModule;
            DWORD cbNeeded;
            if ( !::EnumProcessModules( myProcess, &myModule, sizeof(myModule), &cbNeeded) )
                return false;
            char szProcessShortName[MAX_PATH+1] = {};
            if (!::GetModuleBaseName( myProcess, myModule, szProcessShortName, sizeof(szProcessShortName)-1 ))
                return false;
            if ( boost::iequals(boost::trim_copy(std::string(szProcessShortName)), boost::trim_copy(aShortName)) )
            {
                char szProcessFullName[MAX_PATH+1] = {};
                if (!::GetModuleFileNameEx( myProcess, myModule, szProcessFullName, sizeof(szProcessFullName)-1 ))
                    return false;
                aFullName = szProcessFullName;
                return true;
            }
            return false;
        }

        // throw std::runtime_error on error
        void spawn(const std::string& aCmd, const std::string& aCurDir)
        {
            STARTUPINFO si = { sizeof(STARTUPINFO) };
            PROCESS_INFORMATION pi = {0};
            char* myCmd = new char[aCmd.length() + 1];
            strcpy(myCmd, aCmd.c_str());
            BOOL myRet = ::CreateProcess ( NULL, myCmd, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, (!aCurDir.empty() ? aCurDir.c_str() : NULL), &si, &pi );
            delete []myCmd;
            if (!myRet)
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Failed to spawn %s. Last error: %d") % aCmd % ::GetLastError());
            }
            // @todo On Vista+ we get here but the process is not spawned
            // On such OSs consider Use RestartManager APIs for it
            // example: http://blogs.msdn.com/b/vistacompatteam/archive/2007/02/07/internet-explorer-caches-settings.aspx

            ::CloseHandle (pi.hProcess);
            ::CloseHandle (pi.hThread);
        }
    }

    namespace InternetExplorer
    {
        bool isInstalled()
        {
            const string myAppPath = getProgramFilesDir() + "\\Internet Explorer\\iexplore.exe";
            return ta::isFileExist(myAppPath);
        }

        Version getVersion()
        {
            if (!isInstalled())
                TA_THROW_MSG(std::runtime_error, "Internet Explorer is not installed");
            string myVersionStr;
            Registry::read(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Internet Explorer","Version", myVersionStr);
            std::vector<string> myParts;
            boost::split(myParts, myVersionStr, boost::is_any_of("."));
            if (myParts.size() < 2)
                TA_THROW_MSG(std::runtime_error, boost::format("Ill-formed IE version '%s'") % myVersionStr);
            try
            {
                Version myVersion = {0};
                myVersion.major = ta::Strings::parse<unsigned long>(myParts[0]);
                myVersion.minor = ta::Strings::parse<unsigned long>(myParts[1]);
                if (myParts.size() >= 3)
                    myVersion.subminor = ta::Strings::parse<unsigned long>(myParts[2]);
                if (myParts.size() >= 4)
                    myVersion.revision = ta::Strings::parse<unsigned long>(myParts[3]);
                return myVersion;
            }
            catch (std::bad_cast& e)
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Ill-formed config version '%s'. %s") % myVersionStr % e.what());
            }
        }

        string getInstallDir()
        {
            if (!isInstalled())
                TA_THROW_MSG(std::runtime_error, "Internet Explorer is not installed");
            return getProgramFilesDir() + "\\Internet Explorer";
        }

        ProtectedMode getProtectedMode()
        {
            if (!boost::iequals(ta::Process::getSelfShortName(), "IEXPLORE"))
                return protectedModeNotIeProcess;
            try
            {
                typedef HRESULT (APIENTRY *LPFN_ISIEPROTECTEDMODE) (BOOL* aResult);
                ta::DynLibLoader myIeFrameDll("ieframe.dll");
                LPFN_ISIEPROTECTEDMODE fnIsIeProtectedMode = (LPFN_ISIEPROTECTEDMODE)myIeFrameDll.getFuncPtr("IEIsProtectedModeProcess");
                BOOL myIsProtectedMode = FALSE;
                HRESULT hr = fnIsIeProtectedMode (&myIsProtectedMode);
                if (SUCCEEDED(hr) && myIsProtectedMode)
                    return protectedModeOn;
                return protectedModeOff;
            }
            catch (std::runtime_error&)
            {
                return protectedModeOff;
            }
        }

        string getProtectedModeTempDir()
        {
            ProtectedMode myProtectedMode = getProtectedMode();
            if (myProtectedMode == protectedModeOff)
                TA_THROW_MSG(std::runtime_error, "IE Protected mode is Off");
            if (myProtectedMode == protectedModeNotIeProcess)
                TA_THROW_MSG(std::runtime_error, "The function should be called from IE to check whether IE is running in protected mode");
            return ta::Process::getLocalAppDataLowDir();
        }

        void restartIeUser()
        {
            static const std::string IeUserProcName = "ieuser.exe";
            static const std::string DefIeUserArgs = "-Embedding";

            std::vector<unsigned long> myPids = ta::Process::getAllPids();
            std::string myProcFullName;
            foreach (unsigned long pid, myPids)
            {
                if (isImageName(pid, IeUserProcName, myProcFullName))
                {
                    std::string myCmd, myCurDir;
                    if (!getProcInfo(pid, myCmd, myCurDir))
                    {
                        myCmd = str(boost::format("\"%s\" %s") % myProcFullName % DefIeUserArgs);
                    }
                    ta::Process::kill(IeUserProcName);
                    spawn(myCmd, myCurDir);
                }
            }
        }

    }
}
