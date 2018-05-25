#include "CommonUtils.h"
#ifdef _WIN32
#include "QtExclusiveApp.h"
#include "AuthenticationWizard.h"
#include "RClientAppCommon.h"
#endif
#include "rclient/NativeCertStore.h"
#include "rclient/Settings.h"
#include "rclient/RcdpHandler.h"
#include "rclient/Common.h"
#include "ta/url.h"
#include "ta/version.h"
#include "ta/logappender.h"
#include "ta/logconfiguration.h"
#include "ta/logger.h"
#include "ta/netutils.h"
#include "ta/strings.h"
#include "ta/timeutils.h"
#include "ta/assert.h"
#include "ta/utils.h"
#include "ta/common.h"

#ifdef _WIN32
#include "ta/Registry.h"
#include "ta/InternetExplorer.h"
#include <QtWidgets/QMessageBox>
#include <QApplication>
#include <windows.h>
#endif

#include <memory>
#include "boost/algorithm/string.hpp"

using std::string;
using namespace ta;

namespace rclient
{
    namespace
    {
        void initLogger()
        {
            try
            {
                string myEnvInfo = str(boost::format("%s Client-%s") % resept::ProductName % toStr(rclient::ClientVersion));
#ifdef _WIN32
                string myIeInfo = "IE not installed";
                if (InternetExplorer::isInstalled())
                {
                    try
                    {
                        InternetExplorer::Version myVer = InternetExplorer::getVersion();
                        myIeInfo = str(boost::format("IE-%u.%u.%u") % myVer.major % myVer.minor % myVer.subminor);

                        InternetExplorer::ProtectedMode myProtectedMode = InternetExplorer::getProtectedMode();
                        if (myProtectedMode == InternetExplorer::protectedModeOn)
                            myIeInfo += " protected mode On";
                        else if (myProtectedMode == InternetExplorer::protectedModeOff)
                            myIeInfo += " protected mode Off";
                    }
                    catch (std::runtime_error&)
                    {}
                }
                myEnvInfo += ", " + myIeInfo;
#endif
                string myLogLevelStr = rclient::Settings::getLogLevel();
                ta::LogLevel::val myLogLevel;
                if (!LogLevel::parse(myLogLevelStr.c_str(), myLogLevel))
                    TA_THROW_MSG(LoggerInitError, "Failed to parse logging level " + myLogLevelStr);
                const string myLogFilePath = rclient::getLogDir() + ta::getDirSep() + rclient::LogName;

                ta::LogConfiguration::Config myMemConfig;
                myMemConfig.fileAppender = true;
                myMemConfig.fileAppenderLogThreshold = myLogLevel;
                myMemConfig.fileAppenderLogFileName = myLogFilePath;
                ta::LogConfiguration::instance().load(myMemConfig);

                PROLOG(myEnvInfo);
            }
            catch (LoggerInitError&)
            {
                throw;
            }
            catch (std::exception& e)
            {
                TA_THROW_MSG(LoggerInitError, e.what());
            }
        }

        void deinitLogger()
        {
            EPILOG(resept::ProductName + " Client-" + toStr(rclient::ClientVersion));
        }
    }

    //
    // Public API
    //

    LoggerInitializer::LoggerInitializer()
    {
        initLogger();
    }

    LoggerInitializer::~LoggerInitializer()
    {
        deinitLogger();
    }

    // Return whether path1 resides in or equal to path2
    // Symbols '.' and '..' are not handled
    // E.g. /abc/def and /abc are both subpaths of /abc/, but /abcd is not a subpath of /abc
    static bool isSubPath(const string& aPath1, const string& aPath2)
    {
        string myPath1(boost::trim_copy(aPath1));
        string myPath2(boost::trim_copy(aPath2));
        boost::trim_right_if(myPath1,boost::is_any_of("/"));
        boost::trim_right_if(myPath2,boost::is_any_of("/"));
        if (myPath1 == myPath2 || myPath2.empty())
            return true;
        string::size_type myPath1Len = myPath1.length();
        string::size_type myPath2Len = myPath2.length();
        if (myPath1Len <= myPath2Len)
            return false;
        if (myPath1.substr(0, myPath2Len) != myPath2)
            return false;
        if (myPath1.at(myPath2Len) != '/')
            return false;
        return true;
    }

