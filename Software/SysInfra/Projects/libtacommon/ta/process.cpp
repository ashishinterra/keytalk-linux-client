#include "process.h"
#include "strings.h"
#include "encodingutils.h"
#include "timeutils.h"
#include "dynlibloader.h"
#include "scopedresource.hpp"
#include "utils.h"
#include "common.h"
#include "ta/logger.h"

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#include <ImageHlp.h>
#include <Lmcons.h>
#include <Psapi.h>
#include <shlobj.h>
#define PIPE_ENDPOINT HANDLE
#else
#include <ctime>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wordexp.h>
#define PIPE_ENDPOINT int
#endif

#include <cassert>
#include <algorithm>
#include <iterator>
#include <sstream>
#include <iostream>
#include <queue>
#include "boost/algorithm/string.hpp"
#include "boost/assign/list_of.hpp"
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/convenience.hpp"
#include "boost/range/algorithm.hpp"
#include "boost/regex.hpp"

namespace ta
{
    using std::string;
    using std::vector;

#ifdef _WIN32
    const int MaxCmdLength = 8191;
#endif

    struct ReadFromPipeException : std::runtime_error
    {
        explicit ReadFromPipeException(const string& aMessage = "")	: std::runtime_error(aMessage) {}
    };
    struct WriteToPipeException : std::runtime_error
    {
        explicit WriteToPipeException(const string& aMessage = "")	: std::runtime_error(aMessage) {}
    };

    namespace Process
    {
        // Private stuff
        namespace
        {
#if defined(__linux__)
            int closefrom(int lowfd)
            {
                rlimit rl;
                getrlimit(RLIMIT_NOFILE, &rl);
                for (unsigned int i = lowfd; i < rl.rlim_max; ++i)
                    close(i);
                return 0;
            }
#endif

#ifndef _WIN32
            std::vector<unsigned long> getChildPids(unsigned long aPid)
            {
                const string myCommand = str(boost::format("pgrep -P %u") % aPid);
                vector<unsigned long> myChildPids;
                string myStdOut, myStdErr;
                int myExecCode = shellExecSync(myCommand, myStdOut, myStdErr);
                if (myExecCode == 1) // pgrep 1 if nothing found
                    return myChildPids;
                if (myExecCode != 0)
                    TA_THROW_MSG(std::runtime_error, boost::format("Command '%1%' finished with error code %2%") % myCommand % myExecCode);
                std::istringstream mySs(myStdOut);
                std::copy(std::istream_iterator<unsigned long>(mySs), std::istream_iterator<unsigned long>(), std::back_inserter(myChildPids));
                boost::sort(myChildPids);
                myChildPids.erase(std::unique(myChildPids.begin(), myChildPids.end()), myChildPids.end());
                return myChildPids;
            }
#endif

#ifdef _WIN32
            bool isImageName(unsigned int aProcId, const string& aName)
            {
                ta::ScopedResource<HANDLE> myProcess(::OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, aProcId ), CloseHandle);
                if (!myProcess)
                    return false;
                HMODULE myModule;
                DWORD cbNeeded;
                if ( !::EnumProcessModules( myProcess, &myModule, sizeof(myModule), &cbNeeded) )
                    return false;
                char szProcessName[MAX_PATH+1] = {};
                if (!::GetModuleBaseName( myProcess, myModule, szProcessName, sizeof(szProcessName)-1 ))
                    return false;
                if (boost::iequals(boost::trim_copy(string(szProcessName)), boost::trim_copy(aName)))
                    return true;
                return false;
            }
#endif

            // @throw ReadFromPipeException
            std::vector<unsigned char> readFromPipe(PIPE_ENDPOINT aFrom)
            {
                std::vector<unsigned char> myRetVal;
                char myBuf[128];
                while (true)
                {
#ifdef _WIN32
                    DWORD myRead = 0;
                    if( !::ReadFile(aFrom, myBuf, sizeof(myBuf)-1, &myRead, NULL))
                    {
                        DWORD myLastError = ::GetLastError();
                        if (myLastError == ERROR_BROKEN_PIPE)
                            break;
                        TA_THROW_MSG(ReadFromPipeException, boost::format("Last error is %1%") % myLastError);
                    }
#else
                    ssize_t myRead = read( aFrom, myBuf, sizeof(myBuf)-1);
                    if (myRead == (ssize_t)-1)
                        TA_THROW_MSG(ReadFromPipeException, strerror(errno));
#endif
                    if (myRead == 0)
                        break;
                    myRetVal.insert(myRetVal.end(), myBuf, myBuf + myRead);
                }
                return myRetVal;
            }

            // Performs a best-effort reading stdout and stderr output
            //@nothrow
            void readStdOutputsSafe(PIPE_ENDPOINT anStdOutHandle, PIPE_ENDPOINT anStdErrHandle, std::vector<unsigned char>& anStdOut, std::vector<unsigned char>& anStdErr)
            {
                anStdOut.clear();
                anStdErr.clear();

                try
                {
                    anStdOut = readFromPipe(anStdOutHandle);
                }
                catch (std::exception& e)
                {
                    anStdOut = str2Vec<unsigned char>(str(boost::format("<Failed to read stdout from pipe. %s>") % e.what()));
                }
                try
                {
                    anStdErr = readFromPipe(anStdErrHandle);
                }
                catch (std::exception& e)
                {
                    anStdErr = str2Vec<unsigned char>(str(boost::format("<Failed to read stderr from pipe. %s>") % e.what()));
                }
            }

            //@throw WriteToPipeException
            void writeToPipe(PIPE_ENDPOINT aTo, const std::vector<unsigned char>& anStdin)
            {
                if (anStdin.empty())
                    return;
                size_t myRemain = anStdin.size();
                const unsigned char* myBufPtr = &anStdin[0];
                while (myRemain > 0)
                {
#ifdef _WIN32
                    DWORD myWritten;
                    if(!::WriteFile(aTo, myBufPtr, (DWORD)myRemain, &myWritten, NULL))
                    {
                        DWORD myLastError = ::GetLastError();
                        if (myLastError == ERROR_BROKEN_PIPE)
                            break;
                        TA_THROW_MSG(WriteToPipeException, boost::format("Last error is %1%") % myLastError);
                    }
#else
                    ssize_t myWritten = write( aTo, myBufPtr, myRemain);
                    if (myWritten == (ssize_t)-1)
                        TA_THROW_MSG(WriteToPipeException, strerror(errno));
#endif
                    myBufPtr += myWritten;
                    myRemain -= myWritten;
                }
            }

#ifdef _WIN32
            // Make command:
            // - prepend special characters (special is one of "&<>()@^|") with '^'
            // - provided ate least one special character has been quoted and the command contains inner whitespace:
            //   - enclose the whole command into '"' quotes
            //   - write the command to the temporary batch file and return the path to it.
            //
            // The trick with the bat file is needed to handle command containing both whitespace and special characters e.g. C:\Program Files\AT & T\...
            // Taken from http://ss64.org/viewtopic.php?id=32
            //
            // throw ProcessExecError
            string makeCommand(const string& aCmd)
            {
                // @note: "^" should go first so it will not quote itself
                static const char* SpecialChars[] = {"^", "&","(",")","@"};
                string myQuotedCmd = boost::trim_copy(aCmd);

                for (size_t i = 0; i < sizeof(SpecialChars)/sizeof(SpecialChars[0]); ++i)
                    boost::replace_all(myQuotedCmd, SpecialChars[i], string("^")+SpecialChars[i]);
                bool myIsQuoted = (myQuotedCmd != aCmd);

                if (myIsQuoted && myQuotedCmd.find_first_of(" \t") != string::npos)
                {
                    if (myIsQuoted)
                        myQuotedCmd = "\"" + myQuotedCmd + "\"";
                    char mySzTempDir[MAX_PATH+1] = {};
                    if (!::GetTempPath (sizeof(mySzTempDir)-1, mySzTempDir))
                        TA_THROW_MSG(ProcessExecError, boost::format("Failed to get temporary folder path. Last error (%d)") % ::GetLastError());
                    string myBatFilePath = mySzTempDir + string("\\cmd_launcher.bat");
                    try {
                        writeData(myBatFilePath, "@echo off\r\nCMD /c " + myQuotedCmd);
                    } catch (std::runtime_error& e) {
                        TA_THROW_MSG(ProcessExecError, e.what());
                    }
                    return myBatFilePath;
                }
                return "CMD /c " + myQuotedCmd;
            }
#endif

        }

