#include "ReseptClientApp.h"
#include "CommonUtils.h"
#include "rclient/Settings.h"
#include "rclient/NativeCertStore.h"
#include "rclient/RcdpHandler.h"
#include "rclient/RcdpRequest.h"
#include "rclient/Common.h"
#include "resept/util.h"
#include "resept/common.h"
#include "ta/sysinfo.h"
#include "resept/computeruuid.h"
#include "ta/opensslapp.h"
#include "ta/logger.h"
#include "ta/netutils.h"
#include "ta/dnsutils.h"
#include "ta/hashutils.h"
#include "ta/process.h"
#include "ta/timeutils.h"
#include "ta/strings.h"
#include "ta/url.h"
#include "ta/utils.h"
#include "ta/common.h"

#include "boost/bind.hpp"
#include <memory>

using std::string;
using std::vector;

namespace rclient
{
    struct MissingCredError: std::runtime_error
    {
        explicit MissingCredError (const std::string& aMessage) : std::runtime_error(aMessage) {}
    };

    struct ReseptClientApp::ReseptClientAppImpl
    {
        std::auto_ptr<rclient::LoggerInitializer> loggerInitializer;
        std::auto_ptr<ta::OpenSSLApp> openSSLAppPtr;
        std::string provider;
        std::string service;
        std::string userid;
        void* cookie;

        ReseptClientAppImpl(const Options& anOptions, void* aCookie)
            : cookie(aCookie)
        {
            try
            {
                checkReseptCustomized();
                initLogger();
                initOpenSSL();

                string myUserError;
                if (!init(anOptions, myUserError))
                {
                    TA_THROW_MSG2(ReseptClientAppInitError, myUserError, "Initialization failed. " + myUserError);
                }
            }
            catch (ReseptClientAppInitError& e)
            {
                ERRORLOG2(e.userError(), e.developerError());
                throw;
            }
            catch (std::exception& e)
            {
                const string myUserError = "Initialization failed. See log for more info.";
                const string myDevError =  e.what();
                ERRORLOG2(myUserError, myDevError);
                TA_THROW_MSG2(ReseptClientAppInitError, myUserError, myDevError);
            }
            catch (...)
            {
                const string myUserError = "Initialization failed. See log for more info.";
                const string myDevError =  "Initialization failed. Unknown error.";
                ERRORLOG2(myUserError, myDevError);
                TA_THROW_MSG2(ReseptClientAppInitError, myUserError, myDevError);
            }

            DEBUGLOG(boost::format("Successfully initialized %s Console Client. Provider: %s, service: %s, user: %s") % resept::ProductName % provider % service % userid);
        }

        ~ReseptClientAppImpl() {}


        void checkForNewMessages(rclient::RcdpHandler& anRcdpHandler, ReseptClientApp::OnUserMessagesCb aOnUserMessages)
        {
            rclient::Messages myLastMessages;
            if (rclient::Settings::isLastUserMsgUtcExist())
            {
                const time_t myLastUserMsgFromUtc = ta::TimeUtils::parseUtcIso8601(rclient::Settings::getLastUserMsgUtc()) + 1;
                myLastMessages = anRcdpHandler.getLastMessages(&myLastUserMsgFromUtc);
            }
            else
            {
                myLastMessages = anRcdpHandler.getLastMessages();
            }

            if (!myLastMessages.empty())
            {
                const string myLastUserMsgUtc = ta::TimeUtils::timestampToIso8601(myLastMessages.back().utc);
                rclient::Settings::setLastUserMsgUtc(myLastUserMsgUtc);

                if (aOnUserMessages)
                {
                    // convert received user messages to the format used by API
                    std::vector<ReseptClientApp::UserMessage> myUserMessages;
                    foreach (const rclient::Message& msg, myLastMessages)
                    {
                        myUserMessages.push_back(ReseptClientApp::UserMessage(msg.utc, msg.text));
                    }
                    aOnUserMessages(myUserMessages, cookie);
                }
            }
        }

