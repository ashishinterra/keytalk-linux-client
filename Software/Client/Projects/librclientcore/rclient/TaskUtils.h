#ifndef RCLIENT_TASKUTILS_H
#define RCLIENT_TASKUTILS_H

#include <vector>
#include <string>

namespace rclient
{
    /**
    @return The KeyTalkUtils powershell module path.
    **/
    std::string getKeyTalkUtilsScriptPath();

    /**
    Execute the given PowerShell code
    @param[in] aPowerShellCode the code to be run
    @param[out] anStdOut stdout produced by the PowerShell invocation
    @param[out] anStdErr stderr produced by the PowerShell invocation
    **/
    int executePowerShellCode(const std::string& aPowerShellCode, std::string& anStdOut, std::string& anStdErr);

    /**
    Execute the given PowerShell code, using definitions from the given PowerShell script
    @param[in] aPowerShellScriptPath the code to be run
    @param[in] aPowerShellCode the code to be run
    @param[out] anStdOut stdout produced by the PowerShell invocation
    @param[out] anStdErr stderr produced by the PowerShell invocation
    **/
    int executePowerShellCode(const std::string& aPowerShellScriptPath, const std::string& aPowerShellCode, std::string& anStdOut, std::string& anStdErr);

    /**
    @return true if a PowerShell 3 installation is detected on the system. false otherwise
    **/
    bool isPowerShellInstalled(std::string& anErrorMsg);

    bool isIISInstalled(std::string& anErrorMsg);

    bool isPowerShellWebAdministrationModuleAvailable(std::string& anErrorMsg);

    bool isIISTaskDependenciesFulfilled(std::string& anErrorMsg);

    bool isScheduledTaskRunsOnStartup();
    std::string getScheduledTaskUserName();
    bool enableScheduledTaskAtSystemStartup(const std::string& aUserName, const std::string& aPassword, std::string& anErrorMsg);

    std::string getScriptsDirectoryPath();
}
#endif //RCLIENT_TASKUTILS_H
