#pragma once

// Test for RESEPT Client API (i.e. SDK)

#include "ReseptClientAppTestConfig.h"
#include "CrTestFile.h"
#include "rclient/ReseptClientApp.h"
#include "rclient/CRFile.h"
#include "rclient/Settings.h"
#include "rclient/NativeCertStore.h"
#include "rclient/Common.h"
#include "resept/util.h"
#include "ta/logger.h"
#include "ta/encodingutils.h"
#include "ta/thread.h"
#include "ta/certutils.h"
#include "ta/process.h"
#include "ta/utils.h"
#include "ta/timeutils.h"
#include "ta/hashutils.h"
#include "ta/scopedresource.hpp"
#include "ta/common.h"

#include <cxxtest/TestSuite.h>
#include "boost/cstdint.hpp"
#include "boost/regex.hpp"
#include "boost/assign/list_of.hpp"
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/convenience.hpp"
#include <string>
#include <vector>
#include <memory>
#include <iostream>

using std::string;
using std::vector;
using namespace ta;
using namespace resept;

class ReseptClientApiTest : public CxxTest::TestSuite
{
    enum SupplyUser
    {
        supplyUserYes, supplyUserNo
    };
    enum CredentialsPoisoning
    {
        credentialsPoisonNo, credentialsPoisonYes, credentialsPoisonNoResponse
    };
    static inline std::string toStr(const CredentialsPoisoning aVal)
    {
        switch (aVal)
        {
            case credentialsPoisonNo: return "no";
            case credentialsPoisonYes: return "yes";
            case credentialsPoisonNoResponse: return "yes (no response)";
            default: TA_THROW_MSG(std::invalid_argument, boost::format("Unsupported credential poisoning value %d") % aVal);
        }
    }

    // The idea is that we test:
    // 1. first changes AD password (go to pwdChangeStateChange state). Password change is initiated by AD
    // 2. then reset LDAP password back (go to pwdChangeStateReset state) as a cleanup measure
    // 3. and finally we indicate the test to stop (go to pwdChangeStateCancel state).
    // Without this state the test would enter an infinite loop (pwdChangeStateChange -> pwdChangeStateReset -> pwdChangeStateChange) because AD will keep asking you to change the password over and over again.
    enum LdapPasswordChangeState
    {
        pwdChange, pwdReset, pwdCancelChange
    };


    ReseptClientAppTestConfig theTestConfig;
    CrTestFile theValidCrFile;
    CrTestFile theInvalidCredsCrFile;
    CrTestFile theNoResponseCrFile;
    bool theOnUserMessagesCalled;
    bool theOnAuthenticationDelayedCalled;
    bool theOnAuthenticationUserLockedCalled;
    bool theOnErrorCalled;
    bool theOnPfxCalled;
    bool theOnPemCalled;
    const std::string theUserConfigFilePath;
    const std::string theMasterConfigFilePath;
    LdapPasswordChangeState theLdapPasswordChangeState;
    rclient::ReseptClientApp::Options theOptions;

public:
    static ReseptClientApiTest *createSuite()
    {
        return new ReseptClientApiTest();
    }

    static void destroySuite( ReseptClientApiTest *suite )
    {
        delete suite;
    }

    ReseptClientApiTest()
    :   theValidCrFile("CR-File")
       ,theInvalidCredsCrFile("CR-File.invalid_creds", CrTestFile::contentPoisonResponses)
       ,theNoResponseCrFile("CR-File.no_response", CrTestFile::contentRemoveResponses)
       ,theOnUserMessagesCalled(false)
       ,theOnAuthenticationDelayedCalled(false)
       ,theOnAuthenticationUserLockedCalled(false)
       ,theOnErrorCalled(false)
       ,theOnPfxCalled(false)
       ,theOnPemCalled(false)
       ,theUserConfigFilePath(rclient::Settings::getUserConfigPath())
       ,theMasterConfigFilePath(rclient::Settings::getMasterConfigPath())
       ,theLdapPasswordChangeState(pwdCancelChange)
    {
        backupSettings();
        CxxTest::setAbortTestOnFail(true);
    }

