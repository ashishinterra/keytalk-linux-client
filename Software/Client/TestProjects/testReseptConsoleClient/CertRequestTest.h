#pragma once

#include "ReseptConsoleClientCommon.h"
#include "ReseptClientAppTestConfig.h"
#include "CrTestFile.h"
#include "rclient/CRFile.h"
#include "rclient/Settings.h"
#include "rclient/NativeCertStore.h"
#include "rclient/Common.h"
#include "ta/logger.h"
#include "ta/encodingutils.h"
#include "ta/timeutils.h"
#include "ta/process.h"
#include "ta/strings.h"
#include "ta/utils.h"
#include "ta/hashutils.h"
#include "resept/util.h"

#include <cxxtest/TestSuite.h>
#include "boost/cstdint.hpp"
#include "boost/assign/list_of.hpp"
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <fstream>

using std::string;
using std::vector;
using namespace ta;
using namespace resept;

class CertRequestTest : public CxxTest::TestSuite
{
    enum SavePfxToDisk
    {
        savePfxToDiskYes, savePfxToDiskNo
    };
    enum SupplyUser
    {
        supplyUserYes, supplyUserNo
    };
    enum CredentialsPoisoning
    {
        credentialsPoisonNo, credentialsPoisonYes, credentialsPoisonNoResponse
    };

    ReseptClientAppTestConfig theTestConfig;
    CrTestFile theValidCrFile;
    CrTestFile theInvalidCredsCrFile;
    CrTestFile theNoResponseCrFile;

public:
    static CertRequestTest *createSuite()
    {
        return new CertRequestTest();
    }

    static void destroySuite( CertRequestTest *suite )
    {
        delete suite;
    }

    CertRequestTest()
    :   theValidCrFile("CR-File"),
        theInvalidCredsCrFile("CR-File.invalid_creds", CrTestFile::contentPoisonResponses),
        theNoResponseCrFile("CR-File.no_response", CrTestFile::contentRemoveResponses)
    {}

    //
    // Test cases
    //
    void test_that_certificate_is_received_when_valid_credentials_supplied()
    {
        const string myProvider = rclient::Settings::getLatestProvider();

        foreach (const string& service, rclient::Settings::getServices(myProvider))
        {
            if (theTestConfig.isServiceExist(service))
            {
                requestCertificate(myProvider, service, savePfxToDiskNo, supplyUserYes, credentialsPoisonNo);
                requestCertificate(myProvider, service, savePfxToDiskYes, supplyUserYes, credentialsPoisonNo);
                if (rclient::Settings::getUsers(myProvider, service).size() == 1)
                {
                    // Test that for single-user services userid cmdline argument can be omitted
                    requestCertificate(myProvider, service, savePfxToDiskNo, supplyUserNo, credentialsPoisonNo);
                    requestCertificate(myProvider, service, savePfxToDiskYes, supplyUserNo, credentialsPoisonNo);
                }
            }
        }
    }

    void test_that_certificate_is_not_received_when_invalid_credentials_supplied()
    {
        const string myProvider = rclient::Settings::getLatestProvider();

        foreach (const string& service, rclient::Settings::getServices(myProvider))
        {
            if (!theTestConfig.isServiceExist(service))
                continue;
            requestCertificate(myProvider, service, savePfxToDiskYes, supplyUserYes, credentialsPoisonYes);

            // reauthenticate with valid credentials just to minimize ban time we receive next time testing with invalid creds again
            requestCertificate(myProvider, service, savePfxToDiskYes, supplyUserYes, credentialsPoisonNo);
        }
    }

    void test_that_invalid_usage_is_handled()
    {
        if (rclient::Settings::getProviders().size() > 1 || rclient::Settings::getServices().size() > 1)
        {
            TS_TRACE("--- Testing that console client cannot request certificate when provider/service is ambiguous");
            int myDelay;
            TS_ASSERT(exec(rclient::ReseptConsoleClient, ta::StringArray(), myDelay) == rclient::ReseptClientApp::exitError);
        }
    }

    void test_that_no_response_result_authentication_error()
    {
        TS_TRACE("--- Testing that console client cannot request certificate when no response is supplied for CR authentication");
        const string myProvider = rclient::Settings::getLatestProvider();

        foreach (const string& service, rclient::Settings::getServices(myProvider))
        {
            if (!theTestConfig.isServiceExist(service))
                continue;
            requestCertificate(myProvider, service, savePfxToDiskYes, supplyUserYes, credentialsPoisonNoResponse);
        }
    }

