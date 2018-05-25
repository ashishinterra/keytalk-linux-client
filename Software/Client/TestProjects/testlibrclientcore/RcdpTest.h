#pragma once

#include "rclient/RcdpHandler.h"
#include "rclient/RcdpRequest.h"
#include "rclient/Settings.h"
#include "rclient/Common.h"
#include "resept/util.h"
#include "ta/timeutils.h"
#include "ta/process.h"
#include "ta/timeutils.h"
#include "ta/hashutils.h"
#include "ta/certutils.h"
#include "ta/netutils.h"
#include "ta/scopedresource.hpp"
#include "ta/url.h"
#include "ta/dnsutils.h"
#include "ta/utils.h"
#include "ta/common.h"

#include "cxxtest/TestSuite.h"
#include "boost/assign/list_of.hpp"
#include "boost/range/algorithm.hpp"
#include <string>

//
// Base class for RCDP tests
//
class RcdpTestBase : public CxxTest::TestSuite
{
protected:

    void setUp()
    {
        CxxTest::setAbortTestOnFail(true);
        theSvr = rclient::Settings::getReseptSvrAddress();
    }

    void awaitFor(const unsigned int anAuthDelaySecs)
    {
        ta::TimeUtils::sleep((anAuthDelaySecs * ta::TimeUtils::MsecsInSecond) + 100);
    }

    std::vector<ta::version::Version> getClientSupportedRcdpVersions()
    {
        std::vector<ta::version::Version> myVersions;
        foreach (const std::string& v, resept::rcdpv2::ClientSupportedRcdpVersions)
        {
            myVersions.push_back(ta::version::parse(v));
        }
        return myVersions;
    }

    ta::version::Version getClientHighestSupportedRcdpVersion()
    {
        return *boost::max_element(getClientSupportedRcdpVersions());
    }

    ta::version::Version getServerHighestSupportedRcdpVersion()
    {
        std::vector<ta::version::Version> myVersions;
        foreach (const std::string& v, resept::rcdpv2::ServerSupportdRcdpVersions)
        {
            myVersions.push_back(ta::version::parse(v));
        }
        return *boost::max_element(myVersions);
    }

    static ta::StringArrayDict resolveServiceURIs(const ta::StringArray& anUris)
    {
        ta::StringArrayDict myResolvedUris;

        foreach (const std::string& uri, anUris)
        {
            const ta::url::Parts myParts = ta::url::parse(uri);
            std::vector<std::string> myIps;
            foreach (const ta::NetUtils::IP& ip, ta::DnsUtils::resolveIpsByName(myParts.authority_parts.host))
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
            myResolvedUris[uri] = myIps;
        }
        return myResolvedUris;
    }

    static string makeNativeFilePath(const string& aFileUri)
    {
        string myNativePath = ta::url::makeNativePath(aFileUri);
#ifdef __linux__ // quick hack to translate Windows file path to Linux path
        myNativePath = boost::replace_all_copy(myNativePath, "%windir%", "./");
#endif
        myNativePath = ta::Process::expandEnvVars(myNativePath);
        return myNativePath;
    }

    static ta::StringDict calcServiceUriDigests(const ta::StringArray& anUris)
    {
        ta::StringDict myCalculatedDigests;

        foreach (const std::string& uri, anUris)
        {
            myCalculatedDigests[uri] = ta::HashUtils::getSha256HexFile(makeNativeFilePath(uri));
        }
        return myCalculatedDigests;
    }

protected:
    ta::NetUtils::RemoteAddress theSvr;
};
// RcdpTestBase


//
// Generic RCDP test suite
//
class RcdpGenericTest : public RcdpTestBase
{
public:

    void test_eoc()
    {
        using namespace resept::rcdpv2;
        using std::string;

        rclient::RcdpHandler myRcdp(theSvr);

        // given
        TS_ASSERT(!myRcdp.userSessionData().sid_exist);
        TS_ASSERT(myRcdp.userSessionData().sid.empty());
        TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, resept::rcdpv2::stateClosed);