        //
        // Public stuff
        //

        unsigned long getSelfPid()
        {
#ifdef _WIN32
            return static_cast<unsigned long>(_getpid());
#else
            return static_cast<unsigned long>(getpid());
#endif
        }

        string getSelfShortName(RemoveExt aRemoveExt)
        {
            string myShortName;
#if defined(_WIN32) || defined(__linux__)
            const string myFullName = getSelfFullName();
            myShortName = myFullName.substr(myFullName.find_last_of(ta::getDirSep())+1);
            // string myStdOut, myStdErr;
            // const unsigned long mySelfPid = getSelfPid();
            // const string myCommand = str(boost::format("top -b -p %u") % mySelfPid);
            // try {
            //     int myExecCode = shellExecSync(myCommand, myStdOut, myStdErr);
            //     if (myExecCode != 0)
            //         TA_THROW_MSG(ProcessGetNameError, boost::format("Command %1% finished with error code %2%") % myCommand % myExecCode);
            // } catch(ProcessExecError& e) {
            //     TA_THROW_MSG(ProcessGetNameError, e.what());
            // }
            // boost::regex myRegEx(string("^\\s*PID\\s+USERNAME\\s+PRI\\s+NICE\\s+SIZE\\s+RES\\s+STATE\\s+WAIT\\s+TIME\\s+CPU\\s+COMMAND\\s*\\n") +
            //                      "\\s*" + ta::Strings::toString(mySelfPid) + "\\s+" +
            //                      "(?<username>\\w+)\\s+(?<pri>\\-?\\d+)\\s+(?<nice>\\-?\\d+)\\s+"
            //                      "(?<size>\\w+)\\s+(?<res>\\w+)\\s+(?<state>\\w+)\\s+(?<wait>[\\w\\-]+)\\s+"
            //                      "(?<time>\\d+\\:\\d+)\\s+(?<cpu>\\d+\\.\\d+\\%)\\s+(?<command>[^\\n]+)\\s*");
            // boost::cmatch myMatch;
            // if (!regex_search(myStdOut.c_str(), myMatch, myRegEx))
            // {
            //     TA_THROW_MSG(ProcessGetNameError, boost::format("Cannot parse executable name for process with pid %u from top output: %s") % mySelfPid % myStdOut);
            // }
            // myShortName = myMatch["command"];
#else
#error "Unsupported platform"
#endif
            if (aRemoveExt == extRemoveYes)
            {
                myShortName = myShortName.substr(0, myShortName.find_last_of('.'));
            }
            return myShortName;
        }

#if defined(_WIN32) || defined(__linux__)
        string getSelfFullName()
        {
#ifdef _WIN32
            char myFullName[MAX_PATH+1] = {};
            if (!::GetModuleFileName(NULL, myFullName, sizeof(myFullName)-1))
            {
                DWORD myErrorCode = ::GetLastError();
                TA_THROW_MSG(ProcessGetNameError, boost::format("GetModuleFileName returned error code (%1%)") % myErrorCode);
            }
            return myFullName;
#else
            char myFullName[PATH_MAX];
            ssize_t len = readlink("/proc/self/exe", myFullName, sizeof(myFullName)-1);
            if (len == -1)
            {
                TA_THROW_MSG(ProcessGetNameError, boost::format("Failed to retrieve a full path using /proc. %s") % strerror(errno));
            }
            myFullName[len] = '\0';
            return myFullName;
#endif
        }

        string getSelfDirName()
        {
            const string mySelfFullName = getSelfFullName();
            const string mySelfDir = mySelfFullName.substr(0, mySelfFullName.find_last_of(ta::getDirSep()));
            return mySelfDir;
        }
#endif

        vector<unsigned long> getAllPids()
        {
            vector<unsigned long> myAllPids;
#ifdef _WIN32
            static const vector<unsigned long>::size_type MaxNumOfPids = 1024;
            myAllPids.resize(MaxNumOfPids);
            unsigned long mySizeBytes;
            if (!::EnumProcesses( &myAllPids[0], MaxNumOfPids, &mySizeBytes))
                TA_THROW_MSG(std::runtime_error, boost::format("::EnumProcesses failed. Last error (%1%)") % ::GetLastError());
            size_t myNumOfPids = mySizeBytes / sizeof(vector<unsigned long>::value_type);
            myAllPids.resize(myNumOfPids);
#elif defined(__linux__)
            string myStdOut, myStdErr;
            const string myCommand = "ps -axo pid=";
            const int myExecCode = shellExecSync(myCommand, myStdOut, myStdErr);
            if (myExecCode != 0)
                TA_THROW_MSG(std::runtime_error, boost::format("Command %1% finished with error code %2%") % myCommand % myExecCode);
            std::istringstream mySs(myStdOut);
            std::copy(std::istream_iterator<unsigned long>(mySs), std::istream_iterator<unsigned long>(), std::back_inserter(myAllPids));
#endif
            boost::sort(myAllPids);
            myAllPids.erase(std::unique(myAllPids.begin(), myAllPids.end()), myAllPids.end());
            return myAllPids;
        }

