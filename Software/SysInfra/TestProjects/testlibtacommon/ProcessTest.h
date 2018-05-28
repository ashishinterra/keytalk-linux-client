#pragma once

#include "ta/process.h"
#include "ta/utils.h"

#include "cxxtest/TestSuite.h"
#include "boost/filesystem/operations.hpp"
#include <string>
#include <vector>
#include <algorithm>

#ifdef _WIN32
#include <winsock2.h>
#include <direct.h>
#include <windows.h>
#endif


#define PROCESS_TEST_CONSOLE_OUTPUT

#ifdef PROCESS_TEST_CONSOLE_OUTPUT
#include <iostream>
#include <iterator>
#endif

using namespace ta;
using std::string;

class ProcessTest : public CxxTest::TestSuite
{
#ifdef _WIN32
    // Copies a given executable to the directory containing special characters.
    // Cleanup created dirs/files upon destruction.
    class SpecialCharsCmdCreator
    {
    public:
        SpecialCharsCmdCreator(const string& anOrigCmdPath, bool aWithWs)
            : theDirName(Process::getTempDir() + "\\" + (aWithWs ? "^ & () @@ AT & T&&  @" : "^&()@@AT&T&&@"))
        {
            string::size_type myFound = anOrigCmdPath.find_last_of("\\");
            if (myFound != string::npos)
                theCmdPath = theDirName + "\\" + anOrigCmdPath.substr(myFound + 1);
            else
                theCmdPath = theDirName + "\\" + anOrigCmdPath;
            _mkdir(theDirName.c_str());
            if (!::CopyFile(anOrigCmdPath.c_str(), theCmdPath.c_str(), FALSE))
                TA_THROW_MSG(std::runtime_error, boost::format("Failed to copy '%s' to '%s'. Last error: %d") % anOrigCmdPath % theCmdPath % ::GetLastError());
        }
        ~SpecialCharsCmdCreator()
        {
            ::DeleteFile(theCmdPath.c_str());
            _rmdir(theDirName.c_str());
        }
        string getCmdPath() const
        {
            return theCmdPath;
        }
    private:
        string theDirName;
        string theCmdPath;
    };
#endif
    void safeRemove(const string& aPath)
    {
        try
        {
            boost::filesystem::remove_all(aPath);
        }
        catch (...)
        {}
    }

public:
    void testName()
    {
        string mySelfShortName = Process::getSelfShortName();
        TS_ASSERT(!mySelfShortName.empty());
#ifdef PROCESS_TEST_CONSOLE_OUTPUT
        TS_TRACE(("Self short name: " + mySelfShortName).c_str());
#endif

#if defined(_WIN32) || defined(__linux__)
        string mySelfFullName  = Process::getSelfFullName();
        TS_ASSERT(!mySelfFullName.empty());
        TS_ASSERT(mySelfFullName.length() >= mySelfShortName.length());
#ifdef PROCESS_TEST_CONSOLE_OUTPUT
        TS_TRACE(("Self full name: " + mySelfFullName).c_str());
#endif
        TS_ASSERT(mySelfFullName.find(mySelfShortName) != std::string::npos);

        string mySelfDirName  = Process::getSelfDirName();
        TS_ASSERT(!mySelfDirName.empty());
        TS_ASSERT(mySelfDirName.length() < mySelfFullName.length());
#ifdef PROCESS_TEST_CONSOLE_OUTPUT
        TS_TRACE(("Self directory name: " + mySelfDirName).c_str());
#endif
        TS_ASSERT(mySelfFullName.find(mySelfDirName) != std::string::npos);
        TS_ASSERT_EQUALS(mySelfDirName + ta::getDirSep() + Process::getSelfShortName(Process::extRemoveNo), mySelfFullName);
#endif
    }
    void testEnum()
    {
        std::vector<unsigned long> myAllPids = Process::getAllPids();
        TS_ASSERT(!myAllPids.empty());
        unsigned long myPid = Process::getSelfPid();
        TS_ASSERT(find(myAllPids.begin(), myAllPids.end(), myPid) != myAllPids.end());
    }
    void testIsRunning()
    {
        string mySelfImageName = Process::getSelfShortName();
#ifdef _WIN32
        mySelfImageName += ".exe";
#endif
        TS_ASSERT(Process::isRunning(mySelfImageName));
#ifdef PROCESS_TEST_CONSOLE_OUTPUT
        TS_TRACE(("Self short name: " + mySelfImageName).c_str());
#endif
        std::vector<unsigned long> mySelfPids = Process::getPids(mySelfImageName);
        TS_ASSERT(!mySelfPids.empty());
        TS_ASSERT(std::find(mySelfPids.begin(), mySelfPids.end(), Process::getSelfPid()) != mySelfPids.end());
        static const string myNonexistingProcess = "__nonexisting__";
        TS_ASSERT(!Process::isRunning(myNonexistingProcess));
        TS_ASSERT(Process::getPids(myNonexistingProcess).empty());
    }
    void testExecAsync()
    {
        string myCommand;
        string myStdOut, myStdErr;
        unsigned int myExitCode;
#ifdef _WIN32
        myCommand = "..\\..\\..\\Import\\win32_utils\\sleep.exe -m 1000";
#else
        myCommand = "sleep 1";
#endif
        bool myCommandFinished = Process::shellExecAsync(myCommand, myStdOut, myStdErr, myExitCode, 3000);
        TS_ASSERT(myCommandFinished);
        if (myCommandFinished)
        {
            TS_ASSERT_EQUALS(myExitCode, 0U);
#ifdef PROCESS_TEST_CONSOLE_OUTPUT
            TS_TRACE(str(boost::format("\nCommand: '%s' exited with code %u") % myCommand % myExitCode).c_str());
            TS_TRACE(str(boost::format("\nStdOut: '%s'\nStdErr: '%s'") % myStdOut % myStdErr).c_str());
#endif
        }
        else
        {
#ifdef PROCESS_TEST_CONSOLE_OUTPUT
            TS_TRACE(("Command " + myCommand + " is running detached").c_str());
#endif
        }

#ifdef _WIN32
        myCommand = "..\\..\\..\\Import\\win32_utils\\sleep.exe -m 2000";
#else
        myCommand = "sleep 2";
#endif
        myCommandFinished = Process::shellExecAsync(myCommand, myStdOut, myStdErr, myExitCode, 100);
        TS_ASSERT(!myCommandFinished);
        if (myCommandFinished)
        {
#ifdef PROCESS_TEST_CONSOLE_OUTPUT
            TS_TRACE(str(boost::format("\nCommand: '%s' exited with code %u") % myCommand % myExitCode).c_str());
            TS_TRACE(str(boost::format("\nStdOut: '%s'\nStdErr: '%s'") % myStdOut % myStdErr).c_str());
#endif
        }
        else
        {
#ifdef PROCESS_TEST_CONSOLE_OUTPUT
            TS_TRACE(("Command " + myCommand + " is running detached").c_str());
#endif
        }

#ifdef _WIN32
        myCommand = "..\\..\\..\\Import\\win32_utils\\sleep.exe -bad_args";
#else
        myCommand = "sleep -bad_args";
#endif
        myCommandFinished = Process::shellExecAsync(myCommand, myStdOut, myStdErr, myExitCode);
        TS_ASSERT(myCommandFinished);
        if (myCommandFinished)
        {
            TS_ASSERT_DIFFERS(myExitCode, 0U);
#ifdef PROCESS_TEST_CONSOLE_OUTPUT
            TS_TRACE(str(boost::format("\nCommand: '%s' exited with code %u") % myCommand % myExitCode).c_str());
            TS_TRACE(str(boost::format("\nStdOut: '%s'\nStdErr: '%s'") % myStdOut % myStdErr).c_str());
#endif
        }
        else
        {
#ifdef PROCESS_TEST_CONSOLE_OUTPUT
            TS_TRACE(("Command " + myCommand + " is running detached").c_str());
#endif
        }
    }



