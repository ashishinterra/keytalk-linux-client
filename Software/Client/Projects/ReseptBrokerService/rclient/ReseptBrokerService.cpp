#include "ReseptBrokerService.h"
#include "rclient/ContentConfig.h"
#include "rclient/Settings.h"
#include "rclient/Common.h"
#include "resept/util.h"
#include "ta/utils.h"
#include "ta/process.h"
#include "ta/logappender.h"
#include "ta/logconfiguration.h"
#include "ta/logger.h"
#include "ta/tcpserver.h"
#include "ta/proto.hpp"

#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/convenience.hpp"
#include <windows.h>
#include <string>
#include <sstream>
#include <vector>
#include <list>
#include <memory>

using std::string;
namespace fs = boost::filesystem;

SERVICE_STATUS theServiceStatus = { 0 };
SERVICE_STATUS_HANDLE theServiceStatusHandle = NULL;
static const unsigned int AcceptTimeout = 1000;

// @note When the user imports CA cert by hand (say, PCA) its default location will be (as viewed from mmc certificates snap-in):
// 'Certificates - Current User' =>  'Trusted Root Certificate Authorities' ('ROOT') logical store => 'Registry' ('.Default') physical store
//
// When the service running in the context of LocalSystem account imports this PCA cert, the cert will go to (as viewed from mmc certificates snap-in):
// 'Certificates (Local Computer)' =>  'Trusted Root Certificate Authorities' ('ROOT') logical store => 'Registry' ('.Default') physical store
// and it will be also accessible from:
// 'Certificates - Current User' =>  'Trusted Root Certificate Authorities' ('ROOT') logical store => 'Local Computer' ('.LocalMachine') physical store
//
// To see physical store via mmc Certificates snap-in right-click in "Certificates (Local Computer)" => View => Options => check "Physical Store nodes"
// Additional info: http://www.mail-archive.com/wix-users@lists.sourceforge.net/msg22330.html


namespace rclient
{

    namespace ReseptBrokerService
    {
        // @nothrow
        void initLogger()
        {
            string myLogDir;
            try
            {
                myLogDir = ta::Process::getCommonAppDataDir() + "\\" + resept::CompanyName;
                if (!ta::isDirExist(myLogDir))
                    fs::create_directories(myLogDir);

                const string myLogFileName = str(boost::format("%s\\%s") % myLogDir % BrokerServiceLogName);
                const string myEnvInfo = str(boost::format("%s Client-%s %s (user: %s)") % resept::ProductName % toStr(rclient::ClientVersion) % BrokerServiceName % ta::getUserName());
                ta::LogConfiguration::Config myMemConfig;
                myMemConfig.fileAppender = true;
                myMemConfig.fileAppenderLogThreshold = ta::LogLevel::Debug;
                myMemConfig.fileAppenderLogFileName = myLogFileName;
                ta::LogConfiguration::instance().load(myMemConfig);
                PROLOG(myEnvInfo);
            }
            catch (...)
            {}
        }

        void deInitLogger()
        {
            EPILOG(boost::format("RESEPT Client-%s %s") % toStr(rclient::ClientVersion) % BrokerServiceName);
        }

        void tryRestoreFile(const string& aFrom, const string& aTo)
        {
            try
            {
                DEBUGLOG("Restoring " + aFrom + " to " + aTo);
                fs::copy_file(aFrom, aTo, fs::copy_option::overwrite_if_exists);
            }
            catch (std::exception& e)
            {
                WARNLOG2("Failed to restore " + aFrom + " to " + aTo + ". Tolerating.", e.what());
            }
        }

        void tryRestoreDir(const string& aFrom, const string& aTo)
        {
            try
            {
                WARNLOG("Restoring directory " + aFrom + " to " + aTo);
                fs::remove_all(aTo);
                ta::copyDir(aFrom, aTo);
            }
            catch (std::exception& e)
            {
                WARNLOG2("Failed to restore " + aFrom + " to " + aTo + ". Tolerating.", e.what());
            }
        }