    void test_that_non_existing_cr_file_is_handled()
    {
        TS_TRACE("--- Test that console client handles non-existent CR file");

        const string provider = rclient::Settings::getLatestProvider();
        const string service = "CUST_PASSWD_RADIUS";
        string user;
        if (!findUserWithCrFileRequired(provider, service, user))
        {
            TS_FAIL("Test setup error. No user found with CR file required for provider " + provider + " and service " + service);
        }

        const ta::StringArray myOptions = boost::assign::list_of (makeArgOpt(rclient::ProviderOpt, provider))
                                                               (makeArgOpt(rclient::ServiceOpt, service))
                                                               (makeArgOpt(rclient::UserOpt, user))
                                                               (makeArgOpt(rclient::CrFileOpt, "non-existing-cr-file" ));

        int myDelay;
        TS_ASSERT_EQUALS(exec(rclient::ReseptConsoleClient, myOptions, myDelay), rclient::ReseptClientApp::exitError);
    }

    void test_that_interactive_cr_is_handled_linux()
    {
#ifdef _WIN32
        TS_SKIP("This test is not implemented on Windows");
#else
        TS_TRACE("--- Testing that console client uses input for challenge response");

        const string myProvider = rclient::Settings::getLatestProvider();
        const string service = "CUST_CR_INTERNAL";
        const string testUser = "DemoUser";

        if (!ta::isElemExist(testUser, rclient::Settings::getUsers(myProvider, service)))
        {
            TS_FAIL(testUser + " user not found in the settings file for provider " + myProvider + " and service " + service);
        }

        // make options
        ta::StringArray myOptions = boost::assign::list_of ((std::string)"$HOME")
                                                                (service)
                                                                (testUser);
        int myDelay;
        TS_ASSERT_EQUALS( exec("testKtclient.exp", myOptions, myDelay), rclient::ReseptClientApp::exitSuccess);

#endif
    }

    void test_that_interactive_radius_new_pin_is_handled_linux()
    {
#ifdef _WIN32
    TS_SKIP("This test is not implemented on Windows");
#else
    TS_TRACE("--- Testing that console client uses input for challenge response");

    const string myProvider = rclient::Settings::getLatestProvider();
    const string service  = "CUST_PASSWD_RADIUS";
    const string testUser = "SecuridNewUserPinUser";

    if (!ta::isElemExist(testUser, rclient::Settings::getUsers(myProvider, service)))
    {
        TS_FAIL(testUser + " user not found in the settings file for provider " + myProvider + " and service " + service);
    }

    // make options
    ta::StringArray myOptions = boost::assign::list_of ((std::string)"$HOME")
                                                           (service)
                                                           (testUser);

    int myDelay;
    TS_ASSERT_EQUALS( exec("testSecureIdNewPin.exp", myOptions, myDelay), rclient::ReseptClientApp::exitSuccess);

#endif
    }

    void test_that_interactive_radius_new_system_pin_is_handled_linux()
    {
#ifdef _WIN32
    TS_SKIP("This test is not implemented on Windows");
#else
    TS_TRACE("--- Testing that console client uses input for challenge response");

    const string myProvider = rclient::Settings::getLatestProvider();
    const string service  = "CUST_PASSWD_RADIUS";
    const string testUser = "SecuridNewSystemPinUser";

    if (!ta::isElemExist(testUser, rclient::Settings::getUsers(myProvider, service)))
    {
        TS_FAIL(testUser + " user not found in the settings file for provider " + myProvider + " and service " + service);
    }

     // make options
    ta::StringArray myOptions = boost::assign::list_of ((std::string)"$HOME")
                                                           (service)
                                                           (testUser);

    int myDelay;
    TS_ASSERT_EQUALS( exec("testSecureIdNewPin.exp", myOptions, myDelay), rclient::ReseptClientApp::exitSuccess);

#endif
    }

    void test_that_otp_is_handled()
    {
         TS_TRACE("--- Testing that console client can handle OTP");

         const string myProvider = rclient::Settings::getLatestProvider();
         const string service  = "CUST_PASSWD_RADIUS";
         const string testUser = "OtpDemoUser";

         if (!ta::isElemExist(testUser, rclient::Settings::getUsers(myProvider, service)))
         {
             TS_FAIL(testUser + " user not found in the settings file for provider " + myProvider + " and service " + service);
         }

         // calculate password
         const std::string password = resept::calcOtp();

         // make options
         ta::StringArray myOptions = boost::assign::list_of (makeArgOpt(rclient::ProviderOpt, myProvider))
                                                                (makeArgOpt(rclient::ServiceOpt, service))
                                                                (makeArgOpt(rclient::UserOpt, testUser))
                                                                (makeArgOpt(rclient::PasswordOpt, password));

         int myDelay;
         TS_ASSERT_EQUALS(exec(rclient::ReseptConsoleClient, myOptions, myDelay), rclient::ReseptClientApp::exitSuccess);

     }