    void testExecSync()
    {
        string myGoodCommand, myBadCommand;
        string myStdOut, myStdErr;

#ifdef _WIN32
        myGoodCommand = "date /T";
#else
        myGoodCommand = "echo 'one two  three' | awk '{print $2}'";
#endif
        int myRetVal = Process::shellExecSync(myGoodCommand, myStdOut, myStdErr);
        TS_ASSERT_EQUALS(myRetVal, 0);
#ifdef _WIN32
        TS_ASSERT(!myStdOut.empty());
#else
        TS_ASSERT_EQUALS(myStdOut, "two\n");
#endif
        TS_ASSERT(myStdErr.empty());
#ifdef PROCESS_TEST_CONSOLE_OUTPUT
        TS_TRACE(str(boost::format("\nCommand: '%s' exited with code %u") % myGoodCommand % myRetVal).c_str());
        TS_TRACE(str(boost::format("\nStdOut: '%s'\nStdErr: '%s'") % myStdOut % myStdErr).c_str());
#endif

#ifdef _WIN32
        // Test expansion of env variables with spaces
        myGoodCommand = "..\\..\\..\\Import\\python-2.7\\python.exe -V";
        myRetVal = Process::shellExecSync(myGoodCommand, myStdOut, myStdErr);
        TS_ASSERT_EQUALS(myRetVal, 0);
        TS_ASSERT_DIFFERS(myStdErr.find("2.7"), std::string::npos);
# ifdef PROCESS_TEST_CONSOLE_OUTPUT
        TS_TRACE(str(boost::format("\nCommand: '%s' exited with code %u") % myGoodCommand % myRetVal).c_str());
        TS_TRACE(str(boost::format("\nStdOut: '%s'\nStdErr: '%s'") % myStdOut % myStdErr).c_str());
# endif
#else
        // Test stdin feed
        myGoodCommand = "awk '{print $2}'";
        myRetVal = Process::shellExecSync(myGoodCommand, myStdOut, myStdErr, "one two  three");
        TS_ASSERT_EQUALS(myRetVal, 0);
        TS_ASSERT_EQUALS(myStdOut, "two\n");
        TS_ASSERT(myStdErr.empty());
# ifdef PROCESS_TEST_CONSOLE_OUTPUT
        TS_TRACE(str(boost::format("\nCommand: '%s' exited with code %u") % myGoodCommand % myRetVal).c_str());
        TS_TRACE(str(boost::format("\nStdOut: '%s'\nStdErr: '%s'") % myStdOut % myStdErr).c_str());
# endif
#endif

        myBadCommand = "_date_";
        myRetVal = Process::shellExecSync(myBadCommand, myStdOut, myStdErr);
        TS_ASSERT(myRetVal != 0);
        TS_ASSERT(myStdOut.empty());
        TS_ASSERT(!myStdErr.empty());
#ifdef PROCESS_TEST_CONSOLE_OUTPUT
        TS_TRACE(str(boost::format("\nCommand: '%s' exited with code %u") % myBadCommand % myRetVal).c_str());
        TS_TRACE(str(boost::format("\nStdOut: '%s'\nStdErr: '%s'") % myStdOut % myStdErr).c_str());
#endif
#ifdef _WIN32
        string myCommandBadArg = "dir /nonexisting_arg";
#else
        string myCommandBadArg = "ls -y";
#endif
        myRetVal = Process::shellExecSync(myCommandBadArg, myStdOut, myStdErr);
        TS_ASSERT(myRetVal != 0);
        TS_ASSERT(myStdOut.empty());
        TS_ASSERT(!myStdErr.empty());
#ifdef PROCESS_TEST_CONSOLE_OUTPUT
        TS_TRACE(str(boost::format("\nCommand: '%s' exited with code %u") % myCommandBadArg % myRetVal).c_str());
        TS_TRACE(str(boost::format("\nStdOut: '%s'\nStdErr: '%s'") % myStdOut % myStdErr).c_str());
#endif


#ifdef _WIN32
        Process::shellExecSync("echo Standard output", myStdOut, myStdErr);
        TS_ASSERT_EQUALS(boost::trim_copy(myStdOut), "Standard output");
        TS_ASSERT_EQUALS(boost::trim_copy(myStdErr), "");

        Process::shellExecSync("echo Standard error 1>&2", myStdOut, myStdErr);
        TS_ASSERT_EQUALS(boost::trim_copy(myStdOut), "");
        TS_ASSERT_EQUALS(boost::trim_copy(myStdErr), "Standard error");
#endif
    }

