#include "ta/common.h"
#include "ta/process.h"
#include "ta/utils.h"
#include "ta/logappender.h"
#include "ta/logconfiguration.h"
#include "ta/logger.h"
#include "rclient/Common.h"
#include "rclient/Settings.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <string>
#include "boost/algorithm/string.hpp"
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/convenience.hpp"

//
// Usage:
// ConfigUpdater --set-install-dir <KeyTalk Installation Directory> - update KeyTalk installation directory in resept.ini
// ConfigUpdater --restore-data   - restore KeyTalk provider data (images), CA names and resept.ini from backup. Also convert KeyTalk configuration to a new format when necessary. Normally called during upgrade.

//
// @note Make sure to call the tool with --set-install-dir argument before calling it with --restore-data argument because the latter relies on installation dir to be set in configuration
//
// @note ConfigUpdater should be launched with enough privileges to update common settings (e.g. as Impersonate='no' in wix script)

using std::string;
using std::vector;
namespace fs = boost::filesystem;

enum {retOk = 0, retBadArgs, retError};

static const string SetInstallDirArg = "--set-install-dir";
static const string RestoreDataArg = "--restore-data";

enum Command { commandSetInstallDir, commandRestoreData };

struct ParsedArgs
{
    Command cmd;
    string installDir; // makes sense for commandSetInstallDir only
};

string normalizeReseptInstallDir(const string& aDir)
{
    string myDir = aDir;

    boost::trim(myDir);
    boost::trim_if(myDir, boost::is_any_of("\""));
    boost::trim_right_if(myDir, boost::is_any_of(ta::getDirSep()));

    return myDir;
}

#ifdef _WIN32
bool parseCmdLineArgs(LPTSTR  lpCmdLine, ParsedArgs& aParsedArgs)
{
    if (!lpCmdLine || !(*lpCmdLine))
    {
        ERRORLOG("Invalid arguments (no args provided)");
        return retBadArgs;
    }
    DEBUGLOG(boost::format("Started Config Updater with args %s") % lpCmdLine);
    const string myCmdLine = lpCmdLine;

    if (myCmdLine.find(RestoreDataArg) == 0)
    {
        aParsedArgs.cmd = commandRestoreData;
        return true;
    }
    else if (myCmdLine.find(SetInstallDirArg) == 0)
    {
        aParsedArgs.cmd = commandSetInstallDir;
        aParsedArgs.installDir = normalizeReseptInstallDir(myCmdLine.substr(SetInstallDirArg.length()));
        return true;
    }

    ERRORLOG("Invalid arguments: " + myCmdLine);
    return false;
}
#else
bool parseCmdLineArgs(int argc, char* argv[], ParsedArgs& aParsedArgs)
{
    DEBUGLOG(boost::format("Started Config Updater with %d args") % (argc-1));
    if (argc == 2)
    {
        const string myOpt = argv[1];
        if (myOpt == RestoreDataArg)
        {
            aParsedArgs.cmd = commandRestoreData;
            return true;
        }
    }
    else if (argc == 3)
    {
        const string myOpt = argv[1];
        if (myOpt == SetInstallDirArg)
        {
            aParsedArgs.cmd = commandSetInstallDir;
            aParsedArgs.installDir = normalizeReseptInstallDir(argv[2]);
            return true;
        }
    }
    ERRORLOG("Invalid arguments");
    return false;
}
#endif

void initLogger()
{
    string myLogDir;
    try  { myLogDir = ta::Process::getTempDir();}
    catch (std::runtime_error&) {}
    string myLogFileName = myLogDir + rclient::ConfigUpdaterLogFileName;
    const string myEnvInfo = str(boost::format("%s Client-%s Installation Config Updater (user: %s)") % resept::ProductName % toStr(rclient::ClientVersion) % ta::getUserName());
    ta::LogConfiguration::Config myMemConfig;
    myMemConfig.fileAppender = true;
    myMemConfig.fileAppenderLogThreshold = ta::LogLevel::Debug;
    myMemConfig.fileAppenderLogFileName = myLogFileName;
    ta::LogConfiguration::instance().load(myMemConfig);
    PROLOG(myEnvInfo);
}

void deInitLogger()
{
    EPILOG(boost::format("%s Client-%s Installer") % resept::ProductName % toStr(rclient::ClientVersion));
}

