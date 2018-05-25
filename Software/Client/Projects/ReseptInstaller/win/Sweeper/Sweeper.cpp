#include "rclient/TaskSettings.h"
#include "rclient/Settings.h"
#include "rclient/NativeCertStore.h"
#include "rclient/Common.h"
#include "resept/common.h"

#include "ta/process.h"
#include "ta/strings.h"
#include "ta/logappender.h"
#include "ta/logconfiguration.h"
#include "ta/logger.h"
#include "ta/version.h"
#include "ta/assert.h"
#include "ta/utils.h"
#include "ta/common.h"

#include "boost/algorithm/string.hpp"
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/convenience.hpp"
#include <string>
#include <stdexcept>
#include <vector>
#include <windows.h>

using std::string;
using std::vector;
using std::swap;
namespace fs = boost::filesystem;

//
// Usage: Sweeper --remove-userdata     - removes RESEPT data that belongs to the current user: imported personal certs and user configuration. Should be called in the user's context (e.g. with Impersonate='yes' when called from wix script)
//        Sweeper --remove-commondata   - removes RESEPT data that does not belong to the current user: imported CAs, provider data (communication keys/images) and master config. Should be called with sufficient privileges (e.g. with Impersonate='no' when called from wix script)
//        Sweeper --backup-data - backs up RESEPT provider data (images) and resept.ini to the temporary directory. Normally called when being upgraded and is followed by restoration of the backupped data (ConfigUpdater --restore-data)
//        Sweeper --remove-providerdata - removes RESEPT provider data (images) found in RESEPT installation dir. Should be called with sufficient privileges (e.g. with Impersonate='no' when called from wix script). Normally called when being upgraded.
//
//
// Return values:
// 0 if succeeded, 1 if arguments are incorrect, 2 for other errors

static const string RemoveUserDataArg   = "--remove-userdata";
static const string RemoveCommonDataArg = "--remove-commondata";
static const string RemoveProviderDataArg = "--remove-providerdata";
static const string BackupDataArg = "--backup-data";
static const string InstallerWindowTitle = str(boost::format("%s %s Setup") % resept::ProductName % ta::version::toStr(rclient::ClientVersion, ta::version::fmtMajorMinor));
enum {retOk = 0, retBadSrg, retError};

string _fmtLastError()
{
    const DWORD myLastError = ::GetLastError();
    if (myLastError == ERROR_ACCESS_DENIED)
        return "Access denied.";
    if (myLastError == ERROR_SHARING_VIOLATION)
        return "The process cannot access the file because it is being used by another process.";
    return "";

}

//@nothrow
bool deleteFromPersonalStore(const string& anIssuerName)
{
    DEBUGLOG(boost::format("Deleting certificates issued by %s from the personal store") % anIssuerName);
    try
    {
        rclient::NativeCertStore::deleteUserCertsForIssuerCN(anIssuerName, rclient::NativeCertStore::proceedOnError);
        return true;
    }
    catch (...)
    {
        ERRORLOG(boost::format("Failed deleting certificates issued by %s from the personal store") % anIssuerName);
        return false;
    }
}

bool deleteFromIntermediateStore(const string& aCertCn)
{
    DEBUGLOG(boost::format("Deleting certificates issued to %s from the intermediate store") % aCertCn);
    try
    {
        rclient::NativeCertStore::deleteFromIntermediateStoreByCN(aCertCn, rclient::NativeCertStore::proceedOnError);
        return true;
    }
    catch (...)
    {
        ERRORLOG(boost::format("Failed deleting certificates issued to %s from the intermdeiate store") % aCertCn);
        return false;
    }
}

bool deleteFromRootStore(const string& aCertCn)
{
    DEBUGLOG(boost::format("Deleting certificates issued to %s from the root store") % aCertCn);
    try
    {
        rclient::NativeCertStore::deleteFromRootStoreByCN(aCertCn, rclient::NativeCertStore::proceedOnError);
        return true;
    }
    catch (...)
    {
        ERRORLOG(boost::format("Failed deleting certificates issued to %s from the root store") % aCertCn);
        return false;
    }
}


void initLogger()
{
    string myLogDir;
    try  { myLogDir = ta::Process::getTempDir();}
    catch (std::runtime_error&) {}
    string myLogFileName = myLogDir + rclient::SweeperLogFileName;
    const string myEnvInfo = str(boost::format("%s Client-%s Installation Sweeper (user: %s)") % resept::ProductName % toStr(rclient::ClientVersion) % ta::getUserName());
    ta::LogConfiguration::Config myMemConfig;
    myMemConfig.fileAppender = true;
    myMemConfig.fileAppenderLogThreshold = ta::LogLevel::Debug;
    myMemConfig.fileAppenderLogFileName = myLogFileName;
    ta::LogConfiguration::instance().load(myMemConfig);
    PROLOG(myEnvInfo);
}

void deInitLogger()
{
    EPILOG(boost::format("RESEPT Client-%s Installer") % toStr(rclient::ClientVersion));
}

