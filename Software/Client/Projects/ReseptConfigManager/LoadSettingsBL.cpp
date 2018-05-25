#include "LoadSettingsBL.h"
#ifdef _WIN32
#include "rclient/ReseptBrokerService.h"
#include "rclient/NativeCertStore.h"
#endif
#include "rclient/ContentConfig.h"
#include "rclient/Settings.h"
#include "ta/Zip.h"
#include "ta/url.h"
#include "ta/utils.h"
#include "ta/dnsutils.h"
#include "ta/signutils.h"
#include "ta/process.h"
#include "ta/proto.hpp"
#include "ta/scopedresource.hpp"
#include "ta/tcpclient.h"
#include "ta/logger.h"
#include "ta/common.h"

#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/convenience.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/cstdint.hpp"
#include "curl/curl.h"

using std::string;
using std::vector;

//
// Private API
//
namespace
{
    static const unsigned long ConnectTimeout = 2;

    size_t responseCallback(void* buffer, size_t size, size_t nmemb, void* aResponse)
    {
        assert(buffer && aResponse);
        string* myReponse = (string*)aResponse;
        size_t myNumBytesConsumed = nmemb*size;
        myReponse->append((char*)buffer, myNumBytesConsumed);
        return myNumBytesConsumed;
    }

#ifdef _WIN32
    enum ServiceStatus
    {
        serviceStatusRunning,
        serviceStatusStopped,
        serviceStatusPending,
        serviceStatusNotFound
    };