	void test_ldap_change_password_non_interactively()
	{
		TS_TRACE("--- Testing that console client can handle LDAP password change non-interactively");

		// given
		const string provider = rclient::Settings::getLatestProvider();
		const string service  = "CUST_PASSWD_AD";
		const string user = "TestUser";

		if (!ta::isElemExist(user, rclient::Settings::getUsers(provider, service)))
		{
			TS_FAIL(user + " user not found in the settings file for provider " + provider + " and service " + service);
		}

		const string password = theTestConfig.getPassword(service, user);
		const string new_password = theTestConfig.getNewPassword(service, user);

		ta::StringArray myOptions = boost::assign::list_of (makeArgOpt(rclient::ProviderOpt, provider))
			(makeArgOpt(rclient::ServiceOpt, service))
			(makeArgOpt(rclient::UserOpt, user))
			(makeArgOpt(rclient::PasswordOpt, password))
			(makeArgOpt(rclient::NewPasswordOpt, new_password));

		int myDelay;

		// when: change to a new password
		rclient::ReseptClientApp::ExitCode myExitCode = exec(rclient::ReseptConsoleClient, myOptions, myDelay);

		// then it succeeds
		TS_ASSERT_EQUALS(myExitCode, rclient::ReseptClientApp::exitSuccess);

		// given
		myOptions = boost::assign::list_of (makeArgOpt(rclient::ProviderOpt, provider))
			(makeArgOpt(rclient::ServiceOpt, service))
			(makeArgOpt(rclient::UserOpt, user))
			(makeArgOpt(rclient::PasswordOpt, new_password))
			(makeArgOpt(rclient::NewPasswordOpt, password));

		// when: change to back to an old password
		myExitCode = exec(rclient::ReseptConsoleClient, myOptions, myDelay);

		// then it succeeds
		TS_ASSERT_EQUALS(myExitCode, rclient::ReseptClientApp::exitSuccess);
	}


    void test_ldap_change_password_interactively()
     {
#ifdef _WIN32
    TS_SKIP("This test is not implemented on Windows");
#else
         TS_TRACE("--- Testing that console client can handle LDAP password change interactively");

		 // given
		 const string provider = rclient::Settings::getLatestProvider();
		 const string service  = "CUST_PASSWD_AD";
		 const string user = "TestUser";

		if (!ta::isElemExist(user, rclient::Settings::getUsers(provider, service)))
		{
			TS_FAIL(user + " user not found in the settings file for provider " + provider + " and service " + service);
		}

		 ta::StringArray myOptions = boost::assign::list_of ((std::string)"$HOME")
			 (service)
			 (user);

		 int myDelay;

		 // when
		 rclient::ReseptClientApp::ExitCode myExitCode = exec("testLdapChangePasswd.exp", myOptions, myDelay);

		 // then
		 TS_ASSERT_EQUALS(myExitCode, rclient::ReseptClientApp::exitSuccess);
#endif
     }


private:
    bool findUserWithCrFileRequired(const string& aProvider, const string& aService, string& aUserId)
    {
        foreach (const string& user,  rclient::Settings::getUsers(aProvider, aService))
        {
            if (!theTestConfig.isUserExist(aService, user))
            {
                continue;
            }
            if (theTestConfig.isCrFileRequired(aService, user))
            {
                aUserId = user;
                return true;
            }
        }
        return false;
    }