    void testThatExecSyncCanHandleLargeOutput()
    {
#ifdef _WIN32
		TS_SKIP("testThatExecSyncCanHandleLargeOutput() is currently not supported on Windows");
#else
        string myCommand;
        string myStdOut, myStdErr;

        const size_t myTextLen = 1024*1024;// 1M
        string myText;
        myText.resize(myTextLen);
        int ch = '0';
        for (size_t i=0; i < myTextLen; ++i)
        {
            myText[i] = (char)ch;
            if (++ch > 126)
                ch = '0';
        }
        const string myTextFileName = "big.txt";
        ta::writeData(myTextFileName, myText);

#ifdef _WIN32
        myCommand = str(boost::format("type '%s'") % myTextFileName);
#else
        myCommand = str(boost::format("cat '%s'") % myTextFileName);
#endif
        try
        {
            int myRetVal = Process::shellExecSync(myCommand, myStdOut, myStdErr);
            TS_ASSERT_EQUALS(myRetVal, 0);

            TS_ASSERT_EQUALS(myStdOut, myText);
            TS_ASSERT(myStdErr.empty());
    #ifdef PROCESS_TEST_CONSOLE_OUTPUT
            TS_TRACE(str(boost::format("\nCommand: '%s' exited with code %u") % myCommand % myRetVal).c_str());
    #endif
        }
        catch (...)
        {
            safeRemove(myTextFileName);
            throw;
        }
        safeRemove(myTextFileName);
#endif // _WIN32
    }