    void setUp()
    {
        restoreSettings();
        boost::filesystem::remove(theMasterConfigFilePath.c_str()); // so we can tweak user settings without been hindered by master settings
        theLdapPasswordChangeState = pwdCancelChange;
    }

    void tearDown()
    {
        restoreSettings();
    }



    //
    // Test cases
    //
    void test_that_pfx_certificate_is_received_when_valid_credentials_supplied()
    {
        const string provider = rclient::Settings::getLatestProvider();
        foreach (const string& service, rclient::Settings::getServices(provider))
        {
            if (theTestConfig.isServiceExist(service))
            {
                rclient::Settings::setCertFormat(provider, service, resept::certformatP12);

                if (rclient::Settings::getUsers(provider, service).size() == 1)
                {
                    // test that for single-user services userid does not need to be supplied
                    requestCertificateForAllUsers(provider, service, supplyUserNo, credentialsPoisonNo, resept::certformatP12);
                }
                else if (rclient::Settings::getUsers(provider, service).size() > 1)
                {
                   // test that for single-user services userid needs to be supplied
                    requestCertificateForAllUsers(provider, service, supplyUserYes, credentialsPoisonNo, resept::certformatP12);
                }
            }
        }
    }

    void test_that_pem_certificate_is_received_when_valid_credentials_supplied()
    {
        const string provider = rclient::Settings::getLatestProvider();
        foreach (const string& service, rclient::Settings::getServices(provider))
        {
            if (theTestConfig.isServiceExist(service))
            {
                rclient::Settings::setCertFormat(provider, service, resept::certformatPem);

                if (rclient::Settings::getUsers(provider, service).size() == 1)
                {
                    // test that for single-user services userid does not need to be supplied
                    requestCertificateForAllUsers(provider, service, supplyUserNo, credentialsPoisonNo, resept::certformatPem);
                }
                else if (rclient::Settings::getUsers(provider, service).size() > 1)
                {
                   // test that for single-user services userid needs to be supplied
                    requestCertificateForAllUsers(provider, service, supplyUserYes, credentialsPoisonNo, resept::certformatPem);
                }
            }
        }
    }

    void test_that_certificate_is_not_received_when_invalid_credentials_supplied()
    {
        const string provider = rclient::Settings::getLatestProvider();
        foreach (const string& service, rclient::Settings::getServices(provider))
        {
            if (theTestConfig.isServiceExist(service))
            {
                requestCertificateForAllUsers(provider, service, supplyUserYes, credentialsPoisonYes);

                // reauthenticate with valid credentials just to minimize ban time we receive next time testing with invalid creds again
                requestCertificateForAllUsers(provider, service, supplyUserYes, credentialsPoisonNo);
            }
        }
    }

    void test_that_invalid_provider_service_is_handled()
    {
        using namespace rclient;

        TS_ASSERT_THROWS(ReseptClientApp(makeAppOptions("", "", "")),
                         ReseptClientAppInitError);
        TS_ASSERT_THROWS(ReseptClientApp(makeAppOptions("non_existing_provider", "non_existing_service", "non-existing-user")),
                         ReseptClientAppInitError);
        TS_ASSERT_THROWS(ReseptClientApp(makeAppOptions(Settings::getLatestProvider(), "non_existing_service", "non-existing-user")),
                         ReseptClientAppInitError);
        TS_ASSERT_THROWS(ReseptClientApp(makeAppOptions(Settings::getLatestProvider(), Settings::getLatestService(), "")),
                         ReseptClientAppInitError);
    }

    void test_that_ambiguous_user_is_handled()
    {
        const string provider = rclient::Settings::getLatestProvider();
        foreach (const string& service, rclient::Settings::getServices(provider))
        {
            if (theTestConfig.isServiceExist(service))
            {
                if (rclient::Settings::getUsers(provider, service).size() > 1)
                {
                    TS_ASSERT_THROWS(requestCertificateForAllUsers(provider, service, supplyUserNo), rclient::ReseptClientAppInitError);
                }
            }
        }
    }

    void test_that_no_response_result_authentication_error()
    {
        TS_TRACE("--- Testing that client cannot request certificate when no response is supplied for CR authentication");

        const string provider = rclient::Settings::getLatestProvider();
        foreach (const string& service, rclient::Settings::getServices(provider))
        {
            if (theTestConfig.isServiceExist(service))
            {
                requestCertificateForAllUsers(provider, service, supplyUserYes, credentialsPoisonNoResponse);
            }
        }
    }