        void tryRemoveProviderDirectoryWithRestore(const string& aProviderDir, const string& aProviderName)
        {
            ta::Process::ScopedDir providerDataBackupDir(ta::Process::genTempPath());
            ta::copyDir(aProviderDir, providerDataBackupDir.path + ta::getDirSep() + aProviderName);
            try
            {
                DEBUGLOG("Removing provider data at " + aProviderDir);
                fs::remove_all(aProviderDir);
            }
            catch (std::exception& e)
            {
                ERRORLOG2("Error occurred uninstalling configuration at " + aProviderDir + ". Restoring the original provider data", e.what());
                tryRestoreDir(providerDataBackupDir.path + ta::getDirSep() + aProviderName, aProviderDir);
                throw;
            }
        }

        // Try removing provider data from the specified dir, without removing the directory itself
        // tolerate all errors occurred during removal
        void tryRemoveProviderData(const string& aProviderDir)
        {
            DEBUGLOG("Remove provider data from " + aProviderDir + " without removing the directory itself");

            if (ta::isDirExist(aProviderDir))
            {
                fs::path dir(aProviderDir);
                for (fs::directory_iterator end_dir_it, it(dir); it!=end_dir_it; ++it)
                {
                    try
                    {
                        fs::remove_all(it->path());
                    }
                    catch (std::exception& e)
                    {
                        WARNLOG2("Failed to remove " + it->path().string() + ". Tolerating.", e.what());
                    }
                }
            }
        }

        //
        // Purpose:
        //   Sets the current service status and reports it to the SCM.
        //
        // Parameters:
        //   dwCurrentState - The current state (see SERVICE_STATUS)
        //   dwWin32ExitCode - The system error code
        //   dwWaitHint - Estimated time for pending operation, in milliseconds
        //
        // Return value:
        //   None
        //
        void reportSvcStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
        {
            static DWORD dwCheckPoint = 1;

            theServiceStatus.dwCurrentState = dwCurrentState;
            theServiceStatus.dwWin32ExitCode = dwWin32ExitCode;
            theServiceStatus.dwWaitHint = dwWaitHint;

            if (dwCurrentState == SERVICE_START_PENDING)
                theServiceStatus.dwControlsAccepted = 0;
            else
                theServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

            if ((dwCurrentState == SERVICE_RUNNING) || (dwCurrentState == SERVICE_STOPPED))
                theServiceStatus.dwCheckPoint = 0;
            else
                theServiceStatus.dwCheckPoint = dwCheckPoint++;

            // Report the status of the service to the SCM.
            SetServiceStatus(theServiceStatusHandle, &theServiceStatus);
        }


        // Control handler function
        void ControlHandler(DWORD request)
        {
            switch (request)
            {
            case SERVICE_CONTROL_STOP:
                DEBUGLOG("Stopping due to stop request");
                reportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
                return;
            case SERVICE_CONTROL_SHUTDOWN:
                DEBUGLOG("Stopping due to shutdown");
                reportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
                return;
            default:
                break;
            }
        }

        std::auto_ptr<ta::TcpServer> initService()
        {
            DEBUGLOG("Initializing...");
            std::auto_ptr<ta::TcpServer> mySvr(new ta::TcpServer());
            unsigned int myListenPort = mySvr->listen("127.0.0.1");
            DEBUGLOG(boost::format("Listening on 127.0.0.1:%u") % myListenPort);
            rclient::Settings::setReseptBrokerServicePort(myListenPort);
            return mySvr;
        }

        bool findProviderForNameDifferInCaseOnly(const string& aProviderName, string& aProviderWithDifferentCaseName)
        {
            foreach(const string& provider, rclient::Settings::getProviders())
            {
                if (boost::iequals(provider, aProviderName) && provider != aProviderName)
                {
                    aProviderWithDifferentCaseName = provider;
                    return true;
                }
            }
            return false;
        }