        // when
        myRcdp.eoc();
        // then
        TS_ASSERT(!myRcdp.userSessionData().sid_exist);
        TS_ASSERT(myRcdp.userSessionData().sid.empty());
        TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateClosed);
    }

    void test_thandshake()
    {
        using namespace resept::rcdpv2;
        using std::string;

        rclient::RcdpHandler myRcdp(theSvr);

        // when-then
        TS_ASSERT_THROWS(myRcdp.handshake(), std::exception);

        // when
        myRcdp.hello();
        // then
        TS_ASSERT(myRcdp.userSessionData().sid_exist);
        TS_ASSERT(!myRcdp.userSessionData().sid.empty());
        TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, resept::rcdpv2::stateHello);

        // when
        myRcdp.handshake();
        // then
        TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, resept::rcdpv2::stateConnected);

        // when
        myRcdp.eoc();
        // then
        TS_ASSERT(!myRcdp.userSessionData().sid_exist);
        TS_ASSERT(myRcdp.userSessionData().sid.empty());
        TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, resept::rcdpv2::stateClosed);

        // when-then
        TS_ASSERT_THROWS(myRcdp.handshake(), std::exception);
    }

    void test_error_towards_server()
    {
        using namespace resept::rcdpv2;
        using std::string;

        // given
        rclient::RcdpHandler myRcdp(theSvr);

        myRcdp.hello();
        myRcdp.handshake();
        TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, resept::rcdpv2::stateConnected);

        // when, we have not defined client's error codes yet, so just make one up
        myRcdp.error(99999, "error, I have hangover");
        // then, server doesn't know how to deal with this error, so it should end communication
        TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, resept::rcdpv2::stateClosed);

    }

    void test_password_authentication_with_url_resolution()
    {
        using namespace resept::rcdpv2;
        using resept::Credential;
        using boost::assign::list_of;
        using std::string;

        const string myService = "CUST_PASSWD_INTERNAL_WEB";
        const string myGoodUserId = "DemoUser";
        const string myGoodHwsig = "123456";
        const string myGoodPasswd = "secret";

        for (int auth_attempt = 1; auth_attempt <= 2; ++auth_attempt)
        {
            // given
            rclient::RcdpHandler myRcdp(theSvr);

            myRcdp.hello();
            myRcdp.handshake();

            // when
            const rclient::AuthRequirements myAuthReqs = myRcdp.getAuthRequirements(myService);

            // then
            TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateConnected);
            TS_ASSERT(ta::equalIgnoreOrder(myAuthReqs.cred_types, list_of(resept::credUserId)(resept::credHwSig)(resept::credPasswd)));
            TS_ASSERT(!myAuthReqs.hwsig_formula.empty());
            TS_ASSERT_EQUALS(myAuthReqs.password_prompt, "Password");
            TS_ASSERT_EQUALS(myAuthReqs.service_uris, list_of("http://www.keytalk.com"));
            TS_ASSERT(myAuthReqs.resolve_service_uris);
            TS_ASSERT(!myAuthReqs.calc_service_uris_digest);

            // given
            TS_TRACE("Authenticating with invalid credentials");
            resept::Credentials myCreds = list_of(Credential(resept::credUserId, myGoodUserId))
                                                  (Credential(resept::credHwSig, auth_attempt == 1 ? myGoodHwsig + ".invalid" : myGoodHwsig))
                                                  (Credential(resept::credPasswd, auth_attempt == 2 ? myGoodPasswd + ".invalid" : myGoodPasswd));
            ta::StringArrayDict myResolvedURIs = resolveServiceURIs(myAuthReqs.service_uris);
            // when
            rclient::AuthResponse myAuthResult = myRcdp.authenticate(myService, myCreds, myResolvedURIs);
            // then
            TS_ASSERT_EQUALS(myAuthResult.auth_result.type, resept::AuthResult::Delay);
            TS_ASSERT(myAuthResult.auth_result.delay > 0);
            TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateConnected);
            TS_TRACE(str(boost::format("-- We are locked for %u seconds, wait...") % myAuthResult.auth_result.delay).c_str());
            awaitFor(myAuthResult.auth_result.delay);

            // given
            TS_TRACE("Authenticating with good credentials");
            myCreds = list_of(Credential(resept::credUserId, myGoodUserId))
                               (Credential(resept::credHwSig, myGoodHwsig))
                               (Credential(resept::credPasswd, myGoodPasswd));
             myResolvedURIs = resolveServiceURIs(myAuthReqs.service_uris);
             // when
             myAuthResult = myRcdp.authenticate(myService, myCreds, myResolvedURIs);
            // then
            TS_ASSERT_EQUALS(myAuthResult.auth_result.type, resept::AuthResult::Ok);
            TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateAuthenticated);
        } // for
    }

    void test_authentication_with_invalid_resolved_url()
    {
        using namespace resept::rcdpv2;
        using resept::Credential;
        using boost::assign::list_of;
        using std::string;

        const string myService = "CUST_PASSWD_INTERNAL_WEB";
        const string myGoodUserId = "DemoUser";
        const string myGoodHwsig = "123456";
        const string myGoodPasswd = "secret";

        // given
        rclient::RcdpHandler myRcdp(theSvr);

        myRcdp.hello();
        myRcdp.handshake();

        // when
        const rclient::AuthRequirements myAuthReqs = myRcdp.getAuthRequirements(myService);

        // then
        TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateConnected);
        TS_ASSERT(ta::equalIgnoreOrder(myAuthReqs.cred_types, list_of(resept::credUserId)(resept::credHwSig)(resept::credPasswd)));
        TS_ASSERT(!myAuthReqs.hwsig_formula.empty());
        TS_ASSERT_EQUALS(myAuthReqs.password_prompt, "Password");
        TS_ASSERT_EQUALS(myAuthReqs.service_uris, list_of("http://www.keytalk.com"));
        TS_ASSERT(myAuthReqs.resolve_service_uris);
        TS_ASSERT(!myAuthReqs.calc_service_uris_digest);

        // given
        TS_TRACE("Authenticating with invalid resolved URL");
        resept::Credentials myCreds = list_of(Credential(resept::credUserId, myGoodUserId))
                                              (Credential(resept::credHwSig, myGoodHwsig))
                                              (Credential(resept::credPasswd, myGoodPasswd));
        try
        {
            // when
            const rclient::AuthResponse myAuthResult = myRcdp.authenticate(myService, myCreds);
            TS_FAIL("Error expected to occur authenticating with invalid resolved URLs but got auth result " + str(myAuthResult.auth_result.type) + " instead");
        }
        catch (const rclient::ErrError& e)
        {
            // then
            TS_ASSERT_EQUALS(e.errnum, resept::rcdp::ErrResolvedIpInvalid);
            TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateClosed);
        }
    }

    void test_password_authentication_with_digest_check()
    {
        using namespace resept::rcdpv2;
        using resept::Credential;
        using boost::assign::list_of;
        using std::string;

        const string myService = "CUST_PASSWD_INTERNAL_FILE";
        const string myGoodUserId = "DemoUser";
        const string myGoodHwsig = "123456";
        const string myGoodPasswd = "secret";

        for (int auth_attempt = 1; auth_attempt <= 2; ++auth_attempt)
        {
            // given
            rclient::RcdpHandler myRcdp(theSvr);

            myRcdp.hello();
            myRcdp.handshake();

            // when
            const rclient::AuthRequirements myAuthReqs = myRcdp.getAuthRequirements(myService);

            // then
            TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateConnected);
            TS_ASSERT(ta::equalIgnoreOrder(myAuthReqs.cred_types, list_of(resept::credUserId)(resept::credHwSig)(resept::credPasswd)));
            TS_ASSERT(!myAuthReqs.hwsig_formula.empty());
            TS_ASSERT_EQUALS(myAuthReqs.password_prompt, "Password");
            TS_ASSERT_EQUALS(myAuthReqs.service_uris, list_of("file://%windir%\\winhlp32.exe"));
            TS_ASSERT(!myAuthReqs.resolve_service_uris);
            TS_ASSERT(myAuthReqs.calc_service_uris_digest);

            // given
            TS_TRACE("Authenticating with invalid credentials");
            resept::Credentials myCreds = list_of(Credential(resept::credUserId, myGoodUserId))
                                                  (Credential(resept::credHwSig, auth_attempt == 1 ? myGoodHwsig + ".invalid" : myGoodHwsig))
                                                  (Credential(resept::credPasswd, auth_attempt == 2 ? myGoodPasswd + ".invalid" : myGoodPasswd));
            ta::StringDict myCalculatedDigests = calcServiceUriDigests(myAuthReqs.service_uris);
            // when
            rclient::AuthResponse myAuthResult = myRcdp.authenticate(myService, myCreds, ta::StringArrayDict(), myCalculatedDigests);
            // then
            TS_ASSERT_EQUALS(myAuthResult.auth_result.type, resept::AuthResult::Delay);
            TS_ASSERT(myAuthResult.auth_result.delay > 0);
            TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateConnected);
            TS_TRACE(str(boost::format("-- We are locked for %u seconds, wait...") % myAuthResult.auth_result.delay).c_str());
            awaitFor(myAuthResult.auth_result.delay);

            // given
            TS_TRACE("Authenticating with good credentials");
            myCreds = list_of(Credential(resept::credUserId, myGoodUserId))
                               (Credential(resept::credHwSig, myGoodHwsig))
                               (Credential(resept::credPasswd, myGoodPasswd));
             myCalculatedDigests = calcServiceUriDigests(myAuthReqs.service_uris);
             // when
             myAuthResult = myRcdp.authenticate(myService, myCreds, ta::StringArrayDict(), myCalculatedDigests);
            // then
            TS_ASSERT_EQUALS(myAuthResult.auth_result.type, resept::AuthResult::Ok);
            TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateAuthenticated);
        } // for
    }

    void test_password_authentication_with_invalid_digest()
    {
        using namespace resept::rcdpv2;
        using resept::Credential;
        using boost::assign::list_of;
        using std::string;

        const string myService = "CUST_PASSWD_INTERNAL_FILE";
        const string myGoodUserId = "DemoUser";
        const string myGoodHwsig = "123456";
        const string myGoodPasswd = "secret";

        // given
        rclient::RcdpHandler myRcdp(theSvr);

        myRcdp.hello();
        myRcdp.handshake();

        // when
        const rclient::AuthRequirements myAuthReqs = myRcdp.getAuthRequirements(myService);

        // then
        TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateConnected);
        TS_ASSERT(ta::equalIgnoreOrder(myAuthReqs.cred_types, list_of(resept::credUserId)(resept::credHwSig)(resept::credPasswd)));
        TS_ASSERT(!myAuthReqs.hwsig_formula.empty());
        TS_ASSERT_EQUALS(myAuthReqs.password_prompt, "Password");
        TS_ASSERT_EQUALS(myAuthReqs.service_uris, list_of("file://%windir%\\winhlp32.exe"));
        TS_ASSERT(!myAuthReqs.resolve_service_uris);
        TS_ASSERT(myAuthReqs.calc_service_uris_digest);

        // given
        TS_TRACE("Authenticating with invalid digest");
        resept::Credentials myCreds = list_of(Credential(resept::credUserId, myGoodUserId))
                                              (Credential(resept::credHwSig, myGoodHwsig))
                                              (Credential(resept::credPasswd, myGoodPasswd));
        try
        {
            // when
            const rclient::AuthResponse myAuthResult = myRcdp.authenticate(myService, myCreds);
            TS_FAIL("Error expected to occur authenticating with invalid digest but got auth result " + str(myAuthResult.auth_result.type) + " instead");
        }
        catch (const rclient::ErrError& e)
        {
            // then
            TS_ASSERT_EQUALS(e.errnum, resept::rcdp::ErrDigestInvalid);
            TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateClosed);
        }
    }

    void test_authentication_against_locked_user()
    {
        using namespace resept::rcdpv2;
        using resept::Credential;
        using boost::assign::list_of;
        using std::string;

        const string myService = "CUST_PASSWD_INTERNAL_WEB";
        const string myUserId = "DemoUser3";
        const string myGoodHwsig = "123456";
        const string myGoodPasswd = "secret";

        // given
        rclient::RcdpHandler myRcdp(theSvr);

        myRcdp.hello();
        myRcdp.handshake();

        // when
        const rclient::AuthRequirements myAuthReqs = myRcdp.getAuthRequirements(myService);

        // then
        TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateConnected);
        TS_ASSERT(ta::equalIgnoreOrder(myAuthReqs.cred_types, list_of(resept::credUserId)(resept::credHwSig)(resept::credPasswd)));
        TS_ASSERT(!myAuthReqs.hwsig_formula.empty());
        TS_ASSERT_EQUALS(myAuthReqs.password_prompt, "Password");
        TS_ASSERT_EQUALS(myAuthReqs.service_uris, list_of("http://www.keytalk.com"));
        TS_ASSERT(myAuthReqs.resolve_service_uris);
        TS_ASSERT(!myAuthReqs.calc_service_uris_digest);

        // given, prepare valid creds for locked user
        const resept::Credentials myCreds = list_of(Credential(resept::credUserId, myUserId))
                                                  (Credential(resept::credHwSig, myGoodHwsig))
                                                  (Credential(resept::credPasswd, myGoodPasswd));
        const ta::StringArrayDict myResolvedURIs = resolveServiceURIs(myAuthReqs.service_uris);
        const ta::StringDict myCalculatedDigests;
        // when
        rclient::AuthResponse myAuthResult = myRcdp.authenticate(myService, myCreds, myResolvedURIs, myCalculatedDigests);
        // then
        TS_ASSERT_EQUALS(myAuthResult.auth_result.type, resept::AuthResult::Locked);
        TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateClosed);
    }

    void test_otp_authentication()
    {
        using namespace resept::rcdpv2;
        using resept::Credential;
        using boost::assign::list_of;
        using std::string;

        const string myService = "CUST_PASSWD_RADIUS";
        const string myGoodUserId = "OtpDemoUser";
        const string myGoodHwsig = "123456";

        rclient::RcdpHandler myRcdp(theSvr);

        myRcdp.hello();
        myRcdp.handshake();

        // when
        const rclient::AuthRequirements myAuthReqs = myRcdp.getAuthRequirements(myService);

        // then
        TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateConnected);
        TS_ASSERT(ta::equalIgnoreOrder(myAuthReqs.cred_types, list_of(resept::credUserId)(resept::credHwSig)(resept::credPasswd)));
        TS_ASSERT(!myAuthReqs.hwsig_formula.empty());
        TS_ASSERT_EQUALS(myAuthReqs.password_prompt, "Password");
        TS_ASSERT_EQUALS(myAuthReqs.service_uris, list_of("https://r4webdemo.gotdns.com/"));
        TS_ASSERT(myAuthReqs.resolve_service_uris);
        TS_ASSERT(!myAuthReqs.calc_service_uris_digest);

        // given
        TS_TRACE("Authenticating with invalid credentials");
        resept::Credentials myCreds = list_of(Credential(resept::credUserId, myGoodUserId))
                                              (Credential(resept::credHwSig, myGoodHwsig))
                                              (Credential(resept::credPasswd, resept::calcOtp() + ".invalid"));
        ta::StringArrayDict myResolvedURIs = resolveServiceURIs(myAuthReqs.service_uris);
        // when
        rclient::AuthResponse myAuthResult = myRcdp.authenticate(myService, myCreds, myResolvedURIs);
        // then
        TS_ASSERT_EQUALS(myAuthResult.auth_result.type, resept::AuthResult::Delay);
        TS_ASSERT(myAuthResult.auth_result.delay > 0);
        TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateConnected);
        TS_TRACE(str(boost::format("-- We are locked for %u seconds, wait...") % myAuthResult.auth_result.delay).c_str());
        awaitFor(myAuthResult.auth_result.delay);

        // given
        TS_TRACE("Authenticating with valid credentials");
        myCreds = list_of(Credential(resept::credUserId, myGoodUserId))
                           (Credential(resept::credHwSig, myGoodHwsig))
                           (Credential(resept::credPasswd, resept::calcOtp()));
         myResolvedURIs = resolveServiceURIs(myAuthReqs.service_uris);
         // when
         myAuthResult = myRcdp.authenticate(myService, myCreds, myResolvedURIs);
        // then
        TS_ASSERT_EQUALS(myAuthResult.auth_result.type, resept::AuthResult::Ok);
        TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateAuthenticated);
    }

    void test_multi_round_password_authentication()
    {
        using namespace resept::rcdpv2;
        using resept::Credential;
        using boost::assign::list_of;
        using std::string;

        const string myService = "CUST_PASSWD_RADIUS";
        const string myGoodHwsig = "123456";

        rclient::RcdpHandler myRcdp(theSvr);

        myRcdp.hello();
        myRcdp.handshake();

        // when
        const rclient::AuthRequirements myAuthReqs = myRcdp.getAuthRequirements(myService);

        // then
        TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateConnected);
        TS_ASSERT(ta::equalIgnoreOrder(myAuthReqs.cred_types, list_of(resept::credUserId)(resept::credHwSig)(resept::credPasswd)));
        TS_ASSERT(!myAuthReqs.hwsig_formula.empty());
        TS_ASSERT_EQUALS(myAuthReqs.password_prompt, "Password");
        TS_ASSERT_EQUALS(myAuthReqs.service_uris, list_of("https://r4webdemo.gotdns.com/"));
        TS_ASSERT(myAuthReqs.resolve_service_uris);
        TS_ASSERT(!myAuthReqs.calc_service_uris_digest);

        // given
        TS_TRACE("-- Authenticating RADIUS SECURID INITIAL TOKEN (bad tokencode)");
        const string myGoodUserId = resept::securid::NextTokenUserName;
        const string myGoodInitialTokencode = resept::securid::NextTokenInitialTokenCode;
        const string myGoodNewTokencode = resept::securid::NextTokenNewTokenCode;

        resept::Credentials myCreds = list_of(Credential(resept::credUserId, myGoodUserId))
                                             (Credential(resept::credHwSig, myGoodHwsig))
                                             (Credential(resept::credPasswd, myGoodInitialTokencode + ".invalid"));
         const ta::StringArrayDict myResolvedURIs = resolveServiceURIs(myAuthReqs.service_uris);

        // when
        rclient::AuthResponse myAuthResult = myRcdp.authenticate(myService, myCreds, myResolvedURIs);
        // then
        TS_ASSERT_EQUALS(myAuthResult.auth_result.type, resept::AuthResult::Delay);
        TS_ASSERT(myAuthResult.auth_result.delay > 0);
        TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateConnected);
        TS_TRACE(str(boost::format("-- We are locked for %u seconds, wait...") % myAuthResult.auth_result.delay).c_str());
        awaitFor(myAuthResult.auth_result.delay);

        // given
        TS_TRACE("-- Authenticating RADIUS SECURID INITIAL TOKEN (good tokencode)");
        myCreds = list_of(Credential(resept::credUserId, myGoodUserId))
                        (Credential(resept::credHwSig, myGoodHwsig))
                        (Credential(resept::credPasswd, myGoodInitialTokencode));
        // when
        myAuthResult = myRcdp.authenticate(myService, myCreds, myResolvedURIs);
        // then
        TS_ASSERT_EQUALS(myAuthResult.auth_result.type, resept::AuthResult::Challenge);
        TS_ASSERT_EQUALS(myAuthResult.challenges.size(), 1);
        TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateConnected);

        // given
        TS_TRACE("-- Authenticating RADIUS SECURID NEW TOKEN (good tokencode)");
        myCreds = list_of(Credential(resept::credUserId, myGoodUserId))
                        (Credential(resept::credHwSig, myGoodHwsig))
                        (Credential(resept::credPasswd, myGoodNewTokencode));
        // when
        myAuthResult = myRcdp.authenticate(myService, myCreds, myResolvedURIs);
        // then
        TS_ASSERT_EQUALS(myAuthResult.auth_result.type, resept::AuthResult::Ok);
        TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateAuthenticated);
    }

    void test_cr_authentication()
    {
         using namespace resept::rcdpv2;
         using resept::Credential;
         using boost::assign::list_of;
         using boost::assign::map_list_of;
         using std::string;

         const string myService = "CUST_CR_INTERNAL";
         const string myGoodUserid = "DemoUser";
         const string myGoodHwsig = "123456";

         rclient::RcdpHandler myRcdp(theSvr);

         myRcdp.hello();
         myRcdp.handshake();

         // when
         const rclient::AuthRequirements myAuthReqs = myRcdp.getAuthRequirements(myService);
         // then
         TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateConnected);
         TS_ASSERT(ta::equalIgnoreOrder(myAuthReqs.cred_types, list_of(resept::credUserId)(resept::credHwSig)(resept::credResponse)));

         // given
         TS_TRACE("-- Authenticating CR (phase 1)");
         resept::Credentials myCreds = list_of(Credential(resept::credUserId, myGoodUserid))
                          (Credential(resept::credHwSig, myGoodHwsig));
         // when
         rclient::AuthResponse myAuthResult = myRcdp.authenticate(myService, myCreds);
         // then
         TS_ASSERT_EQUALS(myAuthResult.auth_result.type, resept::AuthResult::Challenge);
         TS_ASSERT_EQUALS(myAuthResult.challenges.size(), 1);
         TS_ASSERT_EQUALS(myAuthResult.response_names.size(), 1);
         TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateConnected);

        // given
        TS_TRACE("-- Authenticating CR (phase 2)");
        const string myChallenge = myAuthResult.challenges.begin()->second;
        const string myResponseName = myAuthResult.response_names.at(0);
        const ta::StringDict myResponses = map_list_of(myResponseName, resept::calcResponse(myGoodUserid, myChallenge));
        myCreds = list_of(Credential(resept::credUserId, myGoodUserid))
                          (Credential(resept::credHwSig, myGoodHwsig))
                          (Credential(myResponses));
         // when
         myAuthResult = myRcdp.authenticate(myService, myCreds);
         // then
         TS_ASSERT_EQUALS(myAuthResult.auth_result.type, resept::AuthResult::Ok);
         TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateAuthenticated);
    }

    void test_cr_authentication_when_user_is_locked()
    {
        using namespace resept::rcdpv2;
        using resept::Credential;
        using boost::assign::list_of;
        using std::string;

        const string myService = "CUST_CR_INTERNAL";
        const string myUserid = "DemoUser3";
        const string myGoodHwsig = "123456";

        rclient::RcdpHandler myRcdp(theSvr);

        myRcdp.hello();
        myRcdp.handshake();

        // when
        const rclient::AuthRequirements myAuthReqs = myRcdp.getAuthRequirements(myService);
        // then
        TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateConnected);
        TS_ASSERT(ta::equalIgnoreOrder(myAuthReqs.cred_types, list_of(resept::credUserId)(resept::credHwSig)(resept::credResponse)));

        // given
        TS_TRACE("-- Authenticating CR (phase 1, user is locked)");
        const resept::Credentials myCreds = list_of(Credential(resept::credUserId, myUserid))(Credential(resept::credHwSig, myGoodHwsig));
        // when
        const rclient::AuthResponse myAuthResult = myRcdp.authenticate(myService, myCreds);
        // then
        TS_ASSERT_EQUALS(myAuthResult.auth_result.type, resept::AuthResult::Locked);
        TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateClosed);
    }

    void test_multi_phase_cr_authentication()
    {
         using namespace resept::rcdpv2;
         using resept::Credential;
         using boost::assign::list_of;
         using boost::assign::map_list_of;
         using std::string;

         const string myService = "CUST_EAP_CR_RADIUS";

         rclient::RcdpHandler myRcdp(theSvr);

         myRcdp.hello();
         myRcdp.handshake();

         // when
         const rclient::AuthRequirements myAuthReqs = myRcdp.getAuthRequirements(myService);
         // then
         TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateConnected);
         TS_ASSERT(ta::equalIgnoreOrder(myAuthReqs.cred_types, list_of(resept::credUserId)(resept::credHwSig)(resept::credResponse)));

         // given
         TS_TRACE("-- Authenticating CR (phase 1)");
         resept::Credentials myCreds = list_of(Credential(resept::credUserId, resept::GsmUserName))
                          (Credential(resept::credHwSig, "123456"));
        const ta::StringArrayDict myResolvedURIs = resolveServiceURIs(myAuthReqs.service_uris);
         // when
         rclient::AuthResponse myAuthResult = myRcdp.authenticate(myService, myCreds, myResolvedURIs);
         // then
         TS_ASSERT_EQUALS(myAuthResult.auth_result.type, resept::AuthResult::Challenge);
         TS_ASSERT_EQUALS(myAuthResult.challenges.size(), 1);
         TS_ASSERT_EQUALS(myAuthResult.response_names.size(), 2);
         TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateConnected);

        // given
        TS_TRACE("--  1st GSM authentication round out of 3");
        ta::StringDict myResponses = resept::calcGsmResponses(resept::GsmUserName, myAuthResult.challenges, myAuthResult.response_names);
        myCreds = list_of(Credential(myResponses));
         // when
         myAuthResult = myRcdp.authenticate(myService, myCreds);
         // then
         TS_ASSERT_EQUALS(myAuthResult.auth_result.type, resept::AuthResult::Challenge);
         TS_ASSERT_EQUALS(myAuthResult.challenges.size(), 1);
         TS_ASSERT_EQUALS(myAuthResult.response_names.size(), 2);
         TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateConnected);

         // given
        TS_TRACE("--  2nd GSM authentication round out of 3");
        myResponses = resept::calcGsmResponses(resept::GsmUserName, myAuthResult.challenges, myAuthResult.response_names);
        myCreds = list_of(Credential(myResponses));
         // when
         myAuthResult = myRcdp.authenticate(myService, myCreds);
         // then
         TS_ASSERT_EQUALS(myAuthResult.auth_result.type, resept::AuthResult::Challenge);
         TS_ASSERT_EQUALS(myAuthResult.challenges.size(), 1);
         TS_ASSERT_EQUALS(myAuthResult.response_names.size(), 2);
         TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateConnected);

         // given
        TS_TRACE("--  3rd GSM authentication round out of 3");
        myResponses = resept::calcGsmResponses(resept::GsmUserName, myAuthResult.challenges, myAuthResult.response_names);
        myCreds = list_of(Credential(myResponses));
         // when
         myAuthResult = myRcdp.authenticate(myService, myCreds);
         // then
         TS_ASSERT_EQUALS(myAuthResult.auth_result.type, resept::AuthResult::Ok);
         TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateAuthenticated);
    }

    void test_cr_authentication_with_multiple_challenges()
    {
         using namespace resept::rcdpv2;
         using resept::Credential;
         using boost::assign::list_of;
         using boost::assign::map_list_of;
         using std::string;

         const string myService = "CUST_EAP_CR_RADIUS";

         rclient::RcdpHandler myRcdp(theSvr);

         myRcdp.hello();
         myRcdp.handshake();

         // when
         const rclient::AuthRequirements myAuthReqs = myRcdp.getAuthRequirements(myService);
         // then
         TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateConnected);
         TS_ASSERT(ta::equalIgnoreOrder(myAuthReqs.cred_types, list_of(resept::credUserId)(resept::credHwSig)(resept::credResponse)));

        // given
        TS_TRACE("-- Authenticating CR (phase 1) with multi-challenges");
        resept::Credentials myCreds = list_of(Credential(resept::credUserId, resept::UmtsUserName))
                          (Credential(resept::credHwSig, "123456"));
        const ta::StringArrayDict myResolvedURIs = resolveServiceURIs(myAuthReqs.service_uris);
        // when
        rclient::AuthResponse myAuthResult = myRcdp.authenticate(myService, myCreds, myResolvedURIs);
        // then
        TS_ASSERT_EQUALS(myAuthResult.auth_result.type, resept::AuthResult::Challenge);
        TS_ASSERT_EQUALS(myAuthResult.challenges.size(), 2);
        TS_ASSERT_EQUALS(myAuthResult.response_names.size(), 3);
        TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateConnected);

        // given
        TS_TRACE("--  Authenticating CR (phase 2) with multi-challenges");
        const ta::StringDict myResponses = resept::calcUmtsResponses(resept::UmtsUserName, myAuthResult.challenges, myAuthResult.response_names);
        myCreds = list_of(Credential(myResponses));
        // when
        myAuthResult = myRcdp.authenticate(myService, myCreds);
        // then
        TS_ASSERT_EQUALS(myAuthResult.auth_result.type, resept::AuthResult::Ok);
        TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateAuthenticated);
    }


    void test_last_messages()
    {
        using namespace resept::rcdpv2;
        using resept::Credential;
        using boost::assign::list_of;
        using std::string;

        const string myService = "CUST_PASSWD_INTERNAL_WEB";
        const string myGoodUserid = "DemoUser";
        const string myGoodHwsig = "123456";
        const string myGoodPasswd = "secret";

        rclient::RcdpHandler myRcdp(theSvr);

        myRcdp.hello();
        myRcdp.handshake();
        const rclient::AuthRequirements myAuthReqs = myRcdp.getAuthRequirements(myService);
        const resept::Credentials myCreds = list_of(Credential(resept::credUserId, myGoodUserid))
                                              (Credential(resept::credHwSig, myGoodHwsig))
                                              (Credential(resept::credPasswd, myGoodPasswd));
        const ta::StringArrayDict myResolvedURIs = resolveServiceURIs(myAuthReqs.service_uris);
        const rclient::AuthResponse myAuthResult = myRcdp.authenticate(myService, myCreds, myResolvedURIs);
        TS_ASSERT_EQUALS(myAuthResult.auth_result.type, resept::AuthResult::Ok);

        // when
        rclient::Messages myMessages = myRcdp.getLastMessages();
        // then
        TS_ASSERT_EQUALS(myMessages.size(), 10);

        // given
        time_t myFromUtc = myMessages.at(0).utc + 1;
        // when-then
        TS_ASSERT_LESS_THAN(myRcdp.getLastMessages(&myFromUtc).size(), myMessages.size());

        // given
        myFromUtc = time(NULL);
        // when-then
        TS_ASSERT_EQUALS(myRcdp.getLastMessages(&myFromUtc).size(), 0);
    }

    void test_that_cert_can_be_retrieved()
    {
        using namespace resept::rcdpv2;
        using resept::Credential;
        using boost::assign::list_of;
        using std::string;

        const string myService = "CUST_PASSWD_INTERNAL_WEB";
        const string myGoodUserid = "DemoUser";
        const string myGoodHwsig = "123456";
        const string myGoodPasswd = "secret";

        // given
        rclient::RcdpHandler myRcdp(theSvr);

        myRcdp.hello();
        myRcdp.handshake();
        const rclient::AuthRequirements myAuthReqs = myRcdp.getAuthRequirements(myService);
        const resept::Credentials myCreds = list_of(Credential(resept::credUserId, myGoodUserid))
                                              (Credential(resept::credHwSig, myGoodHwsig))
                                              (Credential(resept::credPasswd, myGoodPasswd));
        const ta::StringArrayDict myResolvedURIs = resolveServiceURIs(myAuthReqs.service_uris);
        const rclient::AuthResponse myAuthResult = myRcdp.authenticate(myService, myCreds, myResolvedURIs);
        TS_ASSERT_EQUALS(myAuthResult.auth_result.type, resept::AuthResult::Ok);

        static const bool Flags[] = {true, false};
        foreach (bool withChain, Flags)
        {
            TS_TRACE((boost::format("Requesting PKCS#12 certificate %s chain") % (withChain ? "with" : "without")).str().c_str());
            // when
            rclient::CertResponse myCertResult = myRcdp.getCert(resept::certformatP12, withChain);
            // then
            TS_ASSERT_EQUALS(ta::CertUtils::parsePfx(myCertResult.cert, myCertResult.password), (withChain ? 3U : 1U));
            TS_ASSERT(!myCertResult.execute_sync);

            TS_TRACE((boost::format("Requesting PEM certificate %s chain") % (withChain ? "with" : "without")).str().c_str());
            // when
            myCertResult = myRcdp.getCert(resept::certformatPem, withChain);
            // then
            TS_ASSERT(ta::CertUtils::isKeyPair(myCertResult.cert, myCertResult.cert, myCertResult.password.c_str()));
            TS_ASSERT_EQUALS(ta::CertUtils::getPemCertsInfo(myCertResult.cert).size(), (withChain ? 3U : 1U));
            TS_ASSERT(!myCertResult.execute_sync);
        }
    }

    void test_that_keypair_can_be_signed()
    {
        using namespace resept::rcdpv2;
        using resept::Credential;
        using boost::assign::list_of;
        using std::string;

        const string myService = "CUST_PASSWD_INTERNAL_WEB";
        const string myGoodUserid = "DemoUser";
        const string myGoodHwsig = "123456";
        const string myGoodPasswd = "secret";

        // given
        const ta::KeyPair myKeyPair = ta::RsaUtils::genKeyPair(2048, ta::RsaUtils::encPEM, ta::RsaUtils::pubkeyPKCS1);

        // given
        rclient::RcdpHandler myRcdp(theSvr);

        myRcdp.hello();
        myRcdp.handshake();
        const rclient::AuthRequirements myAuthReqs = myRcdp.getAuthRequirements(myService);
        const resept::Credentials myCreds = list_of(Credential(resept::credUserId, myGoodUserid))
                                              (Credential(resept::credHwSig, myGoodHwsig))
                                              (Credential(resept::credPasswd, myGoodPasswd));
        const ta::StringArrayDict myResolvedURIs = resolveServiceURIs(myAuthReqs.service_uris);
        const rclient::AuthResponse myAuthResult = myRcdp.authenticate(myService, myCreds, myResolvedURIs);
        TS_ASSERT_EQUALS(myAuthResult.auth_result.type, resept::AuthResult::Ok);

        static const bool Flags[] = {true, false};
        foreach (bool withChain, Flags)
        {
            TS_TRACE((boost::format("Requesting PKCS#12 certificate from self keypair %s chain") % (withChain ? "with" : "without")).str().c_str());
            // when
            rclient::CertResponse myCertResult = myRcdp.getCert(resept::certformatP12, withChain, &myKeyPair);
            // then
            string myParsedKey, myParsedCert;
            TS_ASSERT_EQUALS(ta::CertUtils::parsePfx(myCertResult.cert, myCertResult.password, myParsedKey, myParsedCert), (withChain ? 3U : 1U));
            TS_ASSERT(ta::CertUtils::isKeyPair(myParsedCert, myParsedKey));
            TS_ASSERT_EQUALS(ta::CertUtils::extractPemPubKey(myParsedCert), ta::RsaUtils::pubKeyPkcs1ToPkcs8(myKeyPair.pubKey));
            TS_ASSERT(!myCertResult.execute_sync);

            TS_TRACE((boost::format("Requesting PEM certificate from self keypair %s chain") % (withChain ? "with" : "without")).str().c_str());
            // when
            myCertResult = myRcdp.getCert(resept::certformatPem, withChain, &myKeyPair);
            // then
            TS_ASSERT(ta::CertUtils::isKeyPair(myCertResult.cert, myCertResult.cert, myCertResult.password.c_str()));
            TS_ASSERT_EQUALS(ta::CertUtils::getPemCertsInfo(myCertResult.cert).size(), (withChain ? 3U : 1U));
            TS_ASSERT_EQUALS(ta::CertUtils::extractPemPubKey(myCertResult.cert), ta::RsaUtils::pubKeyPkcs1ToPkcs8(myKeyPair.pubKey));
            TS_ASSERT(!myCertResult.execute_sync);
        }
    }
};


