//----------------------------------------------------------------------------
//
//  Name          ReseptDesktopClientApp.cpp
//  Description : ReseptDesktopClientApp class implementation
//
//----------------------------------------------------------------------------
#include "ReseptDesktopClientApp.h"
#include "rclient/AuthenticationWizard.h"
#include "rclient/Settings.h"
#include "rclient/NativeCertStore.h"
#include "rclient/RcdpHandler.h"
#include "rclient/Common.h"
#include "resept/Common.h"
#include "ta/InternetExplorer.h"
#include "ta/logger.h"
#include "ta/browser.h"
#include "ta/netutils.h"
#include "ta/timeutils.h"
#include "ta/strings.h"
#include "ta/process.h"
#include "ta/url.h"
#include "ta/utils.h"
#include "ta/common.h"
#include <QtWidgets/QMessageBox>

using namespace ta;
using std::string;

namespace
{
    struct BrowserSelectonError : std::exception
    {
        explicit BrowserSelectonError(const std::string& aUserMessage) : userMsg(aUserMessage) {}
        const string userMsg;
    };

    Browser::BrowserType getTargetBrowser()
    {
#ifdef _WIN32
        if (!ta::InternetExplorer::isInstalled())
        {
            throw BrowserSelectonError("Internet Explorer is not installed. Please install Internet Explorer and try again.");
        }
        if (!rclient::isReseptIeAddonInstalled())
        {
            throw BrowserSelectonError(resept::ProductName + " IE add-on is not installed. Please reinstall " + resept::ProductName + " selecting IE add-on feature during installation.");
        }
        return Browser::IE;
#else
# error "Not implemented"
#endif
    }
}

ReseptDesktopClientApp::ReseptDesktopClientApp(int& UNUSED(argc), char** UNUSED(argv))
{
    initQt();
    checkReseptCustomized();
    initLogger();
    initOpenSSL();

    DEBUGLOG("Initialized RESEPT Desktop Client");
}

ReseptDesktopClientApp::~ReseptDesktopClientApp()
{
    DEBUGLOG("Destroying RESEPT Desktop Client");
}

void ReseptDesktopClientApp::checkReseptCustomized()
{
    try
    {
        if (!rclient::Settings::isCustomized())
        {
            const string myConfigManagerPath = str(boost::format("\"%s%s%s\"") % rclient::Settings::getReseptInstallDir() % ta::getDirSep() % rclient::ReseptConfigManager);
            string myStdOut, myStdErr;
            ta::Process::shellExecSync(myConfigManagerPath, myStdOut, myStdErr);
            if (!rclient::Settings::isCustomized())
                TA_THROW(ReseptDesktopClientAppError);
        }
    }
    catch (std::exception&)
    {
        QMessageBox::warning(NULL,
                             "Installation not customized",
                             (boost::format("%1% Installation has not been customized. Please customize %1% by running %1% Configuration Manager.") % resept::ProductName).str().c_str() );
        TA_THROW(ReseptDesktopClientAppError);
    }
}

void ReseptDesktopClientApp::initQt()
{
    try
    {
        theQtAppPtr.reset(new rclient::QtExclusiveApp());
    }
    catch (rclient::QtExclusiveAppLockError&)
    {
        TA_THROW(ReseptDesktopClientAppError);
    }
    catch (std::runtime_error&)
    {
        TA_THROW(ReseptDesktopClientAppError);
    }
}

void ReseptDesktopClientApp::initLogger()
{
    try
    {
        theLoggerInitializer.reset(new rclient::LoggerInitializer());
    }
    catch (rclient::LoggerInitError& e)
    {
        QMessageBox::warning(NULL, "Failed to initialize logger", str(boost::format("Failed to initialize logger. %s. Please contact %s administrator.") % e.what() % resept::ProductName).c_str());
        TA_THROW(ReseptDesktopClientAppError);
    }
}

void ReseptDesktopClientApp::initOpenSSL()
{
    try
    {
        theOpenSSLAppPtr.reset(new ta::OpenSSLApp());
    }
    catch (std::runtime_error& e)
    {
        const string myUserMsg = "Failed to initialize crypto subsystem. Please contact " + resept::ProductName + " administrator.";
        ERRORLOG2(myUserMsg, e.what());
        QMessageBox::warning(NULL, "Failed to initialize crypto subsystem", myUserMsg.c_str());
        TA_THROW(ReseptDesktopClientAppError);
    }
}