        void kill(unsigned long aPid)
        {
#ifdef _WIN32
            HANDLE myProcessHandle = ::OpenProcess(PROCESS_TERMINATE, FALSE, aPid);
            if (!myProcessHandle)
                TA_THROW_MSG(std::runtime_error, boost::format("OpenProcess (PROCESS_TERMINATE) failed for id = %1%. Last error (%2%)") % aPid % ::GetLastError());
            if (!TerminateProcess(myProcessHandle, (DWORD)-1))
            {
                ::CloseHandle(myProcessHandle);
                TA_THROW_MSG(std::runtime_error, boost::format("TerminateProcess failed for id = %1%. Last error (%2%)") % aPid % ::GetLastError());
            }
            ::CloseHandle(myProcessHandle);
#else
            if (isRunning(aPid))
            {
                ::kill(aPid, SIGTERM);
                // Give the process time to handle SIGTERM
                TimeUtils::sleep(10);

                if (isRunning(aPid))
                {
                    int ret = ::kill (aPid, SIGKILL);
                    if (ret != 0 && errno != ESRCH)
                        TA_THROW_MSG(std::runtime_error, boost::format("kill(SIGKILL) failed for PID %1%. %2%") % aPid % strerror(errno));

                    // Give the kernel time to deliver the signal but do not wait on isRunning() until the process is actually stopped because
                    // the target process might be non-killable (zombie or 'init') or in uninterpretable sleep or can be that a new process with the same pid has been spawned during wait
                    TimeUtils::sleep(1);
                }
            }
#endif
        }

#ifndef _WIN32
        ExitWaiter::ExitWaiter()
            : thePid(0), theOwned(false), theKillOnExit(killOnExitNo)
        {}
        ExitWaiter::ExitWaiter(unsigned long pid, KillOnExit aKillOnExit)
            : thePid(pid), theOwned(true), theKillOnExit(aKillOnExit)
        {}
        ExitWaiter::~ExitWaiter()
        {
            if (theOwned && thePid > 0)
            {
                if (theKillOnExit == killOnExitYes)
                {
                    try { killTree(thePid); }
                    catch (...) {}
                }
                waitpid(thePid, NULL, 0);
            }
        }
        unsigned long ExitWaiter::release()
        {
            theOwned = false;
            return thePid;
        }
        void ExitWaiter::attach(unsigned int pid, KillOnExit aKillOnExit)
        {
            thePid = pid;
            theOwned = true;
            theKillOnExit = aKillOnExit;
        }
        bool ExitWaiter::owned() const
        {
            return theOwned;
        }
        unsigned long ExitWaiter::pid() const
        {
            return thePid;
        }

        vector<unsigned long> getPidTreeWithBreadthFirstTraversal(unsigned long aRootPid)
        {
            std::queue<unsigned long> myTraversalQueue;
            vector<unsigned long> myTree;

            myTraversalQueue.push(aRootPid);
            myTree.push_back(aRootPid);

            while (!myTraversalQueue.empty())
            {
                unsigned long myPid = myTraversalQueue.front();
                myTraversalQueue.pop();
                foreach(unsigned int childPid, getChildPids(myPid))
                {
                    myTree.push_back(childPid);
                    myTraversalQueue.push(childPid);
                }
            }
            return myTree;
        }

        void killTree(unsigned long aRootPid, IncludeRoot anIncludeRoot)
        {
            // Kill from parent towards childeren to avoid zombies
            foreach (unsigned long pid, getPidTreeWithBreadthFirstTraversal(aRootPid))
            {
                if (pid == aRootPid && anIncludeRoot == includeRootNo)
                {
                    continue;
                }
                kill(pid);
            }
        }

        vector<unsigned long> checkStoppedChildren()
        {
            vector<unsigned long> myStoppedChildPids;
            pid_t myStoppedChildPid;
            while ((myStoppedChildPid = waitpid(WAIT_ANY, NULL, WNOHANG)) > 0)
            {
                myStoppedChildPids.push_back(myStoppedChildPid);
                ta::Process::killTree(myStoppedChildPid, includeRootNo);
            }
            return myStoppedChildPids;
        }

        void waitForChildStop()
        {
            while (true)
            {
                TimeUtils::sleep(500);
                const vector<unsigned long> myStoppedChildPids = checkStoppedChildren();
                if (!myStoppedChildPids.empty())
                    TA_THROW_MSG(std::runtime_error, boost::format("%d child processes have been stopped. Exiting...") % myStoppedChildPids.size());
            }
        }
#endif


        size_t kill(const string& anImageName)
        {
            const vector<unsigned long> myPids = getPids(anImageName);
            foreach (unsigned long pid, myPids)
            {
                kill(pid);
            }
            return myPids.size();
        }

        bool isRunning(const string& anImageName)
        {
#if defined(_WIN32)
            vector<unsigned long> myPids = getAllPids();
            foreach (unsigned long pid, myPids)
            {
                if (isImageName(pid, anImageName))
                    return true;
            }
            return false;
#else
            string myStdOut, myStdErr;
            const string myCommand = str(boost::format("pkill -0 -x '%1%'") % anImageName);
            const int myExecCode = shellExecSync(myCommand, myStdOut, myStdErr);
            if (myExecCode == 0)
                return true;
            if (myExecCode == 1)
                return false;
            TA_THROW_MSG(std::runtime_error, boost::format("Command '%1%' finished with error code %2%") % myCommand % myExecCode);
#endif
        }

        bool isRunning(unsigned long aPid)
        {
#ifdef _WIN32
            return ta::isElemExist(aPid, getAllPids());
#else
            string myStdOut, myStdErr;
            const string myCommand = str(boost::format("kill -0 %1%") % aPid);
            const int myExecCode = shellExecSync(myCommand, myStdOut, myStdErr);
            if (myExecCode == 0)
                return true;
            if (myExecCode == 1)
                return false;
            TA_THROW_MSG(std::runtime_error, boost::format("Command '%1%' finished with error code %2%") % myCommand % myExecCode);
#endif
        }

        vector<unsigned long> getPids(const string& anImageName)
        {
            vector<unsigned long> myPids;
#if defined(_WIN32)
            foreach (unsigned long pid, getAllPids())
            {
                if (isImageName(pid, anImageName))
                {
                    myPids.push_back(pid);
                }
            }
#else
            string myStdOut, myStdErr;
            if (shellExecSync(str(boost::format("pidof '%s'") % anImageName), myStdOut, myStdErr) != 0)
            {
                return vector<unsigned long>();
            }
            std::istringstream mySs(myStdOut);
            std::copy(std::istream_iterator<unsigned long>(mySs), std::istream_iterator<unsigned long>(), std::back_inserter(myPids));
#endif
            boost::sort(myPids);
            myPids.erase(std::unique(myPids.begin(), myPids.end()), myPids.end());
            return myPids;
        }

