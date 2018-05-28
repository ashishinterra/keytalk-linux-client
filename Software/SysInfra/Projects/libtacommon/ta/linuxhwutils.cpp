#if defined(__linux__)

#include "linuxhwutils.h"
#include "hwutils.h"
#include "process.h"
#include "utils.h"
#include "osinfoutils.h"
#include "strings.h"
#include "ta/logger.h"
#include "common.h"

#include <errno.h>
#include <sys/utsname.h>
#include "boost/regex.hpp"
#include "boost/assign/list_of.hpp"
#include "boost/filesystem/operations.hpp"

using std::string;
using std::vector;

namespace ta
{
    namespace linuxhwutils
    {
        string getCpuArch()
        {
            struct utsname buf = {};
            if (uname(&buf) == 0)
            {
                return buf.machine;
            }
            else
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Error in the uname system call, error: %s") % strerror(errno) );
            }
        }

        string getCpuModel()
        {
            string mySdtOut, myStdErr;
            if (Process::shellExecSync("grep -m 1 '^model name' /proc/cpuinfo | cut -d ':' -f 2", mySdtOut, myStdErr) != 0)
            {
                TA_THROW_MSG(std::runtime_error, "Failed to retrieve CPU model. Stderr: " + myStdErr);
            }
            return boost::trim_copy(mySdtOut);
        }

        string getSerialNum()
        {
            string mySdtOut, myStdErr;

            if (OsInfoUtils::isRaspberryPi())
            {
                if (Process::shellExecSync("grep -m 1 '^Serial' /proc/cpuinfo | cut -d ':' -f 2", mySdtOut, myStdErr) != 0)
                {
                    TA_THROW_MSG(std::runtime_error, "Failed to retrieve CPU serial. Stderr: " + myStdErr);
                }
            }
            else
            {
                // Reading BIOS serial under Linux requires root privileges. To work this around we placed this function in the external tool setuid bit set .
                // Now we simply call this tool and parse it output. The tool should be located next to our executable.
                const string myHwUtilsCmdLine = str(boost::format("%s/%s %s") % Process::getSelfDirName() % HwUtils_BinName % HwUtils_GetSystemSerialArg);
                if (Process::shellExecSync(myHwUtilsCmdLine, mySdtOut, myStdErr) != 0)
                {
                    TA_THROW_MSG(std::runtime_error, "Failed to retrieve system serial. Stderr: " + myStdErr);
                }
            }

            return boost::trim_copy(mySdtOut);
        }

        vector<SshPubKey> getSsh2HostPubKeys()
        {
            // Parse /etc/ssh/ssh_host_<type>_key.pub
            // An alternative would be to ask the location of host ssh keys by calling 'sshd -T' but this would need root rights;
            // Another alternative would be using ssh-keyscan, but this will not work if sshd is not listening on loopback interface

            vector<SshPubKey> myPubKeys;
            const vector<string> myKeyPaths = boost::assign::list_of("/etc/ssh/ssh_host_rsa_key.pub")
                                              ("/etc/ssh/ssh_host_dsa_key.pub")
                                              ("/etc/ssh/ssh_host_ecdsa_key.pub")
                                              ("/etc/ssh/ssh_host_esdsa_key.pub");
            foreach (const string& path, myKeyPaths)
            {
                if (ta::isFileExist(path))
                {
                    const string myKey = ta::readData(path);
                    foreach (const string line, ta::Strings::split(myKey, '\n', ta::Strings::sepsMergeOn, ta::Strings::emptyTokensDrop))
                    {
                        // Skip comments
                        boost::regex myRegEx("^\\s*#");
                        boost::cmatch myMatch;
                        if (!regex_search(line, myRegEx))
                        {
                            static const vector<char> seps = boost::assign::list_of(' ')('\t');
                            const vector<string> lineParts = ta::Strings::split(line, seps, ta::Strings::sepsMergeOn, ta::Strings::emptyTokensDrop);
                            if (lineParts.size() >= 2)
                            {
                                myPubKeys.push_back(SshPubKey(lineParts.at(0), lineParts.at(1)));
                            }
                        }
                    }
                }
            }
            return myPubKeys;
        }

        bool getPrimaryHardDriveSerial(string& aSerial)
        {
            string mySdtOut, myStdErr;

            // Reading HDD serial under Linux requires root privileges. To work this around we placed this function in the external tool with setuid bit set .
            // Now we simply call this tool and parse it output. The tool should be located next to our executable.
            const string myHwUtilsCmdLine = str(boost::format("%s/%s %s") % Process::getSelfDirName() % HwUtils_BinName % HwUtils_GetHddPrimarySerialArg);
            if (Process::shellExecSync(myHwUtilsCmdLine, mySdtOut, myStdErr) != 0)
            {
                ERRORLOG("Failed to retrieve primary HDD serial. Stderr: " + myStdErr);
                return false;
            }
            aSerial = boost::trim_copy(mySdtOut);
            return true;
        }

    } // linuxhwutils
} // ta

#endif
