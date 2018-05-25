#include "RcdpHandler.h"
#include "RcdpRequest.h"
#include "RcdpResponse.h"
#include "Common.h"
#include "resept/common.h"
#include "ta/sysinfo.h"
#include "ta/rsautils.h"
#include "ta/timeutils.h"
#include "ta/encodingutils.h"
#include "ta/hashutils.h"
#include "ta/tcpsocketutils.h"
#include "ta/logger.h"
#include "ta/utils.h"
#include "ta/scopedresource.hpp"
#include "ta/assert.h"
#include "ta/common.h"

#include "curl/curl.h"
#include "boost/assign/list_of.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/range/algorithm.hpp"
#include "boost/regex.hpp"
#include "boost/bind.hpp"
#include <vector>

using std::string;
using std::vector;
using ta::version::Version;
using boost::assign::map_list_of;

namespace rclient
{
    namespace
    {
        static const unsigned long ConnectTimeout = 10; // seconds
        static const unsigned long TcpNoDelay = 1; // 1-set, 0-cleared
        static const unsigned int TcpIdleTime = 5; // sec
        static const unsigned int TcpKeepAliveInterval = 1; // sec

        int enableKeepAliveCallback(void* UNUSED(clientp), curl_socket_t curlfd, curlsocktype purpose)
        {
            if (purpose != CURLSOCKTYPE_IPCXN)
                return 0;

            try  {
                ta::TcpSocketUtils::enableKeepAlive(curlfd, TcpIdleTime, TcpKeepAliveInterval);
            } catch (std::exception& e) {
                WARNLOG(boost::format("Cannot setup TCP keepalives for RESEPT connection. %s. Ignoring the error.") % e.what());
            }

            return 0;
        }

        size_t logExtraHttpResponseInfoCb(void* ptr, size_t size, size_t nmemb, void* UNUSED(stream))
        {
            if (ptr)
            {
                string myHeader((const char*)ptr, size * nmemb);
                boost::trim(myHeader);
                if (boost::starts_with(myHeader, "Server:"))
                {
                    // knowing basic server info can be useful
                    DEBUGLOG("HTTP response header: " + myHeader);
                }
            }
            return size * nmemb;
        }

        size_t responseCallback(void* buffer, size_t size, size_t nmemb, void* aResponse)
        {
            TA_ASSERT(buffer && aResponse);
            string* myReponse = (string*)aResponse;
            size_t myNumBytesConsumed = nmemb*size;
            myReponse->append((char*)buffer, myNumBytesConsumed);
            return myNumBytesConsumed;
        }

        void verifyTransition(const resept::rcdpv2::State aFrom, const resept::rcdpv2::State aTo)
        {
            using namespace resept::rcdpv2;
            switch (aTo)
            {
            case stateClosed:
                return;
            case stateHello:
                return;
            case stateConnected:
                if (aFrom == stateHello)
                    return;
                if (aFrom == stateConnected)
                    return;
                if (aFrom == stateAuthenticated)
                    return;
                break;
            case stateAuthenticated:
                if (aFrom == stateConnected)
                    return;
                if (aFrom == stateAuthenticated)
                    return;
                break;
            default:
                TA_THROW_MSG(std::logic_error, boost::format("Unsupported transition target state %s") % str(aTo));
            }
            TA_THROW_MSG(std::logic_error, boost::format("No state transition allowed from %s to %s") % str(aFrom) % str(aTo));
        }

        vector<Version> getSupportedRcdpVersions()
        {
            vector<Version> myVersions;
            foreach (const string& v, resept::rcdpv2::ClientSupportedRcdpVersions)
            {
                myVersions.push_back(ta::version::parse(v));
            }
            return myVersions;
        }

        Version getHighestSupportedRcdpVersion()
        {
            return *boost::max_element(getSupportedRcdpVersions());
        }

        // check whether the given RCDP version is supported by the client ignoring subminor part
        bool isRcdpVersionSupported(const Version& aVersion)
        {
            foreach (const Version& supportedVer, getSupportedRcdpVersions())
            {
                if (supportedVer.major() == aVersion.major() && supportedVer.minor() == aVersion.minor())
                {
                    return true;
                }
            }
            return false;
        }