int doMain(const ParsedArgs& aParsedArgs)
{
    switch (aParsedArgs.cmd)
    {
    case commandSetInstallDir:
    {
        DEBUGLOG("Setting " + resept::ProductName + " installation directory  to " + aParsedArgs.installDir);
        rclient::Settings::setReseptInstallDir(aParsedArgs.installDir);
        return retOk;
    }
    case commandRestoreData:
    {
        DEBUGLOG("Restore data requested");

        const string myBackupDir = rclient::getInstallerDataBackupDir();
        DEBUGLOG(boost::format("Restoring from %s") % myBackupDir);

        // Grab necessary settings from the backed up resept.ini
        rclient::Settings::setConfigsPath(myBackupDir + ta::getDirSep() + rclient::Settings::ReseptConfigFileName, rclient::Settings::getUserConfigPath(), rclient::Settings::getMasterConfigPath());
        const vector<string> myProviders = rclient::Settings::getInstalledProviders();
        const vector<string> myUserCAs = rclient::Settings::getInstalledUserCaCNs();
        const vector<string> myServerCAs = rclient::Settings::getInstalledServerCaCNs();
        const vector<string> myPrimaryCAs = rclient::Settings::getInstalledPrimaryCaCNs();
        const vector<string> myRootCAs = rclient::Settings::getInstalledRootCaCNs();
        const vector<string> myInstalledExtraSigningIntCAs = rclient::Settings::getInstalledExtraSigningIntCaSha1Fingerprints();
        const vector<string> myInstalledExtraSigningRootCAs = rclient::Settings::getInstalledExtraSigningRootCaSha1Fingerprints();
        const vector<string> myCustomizedUsers = rclient::Settings::getCustomizedUsers();

        // Update installed resept.ini
        DEBUGLOG("Updating resept.ini");
        rclient::Settings::resetConfigsPath();
        foreach (const string& prov, myProviders)
        {
            rclient::Settings::addInstalledProvider(prov);
        }
        foreach (const string& ca, myUserCAs)
        {
            rclient::Settings::addInstalledUserCA(ca);
        }
        foreach (const string& ca, myServerCAs)
        {
            rclient::Settings::addInstalledServerCA(ca);
        }
        foreach (const string& ca, myPrimaryCAs)
        {
            rclient::Settings::addInstalledPrimaryCA(ca);
        }
        foreach (const string& ca, myRootCAs)
        {
            rclient::Settings::addInstalledRootCA(ca);
        }
        foreach(const string& ca, myInstalledExtraSigningIntCAs)
        {
            rclient::Settings::addInstalledExtraSigningIntCA(ca);
        }
        foreach(const string& ca, myInstalledExtraSigningRootCAs)
        {
            rclient::Settings::addInstalledExtraSigningRootCA(ca);
        }
        foreach (const string& user, myCustomizedUsers)
        {
            rclient::Settings::addCustomizedUser(user);
        }


        // Copy provider data
        foreach (const string& prov, myProviders)
        {
            DEBUGLOG(boost::format("Copying data of %s provider") % prov);
            ta::copyDir(myBackupDir + ta::getDirSep() + prov, rclient::Settings::getProviderInstallDir(prov));
        }

        // Cleanup
        DEBUGLOG("Cleaning up (removing backup)");
        try {
            fs::remove_all(myBackupDir);
        } catch (std::exception& e) {
            WARNLOG2("Failed to remove backup dir " + myBackupDir, e.what());
        }

        //@note place to convert configs when applicable

        return retOk;
    }
    default:
    {
        ERRORLOG(boost::format("Unsupported command %d") % aParsedArgs.cmd);
        return retBadArgs;
    }
    }
}


#ifdef _WIN32
int APIENTRY WinMain(HINSTANCE UNUSED(hInstance), HINSTANCE UNUSED(hPrevInstance), LPTSTR  lpCmdLine, int UNUSED(nCmdShow))
#else
int main(int argc, char* argv[])
#endif
{
    initLogger();
    int myRetVal = retError;
    try
    {
        ParsedArgs myParsedArgs;
#ifdef _WIN32
        if (!parseCmdLineArgs(lpCmdLine, myParsedArgs))
            return retBadArgs;
#else
        if (!parseCmdLineArgs(argc, argv, myParsedArgs))
            return retBadArgs;
#endif
        myRetVal = doMain(myParsedArgs);
    }
    catch (std::exception& e)
    {
        ERRORLOG2("Error", e.what());
    }
    catch (...)
    {
        ERRORLOG("Unknown error");
    }
    INFOLOG(boost::format("Exiting with retval %d") % myRetVal);
    deInitLogger();
    return myRetVal;
}