    void test_that_ldap_password_can_be_changed()
    {
        const string provider = rclient::Settings::getLatestProvider();
        const string service = "CUST_PASSWD_AD";
        const string user = "TestUser";

        if (!ta::isElemExist(user, rclient::Settings::getUsers(provider, service)))
        {
            TS_FAIL(user + " user not found in the settings file for provider " + provider + " and service " + service);
        }

        // the test will effectively change LDAP password and then reset it to the original one
        theLdapPasswordChangeState = pwdChange;
        requestCertificate(provider, service, user);
    }

private:

    //
    // test helpers
    //

    void resetCallbackResults()
    {
        theOnUserMessagesCalled = false;
        rclient::Settings::setLastUserMsgUtc(ta::TimeUtils::timestampToIso8601(0)); // to request all user messages from the server so we can test onUserMessages() callback properly

        theOnAuthenticationDelayedCalled = false;
        theOnAuthenticationUserLockedCalled = false;
        theOnErrorCalled = false;

        theOnPfxCalled = false;
        theOnPemCalled = false;
    }

    void backupSettings()
    {
        boost::filesystem::copy_file(theUserConfigFilePath, theUserConfigFilePath+".bak", boost::filesystem::copy_option::overwrite_if_exists);
        boost::filesystem::copy_file(theMasterConfigFilePath, theMasterConfigFilePath+".bak", boost::filesystem::copy_option::overwrite_if_exists);
    }

    void restoreSettings()
    {
        boost::filesystem::copy_file(theUserConfigFilePath+".bak", theUserConfigFilePath, boost::filesystem::copy_option::overwrite_if_exists);
        boost::filesystem::copy_file(theMasterConfigFilePath+".bak", theMasterConfigFilePath, boost::filesystem::copy_option::overwrite_if_exists);
    }

    void verifyAuthSucceeded(const string& aProviderName, const string& aServiceName, const string& aUserId,
                           CredentialsPoisoning aCredentialsPoisoning, resept::CertFormat aCertFormat, bool anIsUserLockedOnSvr)
    {
        if (anIsUserLockedOnSvr)
        {
            TS_FAIL(str(boost::format("------------ Received authentication success for provider %s, service %s, user %s however \"user locked\" result expected.") % aProviderName % aServiceName % aUserId));
            return;
        }

        if (aCredentialsPoisoning == credentialsPoisonNo)
        {
            TS_ASSERT(theOnUserMessagesCalled);
            if (aCertFormat == resept::certformatP12)
            {
                const unsigned int myNumValidCerts = rclient::NativeCertStore::validateReseptUserCert();
                if (myNumValidCerts > 0)
                {
                    TS_ASSERT(theOnPfxCalled);
                    TS_ASSERT(!theOnPemCalled);
                }
                else
                {
                    TS_FAIL(str(boost::format("------------ Failed to receive certificate for provider %s, service %s, user %s") % aProviderName % aServiceName % aUserId));
                }
            }
            else if (aCertFormat == resept::certformatPem)
            {
                TS_ASSERT(theOnPemCalled);
                TS_ASSERT(!theOnPfxCalled);
            }
        }
        else if (aCredentialsPoisoning == credentialsPoisonNoResponse)
        {
            if (theTestConfig.isCrFileRequired(aServiceName, aUserId))
            {
                TS_FAIL(str(boost::format("------------ Certificate for provider %s, service %s, user %s has been received while no response it supplied") % aProviderName % aServiceName % aUserId));
            }
            else
            {
                TS_ASSERT(theOnUserMessagesCalled);
                if (aCertFormat == resept::certformatP12)
                {
                    const unsigned int myNumValidCerts = rclient::NativeCertStore::validateReseptUserCert();
                    if (myNumValidCerts > 0)
                    {
                        TS_ASSERT(theOnPfxCalled);
                        TS_ASSERT(!theOnPemCalled);
                    }
                    else
                    {
                        TS_FAIL(str(boost::format("------------ Failed to receive certificate for provider %s, service %s, user %s") % aProviderName % aServiceName % aUserId));
                    }
                }
                else if (aCertFormat == resept::certformatPem)
                {
                    TS_ASSERT(theOnPemCalled);
                    TS_ASSERT(!theOnPfxCalled);
                }
            }
        }
        else
        {
            TS_FAIL(str(boost::format("------------ Failed to receive certificate for provider %s, service %s, user %s") % aProviderName % aServiceName % aUserId));
        }
    }