        int shellExecSync(const string& aCommand)
        {
            string myStdOut, myStdErr;
            return shellExecSync(aCommand, myStdOut, myStdErr);
        }

#ifdef _WIN32
        int shellExecSync(const string& aCommand, std::vector<unsigned char>& anStdOut, std::vector<unsigned char>& anStdErr)
        {
            SECURITY_ATTRIBUTES mySa = { 0 };
            mySa.nLength = sizeof(mySa);
            mySa.lpSecurityDescriptor = 0;
            mySa.bInheritHandle = 1;
            HANDLE myStdOutRdHandle, myStdErrRdHandle, myStdOutWrHandle, myStdErrWrHandle;
            if (!::CreatePipe(&myStdOutRdHandle, &myStdOutWrHandle, &mySa, 0))
            {
                DWORD myLastError = ::GetLastError();
                TA_THROW_MSG(ProcessExecError, boost::format("Failed to create pipe. Last error is %1%") % myLastError);
            }
            if (!::CreatePipe(&myStdErrRdHandle, &myStdErrWrHandle, &mySa, 0))
            {
                DWORD myLastError = ::GetLastError();
                ::CloseHandle(myStdOutRdHandle), ::CloseHandle(myStdOutWrHandle);
                TA_THROW_MSG(ProcessExecError, boost::format("Failed to create pipe. Last error is %1%") % myLastError);
            }
            PROCESS_INFORMATION myPi = { 0 };
            STARTUPINFO mySi = { 0 };
            mySi.cb = sizeof(mySi);
            mySi.lpDesktop = "";
            mySi.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
            mySi.wShowWindow = SW_HIDE;
            mySi.hStdOutput = myStdOutWrHandle;
            mySi.hStdError = myStdErrWrHandle;

            char mySzCommand[MaxCmdLength + 1] = {};
            DWORD myExpandRet = ::ExpandEnvironmentStrings(aCommand.c_str(), mySzCommand, sizeof(mySzCommand) - 1);
            if (myExpandRet == 0)
            {
                DWORD myLastError = ::GetLastError();
                ::CloseHandle(myStdOutRdHandle), ::CloseHandle(myStdOutWrHandle), ::CloseHandle(myStdErrRdHandle), ::CloseHandle(myStdErrWrHandle);
                TA_THROW_MSG(ProcessExecError, boost::format("::ExpandEnvironmentStrings failed for command: '%s'. Last error: %d") % mySzCommand % myLastError);
            }
            if (myExpandRet > sizeof(mySzCommand) - 1)
            {
                ::CloseHandle(myStdOutRdHandle), ::CloseHandle(myStdOutWrHandle), ::CloseHandle(myStdErrRdHandle), ::CloseHandle(myStdErrWrHandle);
                TA_THROW_MSG(ProcessExecError, boost::format("Expanded command is too long. Command: %s") % mySzCommand);
            }
            string myCommand(makeCommand(mySzCommand));
            if (myCommand.size() > MaxCmdLength)
            {
                ::CloseHandle(myStdOutRdHandle), ::CloseHandle(myStdOutWrHandle), ::CloseHandle(myStdErrRdHandle), ::CloseHandle(myStdErrWrHandle);
                TA_THROW_MSG(ProcessExecError, boost::format("Command is too long. Command: '%s'") % myCommand);
            }

            // Create the process
            strncpy(mySzCommand, myCommand.c_str(), myCommand.size());
            mySzCommand[myCommand.size()] = '\0';
            DWORD myExitCode;
            if (!::CreateProcess(NULL, mySzCommand, NULL, NULL, TRUE, 0, 0, NULL, &mySi, &myPi))
            {
                DWORD myLastError = ::GetLastError();
                ::CloseHandle(myStdOutRdHandle), ::CloseHandle(myStdOutWrHandle), ::CloseHandle(myStdErrRdHandle), ::CloseHandle(myStdErrWrHandle);
                TA_THROW_MSG(ProcessExecError, boost::format("Failed to create process for %1%. Last error is %2%") % mySzCommand % myLastError);
            }

            // Wait for the process to exit
            //@todo If the process generates a lot of output thus filling the output pipe because we do not yet read it with readStdOutputsSafe()
            // This will end up in the process being blocked forever trying to write to the pipe.
            // Unfortunately calling readStdOutputsSafe() _before_ WaitForSingleObject like in non-windows code results in this function to be infinitely blocked for some reason...
            ::WaitForSingleObject(myPi.hProcess, INFINITE);
            if (!::GetExitCodeProcess(myPi.hProcess, &myExitCode))
            {
                DWORD myLastError = ::GetLastError();
                ::CloseHandle(myPi.hProcess), ::CloseHandle(myPi.hThread);
                ::CloseHandle(myStdOutWrHandle), ::CloseHandle(myStdErrWrHandle);
                ::CloseHandle(myStdOutRdHandle), ::CloseHandle(myStdErrRdHandle);
                TA_THROW_MSG(ProcessExecError, boost::format("Failed to retrieve command exit code. Last error is %1%") % myLastError);
            }
            ::CloseHandle(myPi.hProcess), ::CloseHandle(myPi.hThread);
            ::CloseHandle(myStdOutWrHandle), ::CloseHandle(myStdErrWrHandle);

            // Grab process output
            readStdOutputsSafe(myStdOutRdHandle, myStdErrRdHandle, anStdOut, anStdErr);
            ::CloseHandle(myStdOutRdHandle);
            ::CloseHandle(myStdErrRdHandle);

            return myExitCode;
        }

        int shellExecSync(const string& aCommand, string& anStdOut, string& anStdErr)
        {
            std::vector<unsigned char> myStdOut, myStdErr;
            const int myRetVal = shellExecSync(aCommand, myStdOut, myStdErr);
            anStdOut = ta::vec2Str(myStdOut);
            anStdErr = ta::vec2Str(myStdErr);
            return myRetVal;
        }

        void checkedShellExecSync(const string& aCommand)
        {
            string myStdOut, myStdErr;
            int myExecCode = shellExecSync(aCommand, myStdOut, myStdErr);
            if (myExecCode != 0)
            {
                TA_THROW_MSG(ProcessExecError, boost::format("Command '%s' finished with code %d. Stdout: %s. Stderr: %s") % aCommand % myExecCode % myStdOut % myStdErr);
            }
        }
#else
        int shellExecSync(const string& aCommand, std::vector<unsigned char>& anStdOut, std::vector<unsigned char>& anStdErr, const std::vector<unsigned char>& anStdIn)
        {
            int myChildStdinPipe[2]; // child reads from the pipe start
            int myChildStdoutPipe[2], myChildStderrPipe[2]; // child writes to the pipe end

            if (pipe(myChildStdoutPipe) != 0)
                TA_THROW_MSG(ProcessExecError, boost::format("pipe (1) failed. %s") % strerror(errno));

            if (pipe(myChildStderrPipe) != 0)
            {
                int myErrno = errno;
                close(myChildStdoutPipe[1]), close(myChildStdoutPipe[0]);
                TA_THROW_MSG(ProcessExecError, boost::format("pipe (2) failed. %s") % strerror(myErrno));
            }
            if (!anStdIn.empty())
            {
                if (pipe(myChildStdinPipe) != 0)
                {
                    int myErrno = errno;
                    close(myChildStdoutPipe[1]), close(myChildStdoutPipe[0]), close(myChildStderrPipe[0]), close(myChildStderrPipe[1]);
                    TA_THROW_MSG(ProcessExecError, boost::format("pipe (3) failed. %s") % strerror(myErrno));
                }
            }

            // create a new process
            pid_t myPid = fork();
            if (myPid < 0)
            {
                int myErrno = errno;
                if (!anStdIn.empty())
                {
                    close(myChildStdinPipe[0]);
                    close(myChildStdinPipe[1]);
                }
                close(myChildStdoutPipe[0]);
                close(myChildStdoutPipe[1]);
                close(myChildStderrPipe[0]);
                close(myChildStderrPipe[1]);
                TA_THROW_MSG(ProcessExecError, boost::format("fork failed. %s") % strerror(myErrno));
            }

            if (myPid == 0)
            {   //
                // child
                //

                // close unnecessary pipe fds
                close(myChildStdoutPipe[0]);
                close(myChildStderrPipe[0]);
                if (!anStdIn.empty())
                    close(myChildStdinPipe[1]);

                //  Close all open fds that may have been inherited from parent except for open pipe ends
                vector<long> myOpenFds = boost::assign::list_of(myChildStdoutPipe[1])(myChildStderrPipe[1]);
                if (!anStdIn.empty())
                    myOpenFds.push_back(myChildStdinPipe[0]);
                long myMaxOpenFd = *boost::max_element(myOpenFds);
                for (long fd = myMaxOpenFd; fd >= 0; fd--)
                {
                    if (!ta::isElemExist(fd, myOpenFds))
                        close(fd); // ignore errors
                }
                closefrom(myMaxOpenFd+1);

                // redirect our pipe endpoints to stdout and stderrr
                dup2(myChildStdoutPipe[1], STDOUT_FILENO);
                close(myChildStdoutPipe[1]);
                dup2(myChildStderrPipe[1], STDERR_FILENO);
                close(myChildStderrPipe[1]);

                // redirect stdin to read from pipe or from /dev/null
                if (!anStdIn.empty())
                {
                    dup2(myChildStdinPipe[0], STDIN_FILENO);
                    close(myChildStdinPipe[0]);
                }
                else
                {
                    int nullDev = open("/dev/null", O_RDONLY);
                    dup2(nullDev, STDIN_FILENO);
                    if (nullDev>2)
                        close(nullDev);
                }

                execl("/bin/sh", "sh", "-c", aCommand.c_str(), (char*)0);
                // if execl returns we probably have error
                exit(errno);
            }
            //
            // parent
            //

            // Close unused pipe ends. This is especially important for the
            // myChildStdoutPipe[1] and myChildStderrPipe[1] write descriptor, otherwise readFromPipe will never get an EOF.
            if (!anStdIn.empty())
                close(myChildStdinPipe[0]);
            close(myChildStdoutPipe[1]);
            close(myChildStderrPipe[1]);

            if (!anStdIn.empty())
            {
                try
                {
                    writeToPipe(myChildStdinPipe[1], anStdIn);
                }
                catch (std::exception& e)
                {
                    close(myChildStdinPipe[1]);
                    waitpid( myPid, NULL, 0) ; // we do not like zombies
                    close(myChildStdoutPipe[0]), close(myChildStderrPipe[0]);
                    TA_THROW_MSG(ProcessExecError, e.what());
                }
                // It is important to close stdin pipe endpoint otherwise the executed program will never get EOF while reading stdin
                close(myChildStdinPipe[1]);
            }

            // Grab process output
            readStdOutputsSafe(myChildStdoutPipe[0], myChildStderrPipe[0], anStdOut, anStdErr);
            close(myChildStdoutPipe[0]);
            close(myChildStderrPipe[0]);

            // wait for the process to exit. This is also important to avoid zombies
            int myExitStatus;
            if (waitpid( myPid, &myExitStatus, 0) == -1)
            {
                TA_THROW_MSG(ProcessExecError, boost::format("waitpid failed. %1%. Command '%2%'") % strerror(errno) % aCommand);
            }
            if (!WIFEXITED(myExitStatus))
            {
                TA_THROW_MSG(ProcessExecError, boost::format("Failed to launch '%1%'. %2%") % aCommand % strerror(errno));
            }

            const int myRetVal = WEXITSTATUS(myExitStatus);
            return myRetVal;
        }