        void installSettings(const InstallSettingsRequest& aRequest, ta::TcpClient& aConnection)
        {
            // Become user context-aware because broker service runs as system
            rclient::Settings::setUserConfigPath(aRequest.userConfigPath);

            const string myNewProviderName = aRequest.contentConfig.getProviderName();
            const int myNewContentVersion = aRequest.contentConfig.getContentVersion();

            // disallow messing up with reserved directories
            foreach(const std::string& reservedProviderName, resept::ReservedProviderNames)
            {
                if (boost::iequals(myNewProviderName, reservedProviderName))
                {
                    const string myUserErrorResponse = "Cannot install settings for provider " + myNewProviderName + " because this name is used by " + resept::ProductName + " for internal purposes.";
                    WARNLOG(myUserErrorResponse);
                    ta::proto::send(aConnection, Response(responseStatusUserError, myUserErrorResponse));
                    return;
                }
            }

            if (rclient::Settings::isCustomized())
            {
                string myProviderForNameDifferInCaseOnly;
                if (findProviderForNameDifferInCaseOnly(myNewProviderName, myProviderForNameDifferInCaseOnly))
                {
                    const string myUserErrorResponse = "Cannot install settings for provider " + myNewProviderName + " because it differs from the already installed settings for provider " + myProviderForNameDifferInCaseOnly + " in character case only.";
                    WARNLOG(myUserErrorResponse);
                    ta::proto::send(aConnection, Response(responseStatusUserError, myUserErrorResponse));
                    return;
                }

                // Ask for confirmation when downgrading settings
                if (ta::isElemExist(myNewProviderName, rclient::Settings::getProviders()))
                {
                    if (rclient::Settings::getProviderContentVersion(myNewProviderName) > myNewContentVersion)
                    {
                        const string myUserPrompt = "The version of the settings being installed for provider " + myNewProviderName + " is older than the version of the settings already installed. Are you sure you want to proceed?";
                        DEBUGLOG("The version of the settings being installed for provider " + myNewProviderName + " is older than the version of the settings already installed. Asking user for confirmation");
                        ta::proto::send(aConnection, Response(responseStatusConfirmation, myUserPrompt));
                        const bool myIsConfirmed = ta::proto::receive<bool>(aConnection);
                        DEBUGLOG(boost::format("User confirmed %s to overwrite settings with the older version") % (myIsConfirmed ? "" : "not"));
                        if (!myIsConfirmed)
                        {
                            return;
                        }
                    }
                }
            }

            ContentConfig::install(aRequest.contentConfig);
        }

        void uninstallUserSettings(const UninstallSettingsRequest& aRequest)
        {
            DEBUGLOG("Uninstalling user settings for provider " + aRequest.provider);

            // Become user context-aware because broker service runs as system
            rclient::Settings::setUserConfigPath(aRequest.userConfigPath);

            if (!ta::isElemExist(aRequest.provider, rclient::Settings::getProviders()))
            {
                WARNLOG("Cannot uninstall settings for provider " + aRequest.provider + " because no such a provider installed.");
                return;
            }

            const string myReseptConfigPath = rclient::Settings::getReseptConfigPath();
            const string myProviderInstallDir = rclient::Settings::getProviderInstallDir(aRequest.provider);

            // Uninstall settings in transaction. Backup files first.
            //
            ta::Process::ScopedDir configBackupDir(ta::Process::genTempPath());
            fs::copy_file(aRequest.userConfigPath, configBackupDir.path + ta::getDirSep() + "user.ini", fs::copy_option::overwrite_if_exists);
            fs::copy_file(myReseptConfigPath, configBackupDir.path + ta::getDirSep() + "resept.ini", fs::copy_option::overwrite_if_exists);

            try
            {
                DEBUGLOG("Removing user settings for provider " + aRequest.provider + " at " + aRequest.userConfigPath);
                rclient::Settings::removeProviderFromUserConfig(aRequest.provider);

                try
                {
                    DEBUGLOG("Removing reference to the installed provider " + aRequest.provider + " from the installation registry at " + myReseptConfigPath);
                    rclient::Settings::removeInstalledProvider(aRequest.provider);

                    if (fs::exists(myProviderInstallDir))
                    {
                        foreach (const std::string& reservedProviderName, resept::ReservedProviderNames)
                        {
                            if (!boost::iequals(aRequest.provider, reservedProviderName))
                            {
                                tryRemoveProviderDirectoryWithRestore(myProviderInstallDir, aRequest.provider);
                            }
                            else
                            {
                                tryRemoveProviderData(myProviderInstallDir);
                            }
                        }
                    }
                    else
                    {
                        // We can land up here when e.g. pre-4.4.2 client installed two or more providers which names differ in character case only and then removed one of these providers. See #415
                        WARNLOG("Skip removal provider data because provider directory " + myProviderInstallDir + " does not exist");
                    }
                }
                catch (std::exception& e)
                {
                    ERRORLOG2("Error occurred uninstalling configuration. Restoring the original reference to the installed provider" + aRequest.provider, e.what());
                    tryRestoreFile(configBackupDir.path + ta::getDirSep() + "resept.ini", myReseptConfigPath);
                    throw;
                }
            }
            catch (std::exception& e)
            {
                ERRORLOG2("Error occurred uninstalling configuration. Restoring the original user settings for provider " + aRequest.provider, e.what());
                tryRestoreFile(configBackupDir.path + ta::getDirSep() + "user.ini", aRequest.userConfigPath);
                throw;
            }

            // success!

            //@note we do not remove CAs because they can be used by other users. They will be removed during uninstall anyway.
        }