    void verifyAuthError(const string& aProviderName, const string& aServiceName, const string& aUserId, CredentialsPoisoning aCredentialsPoisoning)
    {
        if ( theTestConfig.isCrFileRequired(aServiceName, aUserId) && aCredentialsPoisoning == credentialsPoisonNoResponse)
        {
            TS_ASSERT(theOnErrorCalled);
            TS_ASSERT(!theOnUserMessagesCalled);
            TS_ASSERT(!theOnPemCalled);
            TS_ASSERT(!theOnPfxCalled);
            DEBUGLOG(boost::format("------------ Authentication correctly failed to receive certificate for provider %s, service %s, user %s because no response supplied") % aProviderName % aServiceName % aUserId);
        }
        else
        {
            TS_FAIL(str(boost::format("------------ Failed to receive certificate for provider %s, service %s, user %s. Error received from the server") % aProviderName % aServiceName % aUserId));
        }
    }

    void verifyAuthUserLocked(const string& aProviderName, const string& aServiceName, const string& aUserId, bool anIsUserLockedOnSvr)
   {
        if (anIsUserLockedOnSvr)
        {
            TS_ASSERT(theOnAuthenticationUserLockedCalled);
            TS_ASSERT(!theOnUserMessagesCalled);
            TS_ASSERT(!theOnPemCalled);
            TS_ASSERT(!theOnPfxCalled);
            DEBUGLOG(boost::format("------------ Authentication correctly failed for provider %s, service %s, user %s because the user is locked") % aProviderName % aServiceName % aUserId);
        }
        else
        {
            TS_FAIL(str(boost::format("------------ Failed to receive certificate for provider %s, service %s, user %s. User is locked.") % aProviderName % aServiceName % aUserId));
        }
   }

   void verifyAuthDelayed(const string& aProviderName, const string& aServiceName, const string& aUserId,
                           CredentialsPoisoning aCredentialsPoisoning, bool anIsUserLockedOnSvr)
    {
        if (anIsUserLockedOnSvr)
        {
            TS_FAIL(str(boost::format("------------ Received authentication failed for provider %s, service %s, user %s however \"user locked\" result expected.") % aProviderName % aServiceName % aUserId));
            return;
        }

        if (aCredentialsPoisoning == credentialsPoisonNo)
        {
            TS_FAIL(str(boost::format("------------ Failed to receive certificate for provider %s, service %s, user %s. User is locked because of invalid credentials.") % aProviderName % aServiceName % aUserId));
            return;
        }

        TS_ASSERT(theOnAuthenticationDelayedCalled);
        TS_ASSERT(!theOnUserMessagesCalled);
        TS_ASSERT(!theOnPemCalled);
        TS_ASSERT(!theOnPfxCalled);
    }