//
// ActiveDirectory RCDP test suite
//
class RcdpTestActiveDirectory : public RcdpTestBase
{
public:
    void setUp()
    {
        RcdpTestBase::setUp();
        resetAdPassword();
    }

    void resetAdPassword()
    {
        using namespace resept::rcdpv2;
        using resept::Credential;
        using boost::assign::list_of;
        using std::string;

        TS_TRACE("Resetting AD password");
        const string myUserId = "TestUser";

        rclient::RcdpHandler myRcdp(theSvr);
        myRcdp.hello();
        myRcdp.handshake();

        resept::Credentials myCreds = list_of(Credential(resept::credUserId, myUserId))(Credential(resept::credHwSig, HWSIG))(Credential(resept::credPasswd, INITIAL_PASSWORD));

        rclient::AuthResponse myAuthResult = myRcdp.authenticate(SERVICE, myCreds);
        if (myAuthResult.auth_result.type == resept::AuthResult::Delay)
        {
            awaitFor(myAuthResult.auth_result.delay);

            myCreds = list_of(Credential(resept::credUserId, myUserId))(Credential(resept::credHwSig, HWSIG))(Credential(resept::credPasswd, ANOTHER_PASSWORD));
            myAuthResult = myRcdp.authenticate(SERVICE, myCreds);
            if (myAuthResult.auth_result.type == resept::AuthResult::Delay)
            {
                awaitFor(myAuthResult.auth_result.delay);
                TA_THROW_MSG(std::runtime_error, "Neither old nor new AD password is valid");
            }
            else if (myAuthResult.auth_result.type != resept::AuthResult::Ok)
            {
                TA_THROW_MSG(std::runtime_error, "New AD password is not valid and " + str(myAuthResult.auth_result.type) + " received trying to login to AD with old password");
            }

            // old password is OK, change it to new
            myAuthResult = myRcdp.changePassword(ANOTHER_PASSWORD, INITIAL_PASSWORD);
            if (myAuthResult.auth_result.type == resept::AuthResult::Delay)
            {
                awaitFor(myAuthResult.auth_result.delay);
            }
            if (myAuthResult.auth_result.type != resept::AuthResult::Ok)
            {
                TA_THROW_MSG(std::runtime_error, str(myAuthResult.auth_result.type) + " trying to change old AD password to new");
            }

            TS_TRACE("Successfully reset AD password");
            myRcdp.eoc();
            return;
        }

        if (myAuthResult.auth_result.type != resept::AuthResult::Ok)
        {
            TA_THROW_MSG(std::runtime_error, str(myAuthResult.auth_result.type) + " received trying to authenticate AD with new password");
        }
        TS_TRACE("New AD password is already in effect, nothing to do");
        myRcdp.eoc();
    }