    ServiceStatus getReseptBrokerServiceStatus()
    {
        SC_HANDLE myScHandle = OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_ENUMERATE_SERVICE);
        if (!myScHandle)
            TA_THROW_MSG(std::runtime_error, boost::format("OpenSCManager failed. Last error: %d") % ::GetLastError());
        ENUM_SERVICE_STATUS myServices[1024] = {};
        DWORD cbBytesNeeded, myNumServices;
        if (!EnumServicesStatus(myScHandle, SERVICE_WIN32, SERVICE_STATE_ALL, myServices, sizeof(myServices), &cbBytesNeeded, &myNumServices, 0))
            TA_THROW_MSG(std::runtime_error, boost::format("EnumServicesStatus failed. Last error: %d") % ::GetLastError());
        for (DWORD i=0; i<myNumServices; ++i)
        {
            if (strcmp(myServices[i].lpServiceName, rclient::BrokerServiceName) == 0)
            {
                switch (myServices[i].ServiceStatus.dwCurrentState)
                {
                case SERVICE_RUNNING:
                    return serviceStatusRunning;
                case SERVICE_STOPPED:
                case SERVICE_PAUSED:
                    return serviceStatusStopped;
                case SERVICE_CONTINUE_PENDING:
                case SERVICE_PAUSE_PENDING:
                case SERVICE_START_PENDING:
                case SERVICE_STOP_PENDING:
                    return serviceStatusPending;
                default:
                    TA_THROW_MSG(std::runtime_error, boost::format("Unsupported service status %d") % myServices[i].ServiceStatus.dwCurrentState);
                }
            }
        }
        return serviceStatusNotFound;
    }

    bool checkBrokerServiceStatusRunning(string& anErrorMsg)
    {
        using namespace rclient::ReseptBrokerService;

        const ServiceStatus myBrokerServiceStatus = getReseptBrokerServiceStatus();

        if (myBrokerServiceStatus == serviceStatusNotFound)
            return anErrorMsg = resept::ProductName + " Broker Service is not installed. Please reinstall " + resept::ProductName + " and try again or contact " + resept::ProductName + " administrator.", false;
        if (myBrokerServiceStatus == serviceStatusPending)
            return anErrorMsg = resept::ProductName + " Broker Service is in pending state. Please try again.", false;
        if (myBrokerServiceStatus == serviceStatusStopped)
            return anErrorMsg = resept::ProductName + " Broker Service is stopped. Please start the service and try again or contact " + resept::ProductName + " administrator.", false;
        if (myBrokerServiceStatus != serviceStatusRunning)
            TA_THROW_MSG(std::runtime_error, boost::format("Unsupported service status %d") % myBrokerServiceStatus);

        return true;
    }

    bool installSettingsWithBrokerService(const rclient::ContentConfig::Config& aContentConfig,
                                          LoadSettingsBL::ConfirmationPromptCallback aConfirmationPromptCb,
                                          void* aConfirmationPromptCbCookie,
                                          string& anErrorMsg)
    {
        using namespace rclient::ReseptBrokerService;

        if (!checkBrokerServiceStatusRunning(anErrorMsg))
        {
            return false;
        }

        const unsigned int myReseptBrokerServicePort = rclient::Settings::getReseptBrokerServicePort();
        DEBUGLOG(boost::format("Connecting to RESEPT Broker Service at 127.0.0.1:%u") % myReseptBrokerServicePort);
        ta::TcpClient myBrokerServiceConnection;
        myBrokerServiceConnection.open("127.0.0.1", myReseptBrokerServicePort);

        DEBUGLOG("Connected");
        const string myUserConfigPath = rclient::Settings::getUserConfigPath();
        ta::proto::send(myBrokerServiceConnection, requestInstallSettings);
        ta::proto::send(myBrokerServiceConnection, InstallSettingsRequest(aContentConfig, myUserConfigPath));
        DEBUGLOG("Installation request sent");

        Response myResponse = ta::proto::receive<Response>(myBrokerServiceConnection);

        switch (myResponse.status)
        {
        case responseStatusOk:
            DEBUGLOG("Settings have been successfully installed");
            return true;
        case responseStatusConfirmation:
        {
            DEBUGLOG("Confirmation prompt received from the service");

            if (!aConfirmationPromptCb)
            {
                TA_THROW_MSG(std::invalid_argument, "Confirmation prompt received from the service but prompt callback routine is not specified");
            }

            if (!aConfirmationPromptCb(myResponse.text, aConfirmationPromptCbCookie))
            {
                anErrorMsg = "Loading settings is cancelled by the user";
                WARNLOG("Loading settings is cancelled by the user (no downgrade desired for provider " + aContentConfig.getProviderName() + ")");
                ta::proto::send(myBrokerServiceConnection, false);
                myResponse = ta::proto::receive<Response>(myBrokerServiceConnection);
                switch (myResponse.status)
                {
                case responseStatusOk:
                    return false;
                case responseStatusUserError:
                    anErrorMsg = myResponse.text;
                    WARNLOG(anErrorMsg);
                    return false;
                case responseStatusError:
                    TA_THROW_MSG(std::runtime_error, myResponse.text);
                default:
                    TA_THROW_MSG(std::runtime_error, boost::format("Unexpected status %d received from RESEPT Broker Service") % myResponse.status);
                }
            }

            ta::proto::send(myBrokerServiceConnection, true);
            myResponse = ta::proto::receive<Response>(myBrokerServiceConnection);

            switch (myResponse.status)
            {
            case responseStatusOk:
                DEBUGLOG("Settings have been successfully installed");
                return true;
            case responseStatusUserError:
                anErrorMsg = myResponse.text;
                WARNLOG(anErrorMsg);
                return false;
            case responseStatusError:
                TA_THROW_MSG(std::runtime_error, myResponse.text);
            default:
                TA_THROW_MSG(std::runtime_error, boost::format("Unexpected status %d received from RESEPT Broker Service") % myResponse.status);
            }
        }
        case responseStatusUserError:
            anErrorMsg = myResponse.text;
            WARNLOG(anErrorMsg);
            return false;
        case responseStatusError:
            TA_THROW_MSG(std::runtime_error, myResponse.text);
        default:
            TA_THROW_MSG(std::runtime_error, boost::format("Unexpected status %d received from RESEPT Broker Service") % myResponse.status);
        }
    }