        int shellExecSync(const string& aCommand, string& anStdOut, string& anStdErr, const string& anStdIn)
        {
            std::vector<unsigned char> myStdOut, myStdErr;
            const int myRetVal = shellExecSync(aCommand, myStdOut, myStdErr, ta::str2Vec<unsigned char>(anStdIn));
            anStdOut = ta::vec2Str(myStdOut);
            anStdErr = ta::vec2Str(myStdErr);
            return myRetVal;
        }

        void checkedShellExecSync(const string& aCommand, const std::string& anStdIn)
        {
            string myStdOut, myStdErr;
            int myExecCode = shellExecSync(aCommand, myStdOut, myStdErr, anStdIn);
            if (myExecCode != 0)
                TA_THROW_MSG(ProcessExecError, boost::format("Command '%s' finished with code %d. Stdout: %s. Stderr: %s") % aCommand % myExecCode % myStdOut % myStdErr);
        }
#endif

        bool shellExecAsync(const string& aCommand, std::vector<unsigned char>& anStdOut, std::vector<unsigned char>& anStdErr, unsigned int& anExitCode, unsigned int aMaxWaitTime)
        {
#ifdef _WIN32
            SECURITY_ATTRIBUTES mySa = {0};
            mySa.nLength = sizeof(mySa);
            mySa.lpSecurityDescriptor = 0;
            mySa.bInheritHandle = 1;
            HANDLE myStdOutRdHandle, myStdErrRdHandle, myStdOutWrHandle, myStdErrWrHandle;
            if (!::CreatePipe(&myStdOutRdHandle, &myStdOutWrHandle, &mySa, 0))
            {
                DWORD myLastError = ::GetLastError();
                TA_THROW_MSG(ProcessExecError, boost::format("Failed to create stdout pipe. Last error is %d") % myLastError);
            }
            if (!::CreatePipe(&myStdErrRdHandle, &myStdErrWrHandle, &mySa, 0))
            {
                DWORD myLastError = ::GetLastError();
                ::CloseHandle(myStdOutRdHandle),::CloseHandle(myStdOutWrHandle);
                TA_THROW_MSG(ProcessExecError, boost::format("Failed to create stderr cmdpipe. Last error is %d") % myLastError);
            }
            PROCESS_INFORMATION myPi = {0};
            STARTUPINFO mySi = {0};
            mySi.cb = sizeof(mySi);
            mySi.lpDesktop = "";
            mySi.lpDesktop = "";
            mySi.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
            mySi.wShowWindow = SW_HIDE;
            mySi.hStdOutput = myStdOutWrHandle;
            mySi.hStdError  = myStdErrWrHandle;

            char mySzCommand[MaxCmdLength+1] = {};
            DWORD myExpandRet = ::ExpandEnvironmentStrings(aCommand.c_str(), mySzCommand, sizeof(mySzCommand)-1);
            if (myExpandRet == 0)
            {
                DWORD myLastError = ::GetLastError();
                ::CloseHandle(myStdOutRdHandle),::CloseHandle(myStdOutWrHandle), ::CloseHandle(myStdErrRdHandle),::CloseHandle(myStdErrWrHandle);
                TA_THROW_MSG(ProcessExecError, boost::format("::ExpandEnvironmentStrings failed for command: '%s'. Last error: %d") % mySzCommand % myLastError);
            }
            if (myExpandRet > sizeof(mySzCommand)-1)
            {
                ::CloseHandle(myStdOutRdHandle),::CloseHandle(myStdOutWrHandle), ::CloseHandle(myStdErrRdHandle),::CloseHandle(myStdErrWrHandle);
                TA_THROW_MSG(ProcessExecError, boost::format("Expanded command is too long. Command: %s") % mySzCommand);
            }
            string myCommand(makeCommand(mySzCommand));
            if (myCommand.size() > MaxCmdLength)
            {
                ::CloseHandle(myStdOutRdHandle),::CloseHandle(myStdOutWrHandle), ::CloseHandle(myStdErrRdHandle),::CloseHandle(myStdErrWrHandle);
                TA_THROW_MSG(ProcessExecError, "Command is too long");
            }
            strncpy(mySzCommand, myCommand.c_str(), myCommand.size());
            mySzCommand[myCommand.size()] = '\0';

            // Create process
            if (!::CreateProcess(NULL, mySzCommand, NULL, NULL, TRUE, 0, 0, NULL, &mySi, &myPi))
            {
                DWORD myLastError = ::GetLastError();
                ::CloseHandle(myStdOutRdHandle),::CloseHandle(myStdOutWrHandle), ::CloseHandle(myStdErrRdHandle),::CloseHandle(myStdErrWrHandle);
                TA_THROW_MSG(ProcessExecError, boost::format("Failed to create process for %s. Last error is %d") % mySzCommand % myLastError);
            }

            //@todo If the process generates a lot of output before timeout reaches thus filling the output pipe because we do not yet read it with readStdOutputsSafe()
            // This will end up in the process being blocked forever trying to write to the pipe.
            bool myIsProcessFinished = (::WaitForSingleObject( myPi.hProcess, aMaxWaitTime ) == WAIT_OBJECT_0);
            if  (myIsProcessFinished)
            {
                // Process finished. Retrieve process exit code and stdout/stderr to help the user figure out what actualy happens.
                DWORD myExitCode;
                if (!::GetExitCodeProcess(myPi.hProcess, &myExitCode))
                {
                    DWORD myLastError = ::GetLastError();
                    ::CloseHandle(myPi.hProcess), ::CloseHandle(myPi.hThread);
                    ::CloseHandle(myStdOutRdHandle),::CloseHandle(myStdOutWrHandle), ::CloseHandle(myStdErrRdHandle),::CloseHandle(myStdErrWrHandle);
                    TA_THROW_MSG(ProcessExecError, boost::format("Failed to retrieve command exit code. Last error is %d") % myLastError);
                }
                anExitCode = myExitCode;
                ::CloseHandle(myPi.hProcess);
                ::CloseHandle(myPi.hThread);
                ::CloseHandle(myStdOutWrHandle);
                ::CloseHandle(myStdErrWrHandle);

                readStdOutputsSafe(myStdOutRdHandle, myStdErrRdHandle, anStdOut, anStdErr);
                ::CloseHandle(myStdOutRdHandle);
                ::CloseHandle(myStdErrRdHandle);

                return myIsProcessFinished;
            }

            ::CloseHandle(myStdOutWrHandle);
            ::CloseHandle(myStdErrWrHandle);
            return myIsProcessFinished;
#else
            int myChildStdoutPipe[2], myChildStderrPipe[2]; // child writes to the pipe end

            if (pipe(myChildStdoutPipe) != 0)
                TA_THROW_MSG(ProcessExecError, boost::format("pipe (1) failed. %s") % strerror(errno));

            if (pipe(myChildStderrPipe) != 0)
            {
                int myErrno = errno;
                close(myChildStdoutPipe[1]), close(myChildStdoutPipe[0]);
                TA_THROW_MSG(ProcessExecError, boost::format("pipe (2) failed. %s") % strerror(myErrno));
            }

            pid_t myPid = fork();
            if (myPid < 0)
            {
                int myErrno = errno;
                close(myChildStdoutPipe[0]);
                close(myChildStdoutPipe[1]);
                close(myChildStderrPipe[0]);
                close(myChildStderrPipe[1]);
                TA_THROW_MSG(ProcessExecError, boost::format("fork failed. %s") % strerror(myErrno));
            }
            if (myPid == 0)
            {   //
                // child
                //

                // close unnecessary pipe descriptors
                close(myChildStdoutPipe[0]);
                close(myChildStderrPipe[0]);

                //  Close all open file descriptors that may have been inherited from parent except for open pipe ends
                vector<long> myOpenFds = boost::assign::list_of(myChildStdoutPipe[1])(myChildStderrPipe[1]);
                long myMaxOpenFd = *boost::max_element(myOpenFds);
                for (long fd = myMaxOpenFd; fd >= 0; fd--)
                {
                    if (!ta::isElemExist(fd, myOpenFds))
                        close(fd); // ignore errors
                }
                closefrom(myMaxOpenFd+1);

                // redirect our pipe endpoints to stdout and stderrr
                dup2(myChildStdoutPipe[1], STDOUT_FILENO);
                close(myChildStdoutPipe[1]);
                dup2(myChildStderrPipe[1], STDERR_FILENO);
                close(myChildStderrPipe[1]);

                // redirect stdin to read from /dev/null
                int nullDev = open("/dev/null", O_RDONLY);
                dup2(nullDev, STDIN_FILENO);
                if (nullDev>2)
                    close(nullDev);

                execl("/bin/sh", "sh", "-c", aCommand.c_str(), (char*)0);
                // if execl returns we probably have error
                exit(errno);
            }
            //
            // parent
            //

            // Close unused pipe ends. This is especially important for the
            // myChildStdoutPipe[1] and myChildStderrPipe[1] write descriptor, otherwise readFromPipe will never get an EOF.
            close(myChildStdoutPipe[1]);
            close(myChildStderrPipe[1]);

            int myExitStatus;
            bool myIsChildFinished = false;
            TimeUtils::LocalTime myChildStartTime;
            while (true)
            {
                //@todo If the process generates a lot of output before timeout reaches thus filling the output pipe because we do not yet read it with readStdOutputsSafe().
                // This will end up in the process being blocked forever trying to write to the pipe.
                int myWaitRet = waitpid( myPid, &myExitStatus, WNOHANG);

                if (myWaitRet == -1) // error
                {
                    int myErrno = errno;
                    close(myChildStdoutPipe[0]), close(myChildStderrPipe[0]);
                    TA_THROW_MSG(ProcessExecError, boost::format("waitpid failed. %1%. Command '%2%'") % strerror(myErrno) % aCommand);
                }

                if (myWaitRet  == 0) // no finished process
                {
                    if (aMaxWaitTime == 0) // no wait
                        break;
                    TimeUtils::sleep(10);
                    if (TimeUtils::LocalTime() - myChildStartTime >= aMaxWaitTime)
                        break;
                    continue;
                }

                myIsChildFinished = true;
                break;
            }

            if (myIsChildFinished)
            {
                if (!WIFEXITED(myExitStatus))
                {
                    int myErrno = errno;
                    close(myChildStdoutPipe[0]), close(myChildStderrPipe[0]);
                    TA_THROW_MSG(ProcessExecError, boost::format("Failed to launch '%1%'. %2%") % aCommand % strerror(myErrno));
                }

                readStdOutputsSafe(myChildStdoutPipe[0], myChildStderrPipe[0], anStdOut, anStdErr);
                close(myChildStdoutPipe[0]);
                close(myChildStderrPipe[0]);

                anExitCode = WEXITSTATUS(myExitStatus);
            }
            else
            {
                close(myChildStdoutPipe[0]);
                close(myChildStderrPipe[0]);
            }
            return myIsChildFinished;
#endif
        }