    // Test with special characters in command path (async)
    void testExecAsyncSpecialChars()
    {
#ifdef _WIN32
        const unsigned int myExecTimeoutMsec = 3000;
        string myCommand;
        string myStdOut, myStdErr;
        unsigned int myExitCode;

        SpecialCharsCmdCreator myCmdNoWs("..\\..\\..\\Import\\win32_utils\\sleep.exe", false);
        SpecialCharsCmdCreator myCmdWithWs("..\\..\\..\\Import\\win32_utils\\sleep.exe", true);

        TS_TRACE("-- Testing for command containing special characters and no whitespace");
        myCommand = "\"" + myCmdNoWs.getCmdPath() + "\"" + " -m 10";
        bool myCommandFinished = Process::shellExecAsync(myCommand, myStdOut, myStdErr, myExitCode, myExecTimeoutMsec);
        TS_ASSERT(myCommandFinished);
        if (myCommandFinished)
        {
            TS_ASSERT_EQUALS(myExitCode, 0);
#ifdef PROCESS_TEST_CONSOLE_OUTPUT
            TS_TRACE(str(boost::format("\nCommand: '%s' exited with code %u") % myCommand % myExitCode).c_str());
            TS_TRACE(str(boost::format("\nStdOut: '%s'\nStdErr: '%s'") % myStdOut % myStdErr).c_str());
#endif
        }


        TS_TRACE("-- Testing for command containing special characters and whitespace");
        myCommand = "\"" + myCmdWithWs.getCmdPath() + "\"" + " -m 10";
        myCommandFinished = Process::shellExecAsync(myCommand, myStdOut, myStdErr, myExitCode, myExecTimeoutMsec);
        TS_ASSERT(myCommandFinished);
        if (myCommandFinished)
        {
            TS_ASSERT_EQUALS(myExitCode, 0);
#ifdef PROCESS_TEST_CONSOLE_OUTPUT
            TS_TRACE(str(boost::format("\nCommand: '%s' exited with code %u") % myCommand % myExitCode).c_str());
            TS_TRACE(str(boost::format("\nStdOut: '%s'\nStdErr: '%s'") % myStdOut % myStdErr).c_str());
#endif
        }
#endif
    }


