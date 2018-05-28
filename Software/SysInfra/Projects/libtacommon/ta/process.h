#pragma once

#include <string>
#include <stdexcept>
#include <vector>

#include "boost/utility.hpp"

namespace ta
{
    struct ProcessGetNameError : std::runtime_error
    {
        explicit ProcessGetNameError(const std::string& aMessage = "")    : std::runtime_error(aMessage) {}
    };

    struct ProcessExecError : std::runtime_error
    {
        explicit ProcessExecError(const std::string& aMessage = "")    : std::runtime_error(aMessage) {}
    };

    namespace Process
    {
        /**
          Retrieve process ID

          @return ID of process
         */
        unsigned long getSelfPid();


        /**
          Retrieve all process ID's

          @return List of process ID's
         */
        std::vector<unsigned long> getAllPids();

        /**
          Retrieve all process ID's by process name

          @pre On BSD/Linux systems max image name is 16 characters
          @param[in] anImageName Name of process to be checked
          @return List of process ID's
         */
        std::vector<unsigned long> getPids(const std::string& anImageName);

        /**
          Retrieve short name description of process

          @return Short name of process, removing extension if needed
          @throw ProcessGetNameError
         */
        enum RemoveExt { extRemoveYes,  extRemoveNo};
        std::string getSelfShortName(RemoveExt aRemoveExt = extRemoveYes);

#if defined(_WIN32) || defined(__linux__)
        /**
          Retrieve full name description of process

          @return Full name of process
          @throw ProcessGetNameError
         */
        std::string getSelfFullName();

        /**
          Retrieve full directory path containing the process

          @throw ProcessGetNameError
         */
        std::string getSelfDirName();
#endif

        /**
          Check if process is running by process name

          @pre On BSD/Linux systems max image name is 16 characters
          @param[in] anImageName Name of process to be checked
         */
        bool isRunning(const std::string& anImageName);
        bool isRunning(unsigned long aPid);


        /**
          Kill process by PID
          @note the function is asynchronous
         */
        void kill(unsigned long aPid);


        /**
          Kill process by process name

          @pre On BSD/Linux systems max image name is 16 characters
          @param[in] anImageName Name of process to be killed
          @return Number of processes killed
          @note the function is asynchronous
         */
        size_t kill(const std::string& anImageName);

#ifndef  _WIN32
        /** Wait for the given child process to exit in its d'tor, optionally killing it.
         This helps avoiding zombies
         */
        enum KillOnExit { killOnExitYes, killOnExitNo };

        class ExitWaiter
        {
        public:
            ExitWaiter();
            ExitWaiter(unsigned long pid, KillOnExit aKillOnExit = killOnExitNo);
            ~ExitWaiter();

            /**
             Let the process run after  d'tor is called.
             This effectively renders this class as noop

             @return process id
            */
            unsigned long release();

            void attach(unsigned int pid, KillOnExit aKillOnExit = killOnExitNo);

            bool owned() const;
            unsigned long pid() const;
        private:
            unsigned long thePid;
            bool theOwned;
            KillOnExit theKillOnExit;
        };

        /**
          Kill process tree given the top-level PID using breadth-first traversal

          @note the function is asynchronous
         */
        enum IncludeRoot { includeRootYes, includeRootNo };
        void killTree(unsigned long aRootPid, IncludeRoot anIncludeRoot = includeRootYes);

        /**
         * Check whether one or more immediate child processes is stopped
         * For each stopped child the function collects its return status and kills all its descendants
         @return pids of the stopped child processes
        */
        std::vector<unsigned long> checkStoppedChildren();

        /**
         * Block until one or more immediate child processes stops
         * For each stopped child the function collects its return status and kills all its descendants
         @throw std::std::exception when one or more children stops
        */
        void waitForChildStop();

#endif


        /**
          Launch a given command in a native shell and wait until it exits
          anStdOut and anStdErr contain the stdout and stderr output of the command
          optional anStdin argument contains string to feed the command via stdin. Ignored on Windows

          @param[in] aCommand command to be executed.
          The command shall be be surrounded with quotes if it contains internal whitespace.
          Environment variables will be automatically expanded by the function. Examples:
          "C:\Program Files\MyVpn\vpn.exe"
          "%ProgramFiles%\MyVpn\vpn.exe" --certauth port:123 "argument with whitespace"
          @param[out] anStdOut Standard output
          @param[out] anStdErr Standard error
          @param[in] anStdIn Standard input
          @return Command exit code
          @throw ProcessExecError
         */
        int shellExecSync(const std::string& aCommand);
#ifdef _WIN32
        int shellExecSync(const std::string& aCommand, std::string& anStdOut, std::string& anStdErr);
        int shellExecSync(const std::string& aCommand, std::vector<unsigned char>& anStdOut, std::vector<unsigned char>& anStdErr);
#else
        int shellExecSync(const std::string& aCommand, std::string& anStdOut, std::string& anStdErr, const std::string& anStdIn = "");
        int shellExecSync(const std::string& aCommand, std::vector<unsigned char>& anStdOut, std::vector<unsigned char>& anStdErr, const std::vector<unsigned char>& anStdIn = std::vector<unsigned char>());
#endif
        /**
            Effectively calls shellExecuteSync and throws ProcessExecError if this call does not return 0
            This function is provided for convenience.
        */
#ifdef _WIN32
        void checkedShellExecSync(const std::string& aCommand);
#else
        void checkedShellExecSync(const std::string& aCommand, const std::string& anStdIn = "");
#endif