        string formatSupportedRcdpVersions()
        {
            string myRetVal;
            foreach (const Version& supportedVer, getSupportedRcdpVersions())
            {
                if (!myRetVal.empty())
                    myRetVal += ", ";
                myRetVal += toStr(supportedVer);
            }
            return myRetVal;
        }


    } // private stuff


    struct RcdpHandler::RcdpHandlerImpl
    {
        RcdpHandlerImpl(const ta::NetUtils::RemoteAddress& aServer)
            : server(aServer)
        {
            DEBUGLOG(boost::format("Created RcdpHandler for server at %s") % toString(server));
        }

        string sendHttpRequest(const resept::rcdpv2::Request aReqType, const ta::StringDict& aReqParams = ta::StringDict());
        string makeRequestUrl(const resept::rcdpv2::Request aReqType, const ta::StringDict& aReqParams) const;
        void setupSSL(CURL* aCurl) const;
        void logExtraHttpResponseInfo(CURL* aCurl);
        void parseSidFromHttpResponse(CURL* aCurl);
        static void disableProxy(CURL* aCurl);

        const ta::NetUtils::RemoteAddress server;
        UserRcdpSessionData session;
    };

    RcdpHandler::RcdpHandler(const ta::NetUtils::RemoteAddress& aServer)
        : pImpl(new RcdpHandlerImpl(aServer))
    {}

    RcdpHandler::~RcdpHandler()
    {
        //if (pImpl->session.rcdpState != resept::rcdpv2::stateClosed)
        //{
        //    eoc(); // just play nice by letting the server garbage this session
        //}
        delete pImpl;
    }


    //
    // Public API
    //

    void RcdpHandler::eoc(const string& aReason)
    {
        using namespace resept::rcdpv2;

        TA_ASSERT(pImpl);

        try
        {
            DEBUGDEVLOG(boost::format("Sending end of communication request with reason: %s") % (aReason.empty() ? "<not set>" : aReason));
            const Request myReqType = reqEOC;
            const ta::StringDict myReqParams = map_list_of(requestParamNameReason, aReason);

            const string myResp = pImpl->sendHttpRequest(myReqType, myReqParams);
            DEBUGDEVLOG(boost::format("Received RCDP response %s on %s request") % myResp % str(myReqType));

            const string myReason = rcdpv2response::parseEoc(myResp);
            DEBUGLOG(boost::format("Sent %s, received %s back with reason: %s") % str(myReqType) % str(respEOC) % (myReason.empty() ? "<not set>" : myReason));
            pImpl->session.reset();
        }
        catch (...)
        {
            pImpl->session.reset();
            throw;
        }
    }

    void RcdpHandler::error(const int aCode, const string& aDescription)
    {
        using namespace resept::rcdpv2;

        TA_ASSERT(pImpl);

        try
        {
            DEBUGDEVLOG(boost::format("Sending error request with code %d and description: %s") % aCode % aDescription);

            const Request myReqType = reqError;
            const ta::StringDict myReqParams = map_list_of(requestParamNameErrorCode, ta::Strings::toString(aCode))
                                               (requestParamNameErrorDescription, aDescription);

            const string myResp = pImpl->sendHttpRequest(myReqType, myReqParams);
            DEBUGDEVLOG(boost::format("Received RCDP response %s on %s request") % myResp % str(myReqType));

            const string myReason = rcdpv2response::parseEoc(myResp);
            DEBUGLOG(boost::format("Sent %s, received %s back with reason: %s") % str(myReqType) % str(respEOC) % (myReason.empty() ? "<not set>" : myReason));
            pImpl->session.reset();
        }
        catch (...)
        {
            pImpl->session.reset();
            throw;
        }
    }