        bool shellExecAsync(const string& aCommand, string& anStdOut, string& anStdErr, unsigned int& anExitCode, unsigned int aMaxWaitTime)
        {
            std::vector<unsigned char> myStdOut, myStdErr;
            const bool myRetVal = shellExecAsync(aCommand, myStdOut, myStdErr, anExitCode, aMaxWaitTime);
            anStdOut = ta::vec2Str(myStdOut);
            anStdErr = ta::vec2Str(myStdErr);
            return myRetVal;
        }

        bool shellExecAsync(const string& aCommand, unsigned int& anExitCode, unsigned int aMaxWaitTime)
        {
            std::vector<unsigned char> myDummyStdOut, myDymmyStdErr;
            return shellExecAsync(aCommand, myDummyStdOut, myDymmyStdErr, anExitCode, aMaxWaitTime);
        }

        Subsystem getSelfSubsystem()
        {
#ifdef _WIN32
            HMODULE myModuleHandle = ::GetModuleHandle(NULL);
            if (!myModuleHandle)
                TA_THROW_MSG(std::runtime_error, "GetModuleHandle");
            BYTE* myImagePtr = (BYTE*)myModuleHandle;
            PIMAGE_DOS_HEADER myDosPtr = (PIMAGE_DOS_HEADER)myImagePtr;
            if (myDosPtr->e_magic != IMAGE_DOS_SIGNATURE)
                TA_THROW_MSG(std::runtime_error, "Invalid image DOS signature");
            PIMAGE_NT_HEADERS myNtHeaderPtr = (IMAGE_NT_HEADERS*)((BYTE*)(myDosPtr) + (DWORD)(myDosPtr->e_lfanew));
            if (myNtHeaderPtr->Signature != IMAGE_NT_SIGNATURE)
                TA_THROW_MSG(std::runtime_error, "Invalid image NT signature");
            PIMAGE_OPTIONAL_HEADER myOptHeaderPtr = &myNtHeaderPtr->OptionalHeader;
            switch (myOptHeaderPtr->Subsystem)
            {
            case IMAGE_SUBSYSTEM_WINDOWS_GUI:
                return Window;
            case IMAGE_SUBSYSTEM_WINDOWS_CUI:
                return Console;
            default:
                TA_THROW_MSG(std::runtime_error, boost::format("Non-supported subsystem argument (%1%)") % myOptHeaderPtr->Subsystem);
            }
#else
            ///@todo ideas?
            return Console;
#endif
        }