        /**
         Launch a given command in a native shell, do not wait until it finishes

        @param[in] aCommand command to be executed.
          The command shall be be surrounded with quotes if it contains internal whitespace.
          Environment variables will be automatically expanded by the function. Examples:
          "C:\Program Files\MyVpn\vpn.exe"
          "%ProgramFiles%\MyVpn\vpn.exe" --certauth port:123 "argument with whitespace"
        @param[out] anStdOut Standard output
        @param[out] anStdErr Standard error
        @param[in] aMaxWaitTime time in msec to wait for the command to start. The command might exit before the timeout reached or fail to start e.g. because command file is not found.
        @return true if the command has been finished or not started within aMaxWaitTime interval In this case anExitCode, anStdOut and anStdErr are respectively filled with exit code, stdout and stderr output of the exited command. It is then the responsibility of the caller to interpret these values correctly.
                Otherwise we consider the command is still running detached and the function return false

         @throw ProcessExecError
        */
        bool shellExecAsync(const std::string& aCommand, std::string& anStdOut, std::string& anStdErr, unsigned int& anExitCode, unsigned int aMaxWaitTime = 1000);
        bool shellExecAsync(const std::string& aCommand, std::vector<unsigned char>& anStdOut, std::vector<unsigned char>& anStdErr, unsigned int& anExitCode, unsigned int aMaxWaitTime = 1000);
        bool shellExecAsync(const std::string& aCommand, unsigned int& anExitCode, unsigned int aMaxWaitTime = 1000);

        enum Subsystem
        {
            Console,
            Window
        };

        /**
          Retrieve the type of the executable associated with our process (e.g. console, windows)
         */
        Subsystem getSelfSubsystem();

        /**
          Retrieve user temporary directory (slash-terminated)

          @return path to temporary directory
          @nothrow
         */
        std::string getTempDir();

        /**
        Generates unique temporary path so the file or directory with such a name can be created
        */
        std::string genTempPath(const std::string& aPrefix = "");

        /**
         RAII wrapper class which creates a directory on construction and removes on destruction
        */
        struct ScopedDir : boost::noncopyable
        {
            explicit ScopedDir(const std::string& aPath);
            ~ScopedDir();
            const std::string path;
        };

        /**
          Windows: retrieve user application profile directory
          Unix: Retrieve home directory
         */
        std::string getUserAppDataDir();

#ifdef _WIN32
        /**
          Retrieve user my documents directory
         */
        std::string getMyDocDir();


        /**
          Retrieve common application profile directory
         */
        std::string getCommonAppDataDir();

        /**
          Retrieve Windows directory
         */
        std::string getWindowsDir();

        /**
          Retrieve LocalLow app directory.
          This directory is one of so called "low integrity mode" write locations used e.g. by IE running protected mode (typically %USERPROFILE%\AppData\LocalLow\)

          @throw std::std::exception e.g. if the Windows version is less than Windows Vista/Windows Server 2008
        */
        std::string getLocalAppDataLowDir();
#endif
        /**
          Retrieve environment variables

          @return List of environment variables
         */
        std::vector<std::string> getEnvVars();

#if defined(__linux__)
        // Linux sysdemd/SystemV service management
        bool isServiceRunning(const std::string& aServiceName);
        void startService(const std::string& aServiceName);
        void restartService(const std::string& aServiceName);
        void stopService(const std::string& aServiceName);
        void enableService(const std::string& aServiceName);
        void disableService(const std::string& aServiceName);
#endif

        /**
          Expands environment variables
          @return string with env variables expanded
         */
        std::string expandEnvVars(const std::string& anStr);

#ifndef _WIN32
        //
        // Named inter-process locks
        // @note the locks are not recursive!
        class FileLock
        {
        public:
            enum LockSharing { shared, exclusive };
            enum LockBlocking { blocking, nonblocking };
            FileLock(const std::string& aLockFileName, const LockSharing aLockSharing, const LockBlocking aLockBlocking = blocking);
            ~FileLock();
        private:
            int theFd;

        };
        bool isExclusivelyLocked(const std::string& aLockFileName);
#endif
    }
}