//@nothrow
//@return true on success or if the user tolerated the error, false otherwise
// Recursively deletes the contents of file or directory aFilePath if it exists, then deletes file p itself
static bool removeAll(const string& aPath)
{
    DEBUGLOG("Removing " + aPath);
    while (true)
    {
        try
        {
            fs::remove_all(aPath);
            return true;
        }
        catch (std::exception& e)
        {
            const string myErrorMsg = str(boost::format("Cannot remove %s. %s") % aPath  % _fmtLastError());
            ERRORLOG(boost::format("%s %s") % myErrorMsg % e.what());
            int myRes = ::MessageBox(::FindWindow(NULL, InstallerWindowTitle.c_str()),
                                     myErrorMsg.c_str(),
                                     InstallerWindowTitle.c_str(),
                                     MB_CANCELTRYCONTINUE | MB_ICONWARNING);
            if (myRes == IDCONTINUE)
                return true;
            if (myRes == IDCANCEL)
                return false;
        }
    }
}


int doMain(LPSTR  lpCmdLine)
{
    if (!lpCmdLine || !(*lpCmdLine))
        return retBadSrg;
    DEBUGLOG(boost::format("Started Sweeper with args %s") % lpCmdLine);

    if (RemoveUserDataArg.compare(lpCmdLine) == 0)
    {
        DEBUGLOG("Remove user data requested");

        DEBUGLOG("-- Removing user personal certificates");
        foreach (const string& sessionCertIssuer, rclient::Settings::getInstalledUserCaCNs())
        {
            deleteFromPersonalStore(sessionCertIssuer);
        }

        DEBUGLOG("-- Removing RESEPT user config directory");
        const string myUserConfigDir = rclient::Settings::getUserConfigDir();
        if (!removeAll(myUserConfigDir))
            return retError;

        return retOk;
    }

    if (RemoveCommonDataArg.compare(lpCmdLine) == 0)
    {
        DEBUGLOG("Remove common data requested");

        DEBUGLOG("--- Removing CAs");
        foreach (string uca, rclient::Settings::getInstalledUserCaCNs())
        {
            if (!deleteFromIntermediateStore(uca))
                return retError;
        }
        foreach (string sca, rclient::Settings::getInstalledServerCaCNs())
        {
            if (!deleteFromIntermediateStore(sca))
                return retError;
        }
        foreach (string pca, rclient::Settings::getInstalledPrimaryCaCNs())
        {
            // PCA can be in intermediate if RCA is present or in the root store if RCA is not present
            if (!deleteFromIntermediateStore(pca))
                return retError;
            if (!deleteFromRootStore(pca))
                return retError;
        }
        foreach (string rca, rclient::Settings::getInstalledRootCaCNs())
        {
            if (!deleteFromRootStore(rca))
                return retError;
        }

        DEBUGLOG("--- Removing provider data");
        foreach (string provider, rclient::Settings::getInstalledProviders())
        {
            const string myProviderInstallDir = rclient::Settings::getProviderInstallDir(provider);
            if (!removeAll(myProviderInstallDir))
                return retError;
        }

        DEBUGLOG("--- Removing master config");
        const string myMasterConfigPath = rclient::Settings::getMasterConfigPath();
        if (!removeAll(myMasterConfigPath))
            return retError;

        DEBUGLOG("--- Removing tasks config");
        const string myTasksConfigPath = rclient::Settings::getTaskConfigPath();
        if (boost::filesystem::exists(myTasksConfigPath))
        {
            if (!removeAll(myTasksConfigPath))
                return retError;
        }

        return retOk;
    }

    if (BackupDataArg.find(lpCmdLine) == 0)
    {
        DEBUGLOG("Backup data requested");

        const string myBackupDir = rclient::getInstallerDataBackupDir();
        DEBUGLOG(boost::format("Backing up to %s") % myBackupDir);
        fs::remove_all(myBackupDir);

        try
        {
            fs::create_directories(myBackupDir);

            DEBUGLOG(boost::format("Copying %s") % rclient::Settings::ReseptConfigFileName);
            fs::copy_file(rclient::Settings::getReseptConfigPath(), myBackupDir + "\\" + rclient::Settings::ReseptConfigFileName);
            foreach (string provider, rclient::Settings::getInstalledProviders())
            {
                DEBUGLOG(boost::format("Copying data of %s provider") % provider);
                ta::copyDir(rclient::Settings::getProviderInstallDir(provider), myBackupDir + "\\" + provider);
            }

            DEBUGLOG(boost::format("Copying %s") % rclient::Settings::TaskConfigFileName);
            const string myTasksConfigPath = rclient::Settings::getTaskConfigPath();
            if (boost::filesystem::exists(myTasksConfigPath))
            {
                fs::copy_file(myTasksConfigPath, myBackupDir + "\\" + rclient::Settings::TaskConfigFileName);
            }
        }
        catch (std::exception&)
        {
            ERRORLOG("Error occurred during backup, cleaning up");
            try { fs::remove_all(myBackupDir); } catch (...) {}
            throw;
        }

        return retOk;
    }

    if (RemoveProviderDataArg.compare(lpCmdLine) == 0)
    {
        DEBUGLOG("Remove keys/images requested");

        DEBUGLOG("--- Removing provider data");
        foreach (string provider, rclient::Settings::getInstalledProviders())
        {
            const string myProviderInstallDir = rclient::Settings::getProviderInstallDir(provider);
            if (!removeAll(myProviderInstallDir))
                return retError;
        }
        return retOk;
    }



    ERRORLOG("Invalid arguments");
    return retBadSrg;
}

int APIENTRY WinMain(HINSTANCE UNUSED(hInstance), HINSTANCE UNUSED(hPrevInstance), LPSTR  lpCmdLine, int  UNUSED(nCmdShow))
{
    initLogger();
    int myRetVal = retError;
    try
    {
        myRetVal = doMain(lpCmdLine);
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