#else
    //
    // Linux
    //



    void fixUserHomeDirOwner()
    {
        if (!ta::isUserRoot()) // don't bother when we are called by root
        {
            //note: just executing "chown -R" in a shell does not work because the shell will be spawn with real uid i.o. effective uid which has all the power

            const string myHomeDir = ta::Process::getUserAppDataDir();
            const string myUserTempDir = myHomeDir + "/tmp";
            const string myKeyTalkUserConfigDir = rclient::Settings::getUserConfigDir();

            ta::chownDir(myHomeDir, ta::recursiveNo);
            ta::chownDir(myUserTempDir, ta::recursiveYes);
            ta::chownDir(myKeyTalkUserConfigDir, ta::recursiveYes);
        }
    }

    bool installSettingsDirectly(const rclient::ContentConfig::Config& aContentConfig,
                                 LoadSettingsBL::ConfirmationPromptCallback aConfirmationPromptCb,
                                 void* aConfirmationPromptCbCookie,
                                 string& anErrorMsg)
    {
        const string myNewProviderName = aContentConfig.getProviderName();

        // Ask for confirmation when downgrading settings
        if (rclient::Settings::isCustomized() && ta::isElemExist(myNewProviderName, rclient::Settings::getProviders()))
        {
            if (rclient::Settings::getProviderContentVersion(myNewProviderName) > aContentConfig.getContentVersion())
            {
                const string myUserPrompt = "The version of the settings being installed for provider " + myNewProviderName + " is older than the version of the settings already installed. Are you sure you want to proceed? [y/n]";
                DEBUGLOG("The version of the settings being installed for provider " + myNewProviderName + " is older than the version of the settings already installed. Asking the user for confirmation");

                if (!aConfirmationPromptCb)
                {
                    TA_THROW_MSG(std::invalid_argument, "Downgrade confirmation required but confirmation callback routine is not specified");
                }
                if (!aConfirmationPromptCb(myUserPrompt, aConfirmationPromptCbCookie))
                {
                    anErrorMsg = "Loading settings is canceled by the user (no downgrade desired for provider " + aContentConfig.getProviderName() + ")";
                    return false;
                }
            }
        }

        rclient::ContentConfig::install(aContentConfig);

        // It is likely that a just created KeyTalk user profile directory maybe along with user home directory use effective uid (root), so they do not belong to a real user.
        // Make sure these directories belong to the real owner.
        fixUserHomeDirOwner();

        // Record user name we customized for.
        // We can later use this this knowledge to remove KeyTalk settings for all customized users during uninstallation
        const string myUserName = ta::getUserName();
        DEBUGLOG("Adding customized user " + myUserName);
        rclient::Settings::addCustomizedUser(myUserName);

        return true;
    }
#endif // _WIN32


    enum HttpReqStatus
    {
        http200Resp, reqError
    };


    //@nothrow
    //@return success flag. If return value is false, anErrorMsg contains user-oriented error message.
    //                      If the function return true, anErrorMsg is not affected
    bool installSettings(const string& anExtractedSettingsDir,
                         LoadSettingsBL::ConfirmationPromptCallback aConfirmationPromptCb,
                         void* aConfirmationPromptCbCookie,
                         string& anErrorMsg)
    {
        try
        {
            const string myIndexFilePath = anExtractedSettingsDir + ta::getDirSep() + rclient::ContentConfig::IndexFileName;

            if (ta::isFileExist(myIndexFilePath))
            {
                DEBUGLOG(boost::format("Loading RCCDv2 index from '%s'") % myIndexFilePath);
            }
            else
            {
                const string myV1IndexFilePath = anExtractedSettingsDir + ta::getDirSep() + rclient::ContentConfig::v1::IndexFileName;
                DEBUGLOG(boost::format("Loading RCCDv1 index from '%s'") % myV1IndexFilePath);
                // we do not verify RCCD index file signature any more
                ta::writeData(myIndexFilePath, ta::SignUtils::loadNotVerifyPKCS7WithSMIME(myV1IndexFilePath));
            }

            const rclient::ContentConfig::Config myContentConfig(myIndexFilePath);
#ifdef _WIN32
            return installSettingsWithBrokerService(myContentConfig, aConfirmationPromptCb, aConfirmationPromptCbCookie, anErrorMsg);
#else
            return installSettingsDirectly(myContentConfig, aConfirmationPromptCb, aConfirmationPromptCbCookie, anErrorMsg);
#endif
        }
        catch (std::exception& e)
        {
            anErrorMsg = "Error installing " + resept::ProductName + " Client Customization File. Please contact " + resept::ProductName + " support.";
            ERRORLOG2(anErrorMsg, e.what());
        }
        return false;
    }