        string getTempDir()
        {
            string myTempDir;
#ifdef _WIN32
            char mySzTempShortDir[MAX_PATH+1] = {};
            if (!::GetTempPath (sizeof(mySzTempShortDir)-1, mySzTempShortDir))
                TA_THROW_MSG(std::runtime_error, boost::format("Failed to get temporary folder path. Last error %d") % ::GetLastError());
            char mySzTempLongDir[MAX_PATH+1] = {};
            if (GetLongPathName(mySzTempShortDir, mySzTempLongDir, sizeof(mySzTempLongDir)) == 0)
                TA_THROW_MSG(std::runtime_error, boost::format("GetLongPathName failed for %s. Last error %d") % mySzTempShortDir % ::GetLastError());
            myTempDir = mySzTempLongDir;

#else
            try {
                myTempDir = getUserAppDataDir() + "/tmp/";
            } catch (...) {
                myTempDir = "/tmp/";
            }
#endif
            return myTempDir;
        }

        std::string genTempPath(const string& aPrefix)
        {
            while (true)
            {
                const string myFilePath = str(boost::format("%s%s%s.tmp") % getTempDir() % aPrefix % ta::genUuid());
                if (!isFileExist(myFilePath) && !isDirExist(myFilePath))
                {
                    return myFilePath;
                }
            }
        }

        ScopedDir::ScopedDir(const string& aPath)
            : path(aPath)
        {
            boost::filesystem::remove_all(aPath);
            boost::filesystem::create_directories(aPath);
        }

        ScopedDir::~ScopedDir()
        {
            try  {
                boost::filesystem::remove_all(path);
            }  catch (...)
            {}
        }

        string getUserAppDataDir()
        {
#ifdef _WIN32
            char szAppData[MAX_PATH+1] = {};
            HRESULT hr = ::SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, szAppData);
            if (FAILED(hr))
                TA_THROW_MSG(std::runtime_error, boost::format("Failed to retrieve the user application profile directory. HRESULT=%d") % hr);
            return szAppData;
#else
            //@note we DO NOT use HOME env variable since when called with 'sudo' it may, depending on the system security policy,
            // give us home directory of the original caller, which is in most cases not what we want.
            // For example HOME env. variable is set to target's user in Debian 8 ('sudo' acts as 'sudo -H') but is preserved in Ubuntu 16 ('sudo' acts as 'sudo -E')
            const int myUserId = getuid();
            const struct passwd* pw = getpwuid(myUserId);
            if (!pw)
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Failed to retrieve the user HOME directory. %s") % strerror(errno));
            }
            if (!pw->pw_dir)
            {
                TA_THROW_MSG(std::runtime_error, boost::format("HOME directory for user ID %d is not set") % myUserId);
            }
            return pw->pw_dir;
#endif
        }

#ifdef _WIN32

        string getMyDocDir()
        {
            char szDir[MAX_PATH+1] = {};
            HRESULT hr = ::SHGetFolderPath(NULL, CSIDL_MYDOCUMENTS, NULL, 0, szDir);
            if (FAILED(hr))
                TA_THROW_MSG(std::runtime_error, boost::format("Failed to retrieve the user documents directory. HRESULT=%d") % hr);
            return szDir;
        }
        string getCommonAppDataDir()
        {
            char szAppData[MAX_PATH+1] = {};
            HRESULT hr = ::SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szAppData);
            if (FAILED(hr))
                TA_THROW_MSG(std::runtime_error, boost::format("Failed to retrieve the common application profile directory. HRESULT=%d") % hr);
            return szAppData;
        }
        string getWindowsDir()
        {
            char szWinDir[MAX_PATH+1] = {};
            HRESULT hr = ::SHGetFolderPath(NULL, CSIDL_WINDOWS, NULL, 0, szWinDir);
            if (FAILED(hr))
                TA_THROW_MSG(std::runtime_error, boost::format("Failed to retrieve Windows directory. HRESULT=%d") % hr);
            return szWinDir;
        }

        string getLocalAppDataLowDir()
        {
#if 0 // Commented because the folder returned  does not exist, while e.g. boost::filesystem says it is... lets avoid magic...
            typedef HRESULT (APIENTRY *LPFN_IEGETWRITEABLEFOLDERPATH) (REFGUID clsidFolderID, LPWSTR *lppwstrPath);
            ta::DynLibLoader myIeFrameDll("ieframe.dll");
            LPFN_IEGETWRITEABLEFOLDERPATH fnIEGetWriteableFolderPath = (LPFN_IEGETWRITEABLEFOLDERPATH)myIeFrameDll.getFuncPtr("IEGetWriteableFolderPath");
            LPWSTR lwszDir = NULL;
# ifndef DEFINE_KNOWN_FOLDER
# define DEFINE_KNOWN_FOLDER(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    const GUID name = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }
            DEFINE_KNOWN_FOLDER(FOLDERID_InternetCache, 0x352481E8, 0x33BE, 0x4251, 0xBA, 0x85, 0x60, 0x07, 0xCA, 0xED, 0xCF, 0x9D);
# endif
            HRESULT hr = fnIEGetWriteableFolderPath (FOLDERID_InternetCache, &lwszDir);
            if (FAILED(hr))
                TA_THROW_MSG(std::runtime_error, boost::format("IEGetWriteableFolderPath failed. Hresult: %ld") % hr);
            if (!lwszDir)
                TA_THROW_MSG(std::runtime_error, "IEGetWriteableFolderPath succeeded but lwszDir is NULL");
            const string myRetVal = EncodingUtils::toMbyte(wszDir);
            CoTaskMemFree(wszDir);
            return myRetVal;
#else
            typedef HRESULT (APIENTRY *LPFN_SHGETKNOWNFOLDERPATH) (REFGUID, DWORD, HANDLE, PWSTR*);
            ta::DynLibLoader myShell32Dll("shell32.dll");
            LPFN_SHGETKNOWNFOLDERPATH fnSHGetKnownFolderPath = (LPFN_SHGETKNOWNFOLDERPATH)myShell32Dll.getFuncPtr("SHGetKnownFolderPath");
            PWSTR wszDir = NULL;
# ifndef DEFINE_KNOWN_FOLDER
# define DEFINE_KNOWN_FOLDER(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    const GUID name = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }
            DEFINE_KNOWN_FOLDER(FOLDERID_LocalAppDataLow, 0xA520A1A4, 0x1780, 0x4FF6, 0xBD, 0x18, 0x16, 0x73, 0x43, 0xC5, 0xAF, 0x16);
# endif
            HRESULT hr = fnSHGetKnownFolderPath (FOLDERID_LocalAppDataLow, 0, NULL, &wszDir);
            if (FAILED(hr))
                TA_THROW_MSG(std::runtime_error, boost::format("SHGetKnownFolderPath failed. Hresult: %ld") % hr);
            if (!wszDir)
                TA_THROW_MSG(std::runtime_error, "SHGetKnownFolderPath succeeded but wszDir is NULL");
            const string myRetVal = EncodingUtils::toMbyte(wszDir);
            CoTaskMemFree(wszDir);
            return myRetVal;
#endif
        }