void ReseptDesktopClientApp::execute()
{
    string myServiceUri;
    try
    {
        rclient::AuthenticationWizard myAuthenticationWizard;
        myAuthenticationWizard.show();
        if (myAuthenticationWizard.exec() == QDialog::Rejected)
        {
            DEBUGLOG("Authentication has been cancelled by the user");
            return;
        }

        myServiceUri = myAuthenticationWizard.getServiceUri();
        DEBUGLOG(boost::format("The certificate has been successfully retrieved, service URI: %s") % myServiceUri);
        rclient::Settings::setServiceUri(myServiceUri);

        if (!myServiceUri.empty())
        {
            switch (url::getScheme(myServiceUri))
            {
            case url::Http:
            case url::Https:
            case url::Ftp:
            case url::Ftps:
            {
                DEBUGLOG(boost::format("Opening %s in a browser") % myServiceUri);
                Browser::BrowserType myBrowserType = getTargetBrowser();
                Browser::open(myServiceUri, myBrowserType);
                break;
            }
            case url::File:
            {
                string myStdOut, myStdErr;
                const string myProgName = url::makeNativePath(myServiceUri);
                bool myExecSync = myAuthenticationWizard.getExecuteSync();
                DEBUGLOG(boost::format("Launching %1% %2%synchronously") % myProgName % (myExecSync?"":"a"));
                unsigned int myExecCode;

                if (myExecSync)
                {
                    myExecCode = ta::Process::shellExecSync(myProgName, myStdOut, myStdErr);
                    string myLogMsg = str(boost::format("%s finished with code %u.\nStderr: %s\nStdout: %s") % myProgName % myExecCode % myStdErr % myStdOut);
                    if (myExecCode == 0)
                    {
                        DEBUGLOG(myLogMsg);
                    }
                    else
                    {
                        // NOTE: If we get there, it does not always mean error because:
                        // - non-zero exit code is indeed returned if the progam cannot be executed (e.g. path is incorrect)
                        // - non-zero exit code is indeed returned by the program when it finishes but non-zero it does not always mean error on Windows
                        WARNLOG(myLogMsg);
                    }
                }
                else
                {
                    if (ta::Process::shellExecAsync(myProgName, myStdOut, myStdErr, myExecCode))
                    {
                        string myLogMsg = str(boost::format("%s finished with code %u.\nStderr: %s\nStdout: %s") % myProgName % myExecCode % myStdErr % myStdOut);
                        if (myExecCode == 0)
                        {
                            DEBUGLOG(myLogMsg);
                        }
                        else
                        {
                            // NOTE: If we get there, it does not always mean an error because:
                            // - non-zero exit code is indeed returned if the progam cannot be executed (e.g. path is incorrect)
                            // - non-zero exit code is indeed returned by the program when it finishes but non-zero it does not always mean error on Windows
                            WARNLOG(myLogMsg);
                        }
                    }
                    else
                    {
                        DEBUGLOG(boost::format("Launching %s succeeded, leave it running in a background") % myProgName.c_str());
                    }
                }
                break;
            }
            case url::Other:
                WARNLOG(boost::format("Unsupported scheme in %s skipping launch of an external application") % myServiceUri);
                break;
            default:
                WARNLOG(boost::format("Unknown in %s skipping launch of an external application") % myServiceUri);
                break;
            }
        }
        else
        {
            INFOLOG("URI is empty, nothing to invoke");
        }


    }
    catch (ReseptDesktopClientAppError&)
    {
        throw;
    }
    catch (rclient::RcdpVersionMismatchError& e)
    {
        const string myUserMsg = "Client/server version mismatch. Please contact " + resept::ProductName + " administrator.";
        ERRORLOG2(myUserMsg, e.what());
        QMessageBox::warning(NULL, "Client/server version mismatch", myUserMsg.c_str());
        TA_THROW(ReseptDesktopClientAppError);
    }
    catch (rclient::UserLockedError&)
    {
        WARNLOG("User is locked");
        QMessageBox::warning(NULL, "User locked", ("User is locked. Please contact " + resept::ProductName + " administrator.").c_str());
        TA_THROW(ReseptDesktopClientAppError);
    }
    catch (rclient::AuthCancelledException&)
    {
        DEBUGLOG("Authentication has been cancelled by the user");
        TA_THROW(ReseptDesktopClientAppError);
    }
    catch (rclient::EocError& e)
    {
        const string myUserMsg = resept::ProductName + " server error occurred. Please contact " + resept::ProductName + " administrator.";
        ERRORLOG2(myUserMsg, boost::format("EOC received from the server. '%s'") % e.what());
        QMessageBox::warning(NULL, "Protocol error", myUserMsg.c_str());
        TA_THROW(ReseptDesktopClientAppError);
    }
    catch (rclient::ErrError& e)
    {
        switch (e.errnum)
        {
        case resept::rcdp::ErrTimeOutOfSync:
        {
            string myUserMsg;
            int myTimeDiff;
            try
            {
                myTimeDiff = ta::Strings::parse<int>(e.description);
            }
            catch (...)
            {
                myUserMsg = resept::ProductName + " server error occurred. Please contact " + resept::ProductName + " administrator.";
                ERRORLOG2(myUserMsg, boost::format("Ill-formed time-out-of-sync error, cannot parse time difference from '%s'") % e.description);
                QMessageBox::warning(NULL, "Protocol error", myUserMsg.c_str());
                TA_THROW(ReseptDesktopClientAppError);
            }

            if (myTimeDiff > 0)
                myUserMsg = str(boost::format("Client time is %s ahead the server time.") % ta::TimeUtils::formatTimeInterval(myTimeDiff));
            else
                myUserMsg = str(boost::format("Client time is %s behind the server time.") % ta::TimeUtils::formatTimeInterval(-myTimeDiff));
            ERRORLOG(myUserMsg);
            QMessageBox::warning(NULL, "Invalid time settings", (myUserMsg + " Please adjust your time settings or contact " + resept::ProductName + " administrator.").c_str());
            TA_THROW(ReseptDesktopClientAppError);
        }
        case resept::rcdp::ErrResolvedIpInvalid:
        {
            const string myUserMsg = "Resolved service URI differs on the client and on the server.";
            ERRORLOG(myUserMsg);
            QMessageBox::warning(NULL, "Service URI mismatch", (myUserMsg + " Please contact " + resept::ProductName + " administrator.").c_str());
            TA_THROW(ReseptDesktopClientAppError);
        }
        case resept::rcdp::ErrDigestInvalid:
        {
            const string myUserMsg = "Calculated executable digest differs on the client and on the server.";
            ERRORLOG(myUserMsg);
            QMessageBox::warning(NULL, "Executable digest mismatch", (myUserMsg + " Please contact " + resept::ProductName + " administrator.").c_str());
            TA_THROW(ReseptDesktopClientAppError);
        }
        case resept::rcdp::ErrMaxLicensedUsersReached:
        {
            const string myUserMsg = "Authentication succeeded but the certificate cannot be supplied.";
            ERRORLOG2(myUserMsg, "Maximum number of licensed users or transactions has been reached on the server.");
            QMessageBox::warning(NULL, "No certificate received", (myUserMsg + " Please contact " + resept::ProductName + " administrator.").c_str());
            TA_THROW(ReseptDesktopClientAppError);
        }
        case resept::rcdp::ErrPasswordExpired:
        {
            const string myUserMsg = "Account password expired.";
            ERRORLOG2(myUserMsg, "");
            QMessageBox::warning(NULL, "Account password expired", (myUserMsg + " Please contact " + resept::ProductName + " or system administrator.").c_str());
            TA_THROW(ReseptDesktopClientAppError);
        }
        default:
        {
            const string myUserMsg = resept::ProductName + " server error occurred. Please contact " + resept::ProductName + " administrator.";
            ERRORLOG2(myUserMsg, boost::format("ERR received from the server. Code: %d, description: '%s'.") % e.errnum % e.description);
            QMessageBox::warning(NULL, "Server error", myUserMsg.c_str());
            TA_THROW(ReseptDesktopClientAppError);
        }
        }
    }
    catch (rclient::HttpRequestError& e)
    {
        const string myUserMsg = "Cannot connect to " + resept::ProductName + " server. Please contact your system or " + resept::ProductName + " administrator.";
        ERRORLOG2(myUserMsg, boost::format("Failed to send HTTP request to the RESEPT server. %s") % e.what());
        QMessageBox::warning(NULL, "Cannot connect to server", myUserMsg.c_str());
        TA_THROW(ReseptDesktopClientAppError);
    }
    catch (ta::IpResolveError& e)
    {
        ERRORLOG(boost::format("Failed to resolve IP. %s") % e.what());
        QMessageBox::warning(NULL, "Failed to resolve IP", ("IP address resolution failed. Please contact your system or " + resept::ProductName + " administrator.").c_str());
        TA_THROW(ReseptDesktopClientAppError);
    }
    catch (rclient::SettingsError& e)
    {
        const string myUserMsg = resept::ProductName + " installation is misconfigured. Please contact " + resept::ProductName + " administrator.";
        ERRORLOG2(myUserMsg, e.what());
        QMessageBox::warning(NULL, "Installation misconfigured", myUserMsg.c_str());
        TA_THROW(ReseptDesktopClientAppError);
    }
    catch (rclient::NativeCertStoreDeleteError& e)
    {
        const string myUserMsg = "Failed to cleanup certificates. Please contact " + resept::ProductName + " administrator.";
        ERRORLOG2(myUserMsg, boost::format("Failed to delete user certificates from the system store. %s") % e.what());
        QMessageBox::warning(NULL, "Failed to delete user certificates", myUserMsg.c_str());
        TA_THROW(ReseptDesktopClientAppError);
    }
    catch(rclient::NativeCertStoreValidateError& e)
    {
        const string myUserMsg = "Failed to validate certificates. Please contact " + resept::ProductName + " administrator.";
        ERRORLOG2(myUserMsg, boost::format("Failed to validate user certificates in the system store. %s") % e.what());
        QMessageBox::warning(NULL, "Failed to validate certificates", myUserMsg.c_str());
        TA_THROW(ReseptDesktopClientAppError);
    }
    catch(rclient::NativeCertStoreImportError& e)
    {
        const string myUserMsg = "Failed to import certificate. Please contact " + resept::ProductName + " administrator.";
        ERRORLOG2(myUserMsg, boost::format("Failed to import user certificates into the system store. %s") % e.what());
        QMessageBox::warning(NULL, "Failed to import user certificates", myUserMsg.c_str());
        TA_THROW(ReseptDesktopClientAppError);
    }
    catch (BrowserSelectonError& e)
    {
        const string myUserMsg = "Cannot open URL in a browser. " + string(e.userMsg);
        ERRORLOG(myUserMsg);
        QMessageBox::warning(NULL, "Cannot open URL in a browser", myUserMsg.c_str());
        TA_THROW(ReseptDesktopClientAppError);
    }
    catch (ta::BrowserNotSupportedError& e)
    {
        const string myUserMsg = "No supported browser found to open URL. Please contact " + resept::ProductName + " administrator.";
        ERRORLOG2(myUserMsg, boost::format("Browser is not supported. %s") % e.what());
        QMessageBox::warning(NULL, "Browser is not supported", myUserMsg.c_str());
        TA_THROW(ReseptDesktopClientAppError);
    }
    catch (ta::BrowserLaunchError& e)
    {
        const string myUserMsg = "Failed to launch browser. Please contact " + resept::ProductName + " administrator.";
        ERRORLOG2(myUserMsg, boost::format("Failed to launch browser. %s") % e.what());
        QMessageBox::warning(NULL, "Failed to launch browser", myUserMsg.c_str());
        TA_THROW(ReseptDesktopClientAppError);
    }
    catch (ta::ProcessExecError& e)
    {
        const string myUserMsg = "Failed to execute " + myServiceUri + " Please contact " + resept::ProductName + " administrator.";
        ERRORLOG2(myUserMsg, boost::format("Failed to execute %s. %s") % myServiceUri % e.what());
        QMessageBox::warning(NULL, "Opening service URI failed", myUserMsg.c_str());
        TA_THROW(ReseptDesktopClientAppError);
    }
    catch (std::exception& e)
    {
        const string myUserMsg = resept::ProductName + " error. Please contact " + resept::ProductName + " administrator.";
        ERRORLOG2(myUserMsg, e.what());
        QMessageBox::warning(NULL, "Error", myUserMsg.c_str());
        TA_THROW(ReseptDesktopClientAppError);
    }
    catch (...)
    {
        const string myUserMsg = resept::ProductName + " error. Please contact " + resept::ProductName + " administrator.";
        ERRORLOG2(myUserMsg, "Unexpected exception occurred");
        QMessageBox::warning(NULL, "Error", myUserMsg.c_str());
        TA_THROW(ReseptDesktopClientAppError);
    }
}