#ifdef _WIN32
    //@nothrow
    bool isResolvable(const string& aHostName)
    {
        try  {
            ta::DnsUtils::resolveIpByName(aHostName);
            return true;
        } catch (std::exception& e) {
            WARNLOG(boost::format("Cannot resolve %s. %s") % aHostName % e.what());
            return false;
        }
    }

    string makeUrl(const ta::NetUtils::RemoteAddress& anAddr)
    {
        if (ta::NetUtils::isValidIpv6(anAddr.host))
            return str(boost::format("[%s]:%u") % anAddr.host % anAddr.port);
        return str(boost::format("%s:%u") % anAddr.host % anAddr.port);
    }
#endif

    void setupSSL(CURL* aCurl)
    {
        if (!aCurl)
        {
            TA_THROW_MSG(std::invalid_argument, "NULL curl handle");
        }

#ifdef _WIN32
        curl_tlssessioninfo * myTlsSessionInfo = NULL;
        CURLcode myCurlRetCode = curl_easy_getinfo(aCurl, CURLINFO_TLS_SSL_PTR, &myTlsSessionInfo);
        if (myCurlRetCode != CURLE_OK)
        {
            TA_THROW_MSG(std::runtime_error, boost::format("Failed to retrieve TLS backend information. %s") % curl_easy_strerror(myCurlRetCode));
        }
        if (myTlsSessionInfo->backend == CURLSSLBACKEND_SCHANNEL)
        {
            // disable certificate revocation checks for curl built against WinSSL (schannel)
            // without disabling this flag WinSSL would cut TLS handshake if it does not find CLR or OSCP lists in the server's issuers CAs (which is way too strict I believe)
            myCurlRetCode = curl_easy_setopt(aCurl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NO_REVOKE);
            if (myCurlRetCode != CURLE_OK)
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Failed to disable CLR option. %s") % curl_easy_strerror(myCurlRetCode));
            }
        }