    void RcdpHandler::hello()
    {
        using namespace resept::rcdpv2;

        TA_ASSERT(pImpl);

        try
        {
            DEBUGDEVLOG("Sending hello request");
            pImpl->session.reset();

            const Request myReqType = reqHello;
            const ta::StringDict myReqParams = map_list_of(requestParamNameCallerAppDescription, ClientDescription);

            const string myResp = pImpl->sendHttpRequest(myReqType, myReqParams);
            DEBUGDEVLOG(boost::format("Received RCDP response %s on %s request") % myResp % str(myReqType));

            handleErrors(myResp, myReqType);

            const ta::version::Version myProposedRcdpVersion = rcdpv2response::parseHello(myResp);
            DEBUGLOG(boost::format("Sent %s, received %s back with proposed RCDP version %s") % str(myReqType) % str(respHello) % toStr(myProposedRcdpVersion));

            // we will check later if this version is ok for us later on during handshake
            pImpl->session.rcdpVersion = myProposedRcdpVersion;

            setState(stateHello);
        }
        catch (...)
        {
            pImpl->session.reset();
            throw;
        }
    }

    void RcdpHandler::handshake()
    {
        using namespace resept::rcdpv2;

        TA_ASSERT(pImpl);

        try
        {
            DEBUGDEVLOG("Requesting handshake");

            if (pImpl->session.rcdpState != stateHello)
            {
                TA_THROW_MSG(std::invalid_argument, "Cannot handshake from " + str(pImpl->session.rcdpState) + " state");
            }

            if (!isRcdpVersionSupported(pImpl->session.rcdpVersion))
            {
                TA_THROW_MSG(rclient::RcdpVersionMismatchError, boost::format("RCDP version %s proposed by the server is not supported by the client. Client supports the following RCDP versions: %s") % toStr(pImpl->session.rcdpVersion) % formatSupportedRcdpVersions());
            }
            DEBUGDEVLOG(boost::format("Client and server agreed on RCDP version %s") % toStr(pImpl->session.rcdpVersion));

            const Request myReqType = reqHandshake;
            const ta::StringDict myReqParams = map_list_of(requestParamNameCallerUtc, ta::TimeUtils::timestampToIso8601(time(NULL)));

            const string myResp = pImpl->sendHttpRequest(myReqType, myReqParams);
            DEBUGDEVLOG(boost::format("Received RCDP response %s on %s request") % myResp % str(myReqType));

            handleErrors(myResp, myReqType);

            const time_t myServerUtc = rcdpv2response::parseHandshake(myResp);
            DEBUGLOG(boost::format("Sent %s, received %s back with server UTC %s") % str(myReqType) % str(respHandshake) % ta::TimeUtils::timestampToUtcStr(myServerUtc));

            setState(stateConnected);
        }
        catch (...)
        {
            pImpl->session.reset();
            throw;
        }
    }

    rclient::AuthRequirements RcdpHandler::getAuthRequirements(const string& aServiceName)
    {
        using namespace resept::rcdpv2;

        TA_ASSERT(pImpl);

        try
        {
            DEBUGDEVLOG("Requesting authentication requirements for service " + aServiceName);

            if (pImpl->session.rcdpState != stateConnected)
            {
                TA_THROW_MSG(std::invalid_argument, "Cannot request authentication requirements from " + str(pImpl->session.rcdpState) + " state");
            }

            const Request myReqType = reqAuthRequirements;
            const ta::StringDict myReqParams = map_list_of(requestParamNameService, aServiceName);

            const string myResp = pImpl->sendHttpRequest(myReqType, myReqParams);
            DEBUGDEVLOG(boost::format("Received RCDP response %s on %s request") % myResp % str(myReqType));

            handleErrors(myResp, myReqType);

            const AuthRequirements myAuthRequirements = rcdpv2response::parseAuthRequirements(myResp);
            DEBUGLOG(boost::format("Sent %s, received %s back with authentication requirements: %s") % str(myReqType) % str(respAuthRequirements) % str(myAuthRequirements));

            return myAuthRequirements;
        }
        catch (...)
        {
            pImpl->session.reset();
            throw;
        }
    }

