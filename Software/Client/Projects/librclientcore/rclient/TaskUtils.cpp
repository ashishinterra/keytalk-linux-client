#include "TaskUtils.h"
#include "Settings.h"
#include "ta/registry.h"
#include "ta/process.h"
#include "ta/logger.h"
#include "ta/utils.h"
#include "ta/common.h"
#include "boost/filesystem/operations.hpp"
#include "boost/format.hpp"
#include "boost/regex.hpp"
#include <string>
#include <stdexcept>

using std::string;
using std::vector;
using std::invalid_argument;
using boost::str;
using boost::format;

namespace rclient
{
    namespace
    {
        string ScheduledTaskName = "KeyTalkScheduledScripts";

        vector<string> collectOneLineXmlTag(const string& aString, const string& anXmlTag)
        {
            vector<string> myResult;

            boost::match_results<string::const_iterator> myMatch;
            string::const_iterator myBeg = aString.begin();
            string::const_iterator myEnd = aString.end();
            const boost::regex myRegex(str(boost::format("<%1%>([^<]*)</%1%>") % boost::trim_copy(ta::regexEscapeStr(anXmlTag))));
            while (boost::regex_search(myBeg, myEnd, myMatch, myRegex))
            {
                string myMatchString = myMatch[1];
                myResult.push_back(boost::trim_copy(myMatchString));

                myBeg = myMatch[0].second;
            }
            return myResult;
        }

        string collectOneLineXmlTagSingle(const string& aString, const string& anXmlTag)
        {
            vector<string> myContent = collectOneLineXmlTag(aString, anXmlTag);

            if (myContent.empty())
            {
                TA_THROW_MSG(std::runtime_error, format("No '%s' tag found in input. One expected.") % anXmlTag);
            }
            if (myContent.size() > 1)
            {
                TA_THROW_MSG(std::runtime_error, format("More than one '%s' tag found in input. One expected.") % anXmlTag);
            }

            return myContent[0];
        }

        string getScheduledTaskSetting(const string& aSettingName)
        {
            const string myTaskQueryCmd = "schtasks /query /tn " + ScheduledTaskName + " /xml";
            string myStdOut, myStdErr;
            const int ret = ta::Process::shellExecSync(myTaskQueryCmd, myStdOut, myStdErr);

            if (ret != 0)
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Could not query scheduled task %s. %s finished with code %d. Stdout: %s. Stderr: %s") % ScheduledTaskName % myTaskQueryCmd % ret % myStdOut % myStdErr);
            }

            return collectOneLineXmlTagSingle(myStdOut, aSettingName);
        }
    }

    string getKeyTalkUtilsScriptPath()
    {
        return getScriptsDirectoryPath() + ta::getDirSep() + "KeyTalkUtils.psm1";
    }

    bool isPowerShellInstalled(string& anErrorMsg)
    {
        const string myInstallKey("SOFTWARE\\Microsoft\\PowerShell\\3");
        const string myInstallValue("Install");
        if (ta::Registry::isExist(HKEY_LOCAL_MACHINE, myInstallKey, myInstallValue))
        {
            DWORD myInstallVal;
            ta::Registry::read(HKEY_LOCAL_MACHINE, myInstallKey, myInstallValue, myInstallVal);
            if (myInstallVal != 1)
            {
                anErrorMsg = "PowerShell installation not detected.";
                return false;
            }

            string myStdOut;
            string myStdErr;
            int ret = executePowerShellCode(getKeyTalkUtilsScriptPath(), "IsPowershellVersionSupported 3", myStdOut, myStdErr);
            if (ret != 0)
            {
                anErrorMsg = myStdOut;
                return false;
            }
            return true;
        }
        anErrorMsg = "PowerShell installation not detected.";
        return false;
    }

    bool isIISInstalled(string& anErrorMsg)
    {
        const string myKey("SOFTWARE\\Microsoft\\InetStp");
        const DWORD myRequiredVersion = 7;
        if (ta::Registry::isExist(HKEY_LOCAL_MACHINE, myKey, "MajorVersion"))
        {
            DWORD myVal;
            ta::Registry::read(HKEY_LOCAL_MACHINE, myKey, "MajorVersion", myVal);
            if (myVal < myRequiredVersion)
            {
                anErrorMsg = str(format("IIS version %d detected. IIS %d or higher required.") % myVal % myRequiredVersion);
                return false;
            }
            return true;
        }
        anErrorMsg = str(format("IIS %d or higher required.") % myRequiredVersion);
        return false;
    }

    int executePowerShellCode(const string& aPowerShellCode, string& anStdOut, string& anStdErr)
    {
        string myPowerShellCode = boost::replace_all_copy(aPowerShellCode, "\"", "\\\"");

        string myCommand("PowerShell -Noninteractive -ExecutionPolicy Bypass -command \"" + myPowerShellCode + "\"");

        string myStdOut;
        string myStdErr;
        const int myRetVal = ta::Process::shellExecSync(myCommand, myStdOut, myStdErr);
        anStdOut = boost::trim_copy(myStdOut);
        anStdErr = boost::trim_copy(myStdErr);
        return myRetVal;
    }

    int executePowerShellCode(const string& aPowerShellScriptPath, const string& aPowerShellCode, string& anStdOut, string& anStdErr)
    {
        if (!(boost::filesystem::exists(aPowerShellScriptPath)))
        {
            TA_THROW_MSG(invalid_argument, format("PowerShell script '%s' does not exist.") %  aPowerShellScriptPath);
        }

        string myPowerShellCode("Import-Module -name \"" + boost::filesystem::absolute(aPowerShellScriptPath).string() + "\"; " + aPowerShellCode);
        return executePowerShellCode(myPowerShellCode, anStdOut, anStdErr);
    }

    bool isPowerShellWebAdministrationModuleAvailable(string& anErrorMsg)
    {
        string myStdOut;
        string myStdErr;
        const int ret = executePowerShellCode(getKeyTalkUtilsScriptPath(), "IsPowershellModuleInstalled 'WebAdministration'", myStdOut, myStdErr);

        if (ret != 0)
        {
            anErrorMsg = myStdOut;
        }
        return ret == 0;
    }

    bool isScheduledTaskRunsOnStartup() {
        return getScheduledTaskSetting("LogonType") == "Password";
    }

    string getScheduledTaskUserName() {
        return boost::trim_copy(getScheduledTaskSetting("UserId"));
    }

    bool enableScheduledTaskAtSystemStartup(const string& aUserName, const string& aPassword, string& anErrorMsg)
    {
        string myCmd = str(format("schtasks /change /tn %s /ru \"%s\" /rp \"%s\"") % ScheduledTaskName % aUserName % aPassword);
        string myStdOut;
        string myStdErr;
        int ret = ta::Process::shellExecSync(myCmd, myStdOut, myStdErr);

        if (ret != 0)
        {
            anErrorMsg = myStdErr;
            ERRORLOG(format("Error %d while invoking schtasks: %s") % ret % myStdErr);
            return false;
        }

        if (!isScheduledTaskRunsOnStartup())
        {
            anErrorMsg = str(format("Could not enable scheduled task at system startup for user '%s'. Please open task '%s' in the windows Task Scheduler and enable setting 'Run whether user is logged on or not' manually.") % aUserName % ScheduledTaskName);
            ERRORLOG(anErrorMsg);
            return false;
        }

        return true;
    }

    string getScriptsDirectoryPath()
    {
        return rclient::Settings::getReseptInstallDir() + ta::getDirSep() + "Scripts";
    }
} // namespace rclient