    rclient::ReseptClientApp::Options makeAppOptions(const string& aProviderName,
                                                    const string& aServiceName,
                                                    const string& aUserId,
                                                    const SupplyUser aSupplyUser = supplyUserYes,
                                                    const CredentialsPoisoning aCredentialsPoisoning = credentialsPoisonNo)
    {
        rclient::ReseptClientApp::Options myOptions;

        myOptions.setProvider(aProviderName);
        myOptions.setService(aServiceName);

        if (aSupplyUser == supplyUserYes)
        {
            //@note we do not test with invalid userid because we will always get max possible ban duration which will slow down our tests
            myOptions.setUserid(aUserId);
        }

        //
        // prepare the remaining credentials
        //
        if (aCredentialsPoisoning == credentialsPoisonNo || aCredentialsPoisoning == credentialsPoisonNoResponse)
        {
            if (theTestConfig.isPasswordExist(aServiceName, aUserId))
            {
                myOptions.setPassword(theTestConfig.getPassword(aServiceName, aUserId));
            }
            if (theTestConfig.isPincodeExist(aServiceName, aUserId))
            {
                myOptions.setPincode(theTestConfig.getPincode(aServiceName, aUserId));
            }
            if (theTestConfig.isCrFileRequired(aServiceName, aUserId))
            {
                if (aCredentialsPoisoning == credentialsPoisonNoResponse)
                {
                    myOptions.setCrFile(theNoResponseCrFile.filename());
                }
                else
                {
                    myOptions.setCrFile(theValidCrFile.filename());
                }
            }
        }
        else
        {
            if (theTestConfig.isPasswordExist(aServiceName, aUserId))
            {
                myOptions.setPassword(theTestConfig.getPassword(aServiceName, aUserId) + "_invalid");
            }
            if (theTestConfig.isPincodeExist(aServiceName, aUserId))
            {
                myOptions.setPincode(theTestConfig.getPincode(aServiceName, aUserId) + "_invalid");
            }
            if (theTestConfig.isCrFileRequired(aServiceName, aUserId))
            {
                myOptions.setCrFile(theInvalidCredsCrFile.filename());
            }
        }

        return myOptions;
    }

    void requestCertificateForAllUsers(const string& aProviderName,
                                      const string& aServiceName,
                                      SupplyUser aSupplyUser = supplyUserYes,
                                      CredentialsPoisoning aCredentialsPoisoning = credentialsPoisonNo,
                                      resept::CertFormat aCertFormat = resept::certformatP12)
    {
        foreach (const string& user, rclient::Settings::getUsers(aProviderName, aServiceName))
        {
            requestCertificate(aProviderName, aServiceName, user, aSupplyUser, aCredentialsPoisoning, aCertFormat);
        }
    }

    void requestCertificate(const string& aProviderName,
                            const string& aServiceName,
                            const string& aUserName,
                            const SupplyUser aSupplyUser = supplyUserYes,
                            const CredentialsPoisoning aCredentialsPoisoning = credentialsPoisonNo,
                            const resept::CertFormat aCertFormat = resept::certformatP12)
    {
        //
        // given
        //
        if (!theTestConfig.isUserExist(aServiceName, aUserName))
        {
            return;
        }


        if (aCredentialsPoisoning == credentialsPoisonYes)
        {
            if (!theTestConfig.isPasswordExist(aServiceName, aUserName) || !theTestConfig.isPincodeExist(aServiceName, aUserName))
            {
                // we are asked to test with invalid credentials but test configuration lacks necessary creds to poison, skip this test
                return;
            }
        }

        theOptions = makeAppOptions(aProviderName, aServiceName, aUserName,  aSupplyUser, aCredentialsPoisoning);

        TS_TRACE(str(boost::format("------------ Requesting %s certificate for provider %s, service %s, user %s. User supplied: %s. Credentials poisoned: %s") % str(aCertFormat) % aProviderName % aServiceName % aUserName % (aSupplyUser==supplyUserYes?"yes":"no") % toStr(aCredentialsPoisoning)).c_str());

        const bool myIsUserLockedOnTheSvr = theTestConfig.isUserLocked(aServiceName, aUserName);

        resetCallbackResults();

        //
        // when
        //
        rclient::ReseptClientApp myApp(theOptions, this);
        const rclient::ReseptClientApp::ExitCode myExitCode = myApp.requestCertificate(
                                    onPasswordPrompt,
                                    onPincodePrompt,
                                    onResponsePrompt,
                                    onChangePasswordPrompt,
                                    onUserMessages,
                                    onAuthenticationDelayed,
                                    onAuthenticationUserLocked,
                                    onPfx,
                                    onPem,
                                    onNotify,
                                    onError
                                    );

        //
        // then
        //
        switch (myExitCode)
        {
            case rclient::ReseptClientApp::exitSuccess:
            {
                verifyAuthSucceeded(aProviderName, aServiceName, aUserName, aCredentialsPoisoning, aCertFormat, myIsUserLockedOnTheSvr);
                break;
            }
            case rclient::ReseptClientApp::exitError:
            {
                verifyAuthError(aProviderName, aServiceName, aUserName, aCredentialsPoisoning);
                break;
            }
            case rclient::ReseptClientApp::exitUserLocked:
            {
                verifyAuthUserLocked(aProviderName, aServiceName, aUserName, myIsUserLockedOnTheSvr);
                break;
            }
            case rclient::ReseptClientApp::exitAuthDelay:
            {
                verifyAuthDelayed(aProviderName, aServiceName, aUserName, aCredentialsPoisoning, myIsUserLockedOnTheSvr);
                break;
            }
            default:
            {
                TS_FAIL(str(boost::format("------------ Failed to receive certificate for provider %s, service %s, user %s. Unsupported exit code %d received") % aProviderName % aServiceName % aUserName % myExitCode));
                break;
            }
        }
    }