    AuthResponse RcdpHandler::authenticate(const string& aServiceName,
                                           const resept::Credentials& aCredentials,
                                           const ta::StringArrayDict& aResolvedURIs,
                                           const ta::StringDict& aCalculatedDigests)
    {
        using namespace resept::rcdpv2;

        TA_ASSERT(pImpl);

        try
        {
            DEBUGDEVLOG("Authenticating for service " + aServiceName);

            if (pImpl->session.rcdpState != stateConnected)
            {
                TA_THROW_MSG(std::invalid_argument, "Cannot authenticate from " + str(pImpl->session.rcdpState) + " state");
            }

            const Request myReqType = reqAuthentication;
            const ta::StringDict myReqParams = rcdpv2request::makeAuthenticateRequestParams(aServiceName, aCredentials, aResolvedURIs, aCalculatedDigests);

            const string myResp = pImpl->sendHttpRequest(myReqType, myReqParams);
            DEBUGDEVLOG(boost::format("Received RCDP response %s on %s request") % myResp % str(myReqType));

            handleErrors(myResp, myReqType);

            const AuthResponse myAuthResult = rcdpv2response::parseAuthResponse(myResp);
            DEBUGLOG(boost::format("Sent %s, received %s back: %s") % str(myReqType) % str(respAuthResult) % str(myAuthResult.auth_result.type));

            switch (myAuthResult.auth_result.type)
            {
            case resept::AuthResult::Ok:
                setState(stateAuthenticated);
                break;
            case resept::AuthResult::Delay:
                setState(stateConnected);
                break;
            case resept::AuthResult::Locked:
                pImpl->session.reset();
                setState(stateClosed);
                break;
            case resept::AuthResult::Expired:
            case resept::AuthResult::Challenge:
                setState(stateConnected);
                break;
            default:
                TA_THROW_MSG(std::runtime_error, "Unsupported authentication result type " + str(myAuthResult.auth_result.type));
            }

            return myAuthResult;
        }
        catch (...)
        {
            pImpl->session.reset();
            throw;
        }
    }

    AuthResponse RcdpHandler::changePassword(const string& anOldPassword, const string& aNewPassword)
    {
        using namespace resept::rcdpv2;

        TA_ASSERT(pImpl);

        try
        {
            DEBUGDEVLOG("Changing user password");

            if (pImpl->session.rcdpState != stateConnected && pImpl->session.rcdpState != stateAuthenticated)
            {
                TA_THROW_MSG(std::invalid_argument, "Cannot change password from " + str(pImpl->session.rcdpState) + " state");
            }

            const Request myReqType = reqChangePassword;
            const ta::StringDict myReqParams = map_list_of(requestParamNameOldPassword, anOldPassword)
                                               (requestParamNameNewPassword, aNewPassword);

            const string myResp = pImpl->sendHttpRequest(myReqType, myReqParams);
            DEBUGDEVLOG(boost::format("Received RCDP response %s on %s request") % myResp % str(myReqType));

            handleErrors(myResp, myReqType);

            const AuthResponse myAuthResult = rcdpv2response::parseAuthResponse(myResp);
            DEBUGLOG(boost::format("Sent %s, received %s back: %s") % str(myReqType) % str(respAuthResult) % str(myAuthResult.auth_result.type));

            switch (myAuthResult.auth_result.type)
            {
            case resept::AuthResult::Ok:
                // Step back, logging out if needed, to force user authenticate with a new password
                setState(stateConnected);
                break;
            case resept::AuthResult::Delay:
                // Remain in the current state when password change fails
                break;
            case resept::AuthResult::Locked:
                pImpl->session.reset();
                setState(stateClosed);
                break;
            case resept::AuthResult::Challenge:
            default:
                TA_THROW_MSG(std::runtime_error, "Unsupported authentication result type " + str(myAuthResult.auth_result.type) + " received for change password request");
            }

            return myAuthResult;
        }
        catch (...)
        {
            pImpl->session.reset();
            throw;
        }
    }