    string b64(const string& aValue)
    {
        return ta::EncodingUtils::toBase64(ta::str2Vec<unsigned char>(aValue),true);
    }
    ta::StringArray makeCmdLineOptions(const string& aProviderName, const string& aServiceName,  const string& aUserId,
                                           SavePfxToDisk aSavePfxToDisk, SupplyUser aSupplyUser, CredentialsPoisoning aCredentialsPoisoning)
    {
        ta::StringArray myOptions = boost::assign::list_of (makeArgOpt(rclient::ProviderOpt, aProviderName))
                                                           (makeArgOpt(rclient::ServiceOpt, aServiceName));

        if (aSupplyUser == supplyUserYes)
        {
            //@note we do not test with invalid userid because we will always get max possible ban duration which will slow down our tests
            myOptions.push_back(makeArgOpt(rclient::B64UserOpt, b64(aUserId)));
        }

        //
        // prepare the remaining credentials
        //
        if (aCredentialsPoisoning == credentialsPoisonNo || aCredentialsPoisoning == credentialsPoisonNoResponse)
        {
            if (theTestConfig.isPasswordExist(aServiceName, aUserId))
            {
                const string myPassword = theTestConfig.getPassword(aServiceName, aUserId);
                myOptions.push_back(makeArgOpt(rclient::B64PasswordOpt, b64(myPassword)));
            }
            if (theTestConfig.isPincodeExist(aServiceName, aUserId))
            {
                const string myPincode = theTestConfig.getPincode(aServiceName, aUserId);
                myOptions.push_back(makeArgOpt(rclient::B64PincodeOpt, b64(myPincode)));
            }
            if (theTestConfig.isCrFileRequired(aServiceName, aUserId))
            {
                if (aCredentialsPoisoning == credentialsPoisonNoResponse)
                {
                    myOptions.push_back( makeArgOpt(rclient::CrFileOpt, theNoResponseCrFile.filename()) );
                }
                else
                {
                    myOptions.push_back( makeArgOpt(rclient::CrFileOpt, theValidCrFile.filename()) );
                }
            }
        }
        else
        {
            if (theTestConfig.isPasswordExist(aServiceName, aUserId))
            {
                const string myPassword = theTestConfig.getPassword(aServiceName, aUserId) + "_invalid";
                myOptions.push_back(makeArgOpt(rclient::B64PasswordOpt, b64(myPassword)));
            }
            if (theTestConfig.isPincodeExist(aServiceName, aUserId))
            {
                const string myPincode = theTestConfig.getPincode(aServiceName, aUserId) + "_invalid";
                myOptions.push_back(makeArgOpt(rclient::B64PincodeOpt, b64(myPincode)));
            }
            if (theTestConfig.isCrFileRequired(aServiceName, aUserId))
            {
                myOptions.push_back( makeArgOpt(rclient::CrFileOpt, theInvalidCredsCrFile.filename()) );
            }
        }

        if (aSavePfxToDisk == savePfxToDiskYes)
        {
            myOptions.push_back(makeNonArgOpt(rclient::SavePfxOpt));
        }

        return myOptions;
    }