        void requestCertificate(rclient::RcdpHandler& anRcdpHandler, OnPfxCb aOnPfx, OnPemCb aOnPem)
        {
            const resept::CertFormat myCertFormat = rclient::Settings::getCertFormat();
            const bool myWithChain = rclient::Settings::isCertChain();
            const rclient::CertResponse myCertResponse = anRcdpHandler.getCert(myCertFormat, myWithChain);

            // Import/save certificate
            if (myCertFormat == resept::certformatP12)
            {
                const rclient::Pfx myPfx(myCertResponse.cert, myCertResponse.password);
                rclient::NativeCertStore::importPfx(myPfx);
                if (aOnPfx)
                {
                    aOnPfx(myCertResponse.cert, myCertResponse.password, cookie);
                }
            }
            else if (myCertFormat == resept::certformatPem)
            {
                if (aOnPem)
                {
                    aOnPem(myCertResponse.cert, myCertResponse.password, cookie);
                }
            }
            else
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Unsupported certificate format in settings %s") % str(myCertFormat));
            }
        }

        //@return whether the password has been changed
        bool changeExpiringPassword(rclient::RcdpHandler& anRcdpHandler, const resept::AuthResult anAuthResult, const resept::Credentials& aSuppliedCreds, OnChangePasswordPromptCb aOnChangePasswordPrompt)
        {
            if (anAuthResult.passwordValidity.status == resept::PasswordValidity::notExpired)
            {
                if (ta::SysInfo::isUserPasswordExpiring(anAuthResult.passwordValidity.validity))
                {
                    const string myExpiringMsg = str(boost::format("Password for user %s, provider %s, service %s is expiring in %s") % userid % provider % service % ta::TimeUtils::formatTimeInterval((unsigned int)anAuthResult.passwordValidity.validity));
                    WARNLOG(myExpiringMsg);

                    if (changeNotExpiredPassword(anRcdpHandler, myExpiringMsg, aSuppliedCreds, aOnChangePasswordPrompt))
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        resept::AuthResult authenticate(rclient::RcdpHandler& anRcdpHandler,
                                        OnPasswordPromptCb aOnPasswordPrompt,
                                        OnPincodePromptCb aOnPincodePrompt,
                                        OnResponsePromptCb aOnResponsePrompt,
                                        OnAuthenticationDelayedCb aOnAuthenticationDelayed,
                                        OnAuthenticationUserLockedCb aOnAuthenticationUserLocked,
                                        /*[out]*/ resept::Credentials& aSuppliedCreds)
        {
            const rclient::AuthRequirements myAuthReqs = anRcdpHandler.getAuthRequirements(service);

            ta::StringArrayDict myResolvedURIs;
            ta::StringDict myCalculatedDigests;

            // resolve URIs is requested
            if (myAuthReqs.resolve_service_uris)
            {
                foreach (const std::string& uri, myAuthReqs.service_uris)
                {
                    const string myHost = ta::url::parse(uri).authority_parts.host;
                    DEBUGLOG("Resolving " + myHost);
                    ta::StringArray myIps;
                    foreach (const ta::NetUtils::IP& ip, ta::DnsUtils::resolveIpsByName(myHost))
                    {
                        if (!ip.ipv4.empty())
                        {
                            myIps.push_back(ip.ipv4);
                        }
                        if (!ip.ipv6.empty())
                        {
                            myIps.push_back(ip.ipv6);
                        }
                    }
                    DEBUGLOG("Resolved IPs of " + myHost + ": " + ta::Strings::join(myIps, ","));
                    myResolvedURIs[uri] = myIps;
                }
            }

            // calculate digests is requested
            if (myAuthReqs.calc_service_uris_digest)
            {
                foreach (const std::string& uri, myAuthReqs.service_uris)
                {
                    const string myExecutableNativePath = ta::Process::expandEnvVars(ta::url::makeNativePath(uri));
                    const string myDigest = ta::HashUtils::getSha256HexFile(myExecutableNativePath);
                    DEBUGLOG("Digest of " + myExecutableNativePath + "  is " + myDigest);
                    myCalculatedDigests[uri] = myDigest;
                }
            }

            // gather credentials
            aSuppliedCreds = getSuppliedCredentials(myAuthReqs.cred_types,
                                                    myAuthReqs.hwsig_formula,
                                                    ta::StringDict(),
                                                    ta::StringArray(),
                                                    aOnPasswordPrompt,
                                                    aOnPincodePrompt,
                                                    aOnResponsePrompt);

            AuthResponse myAuthResponse = anRcdpHandler.authenticate(service, aSuppliedCreds, myResolvedURIs, myCalculatedDigests);

            while (true)
            {
                switch (myAuthResponse.auth_result.type)
                {
                case resept::AuthResult::Ok:
                {
                    return myAuthResponse.auth_result;
                }
                case resept::AuthResult::Locked:
                {
                    WARNLOG(boost::format("User %s is locked trying to authenticate against provider %s, service %s") % userid % provider % service);
                    if (aOnAuthenticationUserLocked)
                    {
                        aOnAuthenticationUserLocked(cookie);
                    }
                    return myAuthResponse.auth_result;
                }
                case resept::AuthResult::Delay:
                {
                    WARNLOG(boost::format("User %s is delayed for %d seconds because of invalid credentials provided for provider %s, service %s") % userid % myAuthResponse.auth_result.delay % provider % service);
                    if (aOnAuthenticationDelayed)
                    {
                        aOnAuthenticationDelayed(myAuthResponse.auth_result.delay, cookie);
                    }
                    return myAuthResponse.auth_result;
                }
                case resept::AuthResult::Expired:
                {
                    return myAuthResponse.auth_result;
                }
                case resept::AuthResult::Challenge:
                {
                    // prompt for the next password in multi-round password authentication or for the the next response for CR authentication
                    aSuppliedCreds = getSuppliedCredentials(myAuthReqs.cred_types,
                                                            myAuthReqs.hwsig_formula,
                                                            myAuthResponse.challenges,
                                                            myAuthResponse.response_names,
                                                            aOnPasswordPrompt,
                                                            aOnPincodePrompt,
                                                            aOnResponsePrompt);

                    myAuthResponse = anRcdpHandler.authenticate(service, aSuppliedCreds, myResolvedURIs, myCalculatedDigests);
                    continue;
                }
                default:
                {
                    TA_THROW_MSG(std::runtime_error, boost::format("Unsupported auth result received: %s") % str(myAuthResponse.auth_result.type));
                }
                }

            }// while (true)

            return myAuthResponse.auth_result;
        }

        bool changeExpiredPassword(rclient::RcdpHandler& anRcdpHandler, const resept::Credentials& aCreds, ReseptClientApp::OnChangePasswordPromptCb aOnChangePasswordPrompt)
        {
            const string myExpiredMsg = str(boost::format("Password for user %s, provider %s, service %s has expired and must be changed") % userid % provider % service);
            WARNLOG(myExpiredMsg);
            return changePassword(anRcdpHandler, myExpiredMsg, aCreds, true, aOnChangePasswordPrompt);
        }

        void handleErrError(const rclient::ErrError& e, OnErrorCb aOnError)
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
                    myUserMsg = resept::ProductName + " server error occurred. Please contact " + resept::ProductName + " support.";
                    ERRORLOG2(myUserMsg, boost::format("Ill-formed time-out-of-sync error, cannot parse time difference from '%s'") % e.description);
                    if (aOnError)
                    {
                        aOnError(myUserMsg, cookie);
                    }
                    break;
                }

                if (myTimeDiff > 0)
                {
                    myUserMsg = str(boost::format("Client time is %s ahead the server time.") % ta::TimeUtils::formatTimeInterval(myTimeDiff));
                }
                else
                {
                    myUserMsg = str(boost::format("Client time is %s behind the server time.") % ta::TimeUtils::formatTimeInterval(-myTimeDiff));
                }
                myUserMsg += " Please adjust your time settings or contact " + resept::ProductName + " support.";
                ERRORLOG(myUserMsg);
                if (aOnError)
                {
                    aOnError(myUserMsg, cookie);
                }
                break;
            }
            case resept::rcdp::ErrResolvedIpInvalid:
            {
                const string myUserMsg = "Resolved service URI differs on the client and on the server. Please contact " + resept::ProductName + " support.";
                ERRORLOG(myUserMsg);
                if (aOnError)
                {
                    aOnError(myUserMsg, cookie);
                }
                break;
            }
            case resept::rcdp::ErrDigestInvalid:
            {
                const string myUserMsg = "Calculated executable digest differs on the client and on the server. Please contact " + resept::ProductName + " support.";
                ERRORLOG(myUserMsg);
                if (aOnError)
                {
                    aOnError(myUserMsg, cookie);
                }
                break;
            }
            case resept::rcdp::ErrMaxLicensedUsersReached:
            {
                const string myUserMsg = "Authentication succeeded but the certificate cannot be supplied. Please contact " + resept::ProductName + " support.";
                ERRORLOG2(myUserMsg, "The maximum number of licensed users or transactions has been reached on the server.");
                if (aOnError)
                {
                    aOnError(myUserMsg, cookie);
                }
                break;
            }
            case resept::rcdp::ErrPasswordExpired:
            {
                const string myUserMsg = "Account password expired. Please contact " + resept::ProductName + " support.";
                ERRORLOG(myUserMsg);
                if (aOnError)
                {
                    aOnError(myUserMsg, cookie);
                }
                break;
            }
            default:
            {
                ERRORLOG2("Error requesting certificate.", boost::format("ERR received from the server. Code: %d, description: '%s'.") % e.errnum % e.description);
                if (aOnError)
                {
                    aOnError("Error requesting certificate. See log for more info.", cookie);
                }
                break;
            }
            }
        }

    private:

        // @throw ReseptClientAppInitError
        static void checkReseptCustomized()
        {
            try
            {
                if (rclient::Settings::isCustomized())
                {
                    return;
                }
            }
            catch (std::exception& e)
            {
                TA_THROW_MSG2(ReseptClientAppInitError,
                              "Error occurred checking whether " + resept::ProductName + " installation has been customized. Please contact " + resept::ProductName + " support.",
                              e.what());
            }
            TA_THROW_MSG2(ReseptClientAppInitError,
                          resept::ProductName + " Installation is not customized. Please run " + resept::ProductName + " Configuration Manager to customize " + resept::ProductName + ".",
                          "Installation is not customized");
        }

        // @throw ReseptClientAppInitError
        void initLogger()
        {
            try
            {
                loggerInitializer.reset(new rclient::LoggerInitializer());
            }
            catch (std::exception& e)
            {
                TA_THROW_MSG2(ReseptClientAppInitError,
                              "Failed to initialize logger. Please contact " + resept::ProductName + " support.",
                              e.what());
            }
        }

        // @throw ReseptClientAppInitError
        void initOpenSSL()
        {
            try
            {
                openSSLAppPtr.reset(new ta::OpenSSLApp());
            }
            catch (std::exception& e)
            {
                TA_THROW_MSG2(ReseptClientAppInitError,
                              "Failed to initialize crypto subsystem. Please contact  " + resept::ProductName + " support.",
                              e.what());
            }
        }

        // @return true for success, false if the user input provided in anOptions is incorrect. If the function return false anErrorStr contains user-friendly error message
        // @throw std::exception on the rest (non-user) errors
        bool init(const ReseptClientApp::Options& anOptions, std::string& anErrorStr)
        {
            // Init provider
            const vector<string> myProviders = rclient::Settings::getProviders();
            if (anOptions.provider_supplied)
            {
                if (!resept::isValidProviderName(anOptions.provider, anErrorStr))
                    return false;
                if (!ta::isElemExist(anOptions.provider, myProviders))
                {
                    anErrorStr = str(boost::format("The specified provider '%s' does not exist in %s settings. Please use another provider name.") % anOptions.provider % resept::ProductName);
                    return false;
                }
                provider = anOptions.provider;
            }
            else
            {
                if (myProviders.empty())
                {
                    anErrorStr = resept::ProductName + " settings does not contain providers";
                    return false;
                }
                if (myProviders.size() > 1)
                {
                    anErrorStr = str(boost::format("%s settings contain more than one provider. Please supply provider name.") % resept::ProductName);
                    return false;
                }
                provider = myProviders[0];
            }

            // Init service
            const vector<string> myServices = rclient::Settings::getServices(provider);
            if (anOptions.service_supplied)
            {
                if (!resept::isValidServiceName(anOptions.service, anErrorStr))
                    return false;
                if (!ta::isElemExist(anOptions.service, myServices))
                {
                    anErrorStr = str(boost::format("The specified service '%s' does not exist in %s settings for provider '%s'. Please use another service name.") % anOptions.service % resept::ProductName % provider);
                    return false;
                }
                service = anOptions.service;
            }
            else
            {
                if (myServices.empty())
                {
                    anErrorStr = str(boost::format("%s settings does not contain services for provider '%s'") % resept::ProductName % provider);
                    return false;
                }
                if (myServices.size() > 1)
                {
                    anErrorStr = str(boost::format("%s settings contain more than one service for provider '%s'. Please supply service name.") % resept::ProductName % provider);
                    return false;
                }
                service = myServices[0];
            }

            // Init user
            if (anOptions.userid_supplied)
            {
                if (!resept::isValidUserName(anOptions.userid, anErrorStr))
                    return false;
                userid = anOptions.userid;
            }
            else
            {
                const vector<string> myUsers = rclient::Settings::getUsers(provider, service);
                if (myUsers.empty())
                {
                    anErrorStr = str(boost::format("%s settings does not contain users for provider '%s' and service '%s'. Please supply user name.") % resept::ProductName % provider % service);
                    return false;
                }
                if (myUsers.size() > 1)
                {
                    anErrorStr = str(boost::format("%s settings contain more than one user for provider '%s' and service '%s'. Please supply user name.") % resept::ProductName % provider % service);
                    return false;
                }
                userid = myUsers[0];
            }

            // Validate supplied credentials
            if (anOptions.password_supplied && !resept::isValidPassword(anOptions.password, anErrorStr))
            {
                return false;
            }

            if (anOptions.new_password_supplied && !resept::isValidPassword(anOptions.new_password, anErrorStr))
            {
                return false;
            }

            if (anOptions.pincode_supplied && !resept::isValidPincode(anOptions.pincode, anErrorStr))
            {
                return false;
            }

            if (anOptions.cr_file_supplied && !ta::isFileExist(anOptions.cr_file))
            {
                anErrorStr = "CR file " + anOptions.cr_file + " does not exist";
                return false;
            }

            return true;
        }

        // Get credentials by prompting user for them
        // @throw MissingCredError
        resept::Credentials getSuppliedCredentials(
            const resept::CredentialTypes& aRequiredCredTypes,
            const string& aHwsigFormula,
            const ta::StringDict& aChallenges,
            const ta::StringArray& aResponseNames,
            ReseptClientApp::OnPasswordPromptCb aOnPasswordPrompt,
            ReseptClientApp::OnPincodePromptCb aOnPincodePrompt,
            ReseptClientApp::OnResponsePromptCb aOnResponsePrompt) const
        {
            try
            {
                resept::Credentials myRetVal;

                foreach (const resept::CredentialType& cred_type, aRequiredCredTypes)
                {
                    switch(cred_type)
                    {
                    case resept::credUserId:
                    {
                        myRetVal.push_back(resept::Credential(cred_type, userid));
                        break;
                    }
                    case resept::credHwSig:
                    {
                        string myParsedFormula;
                        const string myHwsig = resept::ComputerUuid::calcCs(aHwsigFormula, &myParsedFormula);
                        DEBUGLOG(boost::format("Calculated HWSIG: %s (parsed formula: %s)") % myHwsig % myParsedFormula);
                        myRetVal.push_back(resept::Credential(cred_type, myHwsig));
                        break;
                    }
                    case resept::credPasswd:
                    {
                        const resept::Credential myPasswd = getSuppliedPassword(aOnPasswordPrompt, aChallenges);
                        myRetVal.push_back(myPasswd);
                        break;
                    }
                    case resept::credPin:
                    {
                        const resept::Credential myPincode = getSuppliedPincode(aOnPincodePrompt);
                        myRetVal.push_back(myPincode);
                        break;
                    }
                    case resept::credResponse:
                    {
                        if (!aChallenges.empty())
                        {
                            const resept::Credential myResponses = getSuppliedResponses(aOnResponsePrompt, aChallenges, aResponseNames);
                            myRetVal.push_back(myResponses);
                        }
                        break;
                    }
                    default:
                    {
                        WARNLOG(boost::format("Unknown credential type %s") % str(cred_type));
                        break;
                    }
                    }
                }
                return myRetVal;
            }
            catch (MissingCredError& )
            {
                throw;
            }
            catch (std::exception& e)
            {
                TA_THROW_MSG(MissingCredError, e.what());
            }
            catch (...)
            {
                TA_THROW_MSG(MissingCredError, "Unknown errror occurred");
            }
        }

        resept::Credential getSuppliedPassword(ReseptClientApp::OnPasswordPromptCb aOnPasswordPrompt, const ta::StringDict& aChallenges) const
        {
            if (!aOnPasswordPrompt)
            {
                TA_THROW_MSG(MissingCredError, boost::format("Password is required for provider %s, service %s, user %s but no password prompt callback supplied") % provider % service % userid);
            }

            const string myPassword = aOnPasswordPrompt(aChallenges, userid, cookie);
            return resept::Credential(resept::credPasswd, myPassword);
        }

        resept::Credential getSuppliedPincode(ReseptClientApp::OnPincodePromptCb aOnPincodePrompt) const
        {
            if (!aOnPincodePrompt)
            {
                TA_THROW_MSG(MissingCredError, boost::format("Pincode is required for provider %s, service %s, user %s but no pincode prompt callback supplied") % provider % service % userid);
            }

            const string myPincode = aOnPincodePrompt(userid, cookie);
            return resept::Credential(resept::credPin, myPincode);
        }

        resept::Credential getSuppliedResponses(ReseptClientApp::OnResponsePromptCb aOnResponsePrompt, const ta::StringDict& aChallenges, const ta::StringArray& aResponseNames) const
        {
            if (aChallenges.empty())
            {
                TA_THROW_MSG(MissingCredError, boost::format("Response is required for provider %s, service %s, user %s but no challenges supplied by the server") % provider % service % userid);
            }
            if (aResponseNames.empty())
            {
                TA_THROW_MSG(MissingCredError, boost::format("Response is required for provider %s, service %s, user %s but no response names supplied by the server") % provider % service % userid);
            }

            if (!aOnResponsePrompt)
            {
                TA_THROW_MSG(MissingCredError, boost::format("Response is required for provider %s, service %s, user %s but no response prompt callback supplied") % provider % service % userid);
            }

            const ReseptClientApp::StringMap myResponses = aOnResponsePrompt(aChallenges, aResponseNames, userid, cookie);
            return resept::Credential(myResponses);
        }

        bool changeNotExpiredPassword(rclient::RcdpHandler& anRcdpHandler, const string&  aMsg, const resept::Credentials& aCreds, ReseptClientApp::OnChangePasswordPromptCb aOnChangePasswordPrompt)
        {
            return changePassword(anRcdpHandler, aMsg, aCreds, false, aOnChangePasswordPrompt);
        }

        // @return whether a new password is supplied and fill aNewPassword with a new password
        bool requestNewPassword(const string& aMsg, bool aReasonPasswordExpired, string& aNewPassword, ReseptClientApp::OnChangePasswordPromptCb aOnChangePasswordPrompt)
        {
            if (!aOnChangePasswordPrompt)
            {
                WARNLOG("Password change requested but no password changing callback supplied");
                return false;
            }
            return aOnChangePasswordPrompt(aMsg, userid, aReasonPasswordExpired, aNewPassword, cookie);
        }

        bool applyNewPassword(rclient::RcdpHandler& anRcdpHandler, const resept::Credentials& aCreds, const string& aNewPassword)
        {
            const string myCurrentPassword = getCredentialValue(aCreds, resept::credPasswd, "the supplied credentials");
            const rclient::AuthResponse pwdChangeResult = anRcdpHandler.changePassword(myCurrentPassword, aNewPassword);

            if (pwdChangeResult.auth_result.type == resept::AuthResult::Ok)
            {
                return true;
            }
            else if (pwdChangeResult.auth_result.type == resept::AuthResult::Delay)
            {
                TA_THROW_MSG(std::runtime_error, "Failed to change the password. \n" \
                             "Please check that you typed the passwords correctly. Please also make sure the password satisfies password policy.");
            }
            else if (pwdChangeResult.auth_result.type == resept::AuthResult::Locked)
            {
                TA_THROW_MSG(std::runtime_error, "This account is locked. Please contact " + resept::ProductName + " support.");
            }

            return false;
        }


        //@return whether the password has been successfully changed
        bool changePassword(rclient::RcdpHandler& anRcdpHandler, const string& aMsg, const resept::Credentials& aCreds, bool aReasonPasswordExpired, ReseptClientApp::OnChangePasswordPromptCb aOnChangePasswordPrompt)
        {
            string myNewPassword;
            if (requestNewPassword(aMsg, aReasonPasswordExpired, myNewPassword, aOnChangePasswordPrompt))
            {
                return applyNewPassword(anRcdpHandler, aCreds, myNewPassword);
            }
            else
            {
                INFOLOG("Do not change the password because no password is supplied by a user");
                return false;
            }
        }


    }; // ReseptClientAppImpl


    //
    // Public API
    //

    ReseptClientApp::ReseptClientApp(const Options& anOptions, void* aCookie)
        : pImpl(new ReseptClientAppImpl(anOptions, aCookie))
    {}

    ReseptClientApp::~ReseptClientApp()
    {
        FUNCLOG;
        delete pImpl;
    }

    ReseptClientApp::ExitCode ReseptClientApp::requestCertificate(
        OnPasswordPromptCb aOnPasswordPrompt,
        OnPincodePromptCb aOnPincodePrompt,
        OnResponsePromptCb aOnResponsePrompt,
        OnChangePasswordPromptCb aOnChangePasswordPrompt,
        OnUserMessagesCb aOnUserMessages,
        OnAuthenticationDelayedCb aOnAuthenticationDelayed,
        OnAuthenticationUserLockedCb aOnAuthenticationUserLocked,
        OnPfxCb aOnPfx,
        OnPemCb aOnPem,
        OnNotifyCb aOnNotify,
        OnErrorCb aOnError
    )
    {
        try
        {
            rclient::Settings::setLatestProviderService(pImpl->provider, pImpl->service);

            const ta::NetUtils::RemoteAddress mySvr = rclient::Settings::getReseptSvrAddress();
            rclient::NativeCertStore::deleteAllReseptUserCerts();

            DEBUGLOG(boost::format("Connecting to %s server at %s") % resept::ProductName % toString(mySvr));
            rclient::RcdpHandler myRcdpHandler(mySvr);

            while (true)
            {
                myRcdpHandler.hello();
                myRcdpHandler.handshake();

                resept::Credentials mySuppliedCreds;
                const resept::AuthResult myAuthResult = pImpl->authenticate(myRcdpHandler,
                                                        aOnPasswordPrompt,
                                                        aOnPincodePrompt,
                                                        aOnResponsePrompt,
                                                        aOnAuthenticationDelayed,
                                                        aOnAuthenticationUserLocked,
                                                        mySuppliedCreds);

                switch (myAuthResult.type)
                {
                case resept::AuthResult::Ok:
                {
                    break;
                }
                case resept::AuthResult::Locked:
                {
                    return exitUserLocked;
                }
                case resept::AuthResult::Delay:
                {
                    return exitAuthDelay;
                }
                case resept::AuthResult::Expired:
                {
                    if (pImpl->changeExpiredPassword(myRcdpHandler, mySuppliedCreds, aOnChangePasswordPrompt))
                    {
                        if (aOnNotify)
                        {
                            aOnNotify("Restarting after password change", pImpl->cookie);
                        }
                        continue;
                    }
                    else
                    {
                        return exitUserPasswdExpired;
                    }
                }
                default:
                {
                    TA_THROW_MSG(std::runtime_error, boost::format("Unsupported auth result received: %1%") % myAuthResult.type);
                }
                }

                pImpl->checkForNewMessages(myRcdpHandler, aOnUserMessages);

                const bool myPasswordChanged = pImpl->changeExpiringPassword(myRcdpHandler, myAuthResult, mySuppliedCreds, aOnChangePasswordPrompt);
                if (myPasswordChanged)
                {
                    if (aOnNotify)
                    {
                        aOnNotify("Restarting after password change", pImpl->cookie);
                    }
                    continue;
                }
                else
                {
                    break;
                }
            } // while true

            pImpl->requestCertificate(myRcdpHandler, aOnPfx, aOnPem);

            myRcdpHandler.eoc();
            return exitSuccess;
        }
        catch (rclient::RcdpVersionMismatchError& e)
        {
            const string myUserMsg = "Client/server version mismatch. Please contact " + resept::ProductName + " support.";
            ERRORLOG2(myUserMsg, e.what());
            if (aOnError)
            {
                aOnError(myUserMsg, pImpl->cookie);
            }
            return exitError;
        }
        catch (rclient::EocError& e)
        {
            const string myUserMsg = resept::ProductName + " server error occurred. Please contact " + resept::ProductName + " support.";
            ERRORLOG2(myUserMsg, boost::format("EOC received from the server. '%s'") % e.what());
            if (aOnError)
            {
                aOnError(myUserMsg, pImpl->cookie);
            }
            return exitError;
        }
        catch (rclient::ErrError& e)
        {
            pImpl->handleErrError(e, aOnError);
            return exitError;
        }
        catch (rclient::HttpRequestError& e)
        {
            const string myUserMsg = "Cannot connect to " + resept::ProductName + " server. Please contact your system or " + resept::ProductName + " support.";
            ERRORLOG2(myUserMsg, boost::format("Failed to send HTTP request to the RESEPT server. %s") % e.what());
            if (aOnError)
            {
                aOnError("Error requesting certificate. See log for more info.", pImpl->cookie);
            }
            return exitError;
        }
        catch (ta::IpResolveError& e)
        {
            ERRORLOG(boost::format("Failed to resolve IP. %s") % e.what());
            if (aOnError)
            {
                aOnError("IP address resolution failed. Please contact your system or " + resept::ProductName + " support.", pImpl->cookie);
            }
            return exitError;
        }
        catch (rclient::SettingsError& e)
        {
            const string myUserMsg = resept::ProductName + " installation is misconfigured. Please contact " + resept::ProductName + " support.";
            ERRORLOG2(myUserMsg, e.what());
            if (aOnError)
            {
                aOnError(myUserMsg, pImpl->cookie);
            }
            return exitError;
        }
        catch (rclient::NativeCertStoreDeleteError& e)
        {
            const string myUserMsg = "Failed to cleanup certificates. Please contact " + resept::ProductName + " support.";
            ERRORLOG2(myUserMsg, boost::format("Failed to delete user certificates from the system store. %s") % e.what());
            if (aOnError)
            {
                aOnError(myUserMsg, pImpl->cookie);
            }
            return exitError;
        }
        catch (rclient::NativeCertStoreValidateError& e)
        {
            const string myUserMsg = "Failed to validate certificates. Please contact " + resept::ProductName + " support.";
            ERRORLOG2(myUserMsg, boost::format("Failed to validate user certificates in the system store. %s") % e.what());
            if (aOnError)
            {
                aOnError(myUserMsg, pImpl->cookie);
            }
            return exitError;
        }
        catch (rclient::NativeCertStoreImportError& e)
        {
            const string myUserMsg = "Failed to import certificate. Please contact " + resept::ProductName + " support.";
            ERRORLOG2(myUserMsg, boost::format("Failed to import user certificates into the system store. %s") % e.what());
            if (aOnError)
            {
                aOnError(myUserMsg, pImpl->cookie);
            }
            return exitError;
        }
        catch (MissingCredError& e)
        {
            const string myUserMsg = "One or more credentials required by the server are not supplied by the client";
            ERRORLOG2(myUserMsg, e.what());
            if (aOnError)
            {
                aOnError(myUserMsg, pImpl->cookie);
            }
            return exitError;
        }
        catch (std::exception& e)
        {
            ERRORLOG2("Error requesting certificate.", e.what());
            if (aOnError)
            {
                aOnError("Error requesting certificate. See log for more info.", pImpl->cookie);
            }
            return exitError;
        }
        catch (...)
        {
            ERRORLOG("Error requesting certificate. Unknown error.");
            if (aOnError)
            {
                aOnError("Error requesting certificate. See log for more info.", pImpl->cookie);
            }
            return exitError;
        }

    } //requestCertificate



}