    Messages RcdpHandler::getLastMessages(const time_t* aFromUtc)
    {
        using namespace resept::rcdpv2;

        TA_ASSERT(pImpl);

        try
        {
            DEBUGDEVLOG("Requesting last messages" +  (aFromUtc ? ta::TimeUtils::timestampToIso8601(*aFromUtc) : ""));

            if (pImpl->session.rcdpState != stateAuthenticated)
            {
                TA_THROW_MSG(std::invalid_argument, "Cannot request last messages from " + str(pImpl->session.rcdpState) + " state");
            }

            const Request myReqType = reqLastMessages;
            const ta::StringDict myReqParams = aFromUtc ? map_list_of(requestParamNameLastMessagesFromUtc, ta::TimeUtils::timestampToIso8601(*aFromUtc))
                                               : ta::StringDict();

            const string myResp = pImpl->sendHttpRequest(myReqType, myReqParams);
            DEBUGDEVLOG(boost::format("Received RCDP response %s on %s request") % myResp % str(myReqType));

            handleErrors(myResp, myReqType);

            const Messages myMessages = rcdpv2response::parseLastMessages(myResp);
            DEBUGLOG(boost::format("Sent %s, received %s back with authentication requirements: %s") % str(myReqType) % str(respLastMessages) % rclient::formatMessages(myMessages));

            return myMessages;
        }
        catch (...)
        {
            pImpl->session.reset();
            throw;
        }
    }

    CertResponse RcdpHandler::getCert(const resept::CertFormat aCertFormat, const bool anIncludeChain, const ta::KeyPair* aFromKeyPair)
    {
        using namespace resept::rcdpv2;

        TA_ASSERT(pImpl);

        try
        {
            DEBUGDEVLOG(boost::format("Requesting certificate %s in %s format%s") % (anIncludeChain ? "with chain" : "without chain") % str(aCertFormat) % (aFromKeyPair ? " from self keypair" : ""));

            if (pImpl->session.rcdpState != stateAuthenticated)
            {
                TA_THROW_MSG(std::invalid_argument, "Cannot request certificate from " + str(pImpl->session.rcdpState) + " state");
            }

            const Request myReqType = reqCert;
            const ta::StringDict myReqParams = rcdpv2request::makeCertRequestParams(aCertFormat, anIncludeChain, aFromKeyPair);

            const string myResp = pImpl->sendHttpRequest(myReqType, myReqParams);
            DEBUGDEVLOG(boost::format("Received RCDP response %s on %s request") % myResp % str(myReqType));

            handleErrors(myResp, myReqType);

            const CertResponse myCertResponse = rcdpv2response::parseCert(myResp, aCertFormat, pImpl->session.sid);
            DEBUGLOG(boost::format("Sent %s, received %s back") % str(myReqType) % str(respCert));

            return myCertResponse;
        }
        catch (...)
        {
            pImpl->session.reset();
            throw;
        }
    }



    UserRcdpSessionData RcdpHandler::userSessionData() const
    {
        TA_ASSERT(pImpl);
        return pImpl->session;
    }