    void verifyAuthSucceeded(const string& aProviderName, const string& aServiceName, const string& aUserId,
                           SavePfxToDisk aSavePfxToDisk, CredentialsPoisoning aCredentialsPoisoning,
                           bool anIsUserLockedOnSvr, const string& aPfxDir)
    {
        if (anIsUserLockedOnSvr)
        {
            ERRORLOG(boost::format("------------ Received authentication success for provider %s, service %s, user %s however \"user locked\" result expected.") % aProviderName % aServiceName % aUserId);
            TS_ASSERT(false);
            return;
        }

        if (aCredentialsPoisoning == credentialsPoisonNo)
        {
            if (rclient::NativeCertStore::validateReseptUserCert() > 0)
            {
                const bool myPfxSaved = ta::isFileExist(aPfxDir+rclient::PfxFileName);
                const bool myPfxPassSaved = ta::isFileExist(aPfxDir+rclient::PfxPassFileName);

                if ((aSavePfxToDisk == savePfxToDiskYes && myPfxSaved && myPfxPassSaved) || (aSavePfxToDisk == savePfxToDiskNo && (!myPfxSaved && !myPfxPassSaved)))
                {
                    DEBUGLOG(boost::format("------------ Successfully received certificate for provider %s, service %s, user %s") % aProviderName % aServiceName % aUserId);
                }
                else if (aSavePfxToDisk == savePfxToDiskYes && (!myPfxSaved || !myPfxPassSaved))
                {
                    TS_FAIL(str(boost::format("------------ Certificate for provider %s, service %s, user %s has been received but not saved") % aProviderName % aServiceName % aUserId));
                }
                else if (aSavePfxToDisk == savePfxToDiskNo && (myPfxSaved || myPfxPassSaved))
                {
                    TS_FAIL(str(boost::format("------------ Certificate for provider %s, service %s, user %s has been received and saved while saving was not requested") % aProviderName % aServiceName % aUserId));
                }
            }
            else
            {
                TS_FAIL(str(boost::format("------------ Failed to receive certificate for provider %s, service %s, user %s") % aProviderName % aServiceName % aUserId));
            }
        }
        else if (aCredentialsPoisoning == credentialsPoisonNoResponse)
        {
            if (rclient::NativeCertStore::validateReseptUserCert() > 0)
            {
                if (theTestConfig.isCrFileRequired(aServiceName, aUserId))
                {
                    TS_FAIL(str(boost::format("------------ Certificate for provider %s, service %s, user %s has been received while no response it supplied") % aProviderName % aServiceName % aUserId));
                }
                else
                {
                    const bool myPfxSaved = ta::isFileExist(aPfxDir+rclient::PfxFileName);
                    const bool myPfxPassSaved = ta::isFileExist(aPfxDir+rclient::PfxPassFileName);

                    if ((aSavePfxToDisk == savePfxToDiskYes && myPfxSaved && myPfxPassSaved) || (aSavePfxToDisk == savePfxToDiskNo && (!myPfxSaved && !myPfxPassSaved)))
                    {
                        DEBUGLOG(boost::format("------------ Successfully received certificate for provider %s, service %s, user %s") % aProviderName % aServiceName % aUserId);
                    }
                    else if (aSavePfxToDisk == savePfxToDiskYes && (!myPfxSaved || !myPfxPassSaved))
                    {
                        TS_FAIL(str(boost::format("------------ Certificate for provider %s, service %s, user %s has been received but not saved") % aProviderName % aServiceName % aUserId));
                    }
                    else if (aSavePfxToDisk == savePfxToDiskNo && (myPfxSaved || myPfxPassSaved))
                    {
                        TS_FAIL(str(boost::format("------------ Certificate for provider %s, service %s, user %s has been received and saved while saving was not requested") % aProviderName % aServiceName % aUserId));
                    }
                }
            }
            else
            {
                TS_FAIL(str(boost::format("------------ Failed to receive certificate for provider %s, service %s, user %s") % aProviderName % aServiceName % aUserId));
            }
        }
        else
        {
                TS_FAIL(str(boost::format("------------ Received certificate for provider %s, service %s and invalid user %s") % aProviderName % aServiceName % aUserId));
        }
    }

    void verifyAuthError(const string& aProviderName, const string& aServiceName, const string& aUserId, CredentialsPoisoning aCredentialsPoisoning)
    {
        if ( theTestConfig.isCrFileRequired(aServiceName, aUserId) && aCredentialsPoisoning == credentialsPoisonNoResponse)
        {
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
            DEBUGLOG(boost::format("------------ Authentication correctly failed for provider %s, service %s, user %s because the user is locked") % aProviderName % aServiceName % aUserId);
        }
        else
        {
            TS_FAIL(str(boost::format("------------ Failed to receive certificate for provider %s, service %s, user %s. User is locked.") % aProviderName % aServiceName % aUserId));
        }
   }

   void verifyAuthDelayed(const string& aProviderName, const string& aServiceName, const string& aUserId,
                           CredentialsPoisoning aCredentialsPoisoning,
                           bool anIsUserLockedOnSvr, int aDelaySeconds, const string& aPfxDir)
    {
        if (anIsUserLockedOnSvr)
        {
            TS_FAIL(str(boost::format("------------ Received authentication failed for provider %s, service %s, user %s however \"user locked\" result expected.") % aProviderName % aServiceName % aUserId));
            return;
        }

        if (aCredentialsPoisoning == credentialsPoisonNo)
        {
            TS_FAIL(str(boost::format("------------ Failed to receive certificate for provider %s, service %s, user %s. User is banned for %d seconds because of invalid credentials.") % aProviderName % aServiceName % aUserId % aDelaySeconds));
            return;
        }

        const bool myPfxSaved = ta::isFileExist(aPfxDir+rclient::PfxFileName);
        const bool myPfxPassSaved = ta::isFileExist(aPfxDir+rclient::PfxPassFileName);

        if (myPfxSaved || myPfxPassSaved)
        {
            TS_FAIL(str(boost::format("------------ Authentication correctly failed for provider %s, service %s, user %s, however the cert is received?!") % aProviderName % aServiceName % aUserId));
            return;
        }

        DEBUGLOG(boost::format("------------ Authentication correctly failed for provider %s, service %s, user %s. Delaying for at least %d seconds") % aProviderName % aServiceName % aUserId % aDelaySeconds);
        const int myWaitBeforeProceedMsec = 1000*(aDelaySeconds+1);
        ta::TimeUtils::sleep(myWaitBeforeProceedMsec);
    }