    bool isServiceUri(const string& aRequestedUri, const string& aServiceUri)
    {
        try
        {
            const string myCanonicalRequestedUri = url::normalize(aRequestedUri);
            const string myCanonicalHotUri = url::normalize(aServiceUri);

            const url::Parts myRequestedUriParts = url::parse(myCanonicalRequestedUri);
            const url::Parts myHotUriParts = url::parse(myCanonicalHotUri);

            // we compare normalized URLs after parsing them to filter out ill-formed URLs
            if (myCanonicalRequestedUri == myCanonicalHotUri)
            {
                return true;
            }

            if (myRequestedUriParts.scheme != myHotUriParts.scheme)
            {
                return false;
            }
            if (!url::wildcardHostMatch(myRequestedUriParts.authority_parts.host, myHotUriParts.authority_parts.host))
            {
                return false;
            }
            if (myRequestedUriParts.authority_parts.port != myHotUriParts.authority_parts.port)
            {
                return false;
            }
            if (!isSubPath(myRequestedUriParts.path, myHotUriParts.path))
            {
                return false;
            }

            return true;
        }
        catch (std::exception&)
        {
            // if we fall here, this most likely means that one of URLs is ill-formed
            return false;
        }
    }

#ifdef _WIN32
    bool loadBrowserReseptClientAuthUI(const std::vector<std::pair<string, string> >& aProviderServicePairs, const string& aReqestedUri, std::string& anUri2Go)
    {
        DEBUGLOG("Requested URI: " + aReqestedUri);
        std::auto_ptr<QtExclusiveApp> myQtExclusiveApp;
        try
        {
            TA_ASSERT(!aProviderServicePairs.empty());

            myQtExclusiveApp.reset(new QtExclusiveApp()); // not destroyed when leaving 'try' so we can call QMessageBox inside 'catch()'

            rclient::AuthenticationWizard myAuthenticationWizard(aProviderServicePairs);
            myAuthenticationWizard.show();
            if (myAuthenticationWizard.exec() == QDialog::Rejected)
            {
                DEBUGLOG("Authentication has been cancelled by the user");
                return false;
            }

            // Authentication succeeded and service URI received from the server
            const string myServiceUriFromSvr = myAuthenticationWizard.getServiceUri();
            anUri2Go = isServiceUri(aReqestedUri, myServiceUriFromSvr) ? aReqestedUri : myServiceUriFromSvr;
            DEBUGLOG(boost::format("The certificate has been successfully imported. Received Service URI from server: '%s'. Proceeding with '%s'") % myServiceUriFromSvr % anUri2Go);

            return true;
        }
        catch (QtExclusiveAppLockError& e)
        {
            WARNLOG2("Another instance of UI is already running.", boost::format("Another instance of UI is already running. %s") % e.what());
            return false;
        }
        catch (rclient::RcdpVersionMismatchError& e)
        {
            const string myUserMsg = "Client/server version mismatch. Please contact " + resept::ProductName + " administrator.";
            ERRORLOG2(myUserMsg, e.what());
            QMessageBox::warning(NULL, "Client/server version mismatch", myUserMsg.c_str());
            return false;
        }
        catch (rclient::CertStillValidException&)
        {
            anUri2Go = aReqestedUri;
            DEBUGLOG(boost::format("Certificate is still valid. Proceeding with %s") % anUri2Go);
            return true;
        }
        catch (rclient::AuthCancelledException&)
        {
            DEBUGLOG("Authentication has been cancelled by the user");
            return false;
        }
        catch (rclient::UserLockedError&)
        {
            WARNLOG("User has been locked");
            return false;
        }
        catch (rclient::EocError& e)
        {
            const string myUserMsg = resept::ProductName + " server error occurred. Please contact " + resept::ProductName + " administrator.";
            ERRORLOG2(myUserMsg, boost::format("EOC received from the server. '%s'") % e.what());
            QMessageBox::warning(NULL, "Protocol error", myUserMsg.c_str());
            return false;
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
                    myUserMsg = resept::ProductName + " server error occurred. Please contact " + resept::ProductName.c_str() + " administrator.";
                    ERRORLOG2(myUserMsg, boost::format("Ill-formed time-out-of-sync error, cannot parse time difference from '%s'") % e.description);
                    QMessageBox::warning(NULL, "Protocol error", myUserMsg.c_str());
                    return false;
                }

                if (myTimeDiff > 0)
                    myUserMsg = str(boost::format("Client time is %s ahead the server time.") % ta::TimeUtils::formatTimeInterval(myTimeDiff));
                else
                    myUserMsg = str(boost::format("Client time is %s behind the server time.") % ta::TimeUtils::formatTimeInterval(-myTimeDiff));
                ERRORLOG(myUserMsg);
                QMessageBox::warning(NULL, "Invalid time setting", (myUserMsg + " Please adjust your time settings or contact " + resept::ProductName + " administrator.").c_str());
                return false;
            }
            case resept::rcdp::ErrResolvedIpInvalid:
            {
                const string myUserMsg = "Resolved service URI differs on the client and on the server.";
                ERRORLOG(myUserMsg);
                QMessageBox::warning(NULL, "Service URI mismatch", (myUserMsg + " Please contact " + resept::ProductName + " administrator.").c_str());
                return false;
            }
            case resept::rcdp::ErrDigestInvalid:
            {
                const string myUserMsg = "Calculated executable digest differs on the client and on the server.";
                ERRORLOG(myUserMsg);
                QMessageBox::warning(NULL, "Executable digest mismatch", (myUserMsg + " Please contact " + resept::ProductName + " administrator.").c_str());
                return false;
            }
            case resept::rcdp::ErrMaxLicensedUsersReached:
            {
                const string myUserMsg = "Authentication succeeded but the certificate cannot be supplied.";
                ERRORLOG2(myUserMsg, "The maximum number of licensed users or transactions has been reached on the server");
                QMessageBox::warning(NULL, "No certificate received", (myUserMsg + " Please contact " + resept::ProductName + " administrator.").c_str());
                return false;
            }
            case resept::rcdp::ErrPasswordExpired:
            {
                const string myUserMsg = "Account password expired.";
                ERRORLOG2(myUserMsg, "");
                QMessageBox::warning(NULL, "Account password expired", (myUserMsg + " Please contact " + resept::ProductName + " or system administrator.").c_str());
                return false;
            }
            default:
            {
                const string myUserMsg = resept::ProductName + " server error occurred. Please contact " + resept::ProductName + " administrator.";
                ERRORLOG2(myUserMsg, boost::format("ERR received from the server. Code: %d, description: '%s'.") % e.errnum % e.description);
                QMessageBox::warning(NULL, "Server error", myUserMsg.c_str());
                return false;
            }
            }
        }
        catch (rclient::HttpRequestError& e)
        {
            const string myUserMsg = "Cannot connect to " + resept::ProductName + " server. Please contact your system or " + resept::ProductName + " administrator.";
            ERRORLOG2(myUserMsg, boost::format("Failed to send HTTP request to the RESEPT server. %s") % e.what());
            QMessageBox::warning(NULL, "Cannot connect to server", myUserMsg.c_str());
            return false;
        }
        catch (ta::IpResolveError& e)
        {
            ERRORLOG(boost::format("Failed to resolve IP. %s") % e.what());
            QMessageBox::warning(NULL, "Failed to resolve IP", ("IP address resolution failed. Please contact your system or " + resept::ProductName + " administrator.").c_str());
            return false;
        }
        catch (rclient::SettingsError& e)
        {
            const string myUserMsg = resept::ProductName + " installation is misconfigured. Please contact " + resept::ProductName + " administrator.";
            ERRORLOG2(myUserMsg, e.what());
            QMessageBox::warning(NULL, "Misconfigured installation", myUserMsg.c_str());
            return false;
        }
        catch (rclient::NativeCertStoreDeleteError& e)
        {
            const string myUserMsg = "Failed to clean up certificates. Please contact " + resept::ProductName +  " administrator.";
            ERRORLOG2(myUserMsg, boost::format("Failed to delete user certificates from the system store. %s") % e.what());
            QMessageBox::warning(NULL, "Failed to clean up certificates", myUserMsg.c_str());
            return false;
        }
        catch(rclient::NativeCertStoreValidateError& e)
        {
            const string myUserMsg = "Failed to validate certificates. Please contact " + resept::ProductName + " administrator.";
            ERRORLOG2(myUserMsg, boost::format("Failed to validate user certificates in the system store. %s") % e.what());
            QMessageBox::warning(NULL, "Failed to validate certificates", myUserMsg.c_str());
            return false;
        }
        catch(rclient::NativeCertStoreImportError& e)
        {
            const string myUserMsg = "Failed to import certificate. Please contact " + resept::ProductName +  " administrator.";
            ERRORLOG2(myUserMsg, boost::format("Failed to import user certificates into the system store. %s") % e.what());
            QMessageBox::warning(NULL, "Failed to import certificate", myUserMsg.c_str());
            return false;
        }
        catch (std::exception& e)
        {
            const string myUserMsg = resept::ProductName + " error. Please contact " + resept::ProductName + " administrator.";
            ERRORLOG2(myUserMsg, e.what());
            QMessageBox::warning(NULL, "Error", myUserMsg.c_str());
            return false;
        }
        catch (...)
        {
            ERRORLOG("Unexpected exception occurred");
            QMessageBox::warning(NULL, "Error", (resept::ProductName + " error. Please contact " + resept::ProductName + " administrator.").c_str());
            return false;
        }
    }

    bool isReseptIeAddonInstalled()
    {
        if (!ta::InternetExplorer::isInstalled())
            return false;
        string myKey = str(boost::format("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Browser Helper Objects\\{%s}") % RESEPTBHO_CLSID);
        return ta::Registry::isExist(HKEY_LOCAL_MACHINE, myKey, "");

    }
#endif


}