    void RcdpHandler::RcdpHandlerImpl::disableProxy(CURL* aCurl)
    {
        if (!aCurl)
        {
            TA_THROW_MSG(std::runtime_error, "NULL curl handle");
        }

        // In order to completely disable proxy we should explicitly specify proxy address to empty string to prevent curl from implicitly using 'http_proxy' environment variable

        CURLcode myCurlRetCode = curl_easy_setopt(aCurl, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
        if (myCurlRetCode != CURLE_OK)
        {
            TA_THROW_MSG(std::runtime_error, boost::format("Failed to setup supported proxy type. %s") % curl_easy_strerror(myCurlRetCode));
        }

        myCurlRetCode = curl_easy_setopt(aCurl, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
        if (myCurlRetCode != CURLE_OK)
        {
            TA_THROW_MSG(std::runtime_error, boost::format("Failed to setup supported proxy authentication type. %s") % curl_easy_strerror(myCurlRetCode));
        }

        myCurlRetCode = curl_easy_setopt(aCurl, CURLOPT_PROXY, "");
        if (myCurlRetCode != CURLE_OK)
        {
            TA_THROW_MSG(std::runtime_error, boost::format("Failed to disable proxy. %s") % curl_easy_strerror(myCurlRetCode));
        }
    }

    void RcdpHandler::RcdpHandlerImpl::logExtraHttpResponseInfo(CURL* aCurl)
    {
        if (!aCurl)
        {
            return;
        }

        try
        {
            char* myHttpHeader = NULL;
            if (curl_easy_getinfo(aCurl, CURLINFO_PRIMARY_IP, &myHttpHeader) == CURLE_OK && myHttpHeader)
            {
                DEBUGLOG(boost::format("Server IP parsed from HTTP header: %s") % myHttpHeader);
            }
            if (curl_easy_getinfo(aCurl, CURLINFO_LOCAL_IP, &myHttpHeader) == CURLE_OK && myHttpHeader)
            {
                DEBUGLOG(boost::format("Client IP parsed from HTTP header: %s") % myHttpHeader);
            }
        }
        catch(std::exception& e)
        {
            WARNDEVLOG(boost::format("Error fetching HTTP headers for debugging. Error: %s") % e.what());
        }
        catch(...)
        {
            WARNDEVLOG("Error fetching HTTP headers for debugging");
        }
    }

    string RcdpHandler::RcdpHandlerImpl::makeRequestUrl(const resept::rcdpv2::Request aReqType, const ta::StringDict& aReqParams) const
    {
        const ta::version::Version myRequestRcdpVer = (session.rcdpState >= resept::rcdpv2::stateConnected) ? session.rcdpVersion
                : getHighestSupportedRcdpVersion();

        string myUrlQuery;
        foreach (const ta::StringDict::value_type& kv, aReqParams)
        {
            const string myUrlEncodedKv = ta::EncodingUtils::urlEncode(kv.first) + "=" + ta::EncodingUtils::urlEncode(kv.second);
            myUrlQuery += myUrlQuery.empty() ? ("?" + myUrlEncodedKv) : ("&" + myUrlEncodedKv);
        }

        const string myUrl = str(boost::format("https://%s/%s/%s/%s%s") % toString(server)
                                 % resept::rcdpv2::HttpRequestUriPrefix
                                 % toStr(myRequestRcdpVer)
                                 % str(aReqType)
                                 % myUrlQuery);
        return myUrl;
    }

    void RcdpHandler::RcdpHandlerImpl::setupSSL(CURL* aCurl) const
    {
        if (!aCurl)
        {
            TA_THROW_MSG(std::invalid_argument, "NULL curl handle");
        }

        CURLcode myCurlRetCode = curl_easy_setopt(aCurl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2 /* i.e. TLS-XXX and higher */);
        if (myCurlRetCode != CURLE_OK)
        {
            TA_THROW_MSG(std::runtime_error, boost::format("Failed to set TLS version for RESEPT server connection. %s") % curl_easy_strerror(myCurlRetCode));
        }

#ifdef _WIN32
        curl_tlssessioninfo * myTlsSessionInfo = NULL;
        myCurlRetCode = curl_easy_getinfo(aCurl, CURLINFO_TLS_SSL_PTR, &myTlsSessionInfo);
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

    /**
    @return response body
    @throw HttpRequestError e.g. if connection fails or received HTTP code is other than 200
    */
    string RcdpHandler::RcdpHandlerImpl::sendHttpRequest(const resept::rcdpv2::Request aReqType, const ta::StringDict& aReqParams)
    {
        try
        {
            ta::ScopedResource<CURL*> myCurl(curl_easy_init(), curl_easy_cleanup);
            if (!myCurl)
            {
                TA_THROW_MSG(HttpRequestError, "Failed to initialize curl");
            }
            CURLcode myCurlRetCode = curl_easy_setopt(myCurl, CURLOPT_TCP_NODELAY, TcpNoDelay);
            if (myCurlRetCode != CURLE_OK)
            {
                TA_THROW_MSG(HttpRequestError, boost::format("Failed to set TcpNoDelay CURL option to %u. %s") % TcpNoDelay % curl_easy_strerror(myCurlRetCode));
            }
            myCurlRetCode = curl_easy_setopt(myCurl, CURLOPT_WRITEFUNCTION, responseCallback);
            if (myCurlRetCode != CURLE_OK)
            {
                TA_THROW_MSG(HttpRequestError, boost::format("Failed to setup response callback. %s") % curl_easy_strerror(myCurlRetCode));
            }
            string myResponse;
            myCurlRetCode = curl_easy_setopt(myCurl, CURLOPT_WRITEDATA, &myResponse);
            if (myCurlRetCode != CURLE_OK)
            {
                TA_THROW_MSG(HttpRequestError, boost::format("Failed to setup cookie for response callback. %s") % curl_easy_strerror(myCurlRetCode));
            }

            const string myRequestUrl = makeRequestUrl(aReqType, aReqParams);
            // DEBUGDEVLOG("Sending request: " + myRequestUrl);
            myCurlRetCode = curl_easy_setopt(myCurl, CURLOPT_URL, myRequestUrl.c_str());
            if (myCurlRetCode != CURLE_OK)
            {
                TA_THROW_MSG(HttpRequestError, boost::format("Failed to set URL curl option. %s") % curl_easy_strerror(myCurlRetCode));
            }

            setupSSL(myCurl);

            myCurlRetCode = curl_easy_setopt(myCurl, CURLOPT_CONNECTTIMEOUT, ConnectTimeout);
            if (myCurlRetCode != CURLE_OK)
            {
                TA_THROW_MSG(HttpRequestError, boost::format("Failed to set CURLOPT_CONNECTTIMEOUT curl option. %s") % curl_easy_strerror(myCurlRetCode));
            }

            // follow HTTP redirects (3xx)
            myCurlRetCode = curl_easy_setopt(myCurl, CURLOPT_FOLLOWLOCATION, 1L);
            if (myCurlRetCode != CURLE_OK)
            {
                TA_THROW_MSG(HttpRequestError, boost::format("Failed to set CURLOPT_FOLLOWLOCATION curl option. %s") % curl_easy_strerror(myCurlRetCode));
            }


            // setup HTTP cookie
            myCurlRetCode = curl_easy_setopt(myCurl, CURLOPT_COOKIEFILE, ""); /* to start the cookie engine */
            if (myCurlRetCode != CURLE_OK)
            {
                TA_THROW_MSG(HttpRequestError, boost::format("Failed to enable cookie engine. %s") % curl_easy_strerror(myCurlRetCode));
            }
            if (session.sid_exist)
            {
                const string mySidCookie = resept::rcdpv2::HttpSidCookieName + "=" + session.sid;
                myCurlRetCode = curl_easy_setopt(myCurl, CURLOPT_COOKIE, mySidCookie.c_str());
                if (myCurlRetCode != CURLE_OK)
                {
                    TA_THROW_MSG(HttpRequestError, boost::format("Failed to set cookie. %s") % curl_easy_strerror(myCurlRetCode));
                }
            }

            // setup extra debugging and other convenience options; do not bother if they fail
            curl_easy_setopt(myCurl, CURLOPT_HEADERFUNCTION, logExtraHttpResponseInfoCb);
            curl_easy_setopt(myCurl, CURLOPT_USERAGENT, str(boost::format("%s/%s") % resept::ProductName % ta::version::toStr(ClientVersion, ta::version::fmtMajorMinor)).c_str());
            // @note TCP keep-alive will only be useful for direct connections (no HTTP proxy)
            curl_easy_setopt(myCurl, CURLOPT_SOCKOPTFUNCTION, enableKeepAliveCallback);
            // setting this is believed to prevent segfaults in curl_resolv_timeout() when DNS lookup times out
            curl_easy_setopt(myCurl, CURLOPT_NOSIGNAL, 1L);
            char myExtraErrorMsg[CURL_ERROR_SIZE + 1] = {};
            curl_easy_setopt(myCurl, CURLOPT_ERRORBUFFER, myExtraErrorMsg);

            disableProxy(myCurl);

            // Send request and read response
            myCurlRetCode = curl_easy_perform(myCurl);
            if (myCurlRetCode != CURLE_OK)
            {
                if (myCurlRetCode == CURLE_SSL_CONNECT_ERROR && string(myExtraErrorMsg).find("SEC_E_WRONG_PRINCIPAL") != std::string::npos && (ta::NetUtils::isValidIpv4(server.host) || ta::NetUtils::isValidIpv6(server.host)))
                {
                    ERRORLOG("HINT: When IP address is used for " + resept::ProductName + " server address, it should be specified in \"IP:\" and in \"DNS:\" fields of the Subject Alternative Names extension of the client-server communication certificate server-side. Specifying IP in \"DNS:\" fields is necessary to make it work on Windows 7.");
                }
                TA_THROW_MSG(HttpRequestError, boost::format("Failed to send HTTP GET request to %s (curl error code: %d). %s. Extra error info: %s") % toString(server) % myCurlRetCode % curl_easy_strerror(myCurlRetCode) % myExtraErrorMsg);
            }

            long myHttpResponseCode = -1;
            myCurlRetCode = curl_easy_getinfo(myCurl, CURLINFO_RESPONSE_CODE, &myHttpResponseCode);
            if (myCurlRetCode != CURLE_OK)
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Failed to retrieve response code for HTTP GET request to %s. %s") % toString(server) % curl_easy_strerror(myCurlRetCode));
            }
            if (myHttpResponseCode == 0)
            {
                TA_THROW_MSG(HttpRequestError, boost::format("Failed to connect to the RESEPT server at %s") % toString(server));
            }
            logExtraHttpResponseInfo(myCurl);
            if (myHttpResponseCode != 200)
            {
                TA_THROW_MSG(HttpRequestError, boost::format("Got HTTP %d for HTTP GET request at %s") % myHttpResponseCode % toString(server));
            }


            parseSidFromHttpResponse(myCurl);

            return myResponse;
        }
        catch (HttpRequestError&)
        {
            throw;
        }
        catch (std::exception& e)
        {
            TA_THROW_MSG(HttpRequestError, e.what());
        }
    }


    // throw HttpRequestError
    void RcdpHandler::RcdpHandlerImpl::parseSidFromHttpResponse(CURL* aCurl)
    {
        TA_ASSERT(aCurl);

        curl_slist* cookies = NULL;
        CURLcode myCurlRetCode = curl_easy_getinfo(aCurl, CURLINFO_COOKIELIST, &cookies);
        if (myCurlRetCode != CURLE_OK)
        {
            TA_THROW_MSG(HttpRequestError, boost::format("Failed to get cookies from HTTP response. %s") % curl_easy_strerror(myCurlRetCode));
        }

        if (!cookies)
        {
            return;
        }

        // Parse Netscape cookie as:
        // 192.168.0.147      FALSE   /       FALSE   0       keytalkcookie    49c155340a944a168a946a160a946a16
        for (curl_slist* cookie = cookies; cookie; cookie = cookie->next)
        {
            vector<string> myParts;
            boost::split(myParts, cookie->data, boost::is_any_of(" \t"));
            if (myParts.size() == 7 && myParts[5] == resept::rcdpv2::HttpSidCookieName)
            {
                session.sid = myParts[6];
                session.sid_exist = !session.sid.empty();
                break;
            }
        }
        curl_slist_free_all(cookies);
        return;
    }

    void RcdpHandler::handleErrors(const string& aResponse, const resept::rcdpv2::Request aReqTypeHint)
    {
        const resept::rcdpv2::Response myResponseStatus = rcdpv2response::parseResponseStatus(aResponse);

        switch (myResponseStatus)
        {
        case resept::rcdpv2::respEOC:
        {
            const string myReason = rcdpv2response::parseEoc(aResponse);
            WARNLOG(boost::format("Sent %s, received %s back with reason: %s") % str(aReqTypeHint) % str(myResponseStatus) % (myReason.empty() ? "<not set>" : myReason));
            pImpl->session.reset();
            TA_THROW(EocError);
        }
        case resept::rcdpv2::respError:
        {
            string myErrorDescr;
            const int myErrorCode = rcdpv2response::parseError(aResponse, myErrorDescr);
            WARNLOG(boost::format("Sent %s, received %s back with code %d. %s") % str(aReqTypeHint) % str(myResponseStatus) % myErrorCode % myErrorDescr);
            pImpl->session.reset();
            TA_THROW_ARG_STR(ErrError, myErrorCode, myErrorDescr);
        }
        default:
            break;
        }
    }

    void RcdpHandler::setState(const resept::rcdpv2::State aState)
    {
        verifyTransition(pImpl->session.rcdpState, aState);
        pImpl->session.rcdpState = aState;
    }
}