        void doServiceMain(int UNUSED(argc), char** UNUSED(argv))
        {
            theServiceStatusHandle = ::RegisterServiceCtrlHandler(BrokerServiceName, (LPHANDLER_FUNCTION)ControlHandler);
            if (theServiceStatusHandle == (SERVICE_STATUS_HANDLE)0)
            {
                ERRORLOG2("Error registering service", boost::format("RegisterServiceCtrlHandler failed. LastError: %d") % ::GetLastError());
                return;
            }

            theServiceStatus.dwServiceType = SERVICE_WIN32;
            theServiceStatus.dwCurrentState = SERVICE_START_PENDING;
            theServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
            theServiceStatus.dwWin32ExitCode = 0;
            theServiceStatus.dwServiceSpecificExitCode = 0;
            theServiceStatus.dwCheckPoint = 0;
            theServiceStatus.dwWaitHint = 0;

            reportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 50);

            try
            {
                std::auto_ptr<ta::TcpServer> mySvr = initService();
                reportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

                // The worker loop
                while (theServiceStatus.dwCurrentState == SERVICE_RUNNING)
                {
                    std::auto_ptr<ta::TcpClient> myConnection;
                    try
                    {
                        myConnection = mySvr->accept(AcceptTimeout);
                        DEBUGLOG("Incoming connection");
                        const RequestType myReqType = ta::proto::receive<RequestType>(*myConnection);
                        switch (myReqType)
                        {
                        case requestInstallSettings:
                        {
                            DEBUGLOG("Incoming request to install settings");
                            const InstallSettingsRequest myRequest = ta::proto::receive<InstallSettingsRequest>(*myConnection);
                            installSettings(myRequest, *myConnection);
                            ta::proto::send(*myConnection, Response(responseStatusOk));
                            break;
                        }
                        case requestUninstallSettings:
                        {
                            DEBUGLOG("Incoming request to uninstall user settings");
                            const UninstallSettingsRequest myRequest = ta::proto::receive<UninstallSettingsRequest>(*myConnection);
                            uninstallUserSettings(myRequest);
                            ta::proto::send(*myConnection, Response(responseStatusOk));
                            break;
                        }
                        default:
                            WARNLOG(boost::format("Unsupported request %d") % myReqType);
                            break;
                        }
                    }
                    catch (ta::TcpServerConnectionTimedOut&)
                    {
                        // just check if we need to stop and go on
                        continue;
                    }
                    catch (std::exception& e)
                    {
                        ERRORLOG2("ReseptBrokerService error", e.what());
                        if (myConnection.get())
                        {
                            try { ta::proto::send(*myConnection, Response(responseStatusError, e.what())); }
                            catch (std::exception& e) { ERRORLOG2("ReseptBrokerService error", e.what()); }
                        }
                    }
                    DEBUGLOG("Done with this connection");
                } // worker loop

                reportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
            }
            catch (std::exception& e)
            {
                ERRORLOG2("ReseptBrokerService error", e.what());
            }
            catch (...)
            {
                ERRORLOG("Unexpected error");
            }
            reportSvcStatus(SERVICE_STOPPED, (DWORD)-1, 0);
        }


        void ServiceMain(int argc, char** argv)
        {
            initLogger();
            doServiceMain(argc, argv);
            deInitLogger();
        }

    }
}

void main()
{
    static char myReseptBrokerServiceName[sizeof(rclient::BrokerServiceName) + 1];
    strcpy(myReseptBrokerServiceName, rclient::BrokerServiceName);
    SERVICE_TABLE_ENTRY DispatchTable[] =
    {
        { myReseptBrokerServiceName, (LPSERVICE_MAIN_FUNCTION)rclient::ReseptBrokerService::ServiceMain },
        { NULL, NULL }
    };
    StartServiceCtrlDispatcher(DispatchTable);
}