    // Test with special characters in command path (sync)
    void testExecSyncSpecialChars()
    {
#ifdef _WIN32
        string myCommand;
        string myStdOut, myStdErr;

        SpecialCharsCmdCreator myCmdNoWs("..\\..\\..\\Import\\win32_utils\\sleep.exe", false);
        SpecialCharsCmdCreator myCmdWithWs("..\\..\\..\\Import\\win32_utils\\sleep.exe", true);

        TS_TRACE("-- Testing for command containing special characters and no whitespace");
        myCommand = "\"" + myCmdNoWs.getCmdPath() + "\"" + " -m 100";
        int myRetVal = Process::shellExecSync(myCommand, myStdOut, myStdErr);
        TS_ASSERT_EQUALS(myRetVal, 0);
#ifdef PROCESS_TEST_CONSOLE_OUTPUT
        TS_TRACE(str(boost::format("\nCommand: '%s' exited with code %u") % myCommand % myRetVal).c_str());
        TS_TRACE(str(boost::format("\nStdOut: '%s'\nStdErr: '%s'") % myStdOut % myStdErr).c_str());
#endif

        TS_TRACE("-- Testing for command containing special characters and whitespace");
        myCommand = "\"" + myCmdNoWs.getCmdPath() + "\"" + " -m 100";
        myRetVal = Process::shellExecSync(myCommand, myStdOut, myStdErr);
        TS_ASSERT_EQUALS(myRetVal, 0);
#ifdef PROCESS_TEST_CONSOLE_OUTPUT
        TS_TRACE(str(boost::format("\nCommand: '%s' exited with code %u") % myCommand % myRetVal).c_str());
        TS_TRACE(str(boost::format("\nStdOut: '%s'\nStdErr: '%s'") % myStdOut % myStdErr).c_str());
#endif
#endif
    }

    void testSubsystem()
    {
        TS_ASSERT_THROWS_NOTHING(Process::getSelfSubsystem());
    }

    void testTempDir()
    {
        string myTempDir = Process::getTempDir();
        TS_ASSERT(!myTempDir.empty());
#ifdef PROCESS_TEST_CONSOLE_OUTPUT
        TS_TRACE(("Temp dir: " + myTempDir).c_str());
#endif
    }
    void testEnvVars()
    {
        std::vector<string> myEnvVars = Process::getEnvVars();
        TS_ASSERT(!myEnvVars.empty());
#ifdef PROCESS_TEST_CONSOLE_OUTPUT
        TS_TRACE("Environment variables:");
        foreach (string var, myEnvVars)
        {
            TS_TRACE(var.c_str());
        }
#endif
    }

    void testUserAppDir()
    {
        string myUserAppDir = Process::getUserAppDataDir();
        TS_ASSERT(!myUserAppDir.empty());
#ifdef PROCESS_TEST_CONSOLE_OUTPUT
        TS_TRACE("User Application Data directory: " + myUserAppDir);
#endif
    }
    void testCommonAppDir()
    {
#ifdef _WIN32
        string myCommonAppDir = Process::getCommonAppDataDir();
        TS_ASSERT(!myCommonAppDir.empty());
        TS_ASSERT(ta::isDirExist(myCommonAppDir));
#ifdef PROCESS_TEST_CONSOLE_OUTPUT
        TS_TRACE("Common Application Data directory: " + myCommonAppDir);
#endif
#endif
    }

    void testWindowsDir()
    {
#ifdef _WIN32
        string myDir = Process::getWindowsDir();
        TS_ASSERT(!myDir.empty());
        TS_ASSERT(ta::isDirExist(myDir));
#ifdef PROCESS_TEST_CONSOLE_OUTPUT
        TS_TRACE("Windows directory: " + myDir);
#endif
#endif
    }

    void testLocalAppDataLowDir()
    {
#ifdef _WIN32
            string myDir = Process::getLocalAppDataLowDir();
            TS_ASSERT(!myDir.empty());
            TS_ASSERT(ta::isDirExist(myDir));
# ifdef PROCESS_TEST_CONSOLE_OUTPUT
            TS_TRACE("Low Integrity Mode Application Data directory: " + myDir);
# endif
#endif
    }