    void requestCertificate(const string& aProviderName, const string& aServiceName,
                           SavePfxToDisk aSavePfxToDisk, SupplyUser aSupplyUser,
                           CredentialsPoisoning aCredentialsPoisoning)
    {
        const string myPfxDir = ta::Process::getTempDir();

        foreach (string user,  rclient::Settings::getUsers(aProviderName, aServiceName))
        {
            //
            // given
            //
            if (!theTestConfig.isUserExist(aServiceName, user))
            {
                continue;
            }
            if (aCredentialsPoisoning == credentialsPoisonYes)
            {
                if (!theTestConfig.isPasswordExist(aServiceName, user) || !theTestConfig.isPincodeExist(aServiceName, user))
                {
                    // we are asked to test with invalid credentials but test configuration lacks necessary creds we shall poison, skip this test
                    continue;
                }
            }

            const ta::StringArray myCmdLineOptions = makeCmdLineOptions(aProviderName, aServiceName, user, aSavePfxToDisk, aSupplyUser, aCredentialsPoisoning);

            remove((myPfxDir + rclient::PfxFileName).c_str());
            remove((myPfxDir + rclient::PfxPassFileName).c_str());

            TS_TRACE(str(boost::format("------------ Requesting certificate for provider %s, service %s, user %s, save pfx: %s. User supplied: %s") % aProviderName % aServiceName % user % (aSavePfxToDisk==savePfxToDiskYes?"yes":"no") % (aSupplyUser==supplyUserYes?"yes":"no")).c_str());

            int myDelaySeconds = 0;
            const bool myIsUserLockedOnTheSvr = theTestConfig.isUserLocked(aServiceName, user);

            //
            // when
            //
            const rclient::ReseptClientApp::ExitCode myExitCode = exec(rclient::ReseptConsoleClient, myCmdLineOptions, myDelaySeconds);

            //
            // then
            //
            switch (myExitCode)
            {
                case rclient::ReseptClientApp::exitSuccess:
                {
                    verifyAuthSucceeded(aProviderName, aServiceName, user, aSavePfxToDisk, aCredentialsPoisoning, myIsUserLockedOnTheSvr, myPfxDir);
                    break;
                }
                case rclient::ReseptClientApp::exitError:
                {
                    verifyAuthError(aProviderName, aServiceName, user, aCredentialsPoisoning);
                    break;
                }
                case rclient::ReseptClientApp::exitUserLocked:
                {
                    verifyAuthUserLocked(aProviderName, aServiceName, user, myIsUserLockedOnTheSvr);
                    break;
                }
                case rclient::ReseptClientApp::exitAuthDelay:
                {
                    verifyAuthDelayed(aProviderName, aServiceName, user, aCredentialsPoisoning, myIsUserLockedOnTheSvr, myDelaySeconds, myPfxDir);
                    break;
                }
                default:
                {
                    TS_FAIL(str(boost::format("------------ Failed to receive certificate for provider %s, service %s, user %s. Unsupported exit code %d received") % aProviderName % aServiceName % user % myExitCode));
                    break;
                }
            }
        }
    }

    static string makeArgOpt(const string& anOpt, const string& anArg)
    {
        return str(boost::format("--%s %s") % anOpt % anArg);
    }

    static string makeNonArgOpt(const string& anOpt)
    {
        return str(boost::format("--%s") % anOpt);
    }

    rclient::ReseptClientApp::ExitCode exec(const string& aProgramName, const ta::StringArray& anOptions, int& aDelaySeconds)
    {
        const string myCmd = "." + ta::getDirSep() + aProgramName + " " + ta::Strings::join(anOptions, " ");
        DEBUGLOG("Executing " + myCmd);
        string myStdout, myStderr;

        const int myExitCode = ta::Process::shellExecSync(myCmd, myStdout, myStderr);
        TS_TRACE(str(boost::format("Command \"%s\" finished with code %d. Stdout:\n%s\n%s") % myCmd % myExitCode % myStdout % (!myStderr.empty() ? "Stderr:\n" + myStderr + "\n" : "")).c_str());

        rclient::ReseptClientApp::ExitCode myRetVal;
        if (!rclient::parseExitCode(myExitCode, myStderr, myRetVal, aDelaySeconds))
        {
            TA_THROW_MSG(std::runtime_error, boost::format("Unsupported exit code %d") % myExitCode);
        }

        return myRetVal;
    }
};