    // strip leading and trailing whitespace and replace internal adjucent whitespace with a single space character
    static string stripWs(const string& aStr)
    {
        static const boost::regex ex("\\s+");
        return boost::regex_replace(boost::trim_copy(aStr), ex, " ");
    }


    //
    // implementation of callbacks
    //

    static std::string onPasswordPrompt(const rclient::ReseptClientApp::StringMap& aChallenges, const std::string& aUserId, void* aCookie)
    {
        TS_ASSERT(aCookie);

        string myPassword;

        if (aCookie)
        {
            ReseptClientApiTest* self = (ReseptClientApiTest*)aCookie;

            if (boost::starts_with(aUserId, "Securid"))
            {
                // take the password from test CR file for multi-phase RADIUS SecurID password authentication

                rclient::CRFile myCRFile(self->theOptions.cr_file);

                if (aChallenges.empty()) // initially password challenges list is empty
                {
                    const rclient::ReseptClientApp::StringMap myFilter = boost::assign::map_list_of(rclient::crfile::UserKey, aUserId);
                    myPassword = myCRFile.getKey(rclient::crfile::InitialTokenKey, myFilter );
                }
                else
                {
                    const rclient::ReseptClientApp::StringMap myFilter = boost::assign::map_list_of(aChallenges.begin()->first, stripWs(aChallenges.begin()->second));
                    myPassword = myCRFile.getResponse(rclient::crfile::ResponseKey, aUserId, myFilter );
                }
            }
            else
            {
                // fallback to getting password from test config used for simple single-phase password authentication when password known in advance
                TS_ASSERT(self->theTestConfig.isPasswordExist(self->theOptions.service, aUserId))
                myPassword = self->theTestConfig.getPassword(self->theOptions.service, aUserId);
            }
        }
        return myPassword;
    }

    static std::string onPincodePrompt(const std::string& aUserId, void* aCookie)
    {
        TS_ASSERT(aCookie);

        string myPincode;

        if (aCookie)
        {
            ReseptClientApiTest* self = (ReseptClientApiTest*)aCookie;
            TS_ASSERT(self->theTestConfig.isPincodeExist(self->theOptions.service, aUserId))
            myPincode = self->theTestConfig.getPincode(self->theOptions.service, aUserId);
        }
        return myPincode;
    }

    static rclient::ReseptClientApp::StringMap onResponsePrompt(const rclient::ReseptClientApp::StringMap& aChallenges, const vector<string>& aResponseNames, const string& aUserId, void* aCookie)
    {
        TS_ASSERT(aCookie);

        rclient::ReseptClientApp::StringMap myResponses;

        if (aCookie)
        {
            ReseptClientApiTest* self = (ReseptClientApiTest*)aCookie;

            TS_ASSERT(!aChallenges.empty());
            TS_ASSERT(!aResponseNames.empty());

            rclient::CRFile myCRFile(self->theOptions.cr_file);

            foreach (const string& responseName, aResponseNames)
            {
                rclient::ReseptClientApp::StringMap myFilter;
                foreach (const rclient::ReseptClientApp::StringMap::value_type& challNameVal, aChallenges)
                {
                    myFilter [challNameVal.first] = stripWs(challNameVal.second);
                }
                myResponses[responseName] = myCRFile.getResponse(responseName, aUserId, myFilter );
            }
        }
        return myResponses;
    }