   void testExpandEnvVars()
   {
       TS_ASSERT(Process::expandEnvVars("").empty());
       TS_ASSERT_EQUALS(Process::expandEnvVars("buffer without env variables"), "buffer without env variables");
# ifdef _WIN32
       TS_ASSERT(boost::iequals(Process::expandEnvVars("%windir%\\winhlp32.exe -h"), "C:\\WINDOWS\\winhlp32.exe -h"));
       TS_ASSERT_EQUALS(Process::expandEnvVars("x%I_HOPE_THIS_VARIABLE_DOES_NOT_EXIST%"), "x%I_HOPE_THIS_VARIABLE_DOES_NOT_EXIST%");
# else
       TS_ASSERT_DIFFERS(Process::expandEnvVars("$PATH").size(), 0L);
       TS_ASSERT_DIFFERS(Process::expandEnvVars("$PATH").find("/sbin"), std::string::npos);
       TS_ASSERT_EQUALS(Process::expandEnvVars("$PATH"), Process::expandEnvVars("${PATH}"));
       TS_ASSERT_EQUALS(Process::expandEnvVars("x$PATHZZZZZ"), "x");
       TS_ASSERT_EQUALS(Process::expandEnvVars("x${I_HOPE_THIS_VARIABLE_DOES_NOT_EXIST}"), "x");
# endif
   }

   void testLinuxServiceManagement()
   {
#if defined(__linux__)
        // given
        TS_ASSERT(Process::isServiceRunning("cron"));
        TS_ASSERT(Process::isServiceRunning("rsyslog"));

#ifdef RESEPT_SERVER
        // when
        Process::restartService("rsyslog");
        // then
        TS_ASSERT(Process::isServiceRunning("rsyslog"));

        // when
        Process::stopService("rsyslog");
        // then
        TS_ASSERT(!Process::isServiceRunning("rsyslog"));

        // when
        Process::startService("rsyslog");
        // then
        TS_ASSERT(Process::isServiceRunning("rsyslog"));
#endif
#else
        TS_SKIP("The test is for Linux only");
#endif
   }

   void testFileLocking()
   {
#if defined(__linux__)
        using Process::FileLock;

        Process::ScopedDir myTempDir(Process::genTempPath());
        const string myFilePath = myTempDir.path + "/test.lock";

        // given blocking exclusive lock
        {
            TS_ASSERT(!Process::isExclusivelyLocked(myFilePath));
            FileLock lock(myFilePath, FileLock::exclusive, FileLock::blocking);
            TS_ASSERT(Process::isExclusivelyLocked(myFilePath));
            TS_ASSERT_THROWS(FileLock(myFilePath, FileLock::exclusive, FileLock::nonblocking), std::exception);
            TS_ASSERT_THROWS(FileLock(myFilePath, FileLock::shared, FileLock::nonblocking), std::exception);
        }

        //  given non-blocking exclusive lock
        {
            TS_ASSERT(!Process::isExclusivelyLocked(myFilePath));
            FileLock lock(myFilePath, FileLock::exclusive, FileLock::nonblocking);
            TS_ASSERT(Process::isExclusivelyLocked(myFilePath));
            TS_ASSERT_THROWS(FileLock(myFilePath, FileLock::exclusive, FileLock::nonblocking), std::exception);
            TS_ASSERT_THROWS(FileLock(myFilePath, FileLock::shared, FileLock::nonblocking), std::exception);
        }

        //  given blocking shared lock
        {
            TS_ASSERT(!Process::isExclusivelyLocked(myFilePath));
            FileLock lock(myFilePath, FileLock::shared, FileLock::blocking);
            TS_ASSERT_THROWS_NOTHING(FileLock(myFilePath, FileLock::shared, FileLock::nonblocking));
            // we don't try to acquire exclusive lock because it already exists in the same process and our locks are not recursive
        }

        //  given non-blocking shared lock
        {
            TS_ASSERT(!Process::isExclusivelyLocked(myFilePath));
            FileLock lock(myFilePath, FileLock::shared, FileLock::nonblocking);
            TS_ASSERT_THROWS_NOTHING(FileLock(myFilePath, FileLock::shared, FileLock::nonblocking));
            // we don't try to acquire exclusive lock because it already exists in the same process and our locks are not recursive
        }

        TS_ASSERT(!Process::isExclusivelyLocked("/non/existing/file"));
#else
        TS_SKIP("The test is for Linux only");
#endif
   }
};