#endif

        vector<string> getEnvVars()
        {
            vector<string> myRetVal;
#ifdef _WIN32
            char* myEnvData = ::GetEnvironmentStrings();
            if (!myEnvData)
                TA_THROW_MSG(std::runtime_error, "Failed to retrieve environment variables");
            for (char* myVariable = myEnvData; *myVariable; myVariable += strlen(myVariable) + 1)
                myRetVal.push_back(myVariable);
            ::FreeEnvironmentStrings(myEnvData);
#elif defined(__linux__)
            FILE* myPipe = popen ("env", "r");
            if (!myPipe)
                TA_THROW_MSG(std::runtime_error, "Failed to retrieve environment variables");
            char myBuf[128];
            string myEnvVars;
            while (!feof(myPipe))
            {
                size_t myRead = fread( myBuf, 1, sizeof(myBuf)-1, myPipe);
                if (myRead)
                {
                    myBuf[myRead] = 0;
                    myEnvVars += myBuf;
                }
                else
                    break;
            }
            pclose(myPipe);

            std::istringstream myEnvVarsStream(myEnvVars);
            string myEnvVar;
            while (std::getline(myEnvVarsStream, myEnvVar, '\n'))
                myRetVal.push_back(myEnvVar);
#endif
            return myRetVal;
        }

#if defined(__linux__)
        bool isServiceRunning(const string& aServiceName)
        {
            string myStdOut, myStdErr;
            const string myCmd = "service " + aServiceName + " status";
            const int myResult = shellExecSync(myCmd, myStdOut, myStdErr);
            switch (myResult)
            {
            case 0:
                return true;
            case 1:
                WARNLOG(aServiceName + " service is not running but its PID exists");
                return false;
            case 2:
                WARNLOG(aServiceName + " service is not running but its lock file exists");
                return false;
            case 3:
                return false;
            case 4:
                TA_THROW_MSG(std::runtime_error, boost::format("Status of service %s is unknown. StdOut: '%s'. StdErr: '%s'") % aServiceName % boost::trim_copy(myStdOut) % boost::trim_copy(myStdErr));
            default:
                TA_THROW_MSG(std::runtime_error, boost::format("Failed to check run status of service %s. Command '%s' finished with code %d. StdOut: '%s'. StdErr: '%s'") % aServiceName % myCmd % myResult % boost::trim_copy(myStdOut) % boost::trim_copy(myStdErr));
            }
        }

        void startService(const std::string& aServiceName)
        {
            DEBUGLOG("Starting " + aServiceName + " service");
            checkedShellExecSync("sudo service " + aServiceName + " start");
        }

        void restartService(const std::string& aServiceName)
        {
            DEBUGLOG("Restarting " + aServiceName + " service");
            checkedShellExecSync("sudo service " + aServiceName + " restart");
        }

        void stopService(const std::string& aServiceName)
        {
            DEBUGLOG("Stopping " + aServiceName + " service");
            checkedShellExecSync("sudo service " + aServiceName + " stop");
        }

        void enableService(const std::string& aServiceName)
        {
            DEBUGLOG("Enabling " + aServiceName + " service");
            checkedShellExecSync("sudo systemctl enable " + aServiceName + ".service");
        }

        void disableService(const std::string& aServiceName)
        {
            DEBUGLOG("Disabling " + aServiceName + " service");
            checkedShellExecSync("sudo systemctl disable " + aServiceName + ".service");
        }
#endif

        string expandEnvVars(const string& anStr)
        {
# if defined(_WIN32)
            char mySzStr[MAX_PATH+1] = {};
            DWORD myRetVal = ::ExpandEnvironmentStrings(anStr.c_str(), mySzStr, sizeof(mySzStr)-1);
            if (myRetVal == 0)
                TA_THROW_MSG(std::runtime_error, boost::format("::ExpandEnvironmentStrings failed for buffer '%s'. Last error: %d") % anStr % ::GetLastError());
            if (myRetVal > sizeof(mySzStr)-1)
                TA_THROW_MSG(std::runtime_error, boost::format("Expanded buffer is too long. Buffer: '%s'") % anStr);
            return mySzStr;
# else
            wordexp_t result;
            int res = wordexp(anStr.c_str(), &result, 0 );
            if (res != 0)
            {
                //if (res == WRDE_NOSPACE) //@todo needed?
                //     wordfree(&result);
                TA_THROW_MSG(std::runtime_error, boost::format("wordexp() failed for buffer '%s' with error code %d") % anStr % res);
            }
            string retval;
            for (size_t i=0; i<result.we_wordc; ++i)
            {
                if (!retval.empty())
                    retval += " ";
                retval += result.we_wordv[i];
            }
            wordfree(&result);
            return retval;
# endif
        }

#ifndef _WIN32
        struct FileLockError : std::runtime_error
        {
            explicit FileLockError(const std::string& aMessage = "")    : std::runtime_error(aMessage) {}
        };

        FileLock::FileLock(const std::string& aLockFileName, const FileLock::LockSharing aLockSharing, const FileLock::LockBlocking aLockBlocking)
            : theFd(open(aLockFileName.c_str(), O_RDONLY|O_CREAT, 00644))
        {
            if (theFd == -1)
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Failed to open %s for further locking. %s") % aLockFileName % strerror(errno));
            }

            int myOperation = 0;
            string myOperationStr;
            if (aLockSharing == exclusive)
            {
                myOperation = LOCK_EX, myOperationStr = "exclusive";
            }
            else
            {
                myOperation = LOCK_SH, myOperationStr = "shared";
            }
            if (aLockBlocking == blocking)
            {
                myOperationStr = " blocking";
            }
            else
            {
                myOperationStr = " non-blocking";
                myOperation |= LOCK_NB;
            }

            if (flock(theFd,  myOperation) != 0)
            {
                const int myErrno = errno;
                close(theFd);
                TA_THROW_MSG(FileLockError, boost::format("Failed to acquire %s lock on %s. %s") % myOperationStr %  aLockFileName % strerror(myErrno));
            }
        }
        FileLock::~FileLock()
        {
            flock(theFd, LOCK_UN);
            close(theFd);
        }

        bool isExclusivelyLocked(const string& aLockFileName)
        {
            if (!isFileExist(aLockFileName))
            {
                return false;
            }
            try
            {
                FileLock lock(aLockFileName, FileLock::exclusive, FileLock::nonblocking);
                return false;
            }
            catch (FileLockError&)
            {
                return true;
            }
            catch (std::exception& e)
            {
                WARNDEVLOG(e.what()); // that's not totally ok, lazily inform developer
                return true;
            }
        }
#endif

    } // Process
} // ta