#endif
    }

    void disableProxy(CURL* aCurl)
    {
        if (!aCurl)
        {
            TA_THROW_MSG(std::invalid_argument, "CURL handle is NULL");
        }

        // In order to completely disable proxy we shall explicitly specify proxy address to empty string to prevent curl from using 'http_proxy' environment variable

        CURLcode myCurlRetCode = curl_easy_setopt(aCurl, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
        if (myCurlRetCode != CURLE_OK)
            TA_THROW_MSG(std::runtime_error, boost::format("Failed to setup supported proxy type. %s") % curl_easy_strerror(myCurlRetCode));

        myCurlRetCode = curl_easy_setopt(aCurl, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
        if (myCurlRetCode != CURLE_OK)
            TA_THROW_MSG(std::runtime_error, boost::format("Failed to setup supported proxy authentication type. %s") % curl_easy_strerror(myCurlRetCode));

        myCurlRetCode = curl_easy_setopt(aCurl, CURLOPT_PROXY, "");
        if (myCurlRetCode != CURLE_OK)
            TA_THROW_MSG(std::runtime_error, boost::format("Failed to disable proxy. %s") % curl_easy_strerror(myCurlRetCode));
    }

    string fixUrl(string& anUrl)
    {
        string myUrl = boost::trim_copy(anUrl);

        if (!ta::url::hasScheme(myUrl))
        {
            INFOLOG("Adding http:// to " + myUrl);
            myUrl = "http://" + myUrl;
        }
        if (!boost::ends_with(myUrl, ".rccd"))
        {
            INFOLOG("Appending .rccd to " + myUrl);
            myUrl += ".rccd";
        }

        return myUrl;

    }

#ifdef _WIN32
    bool uninstallUserSettingsForProvider(const string& aProvider, string& anErrorMsg)
    {
        using namespace rclient::ReseptBrokerService;

        try
        {
            DEBUGLOG(boost::format("Uninstalling user settings for provider '%s'") % aProvider);

            bool myFromMasterConfig;
            rclient::Settings::getProviderContentVersion(aProvider, myFromMasterConfig);
            if (myFromMasterConfig)
            {
                TA_THROW_MSG(std::invalid_argument, "Cannot remove master settings for provider " + aProvider + ". Only users settings can be removed.");
            }

            if (!checkBrokerServiceStatusRunning(anErrorMsg))
            {
                return false;
            }

            const string myIssuer = rclient::Settings::getUserCaName(aProvider);// grab cert issuer before uninstalling settings

            const unsigned int myReseptBrokerServicePort = rclient::Settings::getReseptBrokerServicePort();
            DEBUGLOG(boost::format("Connecting to RESEPT Broker Service at 127.0.0.1:%u") % myReseptBrokerServicePort);
            ta::TcpClient myBrokerServiceConnection;
            myBrokerServiceConnection.open("127.0.0.1", myReseptBrokerServicePort);

            DEBUGLOG("Connected");
            const string myUserConfigPath = rclient::Settings::getUserConfigPath();
            ta::proto::send(myBrokerServiceConnection, requestUninstallSettings);
            ta::proto::send(myBrokerServiceConnection, UninstallSettingsRequest(aProvider, myUserConfigPath));
            DEBUGLOG("Settings uninstallation request sent");

            const Response myResponse = ta::proto::receive<Response>(myBrokerServiceConnection);

            switch (myResponse.status)
            {
            case responseStatusOk:
                // remove user certs ourselves because broker service running as system could not access user personal cert store
                rclient::NativeCertStore::deleteUserCertsForIssuerCN(myIssuer, rclient::NativeCertStore::failOnError);
                DEBUGLOG("Settings have been successfully uninstalled");
                return true;
            case responseStatusUserError:
                anErrorMsg = myResponse.text;
                WARNLOG(anErrorMsg);
                return false;
            case responseStatusError:
                TA_THROW_MSG(std::runtime_error, myResponse.text);
            default:
                TA_THROW_MSG(std::runtime_error, boost::format("Unexpected status %d received from BrokerService trying to uninstall settings for provider %s") % myResponse.status % aProvider);
            }
        }
        catch (std::exception& e)
        {
            anErrorMsg = "Error uninstalling settings for provider " + aProvider + ". Please contact " + resept::ProductName + " support.";
            ERRORLOG2(anErrorMsg, e.what());
            return false;
        }
    }
#endif

} // end private API


//
// Public API
//

namespace LoadSettingsBL
{
    bool loadRccdFromUrl(const string& anUrl, vector<unsigned char>& aBlob, string& anErrorMsg)
    {
        string myUrl = boost::trim_copy(anUrl);
        DEBUGLOG(boost::format("Loading RCCD from URL '%s'") % myUrl);
        if (myUrl.empty())
        {
            anErrorMsg = "Please specify URL to " + resept::ProductName + " Client Customization File";
            return false;
        }

        myUrl = fixUrl(myUrl);

        try
        {
            ta::ScopedResource<CURL*> myCurl(curl_easy_init(), curl_easy_cleanup);
            if (!myCurl)
            {
                anErrorMsg = "Cannot initialize network subsystem";
                ERRORLOG2(anErrorMsg, "Failed to initialize curl");
                return false;
            }
            CURLcode myCurlRetCode = curl_easy_setopt(myCurl, CURLOPT_WRITEFUNCTION, responseCallback);
            if (myCurlRetCode != CURLE_OK)
            {
                anErrorMsg = "Cannot initialize network subsystem";
                ERRORLOG2(anErrorMsg, boost::format("Failed to setup response callback. %s") % curl_easy_strerror(myCurlRetCode));
                return false;
            }
            string myResponse;
            myCurlRetCode = curl_easy_setopt(myCurl, CURLOPT_WRITEDATA, &myResponse);
            if (myCurlRetCode != CURLE_OK)
            {
                anErrorMsg = "Cannot initialize network subsystem";
                ERRORLOG2(anErrorMsg, boost::format("Failed to setup cookie for response callback. %s") % curl_easy_strerror(myCurlRetCode));
                return false;
            }
            myCurlRetCode = curl_easy_setopt(myCurl, CURLOPT_URL, myUrl.c_str());
            if (myCurlRetCode != CURLE_OK)
            {
                anErrorMsg = "Cannot initialize network subsystem";
                ERRORLOG2(anErrorMsg, boost::format("Failed to set CURLOPTURL curl option. %s") % curl_easy_strerror(myCurlRetCode));
                return false;
            }

            myCurlRetCode = curl_easy_setopt(myCurl, CURLOPT_CONNECTTIMEOUT, ConnectTimeout);
            if (myCurlRetCode != CURLE_OK)
            {
                anErrorMsg = "Cannot initialize network subsystem";
                ERRORLOG2(anErrorMsg, boost::format("Failed to set CURLOPT_CONNECTTIMEOUT curl option. %s") % curl_easy_strerror(myCurlRetCode));
                return false;
            }

            // follow HTTP redirects (3xx)
            myCurlRetCode = curl_easy_setopt(myCurl, CURLOPT_FOLLOWLOCATION, 1L);
            if (myCurlRetCode != CURLE_OK)
            {
                anErrorMsg = "Cannot enable HTTP redirects";
                ERRORLOG2(anErrorMsg, boost::format("Failed to set CURLOPT_FOLLOWLOCATION curl option. %s") % curl_easy_strerror(myCurlRetCode));
                return false;
            }


            // set buffer for error messages
            char myExtraErrorMsg[CURL_ERROR_SIZE + 1] = {};
            curl_easy_setopt(myCurl, CURLOPT_ERRORBUFFER, myExtraErrorMsg);
            // setting this is believed to prevent segfaults in curl_resolv_timeout() when DNS lookup times out
            curl_easy_setopt(myCurl, CURLOPT_NOSIGNAL, 1);

            try {
                setupSSL(myCurl);
            } catch (std::exception& e) {
                anErrorMsg = "Failed to configure SSL";
                ERRORLOG2(anErrorMsg, e.what());
                return false;
            }

            try  {
                ta::url::parse(myUrl);
            } catch (ta::UrlParseError& e) {
                anErrorMsg = "Invalid customization URL " + myUrl + ". Valid https:// or http:// URL expected such as https://server.com/path-to-rccd";
                ERRORLOG2(anErrorMsg, e.what());
                return false;
            }

            disableProxy(myCurl);

            myCurlRetCode = curl_easy_perform(myCurl);
            if (myCurlRetCode != CURLE_OK)
            {
                anErrorMsg = "Cannot fetch URL " + myUrl;
                if (myCurlRetCode == CURLE_PEER_FAILED_VERIFICATION || myCurlRetCode == CURLE_SSL_CACERT)
                {
                    anErrorMsg += ". Your remote server cannot be trusted by known CA certificates.";
                }
                else if (myCurlRetCode == CURLE_SSL_CONNECT_ERROR)
                {
                    anErrorMsg += ". Error establishing secure SSL connection.";
                }
                ERRORLOG2(anErrorMsg, boost::format("Failed to fetch URL %s. %s (curl error code %d). Extra error info: %s") % myUrl % curl_easy_strerror(myCurlRetCode) % myCurlRetCode % myExtraErrorMsg);
                return false;
            }
            long myHttpResponseCode = -1;
            myCurlRetCode = curl_easy_getinfo(myCurl, CURLINFO_RESPONSE_CODE, &myHttpResponseCode);
            if (myCurlRetCode != CURLE_OK)
                return anErrorMsg = "Cannot get HTTP response code from URL " + myUrl, false;
            if (myHttpResponseCode == 0)
                return anErrorMsg = "Cannot connect to " + myUrl, false;
            if (myHttpResponseCode != 200)
                return anErrorMsg = str(boost::format("HTTP %d received when fetching %s") % myHttpResponseCode % myUrl), false;
            aBlob = ta::str2Vec<unsigned char>(myResponse);
            return true;
        }
        catch (std::exception& e)
        {
            ERRORDEVLOG(e.what());
            return anErrorMsg = "Unexpected error occurred trying to fetch " + myUrl, false;
        }
    }

    bool loadRccdFromFile(const string& aPath, vector<unsigned char>& aBlob, string& anErrorMsg)
    {
        DEBUGLOG(boost::format("Loading RCCD from file '%s'") % aPath);
        if (aPath.empty())
            return anErrorMsg = "Please specify location of " + resept::ProductName + " Client Customization File", false;
        try
        {
            aBlob = ta::readData(aPath);
            return true;
        }
        catch(std::exception&)
        {
            return anErrorMsg = "Cannot read " + resept::ProductName + " Client Customization File from " + aPath, false;
        }
    }

    bool installRccd(const vector<unsigned char>& anRccdBlob, const string& anUrlHint, ConfirmationPromptCallback aConfirmationPromptCb, void* aConfirmationPromptCbCookie, string& anErrorMsg)
    {
        FUNCLOG;

        string myDownloadedRccdFilePath, myExtractedDir;
        try
        {
            myDownloadedRccdFilePath = ta::Process::genTempPath("rccd_");
            ta::writeData(myDownloadedRccdFilePath, anRccdBlob);
            myExtractedDir = ta::Zip::extract(myDownloadedRccdFilePath, ta::Process::getTempDir());
            if (!myDownloadedRccdFilePath.empty())
            {
                remove(myDownloadedRccdFilePath.c_str());
            }
        }
        catch (std::exception& e)
        {
            anErrorMsg = "Incorrect " + resept::ProductName + " Client Customization File received from " + anUrlHint;
            ERRORLOG2(anErrorMsg, e.what());
            if (!myDownloadedRccdFilePath.empty())
            {
                remove(myDownloadedRccdFilePath.c_str());
            }
            return false;
        }

        const bool myRetVal = installSettings(myExtractedDir, aConfirmationPromptCb, aConfirmationPromptCbCookie, anErrorMsg);

        try {
            boost::filesystem::remove_all(myExtractedDir);
        } catch (std::exception& e) {
            WARNLOG2("Failed to remove extraction directory", boost::format("Failed to remove directory %s. %s") % myExtractedDir % e.what());
        }
        return myRetVal;
    }

#ifdef _WIN32
    bool uninstallUserSettings(const string& aProvider, string& anErrorMsg)
    {
        FUNCLOG;

        foreach(const string& provider, rclient::Settings::getProviders())
        {
            if (boost::iequals(provider, aProvider))
            {
                if (!uninstallUserSettingsForProvider(provider, anErrorMsg))
                {
                    return false; // exit on the first error.
                    // given the fact that we disallow installing multiple providers which differ in letter case only, we will not end up with partially uninstalled settings when provider "KPN" is successfully uninstalled but uninstallation for provider "kpn" fails
                }
            }
        }
        return true;
    }
#endif

}