    void test_that_ldap_password_can_be_changed()
    {
        using namespace resept::rcdpv2;
        using resept::Credential;
        using boost::assign::list_of;
        using std::string;

        const ta::StringArray myUserIds = list_of("TestUser")
                                        // ("TestUser@Resept.2012.local")
                                        // ("Resept\\TestUser")
                                            ;

        rclient::RcdpHandler myRcdp(theSvr);
        myRcdp.hello();
        myRcdp.handshake();

        foreach (const string& userid, myUserIds)
        {
            // given
            resept::Credentials myCreds = list_of(Credential(resept::credUserId, userid))
                                                    (Credential(resept::credHwSig, HWSIG))
                                                    (Credential(resept::credPasswd, INITIAL_PASSWORD));
            TS_ASSERT_EQUALS(myRcdp.authenticate(SERVICE, myCreds).auth_result.type, resept::AuthResult::Ok);
            // when-then
            TS_TRACE(str(boost::format("Changing AD password for user %s") % userid).c_str());
            TS_ASSERT_EQUALS(myRcdp.changePassword(INITIAL_PASSWORD, ANOTHER_PASSWORD).auth_result.type, resept::AuthResult::Ok);
            // then
            TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateConnected);
            rclient::AuthResponse myAuthResult = myRcdp.authenticate(SERVICE, myCreds);
            TS_ASSERT_EQUALS(myAuthResult.auth_result.type, resept::AuthResult::Delay);
            awaitFor(myAuthResult.auth_result.delay);
            myCreds = list_of(Credential(resept::credUserId, userid))
                                        (Credential(resept::credHwSig, HWSIG))
                                        (Credential(resept::credPasswd, ANOTHER_PASSWORD));
            TS_ASSERT_EQUALS(myRcdp.authenticate(SERVICE, myCreds).auth_result.type, resept::AuthResult::Ok);
            // when-then
            TS_TRACE(str(boost::format("Changing AD password back for user %s") % userid).c_str());
            TS_ASSERT_EQUALS(myRcdp.changePassword(ANOTHER_PASSWORD, INITIAL_PASSWORD).auth_result.type, resept::AuthResult::Ok);
            // then
            TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateConnected);
            myCreds = list_of(Credential(resept::credUserId, userid))
                                        (Credential(resept::credHwSig, HWSIG))
                                        (Credential(resept::credPasswd, INITIAL_PASSWORD));
            TS_ASSERT_EQUALS(myRcdp.authenticate(SERVICE, myCreds).auth_result.type, resept::AuthResult::Ok);
        }
    }

    void test_that_ldap_password_change_fails_for_invalid_old_password()
    {
        using namespace resept::rcdpv2;
        using resept::Credential;
        using boost::assign::list_of;
        using std::string;

        const string myUserId = "TestUser";

        rclient::RcdpHandler myRcdp(theSvr);
        myRcdp.hello();
        myRcdp.handshake();

        // given
        const resept::Credentials myCreds = list_of(Credential(resept::credUserId, myUserId))
                                                (Credential(resept::credHwSig, HWSIG))
                                                (Credential(resept::credPasswd, INITIAL_PASSWORD));
        TS_ASSERT_EQUALS(myRcdp.authenticate(SERVICE, myCreds).auth_result.type, resept::AuthResult::Ok);
        // when
        TS_TRACE(str(boost::format("Trying to change AD password for user %s with invalid old password") % myUserId).c_str());
        const rclient::AuthResponse myAuthResult = myRcdp.changePassword(INITIAL_PASSWORD + ".invalid", ANOTHER_PASSWORD);
        // then
        TS_ASSERT_EQUALS(myAuthResult.auth_result.type, resept::AuthResult::Delay);
        TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateAuthenticated); // we are still authenticated
        awaitFor(myAuthResult.auth_result.delay);
    }

    void test_that_ldap_password_change_fails_for_locked_user()
    {
        using namespace resept::rcdpv2;
        using resept::Credential;
        using boost::assign::list_of;
        using std::string;

        const string myUserId = "TestUser";

        rclient::RcdpHandler myRcdp(theSvr);
        myRcdp.hello();
        myRcdp.handshake();

        // given
        const resept::Credentials myCreds = list_of(Credential(resept::credUserId, myUserId))
                                                (Credential(resept::credHwSig, HWSIG))
                                                (Credential(resept::credPasswd, INITIAL_PASSWORD));
        TS_ASSERT_EQUALS(myRcdp.authenticate(SERVICE, myCreds).auth_result.type, resept::AuthResult::Ok);
        // when
        TS_TRACE(str(boost::format("Trying to change AD password for user %s with invalid old password") % myUserId).c_str());
        const rclient::AuthResponse myAuthResult = myRcdp.changePassword(INITIAL_PASSWORD + ".invalid", ANOTHER_PASSWORD);
        // then
        TS_ASSERT_EQUALS(myAuthResult.auth_result.type, resept::AuthResult::Delay);
        TS_ASSERT_EQUALS(myRcdp.userSessionData().rcdpState, stateAuthenticated); // we are still authenticated
        awaitFor(myAuthResult.auth_result.delay);
    }

private:
	static const std::string SERVICE;
    static const std::string INITIAL_PASSWORD;
    static const std::string ANOTHER_PASSWORD;
    static const std::string HWSIG;
};

const std::string RcdpTestActiveDirectory::SERVICE  = "CUST_PASSWD_AD";
const std::string RcdpTestActiveDirectory::INITIAL_PASSWORD  = "Sioux2010";
const std::string RcdpTestActiveDirectory::ANOTHER_PASSWORD  = "Sioux2011";
const std::string RcdpTestActiveDirectory::HWSIG  = "123456";