    static bool onChangePasswordPrompt(const std::string& UNUSED(aMsg), const string& aUserId, bool UNUSED(aReasonPasswordExpired), std::string& aNewPassword, void* aCookie)
    {
        TS_ASSERT(aCookie);
        bool myPasswordSupplied = false;

        if (aCookie)
        {
            ReseptClientApiTest* self = (ReseptClientApiTest*)aCookie;

            switch (self->theLdapPasswordChangeState)
            {
                case pwdChange:
                {
                    const string myOldPassword = self->theTestConfig.getPassword(self->theOptions.service, aUserId);
                    const string myNewPassword = self->theTestConfig.getNewPassword(self->theOptions.service, aUserId);

                    aNewPassword = myNewPassword;

                    // swap old and and new passwords
                    self->theTestConfig.setPassword(self->theOptions.service, aUserId, myNewPassword);
                    self->theTestConfig.setNewPassword(self->theOptions.service, aUserId, myOldPassword);

                    self->theLdapPasswordChangeState = pwdReset; // next state

                    TS_TRACE("Changing password to: " + aNewPassword);
                    myPasswordSupplied = true;
                    break;
                }
                case pwdReset:
                {
                    const string myOldPassword = self->theTestConfig.getNewPassword(self->theOptions.service, aUserId);
                    const string myNewPassword = self->theTestConfig.getPassword(self->theOptions.service, aUserId);

                    aNewPassword = myOldPassword;

                    // swap old and and new passwords
                    self->theTestConfig.setPassword(self->theOptions.service, aUserId, myOldPassword);
                    self->theTestConfig.setNewPassword(self->theOptions.service, aUserId, myNewPassword);

                    self->theLdapPasswordChangeState = pwdCancelChange; // next state is cancel password change to avoid in

                    TS_TRACE("Resetting password to: " + aNewPassword);
                    myPasswordSupplied = true;
                    break;
                }
                case pwdCancelChange:
                default:
                {
                    myPasswordSupplied = false;
                    break;
                }
            }//switch
        }

        return myPasswordSupplied;
    }



    static void onUserMessages(const std::vector<rclient::ReseptClientApp::UserMessage>& aMessages, void* aCookie)
    {
        TS_ASSERT(aCookie);
        if (aCookie)
        {
            ReseptClientApiTest* self = (ReseptClientApiTest*)aCookie;
            self->theOnUserMessagesCalled = true;
            TS_ASSERT(!aMessages.empty());
        }
    }

    static void onAuthenticationDelayed(size_t UNUSED(aDelaySecs), void* aCookie)
    {
        TS_ASSERT(aCookie);
        if (aCookie)
        {
            ReseptClientApiTest* self = (ReseptClientApiTest*)aCookie;
            self->theOnAuthenticationDelayedCalled = true;
        }
    }

    static void onAuthenticationUserLocked(void* aCookie)
    {
        TS_ASSERT(aCookie);
        if (aCookie)
        {
            ReseptClientApiTest* self = (ReseptClientApiTest*)aCookie;
            self->theOnAuthenticationUserLockedCalled = true;
        }
    }

    static void onPfx(const std::vector<unsigned char>& aPfx, const std::string& aPassword, void* aCookie)
    {
        TS_ASSERT(aCookie);
        if (aCookie)
        {
            ReseptClientApiTest* self = (ReseptClientApiTest*)aCookie;
            self->theOnPfxCalled = true;
            TS_ASSERT_THROWS_NOTHING(ta::CertUtils::parsePfx(aPfx, aPassword));
        }
    }

    static void onPem(const std::vector<unsigned char>& aCert, const std::string& aPassword, void* aCookie)
    {
        TS_ASSERT(aCookie);
        if (aCookie)
        {
            ReseptClientApiTest* self = (ReseptClientApiTest*)aCookie;
            self->theOnPemCalled = true;
            TS_ASSERT(!aPassword.empty());
            TS_ASSERT(ta::CertUtils::hasPemCert(aCert));
            TS_ASSERT(ta::CertUtils::hasPemPrivKey(aCert));
        }
    }

    static void onNotify(const std::string& aMsg, void* UNUSED(aCookie))
    {
        DEBUGLOG(aMsg);
    }

    static void onError(const string& UNUSED(anError), void* aCookie)
    {
        TS_ASSERT(aCookie);
        if (aCookie)
        {
            ReseptClientApiTest* self = (ReseptClientApiTest*)aCookie;
            self->theOnErrorCalled = true;
        }
    }

};
