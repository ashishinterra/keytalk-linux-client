#pragma once

#include "rclient/Settings.h"
#include "rclient/Common.h"
#include "resept/common.h"
#include "ta/utils.h"
#include "ta/process.h"
#include "ta/netutils.h"
#include "ta/url.h"

#include "cxxtest/TestSuite.h"
#include "boost/algorithm/string.hpp"
#include "boost/assign/list_of.hpp"
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/convenience.hpp"

#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <stdio.h>

using namespace std;
using namespace ta;

#define SETTINGS_TEST_DEBUG

namespace SettingsTestUtils
{
    static bool UrlMatch(const string& anUrl, const string& aHotUrl)
    {
        return url::normalize(anUrl) == url::normalize(aHotUrl);
    }
    static void removeFile(const std::string& aFile)
    {
        remove(aFile.c_str());
        if (ta::isFileExist(aFile))
            TA_THROW_MSG(std::runtime_error, "Failed to remove file " + aFile);
    }

} // SettingsTestUtils

// Test regular usage
class SettingsTest : public CxxTest::TestSuite
{
public:
    void setUp()
    {
        CxxTest::setAbortTestOnFail(true);
        namespace fs = boost::filesystem;
        fs::copy_file("user.ini.orig", "user.ini", fs::copy_option::overwrite_if_exists);
        fs::copy_file("master.ini.orig", "master.ini", fs::copy_option::overwrite_if_exists);
        rclient::Settings::setConfigsPath("resept.ini", "user.ini", "master.ini");
        CxxTest::setAbortTestOnFail(false);
    }
    void tearDown()
    {
        try
        {
            rclient::Settings::resetConfigsPath();
            SettingsTestUtils::removeFile("user.ini");
            SettingsTestUtils::removeFile("user.yaml");
            SettingsTestUtils::removeFile("master.ini");
            SettingsTestUtils::removeFile("master.yaml");
        }
        catch(std::exception& e)
        {
            TS_TRACE(e.what());
        }
        catch(...)
        {
            TS_TRACE("tearDown() failed with unknown error");
        }
    }

    void testGlobalSettings()
    {
        using namespace rclient;
        using boost::assign::list_of;

        TS_TRACE("--- Testing config locations");
        TS_ASSERT_EQUALS(Settings::getReseptConfigPath(), "resept.ini");
        TS_ASSERT_EQUALS(Settings::getReseptConfigDir(), "");
        TS_ASSERT_EQUALS(Settings::getUserConfigPath(), "user.ini");
        TS_ASSERT_EQUALS(Settings::getUserConfigDir(), "");
        TS_ASSERT_EQUALS(Settings::getMasterConfigPath(), "master.ini");
        TS_ASSERT_EQUALS(Settings::getMasterConfigDir(), "");

        TS_TRACE("--- Testing ReseptBrokerService port");
        TS_ASSERT_EQUALS(Settings::getReseptBrokerServicePort(), 0U);
        Settings::setReseptBrokerServicePort(2101);
        TS_ASSERT_EQUALS(Settings::getReseptBrokerServicePort(), 2101U);
        Settings::setReseptBrokerServicePort(2102);
        TS_ASSERT_EQUALS(Settings::getReseptBrokerServicePort(), 2102U);

        TS_TRACE("--- Testing KeyTalk installation directory");
#ifdef _WIN32
        const string myExpectedInstallDir = "C:\\Program Files\\keytalk";
        const string myNewInstallDir = "C:\\Program Files\\keytalk-new";
#else
        const string myExpectedInstallDir = "/usr/local/bin/keytalk";
        const string myNewInstallDir = "/usr/local/bin/keytalk-new";
#endif
        TS_ASSERT_EQUALS(Settings::getReseptInstallDir(), myExpectedInstallDir);
        Settings::setReseptInstallDir(myNewInstallDir);
        TS_ASSERT_EQUALS(Settings::getReseptInstallDir(), myNewInstallDir);
        Settings::setReseptInstallDir(myExpectedInstallDir);
        TS_ASSERT_EQUALS(Settings::getReseptInstallDir(), myExpectedInstallDir);

        TS_TRACE("--- Testing installed providers");

        TS_ASSERT_EQUALS(Settings::getInstalledProviders().size(), 0U);
    	Settings::removeInstalledProvider("Provider1");
    	TS_ASSERT_EQUALS(Settings::getInstalledProviders().size(), 0U);

        Settings::addInstalledProvider("Provider1");
        TS_ASSERT_EQUALS(Settings::getInstalledProviders(), list_of("Provider1"));

        Settings::addInstalledProvider("Provider2");
        TS_ASSERT_EQUALS(Settings::getInstalledProviders(), list_of("Provider1")("Provider2"));

        Settings::addInstalledProvider("Provider2");
        TS_ASSERT_EQUALS(Settings::getInstalledProviders(), list_of("Provider1")("Provider2"));

    	Settings::removeInstalledProvider("Non_existing_provider");
    	TS_ASSERT_EQUALS(Settings::getInstalledProviders(), list_of("Provider1")("Provider2"));

    	Settings::removeInstalledProvider("Provider1");
    	TS_ASSERT_EQUALS(Settings::getInstalledProviders(), list_of("Provider2"));

    	Settings::removeInstalledProvider("Provider2");
    	TS_ASSERT_EQUALS(Settings::getInstalledProviders().size(), 0U);


        TS_TRACE("--- Testing installed KeyTalk CAs");
        TS_ASSERT_EQUALS(Settings::getInstalledUserCaCNs(), list_of("TrustAlert DEMO Signing CA"));
        TS_ASSERT_EQUALS(Settings::getInstalledServerCaCNs(), list_of("TrustAlert DEMO Communication CA")("TrustAlert DEMO Communication CA1"));
        TS_ASSERT_EQUALS(Settings::getInstalledPrimaryCaCNs(), list_of("TrustAlert DEMO ROOT CA"));
        TS_ASSERT_EQUALS(Settings::getInstalledRootCaCNs().size(), 0U);

        Settings::addInstalledUserCA("TrustAlert DEMO Signing CA");
        TS_ASSERT_EQUALS(Settings::getInstalledUserCaCNs(), list_of("TrustAlert DEMO Signing CA"));

        Settings::addInstalledServerCA("TrustAlert DEMO Communication CA2");
        TS_ASSERT_EQUALS(Settings::getInstalledServerCaCNs(), list_of("TrustAlert DEMO Communication CA")("TrustAlert DEMO Communication CA1")("TrustAlert DEMO Communication CA2"));

        Settings::addInstalledPrimaryCA("TrustAlert DEMO ROOT CA1");
        TS_ASSERT_EQUALS(Settings::getInstalledPrimaryCaCNs(), list_of("TrustAlert DEMO ROOT CA")("TrustAlert DEMO ROOT CA1"));

        Settings::addInstalledRootCA("TrustAlert DEMO Really ROOT CA");
        TS_ASSERT_EQUALS(Settings::getInstalledRootCaCNs(), list_of("TrustAlert DEMO Really ROOT CA"));

        TS_TRACE("--- Testing installed extra signing intermediate CAs");
        TS_ASSERT_EQUALS(Settings::getInstalledExtraSigningIntCaSha1Fingerprints().size(), 0U);
        Settings::addInstalledExtraSigningIntCA("123");
        Settings::addInstalledExtraSigningIntCA("456");
        Settings::addInstalledExtraSigningIntCA("123");
        TS_ASSERT_EQUALS(Settings::getInstalledExtraSigningIntCaSha1Fingerprints(), list_of("123")("456"));

        TS_TRACE("--- Testing installed extra signing root CAs");
        TS_ASSERT_EQUALS(Settings::getInstalledExtraSigningRootCaSha1Fingerprints().size(), 0U);
        Settings::addInstalledExtraSigningRootCA("789");
        Settings::addInstalledExtraSigningRootCA("789");
        Settings::addInstalledExtraSigningRootCA("123");
        TS_ASSERT_EQUALS(Settings::getInstalledExtraSigningRootCaSha1Fingerprints(), list_of("789")("123"));

        TS_TRACE("--- Testing customized users");
        TS_ASSERT_EQUALS(Settings::getCustomizedUsers().size(), 0U);
        Settings::addCustomizedUser("user1");
        TS_ASSERT_EQUALS(Settings::getCustomizedUsers(), list_of("user1"));
        Settings::addCustomizedUser("user2");
        Settings::addCustomizedUser("user3");
        TS_ASSERT_EQUALS(Settings::getCustomizedUsers(), list_of("user1")("user2")("user3"));
        Settings::addCustomizedUser("user3");
        TS_ASSERT_EQUALS(Settings::getCustomizedUsers(), list_of("user1")("user2")("user3"));

    }

    void testProviderSettings()
    {
        using namespace rclient;
        using boost::assign::list_of;

        TS_TRACE("--- Testing get providers");
        TS_ASSERT_EQUALS(Settings::getProviders(), list_of("Provider1")("Provider2")("Provider4")("Provider3"));

        TS_TRACE("--- Testing latest provider");
        TS_ASSERT_EQUALS(Settings::getLatestProvider(), "Provider1");
        Settings::setLatestProviderService("Provider2", "Service1");
        TS_ASSERT_EQUALS(Settings::getLatestProvider(), "Provider2");
        Settings::setLatestProviderService("Provider3", "Service1");
        TS_ASSERT_EQUALS(Settings::getLatestProvider(), "Provider3");
        Settings::setLatestProviderService("Provider1", "Service1");
        TS_ASSERT_EQUALS(Settings::getLatestProvider(), "Provider1");
        Settings::setLatestProviderService("Provider4", "Service1");
        TS_ASSERT_EQUALS(Settings::getLatestProvider(), "Provider4");

        TS_TRACE("--- Testing provider installation directory");
        Settings::setLatestProviderService("Provider4", "Service1");
#ifdef _WIN32
        const string myExpectedProviderParentDir = "C:\\Program Files\\keytalk";
#else
        const string myExpectedProviderParentDir = ""; // because setConfigsPath() is in effect in setUp()
#endif
        TS_ASSERT_EQUALS(Settings::getProviderInstallDir(), myExpectedProviderParentDir + ta::getDirSep() + "Provider4");
        TS_ASSERT_EQUALS(Settings::getProviderInstallDir("Provider1"), myExpectedProviderParentDir + ta::getDirSep() + "Provider1");
        TS_ASSERT_EQUALS(Settings::getProviderInstallDir("Provider2"), myExpectedProviderParentDir + ta::getDirSep() + "Provider2");
        TS_ASSERT_EQUALS(Settings::getProviderInstallDir("Provider3"), myExpectedProviderParentDir + ta::getDirSep() + "Provider3");
        TS_ASSERT_EQUALS(Settings::getProviderInstallDir("Provider4"), myExpectedProviderParentDir + ta::getDirSep() + "Provider4");

        Settings::setLatestProviderService("Provider2", "Service1");
        TS_ASSERT_EQUALS(Settings::getProviderInstallDir(), myExpectedProviderParentDir + ta::getDirSep() + "Provider2");
        TS_ASSERT_EQUALS(Settings::getProviderInstallDir("Provider1"), myExpectedProviderParentDir + ta::getDirSep() + "Provider1");
        TS_ASSERT_EQUALS(Settings::getProviderInstallDir("Provider2"), myExpectedProviderParentDir + ta::getDirSep() + "Provider2");
        TS_ASSERT_EQUALS(Settings::getProviderInstallDir("Provider3"), myExpectedProviderParentDir + ta::getDirSep() + "Provider3");
        TS_ASSERT_EQUALS(Settings::getProviderInstallDir("Provider4"), myExpectedProviderParentDir + ta::getDirSep() + "Provider4");

        Settings::setLatestProviderService("Provider1", "Service1");
        TS_ASSERT_EQUALS(Settings::getProviderInstallDir(), myExpectedProviderParentDir + ta::getDirSep() + "Provider1");
        TS_ASSERT_EQUALS(Settings::getProviderInstallDir("Provider1"), myExpectedProviderParentDir + ta::getDirSep() + "Provider1");
        TS_ASSERT_EQUALS(Settings::getProviderInstallDir("Provider2"), myExpectedProviderParentDir + ta::getDirSep() + "Provider2");
        TS_ASSERT_EQUALS(Settings::getProviderInstallDir("Provider3"), myExpectedProviderParentDir + ta::getDirSep() + "Provider3");
        TS_ASSERT_EQUALS(Settings::getProviderInstallDir("Provider4"), myExpectedProviderParentDir + ta::getDirSep() + "Provider4");

        TS_TRACE("--- Testing Content version");
        bool myFromMasterConfig;
        TS_ASSERT_EQUALS(Settings::getProviderContentVersion("Provider1", myFromMasterConfig), 2010080401);
        TS_ASSERT(myFromMasterConfig);
        TS_ASSERT_EQUALS(Settings::getProviderContentVersion("Provider1"), 2010080401);
        TS_ASSERT_EQUALS(Settings::getProviderContentVersion("Provider2", myFromMasterConfig), 2010080402);
        TS_ASSERT(myFromMasterConfig);
        TS_ASSERT_EQUALS(Settings::getProviderContentVersion("Provider2"), 2010080402);
        TS_ASSERT_EQUALS(Settings::getProviderContentVersion("Provider3", myFromMasterConfig), 2010080403);
        TS_ASSERT(myFromMasterConfig);
        TS_ASSERT_EQUALS(Settings::getProviderContentVersion("Provider3"), 2010080403);
        TS_ASSERT_EQUALS(Settings::getProviderContentVersion("Provider4", myFromMasterConfig), 2010080441);
        TS_ASSERT(!myFromMasterConfig);
        TS_ASSERT_EQUALS(Settings::getProviderContentVersion("Provider4"), 2010080441);

        TS_TRACE("--- Testing RESEPT server address");
        Settings::setLatestProviderService("Provider1", "Service1");
        TS_ASSERT_EQUALS(Settings::getReseptSvrAddress(), ta::NetUtils::RemoteAddress("siouxdemo.trustalert.com", 443));
        TS_ASSERT_EQUALS(Settings::getReseptSvrAddress(), Settings::getReseptSvrAddress("Provider1"));
        bool myIsFromMasterConfig;
        TS_ASSERT_EQUALS(Settings::getReseptSvrAddress(myIsFromMasterConfig), Settings::getReseptSvrAddress("Provider1"));
        TS_ASSERT(myIsFromMasterConfig);
        Settings::setReseptSvrAddress("Provider1", ta::NetUtils::RemoteAddress("www.newhost.com", 123));
        TS_ASSERT_EQUALS(Settings::getReseptSvrAddress("Provider1"), ta::NetUtils::RemoteAddress("siouxdemo.trustalert.com", 443));

        TS_ASSERT_EQUALS(Settings::getReseptSvrAddress("Provider2", myIsFromMasterConfig), ta::NetUtils::RemoteAddress("fd7c:1111:1111:10::110", 443));
        TS_ASSERT(!myIsFromMasterConfig);
        Settings::setReseptSvrAddress("Provider2", ta::NetUtils::RemoteAddress("www.newhost.com", 123));
        TS_ASSERT_EQUALS(Settings::getReseptSvrAddress("Provider2"),  ta::NetUtils::RemoteAddress("www.newhost.com", 123));

        TS_ASSERT_EQUALS(Settings::getReseptSvrAddress("Provider3", myIsFromMasterConfig), ta::NetUtils::RemoteAddress("192.168.1.3", 1234));
        TS_ASSERT(myIsFromMasterConfig);
        Settings::setReseptSvrAddress("Provider3", ta::NetUtils::RemoteAddress("www.newhost.com", 123));
        TS_ASSERT_EQUALS(Settings::getReseptSvrAddress("Provider3"),  ta::NetUtils::RemoteAddress("192.168.1.3", 1234));

        TS_ASSERT_EQUALS(Settings::getReseptSvrAddress("Provider4", myIsFromMasterConfig), ta::NetUtils::RemoteAddress("fd7c:1111:1111:10::112", 1234));
        TS_ASSERT(!myIsFromMasterConfig);
        Settings::setReseptSvrAddress("Provider4", ta::NetUtils::RemoteAddress("www.newhost.com", 123));
        TS_ASSERT_EQUALS(Settings::getReseptSvrAddress("Provider4"), ta::NetUtils::RemoteAddress("www.newhost.com", 123));

        TS_TRACE("--- Testing CA");
        Settings::setLatestProviderService("Provider1", "Service1");
        TS_ASSERT_EQUALS(Settings::getUserCaName(), "RESEPT Demo UCA1");
        TS_ASSERT_EQUALS(Settings::getUserCaName("Provider1"), Settings::getUserCaName());
        TS_ASSERT_EQUALS(Settings::getServerCaName(), "RESEPT Demo SCA1");
        TS_ASSERT_EQUALS(Settings::getServerCaName("Provider1"), Settings::getServerCaName());
        TS_ASSERT_EQUALS(Settings::getPrimaryCaName(), "RESEPT Demo PCA1");
        TS_ASSERT_EQUALS(Settings::getPrimaryCaName("Provider1"), Settings::getPrimaryCaName());
        TS_ASSERT(!Settings::isRootCaExist());
        TS_ASSERT_EQUALS(Settings::isRootCaExist("Provider1"), Settings::isRootCaExist());

        Settings::setLatestProviderService("Provider2", "Service1");
        TS_ASSERT_EQUALS(Settings::getUserCaName(), "RESEPT Demo UCA2");
        TS_ASSERT_EQUALS(Settings::getServerCaName(), "RESEPT Demo SCA2");
        TS_ASSERT_EQUALS(Settings::getPrimaryCaName(), "RESEPT Demo PCA2");
        TS_ASSERT(!Settings::isRootCaExist());

        Settings::setLatestProviderService("Provider3", "Service1");
        TS_ASSERT_EQUALS(Settings::getUserCaName(), "RESEPT Demo UCA2");
        TS_ASSERT_EQUALS(Settings::getServerCaName(), "RESEPT Demo SCA2");
        TS_ASSERT_EQUALS(Settings::getPrimaryCaName(), "RESEPT Demo PCA2");
        TS_ASSERT(Settings::isRootCaExist());
        TS_ASSERT_EQUALS(Settings::getRootCaName(), "RESEPT Demo RCA2");

        TS_ASSERT_EQUALS(Settings::getUserCaName("Provider4"), "RESEPT Demo UCA4");
        TS_ASSERT_EQUALS(Settings::getServerCaName("Provider4"), "RESEPT Demo SCA4");
        TS_ASSERT_EQUALS(Settings::getPrimaryCaName("Provider4"), "RESEPT Demo PCA4");
        TS_ASSERT(Settings::isRootCaExist("Provider4"));
        TS_ASSERT_EQUALS(Settings::getRootCaName("Provider4"), "RESEPT Demo RCA4");


        TS_TRACE("--- Testing Log level");
        Settings::setLatestProviderService("Provider1", "Service1");
        TS_ASSERT_EQUALS(Settings::getLogLevel(myIsFromMasterConfig), "DEBUG");
        TS_ASSERT_EQUALS(Settings::getLogLevel("Provider1"), Settings::getLogLevel());
        TS_ASSERT(myIsFromMasterConfig);
        Settings::setLogLevel("Provider1", "INFO");
        TS_ASSERT_EQUALS(Settings::getLogLevel(), "DEBUG");

        Settings::setLatestProviderService("Provider2", "Service1");
        TS_ASSERT_EQUALS(Settings::getLogLevel(myIsFromMasterConfig), "ERROR");
        TS_ASSERT_EQUALS(Settings::getLogLevel("Provider2"), Settings::getLogLevel());
        TS_ASSERT(myIsFromMasterConfig);
        Settings::setLogLevel("Provider1", "INFO");
        TS_ASSERT_EQUALS(Settings::getLogLevel(), "ERROR");

        Settings::setLatestProviderService("Provider3", "Service1");
        TS_ASSERT_EQUALS(Settings::getLogLevel(myIsFromMasterConfig), Settings::DefLogLevel);
        TS_ASSERT_EQUALS(Settings::getLogLevel("Provider3"), Settings::DefLogLevel);
        TS_ASSERT(!myIsFromMasterConfig);

        Settings::setLatestProviderService("Provider4", "Service1");
        TS_ASSERT_EQUALS(Settings::getLogLevel(), "DEBUG");
        TS_ASSERT_EQUALS(Settings::getLogLevel("Provider4", myIsFromMasterConfig), Settings::getLogLevel());
        TS_ASSERT(!myIsFromMasterConfig);
        Settings::setLogLevel("Provider4", "INFO");
        TS_ASSERT_EQUALS(Settings::getLogLevel(), "INFO");


        TS_TRACE("--- Testing Last user message UTC");
        Settings::setLatestProviderService("Provider1", "Service1");
        TS_ASSERT(!Settings::isLastUserMsgUtcExist());
        TS_ASSERT_EQUALS(Settings::isLastUserMsgUtcExist("Provider1"), Settings::isLastUserMsgUtcExist());
        TS_ASSERT_THROWS(Settings::getLastUserMsgUtc(), SettingsError);
        TS_ASSERT_THROWS(Settings::getLastUserMsgUtc("Provider1"), SettingsError);
        Settings::setLastUserMsgUtc("2010-11-05T13:15:30+0000");
        TS_ASSERT(Settings::isLastUserMsgUtcExist());
        TS_ASSERT_EQUALS(Settings::isLastUserMsgUtcExist("Provider1"), Settings::isLastUserMsgUtcExist());
        TS_ASSERT_EQUALS(Settings::getLastUserMsgUtc(), "2010-11-05T13:15:30+0000");
        TS_ASSERT_EQUALS(Settings::getLastUserMsgUtc("Provider1"), Settings::getLastUserMsgUtc());

        TS_ASSERT(Settings::isLastUserMsgUtcExist("Provider2"));
        TS_ASSERT_EQUALS(Settings::getLastUserMsgUtc("Provider2"), "2010-11-05T00:00:30+0000");

    }

	void test_that_provider_user_settings_can_be_removed()
    {
        using namespace rclient;
        using boost::assign::list_of;

		TS_TRACE("Test that provider having master settings associated with it cannot be removed");
		TS_ASSERT_EQUALS(Settings::getProviders(), list_of("Provider1")("Provider2")("Provider4")("Provider3"));
		// when-then
		TS_ASSERT_THROWS(Settings::removeProviderFromUserConfig("Provider1"), SettingsError);
		TS_ASSERT_EQUALS(Settings::getProviders(), list_of("Provider1")("Provider2")("Provider4")("Provider3"));


		TS_TRACE("Test that removal of non-exiating provider has no effect");
		// when-then
		TS_ASSERT(!Settings::removeProviderFromUserConfig("non_existing_provider"));
		TS_ASSERT_EQUALS(Settings::getProviders(), list_of("Provider1")("Provider2")("Provider4")("Provider3"));


        TS_TRACE("Test remove provider");
		// given
		Settings::setLatestProviderService("Provider4", "Service1");
		// when-then
		// only Provider4 has no master settings associated
		TS_ASSERT(Settings::removeProviderFromUserConfig("Provider4"));
		// then
		TS_ASSERT_EQUALS(Settings::getProviders(), list_of("Provider1")("Provider2")("Provider3"));
		TS_ASSERT_DIFFERS(Settings::getLatestProvider(), "Provider4");


		TS_TRACE("Test remove all providers");
		// given no maser profile exists
		SettingsTestUtils::removeFile("master.ini");
		TS_ASSERT(ta::isFileExist("user.ini"))
		// when-then
		TS_ASSERT(Settings::removeProviderFromUserConfig("Provider1"));
		TS_ASSERT_EQUALS(Settings::getProviders(), list_of("Provider2")("Provider3"));
		// when-then
		TS_ASSERT(Settings::removeProviderFromUserConfig("Provider3"));
		TS_ASSERT_EQUALS(Settings::getProviders(), list_of("Provider2"));
		// when-then
		TS_ASSERT(Settings::removeProviderFromUserConfig("Provider2"));
		TS_ASSERT(!Settings::isCustomized());
		TS_ASSERT(!ta::isFileExist("user.ini"));
    }

    void testServiceSettings()
    {
        using namespace rclient;
        using boost::assign::list_of;

        TS_TRACE("--- Testing service list");
        Settings::setLatestProviderService("Provider1", "Service1");
        TS_ASSERT_EQUALS(Settings::getServices(), list_of("Service1")("Service2")("Service3")("Service4")("Service5")("Service6")("Service7")("Service8"));
        TS_ASSERT_EQUALS(Settings::getServices("Provider1"), Settings::getServices());
        TS_ASSERT_EQUALS(Settings::getServices("Provider2"), list_of("Service1"));
        TS_ASSERT_EQUALS(Settings::getServices("Provider3"), list_of("Service1"));
        TS_ASSERT_EQUALS(Settings::getServices("Provider4"), list_of("Service1"));

        Settings::setLatestProviderService("Provider2", "Service1");
        TS_ASSERT_EQUALS(Settings::getServices("Provider2"), Settings::getServices());
        Settings::setLatestProviderService("Provider4", "Service1");
        TS_ASSERT_EQUALS(Settings::getServices("Provider4"), Settings::getServices());


        TS_TRACE("--- Testing latest service");
        Settings::setLatestProviderService("Provider1", "Service5");
        TS_ASSERT_EQUALS(Settings::getLatestProvider(), "Provider1");
        TS_ASSERT_EQUALS(Settings::getLatestService(), "Service5");

        Settings::setLatestProviderService("Provider1", "Service8");
        TS_ASSERT_EQUALS(Settings::getLatestProvider(), "Provider1");
        TS_ASSERT_EQUALS(Settings::getLatestService(), "Service8");

        Settings::setLatestProviderService("Provider4", "Service1");
        TS_ASSERT_EQUALS(Settings::getLatestProvider(), "Provider4");
        TS_ASSERT_EQUALS(Settings::getLatestService(), "Service1");


        TS_ASSERT_THROWS(Settings::setLatestProviderService("Provider1", "Service999"), SettingsError);
        TS_ASSERT_THROWS(Settings::setLatestProviderService("Provider999", "Service1"), SettingsError);
        TS_ASSERT_THROWS(Settings::setLatestProviderService("Provider999", "Service999"), SettingsError);


        TS_TRACE("--- Testing display service name");
        Settings::setLatestProviderService("Provider1", "Service2");
        TS_ASSERT(Settings::isDisplayServiceName());
        Settings::setLatestProviderService("Provider1", "Service3");
        TS_ASSERT(Settings::isDisplayServiceName());
        Settings::setLatestProviderService("Provider1", "Service4");
        TS_ASSERT(!Settings::isDisplayServiceName());
        Settings::setLatestProviderService("Provider1", "Service5");
        TS_ASSERT_EQUALS(Settings::isDisplayServiceName(), Settings::DefServiceDisplayName);
        Settings::setLatestProviderService("Provider1", "Service6");
        TS_ASSERT(!Settings::isDisplayServiceName());
        Settings::setLatestProviderService("Provider1", "Service7");
        TS_ASSERT(!Settings::isDisplayServiceName());
        Settings::setLatestProviderService("Provider1", "Service8");
        TS_ASSERT_EQUALS(Settings::isDisplayServiceName(), Settings::DefServiceDisplayName);


        TS_TRACE("--- Testing cleanup user certificates");
        Settings::setLatestProviderService("Provider1", "Service2");
        TS_ASSERT(Settings::isCleanupUserCert());
        Settings::setLatestProviderService("Provider1", "Service3");
        TS_ASSERT(Settings::isCleanupUserCert());
        Settings::setLatestProviderService("Provider1", "Service4");
        TS_ASSERT(!Settings::isCleanupUserCert());
        Settings::setLatestProviderService("Provider1", "Service5");
        TS_ASSERT_EQUALS(Settings::isCleanupUserCert(), Settings::DefServiceCleanupUserCert);
        Settings::setLatestProviderService("Provider1", "Service6");
        TS_ASSERT(!Settings::isCleanupUserCert());
        Settings::setLatestProviderService("Provider1", "Service7");
        TS_ASSERT(!Settings::isCleanupUserCert());
        Settings::setLatestProviderService("Provider1", "Service8");
        TS_ASSERT(!Settings::isCleanupUserCert());

        TS_TRACE("--- Testing CertChain");
        Settings::setLatestProviderService("Provider1", "Service2");
        TS_ASSERT_EQUALS(Settings::isCertChain(), Settings::DefIsCertChain);
        Settings::setLatestProviderService("Provider1", "Service3");
        TS_ASSERT_THROWS(Settings::isCertChain(), SettingsError);
        Settings::setLatestProviderService("Provider1", "Service4");
        TS_ASSERT(!Settings::isCertChain());
        Settings::setLatestProviderService("Provider1", "Service5");
        TS_ASSERT(Settings::isCertChain());
        Settings::setLatestProviderService("Provider1", "Service6");
        TS_ASSERT(!Settings::isCertChain());
        Settings::setLatestProviderService("Provider1", "Service7");
        TS_ASSERT_EQUALS(Settings::isCertChain(), Settings::DefIsCertChain);
        Settings::setLatestProviderService("Provider1", "Service8");
        TS_ASSERT(Settings::isCertChain());


        TS_TRACE("--- Testing CertValidPercent");
        Settings::setLatestProviderService("Provider1", "Service2");
        TS_ASSERT_EQUALS(Settings::getCertValidPercentage(), Settings::DefCertValidPercent);
        Settings::setLatestProviderService("Provider1", "Service3");
        TS_ASSERT_THROWS(Settings::getCertValidPercentage(), SettingsError);
        Settings::setLatestProviderService("Provider1", "Service4");
        TS_ASSERT_EQUALS(Settings::getCertValidPercentage(), 75U);
        Settings::setLatestProviderService("Provider1", "Service5");
        TS_ASSERT_EQUALS(Settings::getCertValidPercentage(), 25U);
        Settings::setLatestProviderService("Provider1", "Service6");
        TS_ASSERT_EQUALS(Settings::getCertValidPercentage(), 25U);
        Settings::setLatestProviderService("Provider1", "Service7");
        TS_ASSERT_THROWS(Settings::getCertValidPercentage(), SettingsError);
        Settings::setLatestProviderService("Provider1", "Service8");
        TS_ASSERT_EQUALS(Settings::getCertValidPercentage(), 99U);

        TS_ASSERT_EQUALS(Settings::getCertValidPercentage("Provider1", "Service2"), Settings::DefCertValidPercent);
        TS_ASSERT_THROWS(Settings::getCertValidPercentage("Provider1", "Service3"), SettingsError);
        TS_ASSERT_EQUALS(Settings::getCertValidPercentage("Provider1", "Service4"), 75U);
        TS_ASSERT_EQUALS(Settings::getCertValidPercentage("Provider1", "Service5"), 25U);
        TS_ASSERT_EQUALS(Settings::getCertValidPercentage("Provider1", "Service6"), 25U);
        TS_ASSERT_THROWS(Settings::getCertValidPercentage("Provider1", "Service7"), SettingsError);// that's not PHP, baby
        TS_ASSERT_EQUALS(Settings::getCertValidPercentage("Provider1", "Service8"), 99U);

        TS_TRACE("--- Testing CertFormat");
        Settings::setLatestProviderService("Provider1", "Service2");
        TS_ASSERT_EQUALS(Settings::getCertFormat(), Settings::DefCertFormat);
        Settings::setLatestProviderService("Provider1", "Service3");
        TS_ASSERT_THROWS(Settings::getCertFormat(), SettingsError);
        Settings::setLatestProviderService("Provider1", "Service4");
        TS_ASSERT_EQUALS(Settings::getCertFormat(), resept::certformatPem);
        Settings::setLatestProviderService("Provider1", "Service5");
        TS_ASSERT_EQUALS(Settings::getCertFormat(), resept::certformatP12);
        Settings::setLatestProviderService("Provider1", "Service6");
        TS_ASSERT_EQUALS(Settings::getCertFormat(), resept::certformatP12);
        Settings::setLatestProviderService("Provider1", "Service7");
        TS_ASSERT_EQUALS(Settings::getCertFormat(), Settings::DefCertFormat);
        Settings::setLatestProviderService("Provider1", "Service8");
        TS_ASSERT_EQUALS(Settings::getCertFormat(), resept::certformatPem);


        TS_TRACE("--- Testing get/set Service URI");
        Settings::setLatestProviderService("Provider1", "Service1");
        TS_ASSERT_EQUALS(Settings::getServiceUri(), "https://siouxdemo.trustalert.com/");
        TS_ASSERT_EQUALS(Settings::getServiceUri("Provider1", "Service1"), Settings::getServiceUri());
        TS_ASSERT_EQUALS(Settings::getServiceUri("Provider1", "Service2"), "HTTPs://siouxdemo.trustalert.com:443");
        TS_ASSERT_EQUALS(Settings::getServiceUri("Provider1", "Service8"), "https://p1s8-default.com");
        Settings::setServiceUri("https://siouxdemo.trustalert.com");
        TS_ASSERT_EQUALS(Settings::getServiceUri(), "https://siouxdemo.trustalert.com");
        TS_ASSERT_EQUALS(Settings::getServiceUri("Provider2", "Service1"), "https://p2s1-default.com");
        TS_ASSERT_EQUALS(Settings::getServiceUri("Provider3", "Service1"), "https://p3s1-default.com");

        TS_TRACE("--- Testing Service URI with getProviderServiceForRequestedUri");
        TS_ASSERT_EQUALS(Settings::getProviderServiceForRequestedUri("https://siouxdemo.trustalert.com", SettingsTestUtils::UrlMatch).size(), 6U);
        Settings::setLatestProviderService("Provider1", "Service1");
        Settings::setServiceUri("https://nu.nl");
        TS_ASSERT_EQUALS(Settings::getProviderServiceForRequestedUri("https://siouxdemo.trustalert.com", SettingsTestUtils::UrlMatch).size(), 5U);
        Settings::setServiceUri("htTps://siouxdemo.trustalert.com:443/#fragment");
        TS_ASSERT_EQUALS(Settings::getProviderServiceForRequestedUri("https://siouxdemo.trustalert.com", SettingsTestUtils::UrlMatch).size(), 6U);
        Settings::setServiceUri(""); // Test empty URIs are handled correctly (URI can be empty if nothing needs to be invoked after authentication)
        TS_ASSERT_EQUALS(Settings::getProviderServiceForRequestedUri("https://siouxdemo.trustalert.com", SettingsTestUtils::UrlMatch).size(), 5U);

        TS_TRACE("--- Testing get/set imported user certificates");
        // given
        Settings::setLatestProviderService("Provider1", "Service1");
        // when-then
        TS_ASSERT_EQUALS(Settings::getImportedUserCertFingerprints(), std::vector<std::string>());
        TS_ASSERT_EQUALS(Settings::getImportedUserCertFingerprints("Provider1", "Service1"), std::vector<std::string>());
        // when
        Settings::addImportedUserCertFingerprint("111111");
        // then
        TS_ASSERT_EQUALS(Settings::getImportedUserCertFingerprints(), list_of("111111"));
        TS_ASSERT_EQUALS(Settings::getImportedUserCertFingerprints("Provider1", "Service1"), list_of("111111"));
        // when
        Settings::addImportedUserCertFingerprint("111111");
        Settings::addImportedUserCertFingerprint("222222");
        // then
        TS_ASSERT_EQUALS(Settings::getImportedUserCertFingerprints(), list_of("111111")("222222"));
        TS_ASSERT_EQUALS(Settings::getImportedUserCertFingerprints("Provider1", "Service1"), list_of("111111")("222222"));
        // when
        Settings::removeImportedUserCertFingerprints(list_of("111111")("333333"));
        TS_ASSERT_EQUALS(Settings::getImportedUserCertFingerprints(), list_of("222222"));
        TS_ASSERT_EQUALS(Settings::getImportedUserCertFingerprints("Provider1", "Service1"), list_of("222222"));
        // when
        Settings::removeImportedUserCertFingerprints(list_of("222222")("333333"));
        // then
        TS_ASSERT_EQUALS(Settings::getImportedUserCertFingerprints(), std::vector<std::string>());
        TS_ASSERT_EQUALS(Settings::getImportedUserCertFingerprints("Provider1", "Service1"), std::vector<std::string>());
        // when
        Settings::removeImportedUserCertFingerprints(list_of("222222")("333333"));
        // then
        TS_ASSERT_EQUALS(Settings::getImportedUserCertFingerprints(), std::vector<std::string>());
        TS_ASSERT_EQUALS(Settings::getImportedUserCertFingerprints("Provider1", "Service1"), std::vector<std::string>());
    }

    void testUserSettings()
    {
        using namespace rclient;
        using boost::assign::list_of;

        TS_TRACE("--- Testing read users");
        bool myFromMasterConfig = false;

        Settings::setLatestProviderService("Provider1", "Service1");
        TS_ASSERT_EQUALS(Settings::getUsers(myFromMasterConfig), list_of("DemoUser2")("DemoUser3"));
        TS_ASSERT(myFromMasterConfig);
        TS_ASSERT_EQUALS(Settings::getUsers("Provider1", "Service1", myFromMasterConfig), Settings::getUsers(myFromMasterConfig));
        TS_ASSERT(myFromMasterConfig);
        TS_ASSERT(Settings::getUsers("Provider1", "Service2", myFromMasterConfig). empty());
        TS_ASSERT_EQUALS(Settings::getUsers("Provider1", "Service3", myFromMasterConfig), list_of("DemoUser"));
        TS_ASSERT(!myFromMasterConfig);
        TS_ASSERT(Settings::getUsers("Provider1", "Service4", myFromMasterConfig).empty());
        TS_ASSERT(myFromMasterConfig);
        TS_ASSERT_EQUALS(Settings::getUsers("Provider1", "Service5", myFromMasterConfig), list_of(" Анди")("ДЕМО_/@.'\\€ -"));
        TS_ASSERT(!myFromMasterConfig);
        TS_ASSERT_EQUALS(Settings::getUsers("Provider1", "Service6", myFromMasterConfig), list_of("DemoUser"));
        TS_ASSERT(!myFromMasterConfig);
        TS_ASSERT(Settings::getUsers("Provider1", "Service7", myFromMasterConfig).empty());
        TS_ASSERT(!myFromMasterConfig);
        TS_ASSERT(Settings::getUsers("Provider1", "Service8", myFromMasterConfig).empty());
        TS_ASSERT(!myFromMasterConfig);


        TS_TRACE("--- Testing add/remove users");
        Settings::setLatestProviderService("Provider1", "Service3");
        Settings::removeUsers();
        TS_ASSERT(Settings::getUsers(myFromMasterConfig).empty());
        TS_ASSERT(!myFromMasterConfig);

        Settings::addUser("NewDemoUser1");
        TS_ASSERT_EQUALS(Settings::getUsers(myFromMasterConfig), list_of("NewDemoUser1"));
        TS_ASSERT(!myFromMasterConfig);

        Settings::addUser("NewDemoUser2");
        TS_ASSERT_EQUALS(Settings::getUsers(myFromMasterConfig), list_of("NewDemoUser1")("NewDemoUser2"));
        TS_ASSERT(!myFromMasterConfig);
        TS_ASSERT_THROWS(Settings::addUser("NewDemoUser1"), SettingsError);
        TS_ASSERT_THROWS(Settings::addUser("NewDemoUser2"), SettingsError);
        TS_ASSERT_EQUALS(Settings::getUsers(myFromMasterConfig), list_of("NewDemoUser1")("NewDemoUser2"));

        Settings::removeUsers();
        TS_ASSERT(Settings::getUsers(myFromMasterConfig).empty());
        TS_ASSERT(!myFromMasterConfig);
        Settings::removeUsers();
        TS_ASSERT(Settings::getUsers(myFromMasterConfig).empty());
        TS_ASSERT(!myFromMasterConfig);


        Settings::addUser("Provider1", "Service2", "FirstUser");
        TS_ASSERT_EQUALS(Settings::getUsers("Provider1", "Service2", myFromMasterConfig), list_of("FirstUser"));
        TS_ASSERT(!myFromMasterConfig);
        TS_ASSERT_THROWS(Settings::addUser("Provider1", "Service2", "FirstUser"), SettingsError);
        TS_ASSERT_EQUALS(Settings::getUsers("Provider1", "Service2", myFromMasterConfig), list_of("FirstUser"));

        Settings::addUser("Provider1", "Service8", "FirstUser");
        TS_ASSERT_EQUALS(Settings::getUsers("Provider1", "Service2", myFromMasterConfig), list_of("FirstUser"));
        TS_ASSERT(!myFromMasterConfig);
        Settings::addUser("Provider1", "Service8", "SecondUser");
        TS_ASSERT_EQUALS(Settings::getUsers("Provider1", "Service8"), list_of("FirstUser")("SecondUser"));
        Settings::removeUsers("Provider1", "Service8");
        TS_ASSERT(Settings::getUsers("Provider1", "Service8").empty());

        TS_TRACE("--- Testing that adding/removing users has no effect when overriden by master config");
        Settings::setLatestProviderService("Provider1", "Service1");
        Settings::removeUsers();
        TS_ASSERT_EQUALS(Settings::getUsers(myFromMasterConfig), list_of("DemoUser2")("DemoUser3"));
        TS_ASSERT(myFromMasterConfig);
        TS_ASSERT_EQUALS(Settings::getUsers("Provider1", "Service1", myFromMasterConfig), list_of("DemoUser2")("DemoUser3"));
        TS_ASSERT(myFromMasterConfig);

        Settings::addUser("NewUser");
        TS_ASSERT_EQUALS(Settings::getUsers(myFromMasterConfig), list_of("DemoUser2")("DemoUser3"));
        TS_ASSERT(myFromMasterConfig);

    }

    void testRecoverMissingUserConfigFromMaster()
    {
        using namespace rclient;
        using boost::assign::list_of;

        TS_TRACE("--- Test that missing user config is recovered from the master");
        TS_ASSERT(ta::isFileExist("user.ini"));
        TS_ASSERT(ta::isFileExist("master.ini"));
        TS_ASSERT_EQUALS(Settings::getProviders(), list_of("Provider1")("Provider2")("Provider4")("Provider3"));
        TS_ASSERT_EQUALS(Settings::getServices("Provider1"), list_of("Service1")("Service2")("Service3")("Service4")("Service5")("Service6")("Service7")("Service8"));
        TS_ASSERT_EQUALS(Settings::getServices("Provider2"), list_of("Service1"));
        TS_ASSERT_EQUALS(Settings::getServices("Provider3"), list_of("Service1"));
        TS_ASSERT_EQUALS(Settings::getServices("Provider4"), list_of("Service1"));
        TS_ASSERT_EQUALS(Settings::getLatestProvider(), "Provider1");
        TS_ASSERT_EQUALS(Settings::getLatestService(), "Service5");

        SettingsTestUtils::removeFile("user.ini");
        TS_ASSERT_EQUALS(Settings::getProviders(), list_of("Provider1")("Provider2")("Provider3"));
        TS_ASSERT(ta::isFileExist("user.ini"));
        TS_ASSERT_EQUALS(Settings::getServices("Provider1"), list_of("Service1")("Service2")("Service3")("Service4")("Service5")("Service6")("Service7")("Service8"));
        TS_ASSERT_EQUALS(Settings::getServices("Provider2"), list_of("Service1"));
        TS_ASSERT_EQUALS(Settings::getServices("Provider3"), list_of("Service1"));
        TS_ASSERT_THROWS(Settings::getServices("Provider4"), SettingsError);
        TS_ASSERT_EQUALS(Settings::getLatestProvider(), "Provider1");
        TS_ASSERT_EQUALS(Settings::getLatestService(), "Service1");

    }

    void testRecoverCorruptedUserConfigFromMaster()
    {
        using namespace rclient;
        using boost::assign::list_of;

        TS_TRACE("--- Test that missing user config is recovered from the master");
        TS_ASSERT(ta::isFileExist("user.ini"));
        TS_ASSERT_EQUALS(Settings::getProviders(), list_of("Provider1")("Provider2")("Provider4")("Provider3"));
        TS_ASSERT_EQUALS(Settings::getServices("Provider1"), list_of("Service1")("Service2")("Service3")("Service4")("Service5")("Service6")("Service7")("Service8"));
        TS_ASSERT_EQUALS(Settings::getServices("Provider2"), list_of("Service1"));
        TS_ASSERT_EQUALS(Settings::getServices("Provider3"), list_of("Service1"));
        TS_ASSERT_EQUALS(Settings::getServices("Provider4"), list_of("Service1"));
        TS_ASSERT_EQUALS(Settings::getLatestProvider(), "Provider1");
        TS_ASSERT_EQUALS(Settings::getLatestService(), "Service5");

        ta::writeData("user.ini", string("ConfigVersion = \"999999999999.0\";")); // bad version
        TS_ASSERT_EQUALS(Settings::getProviders(), list_of("Provider1")("Provider2")("Provider3"));
        TS_ASSERT_EQUALS(Settings::getServices("Provider1"), list_of("Service1")("Service2")("Service3")("Service4")("Service5")("Service6")("Service7")("Service8"));
        TS_ASSERT_EQUALS(Settings::getServices("Provider2"), list_of("Service1"));
        TS_ASSERT_EQUALS(Settings::getServices("Provider3"), list_of("Service1"));
        TS_ASSERT_THROWS(Settings::getServices("Provider4"), SettingsError);
        TS_ASSERT_EQUALS(Settings::getLatestProvider(), "Provider1");
        TS_ASSERT_EQUALS(Settings::getLatestService(), "Service1");

    }
};


// Suite to test config files generation
class GenSettingsTest : public CxxTest::TestSuite
{
public:
    void setUp()
    {
        CxxTest::setAbortTestOnFail(true);
        SettingsTestUtils::removeFile("master.ini");
        SettingsTestUtils::removeFile("master.yaml");
        SettingsTestUtils::removeFile("user.ini");
        SettingsTestUtils::removeFile("user.yaml");
        rclient::Settings::setConfigsPath("resept.ini", "user.ini", "master.ini");
        CxxTest::setAbortTestOnFail(false);
    }
    void tearDown()
    {
        try
        {
            rclient::Settings::resetConfigsPath();
            SettingsTestUtils::removeFile("master.ini");
            SettingsTestUtils::removeFile("master.yaml");
            SettingsTestUtils::removeFile("user.ini");
            SettingsTestUtils::removeFile("user.yaml");
        }
        catch(std::exception& e)
        {
            TS_TRACE(e.what());
        }
        catch(...)
        {
            TS_TRACE("tearDown() failed with unknown error");
        }
    }


    void traceFileContent(const std::string& aFileName)
    {
#ifdef SETTINGS_TEST_DEBUG
        const std::string myContent = ta::readData(aFileName);
        TS_TRACE((aFileName + ":\n" + myContent).c_str());
#endif
    }

    void testGenerateAdminConfigsWithoutRca()
    {
        using namespace rclient;
        using boost::assign::list_of;

        TS_ASSERT_THROWS(Settings::getProviders(), SettingsError);

        // given
        static const bool AllowOverwriteYes = true;
        static const bool AllowOverwriteNo  = false;

        Settings::RccdRequestData myReq;
        myReq.providerName = "p1";
        myReq.contentVersion = 2010080401;
        myReq.svrAddress =  ta::NetUtils::RemoteAddress("test.keytalk.com", 80);
        myReq.allowOverwriteSvrAddress = AllowOverwriteYes;
        myReq.signingCaPem = ta::readData("signingcacert.pem");
        myReq.commCaPem = ta::readData("commcacert.pem");
        myReq.pcaPem = ta::readData("pcacert.pem");
        const Settings::RccdRequestData::Service service1("s1",
                                                                "https://s1.com", // uri
                                                                 11, AllowOverwriteNo, // cert validity percentage
                                                                 list_of("s1u1")("s1u2"), AllowOverwriteNo);
        const Settings::RccdRequestData::Service service2("s2",
                                                                "https://s2.com", // uri
                                                                 12, AllowOverwriteYes, // cert validity percentage
                                                                 list_of("s2u1"), AllowOverwriteYes);
        myReq.services = list_of(service1)(service2);

        // when
        bool myIsAdminRccd = Settings::generateConfigs(myReq, "user.ini", "user.yaml", "master.ini", "master.yaml");

        // then
        TS_ASSERT(myIsAdminRccd);
        TS_ASSERT(ta::isFileExist("master.ini"));
        TS_ASSERT(ta::isFileExist("master.yaml"));
        TS_ASSERT(ta::isFileExist("user.ini"));
        TS_ASSERT(ta::isFileExist("user.yaml"));
        // just visually inspect YAML configs since we do not have API to read them for proper testing
        traceFileContent("master.yaml");
        traceFileContent("user.yaml");
        TS_ASSERT_EQUALS(Settings::getLatestProvider(), myReq.providerName);
        TS_ASSERT_EQUALS(Settings::getProviders(), list_of(myReq.providerName));
        TS_ASSERT_EQUALS(Settings::getProviderContentVersion(myReq.providerName), myReq.contentVersion);
        TS_ASSERT_EQUALS(Settings::getUserCaName(), "KeyTalk Demo Signing CA");
        TS_ASSERT_EQUALS(Settings::getServerCaName(), "KeyTalk Demo Communication CA");
        TS_ASSERT_EQUALS(Settings::getPrimaryCaName(), "KeyTalk Demo Primary CA");
        TS_ASSERT(!Settings::isRootCaExist());
        TS_ASSERT_EQUALS(Settings::getLogLevel(), Settings::DefLogLevel);
        TS_ASSERT_EQUALS(Settings::getReseptSvrAddress(), myReq.svrAddress);

        TS_ASSERT_EQUALS(Settings::getServices(), list_of(service1.name)(service2.name));
        foreach (const Settings::RccdRequestData::Service service, myReq.services)
        {
            TS_ASSERT(!Settings::isCertChain(myReq.providerName, service.name));
            TS_ASSERT_EQUALS(Settings::getCertValidPercentage(myReq.providerName, service.name), service.certValidityPercentage);
            TS_ASSERT_EQUALS(Settings::getCertFormat(myReq.providerName, service.name), resept::certformatP12);
            TS_ASSERT_EQUALS(Settings::getServiceUri(myReq.providerName, service.name), service.uri);
            TS_ASSERT_EQUALS(Settings::getUsers(myReq.providerName, service.name), service.users);
        }

        // test that master config does enforce settings
        bool myIsFromMasterConfig = false;
        // when
        Settings::addUser(myReq.providerName, service1.name, "s1u_new");
        // then users stay intact because they are marked as 'not-overwrite'
        TS_ASSERT_EQUALS(Settings::getUsers(myReq.providerName, service1.name, myIsFromMasterConfig), service1.users);
        TS_ASSERT(myIsFromMasterConfig);

        // test that master config is capable of recovering of user settings giving defaults for the settings we did not define above
        // when
        SettingsTestUtils::removeFile("user.ini");
        // then
        TS_ASSERT_EQUALS(Settings::getLatestProvider(), myReq.providerName);
        TS_ASSERT_EQUALS(Settings::getProviders(), list_of(myReq.providerName));
        TS_ASSERT_EQUALS(Settings::getProviderContentVersion(myReq.providerName), myReq.contentVersion);
        TS_ASSERT_EQUALS(Settings::getUserCaName(), "KeyTalk Demo Signing CA"); // restored from the default CAs we supplied
        TS_ASSERT_EQUALS(Settings::getServerCaName(), "KeyTalk Demo Communication CA"); // restored from the default CAs we supplied
        TS_ASSERT_EQUALS(Settings::getPrimaryCaName(), "KeyTalk Demo Primary CA"); // restored from the default CAs we supplied
        TS_ASSERT(!Settings::isRootCaExist()); // restored from the default CAs we supplied
        TS_ASSERT_EQUALS(Settings::getReseptSvrAddress(), myReq.svrAddress); // restored from the default server address we supplied
        TS_ASSERT_EQUALS(Settings::getLogLevel(), Settings::DefLogLevel); // default
        TS_ASSERT_EQUALS(Settings::isDisplayServiceName(), Settings::DefServiceDisplayName); // default
        TS_ASSERT_EQUALS(Settings::isCleanupUserCert(), Settings::DefServiceCleanupUserCert); // default

        TS_ASSERT_EQUALS(Settings::getServices(), list_of(service1.name)(service2.name));
        foreach (const Settings::RccdRequestData::Service service, myReq.services)
        {
            TS_ASSERT_EQUALS(Settings::getServiceUri(myReq.providerName, service.name), service.uri); // restored from the default URI we supplied
            TS_ASSERT_EQUALS(Settings::isCertChain(myReq.providerName, service.name), Settings::DefIsCertChain);// default
            TS_ASSERT_EQUALS(Settings::getCertFormat(myReq.providerName, service.name), Settings::DefCertFormat);// default

            if (service.allowOverwriteCertValidityPercentage) {
                TS_ASSERT_EQUALS(Settings::getCertValidPercentage(myReq.providerName, service.name), Settings::DefCertValidPercent);// default
            } else {
                TS_ASSERT_EQUALS(Settings::getCertValidPercentage(myReq.providerName, service.name), service.certValidityPercentage);
            }

            if (service.allowOverwriteUsers) {
                TS_ASSERT_EQUALS(Settings::getUsers(myReq.providerName, service.name).size(), 0U); // default
            } else {
                TS_ASSERT_EQUALS(Settings::getUsers(myReq.providerName, service.name), service.users);
            }
        }
    }

    void testGenerateAdminConfigsWithRca()
    {
        using namespace rclient;
        using boost::assign::list_of;

        // given
        static const bool AllowOverwriteYes = true;
        static const bool AllowOverwriteNo  = false;

        TS_TRACE("--- Test generate admin configs (with RCA)");

        // given
        Settings::RccdRequestData myReq;
        myReq.providerName = "p2";
        myReq.contentVersion = 2010080402;
        myReq.svrAddress =  ta::NetUtils::RemoteAddress("test2.keytalk.com", 8080);
        myReq.allowOverwriteSvrAddress = AllowOverwriteNo;
        myReq.signingCaPem = ta::readData("signingcacert.pem");
        myReq.commCaPem = ta::readData("commcacert.pem");
        myReq.pcaPem = ta::readData("pcacert.pem");
        myReq.rcaPem = ta::readData("pcacert.pem"); // somewhat dirty trick to avoid creating a new cert tree
        const Settings::RccdRequestData::Service service("s3",
                                                        "https://s3.com",
                                                        13, AllowOverwriteYes,
                                                        list_of("s3u1")("s3u2"), AllowOverwriteNo);
        myReq.services = list_of(service);

        // when
        const bool myIsAdminRccd = Settings::generateConfigs(myReq, "user.ini", "user.yaml", "master.ini", "master.yaml");

        // then
        TS_ASSERT(myIsAdminRccd);
        TS_ASSERT(ta::isFileExist("master.ini"));
        TS_ASSERT(ta::isFileExist("master.yaml"));
        TS_ASSERT(ta::isFileExist("user.ini"));
        TS_ASSERT(ta::isFileExist("user.yaml"));
        // just visually inspect YAML configs since we do not have API to read them for proper testing
        traceFileContent("master.yaml");
        traceFileContent("user.yaml");
        TS_ASSERT_EQUALS(Settings::getLatestProvider(), myReq.providerName);
        TS_ASSERT_EQUALS(Settings::getProviders(), list_of(myReq.providerName));
        TS_ASSERT_EQUALS(Settings::getProviderContentVersion(myReq.providerName), myReq.contentVersion);
        TS_ASSERT_EQUALS(Settings::getUserCaName(), "KeyTalk Demo Signing CA");
        TS_ASSERT_EQUALS(Settings::getServerCaName(), "KeyTalk Demo Communication CA");
        TS_ASSERT_EQUALS(Settings::getPrimaryCaName(), "KeyTalk Demo Primary CA");
        TS_ASSERT_EQUALS(Settings::getRootCaName(), "KeyTalk Demo Primary CA");
        TS_ASSERT(Settings::isRootCaExist());
        TS_ASSERT_EQUALS(Settings::getLogLevel(), "DEBUG");
        TS_ASSERT_EQUALS(Settings::getReseptSvrAddress(), myReq.svrAddress);

        TS_ASSERT_EQUALS(Settings::getServices(), list_of(service.name));
        foreach (const Settings::RccdRequestData::Service service, myReq.services)
        {
            TS_ASSERT(!Settings::isCertChain(myReq.providerName, service.name));
            TS_ASSERT_EQUALS(Settings::getCertValidPercentage(myReq.providerName, service.name), service.certValidityPercentage);
            TS_ASSERT_EQUALS(Settings::getCertFormat(myReq.providerName, service.name), resept::certformatP12);
            TS_ASSERT_EQUALS(Settings::getServiceUri(myReq.providerName, service.name), service.uri);
            TS_ASSERT_EQUALS(Settings::getUsers(myReq.providerName, service.name), service.users);
        }

        // when, test that master config does enforce settings
        Settings::removeUsers();
        bool myIsFromMasterConfig = false;
        // then
        TS_ASSERT_EQUALS(Settings::getUsers(myIsFromMasterConfig), service.users);
        TS_ASSERT(myIsFromMasterConfig);
    }

    void testGenerateAdminConfigsInvalidUsage()
    {
        using namespace rclient;
        using boost::assign::list_of;

        // given
        static const bool AllowOverwriteYes = true;
        static const bool AllowOverwriteNo  = false;

        TS_TRACE("--- Test generate admin configs (with RCA)");

        // given
        Settings::RccdRequestData myReq;
        myReq.providerName = "p2";
        myReq.contentVersion = 2010080402;
        myReq.svrAddress =  ta::NetUtils::RemoteAddress("test2.keytalk.com", 8080);
        myReq.allowOverwriteSvrAddress = AllowOverwriteYes;
        myReq.signingCaPem = ta::readData("signingcacert.pem");
        myReq.commCaPem = ta::readData("commcacert.pem");
        myReq.pcaPem = ta::readData("pcacert.pem");
        myReq.rcaPem = ta::readData("pcacert.pem"); // somewhat dirty trick to avoid creating a new cert tree
        const Settings::RccdRequestData::Service service("s3",
                                                         "https://s3.com",
                                                          13, AllowOverwriteYes,
                                                          list_of("s3u1")("s3u2"), AllowOverwriteNo);
        myReq.services = list_of(service);

         // given (no services)
        Settings::RccdRequestData myBadReq = myReq;
        myBadReq.services.clear();
        // when-then
        TS_ASSERT_THROWS(Settings::generateConfigs(myBadReq, "user.ini", "user.yaml", "master.ini", "master.yaml"), std::exception);

        // given (invalid CA)
        myBadReq = myReq;
        myBadReq.commCaPem.clear();
        // when-then
        TS_ASSERT_THROWS(Settings::generateConfigs(myBadReq, "user.ini", "user.yaml", "master.ini", "master.yaml"), std::exception);

        // given-when-then (invalid output file name)
        TS_ASSERT_THROWS(Settings::generateConfigs(myReq, "", "user.yaml", "master.ini", "master.yaml"), std::exception);
        TS_ASSERT_THROWS(Settings::generateConfigs(myReq, "user.ini", "", "", "master.yaml"), std::exception);
        TS_ASSERT_THROWS(Settings::generateConfigs(myReq, "user.ini", "user.yaml", "", "master.yaml"), std::exception);
        TS_ASSERT_THROWS(Settings::generateConfigs(myReq, "user.ini", "user.yaml", "master.ini", ""), std::exception);
    }

    void testGenerateUserConfig()
    {
        using namespace rclient;
        using boost::assign::list_of;

        TS_TRACE("--- Test generate user configs (no RCA)");
        TS_ASSERT_THROWS(Settings::getProviders(), SettingsError);

        // given
        static const bool AllowOverwriteYes = true;
        Settings::RccdRequestData myReq;
        myReq.providerName = "p1";
        myReq.contentVersion = 2010080401;
        myReq.svrAddress =  ta::NetUtils::RemoteAddress("test.keytalk.com", 80);
        myReq.signingCaPem = ta::readData("signingcacert.pem");
        myReq.commCaPem = ta::readData("commcacert.pem");
        myReq.pcaPem = ta::readData("pcacert.pem");
        const Settings::RccdRequestData::Service service1("s1",
                                                         "https://s1.com",
                                                         11, AllowOverwriteYes,
                                                         list_of("s1u1")("s1u2"), AllowOverwriteYes);
        const Settings::RccdRequestData::Service service2("s2",
                                                         "https://s2.com",
                                                         12, AllowOverwriteYes,
                                                         vector<string>(), AllowOverwriteYes);
        myReq.services = list_of(service1)(service2);

        // when
        bool myIsAdminRccd = Settings::generateConfigs(myReq, "user.ini", "user.yaml");

        // then
        TS_ASSERT(!myIsAdminRccd);
        TS_ASSERT(!ta::isFileExist("master.ini"));
        TS_ASSERT(!ta::isFileExist("master.yaml"));
        TS_ASSERT(ta::isFileExist("user.ini"));
        TS_ASSERT(ta::isFileExist("user.yaml"));
        // just visually inspect YAML configs since we do not have API to read them for proper testing
        traceFileContent("user.yaml");
        TS_ASSERT_EQUALS(Settings::getLatestProvider(), myReq.providerName);
        TS_ASSERT_EQUALS(Settings::getProviders(), list_of(myReq.providerName));
        TS_ASSERT_EQUALS(Settings::getProviderContentVersion(myReq.providerName), myReq.contentVersion);
        TS_ASSERT_EQUALS(Settings::getUserCaName(), "KeyTalk Demo Signing CA");
        TS_ASSERT_EQUALS(Settings::getServerCaName(), "KeyTalk Demo Communication CA");
        TS_ASSERT_EQUALS(Settings::getPrimaryCaName(), "KeyTalk Demo Primary CA");
        TS_ASSERT(!Settings::isRootCaExist());
        TS_ASSERT_EQUALS(Settings::getLogLevel(), "DEBUG");
        TS_ASSERT_EQUALS(Settings::getReseptSvrAddress(), myReq.svrAddress);

        TS_ASSERT_EQUALS(Settings::getServices(), list_of(service1.name)(service2.name));
        foreach (const Settings::RccdRequestData::Service& service, myReq.services)
        {
            TS_ASSERT(!Settings::isCertChain(myReq.providerName, service.name));
            TS_ASSERT_EQUALS(Settings::getCertValidPercentage(myReq.providerName, service.name), service.certValidityPercentage);
            TS_ASSERT_EQUALS(Settings::getCertFormat(myReq.providerName, service.name), resept::certformatP12);
            TS_ASSERT_EQUALS(Settings::getServiceUri(myReq.providerName, service.name), service.uri);
            TS_ASSERT_EQUALS(Settings::getUsers(myReq.providerName, service.name), service.users);
        }


        TS_TRACE("--- Test generate user configs (with RCA)");

        // given
        myReq = Settings::RccdRequestData();
        myReq.providerName = "p2";
        myReq.contentVersion = 2010080402;
        myReq.svrAddress =  ta::NetUtils::RemoteAddress("test2.keytalk.com", 8080);
        myReq.signingCaPem = ta::readData("signingcacert.pem");
        myReq.commCaPem = ta::readData("commcacert.pem");
        myReq.pcaPem = ta::readData("pcacert.pem");
        myReq.rcaPem = ta::readData("pcacert.pem"); // somewhat dirty trick to avoid creating a new cert tree
        const Settings::RccdRequestData::Service service3("s3",
                                                         "https://s3.com",
                                                          13, AllowOverwriteYes,
                                                          list_of("s3u1")("s3u2"), AllowOverwriteYes);
        myReq.services = list_of(service3);

        // when
        myIsAdminRccd = Settings::generateConfigs(myReq, "user.ini", "user.yaml");

        // then
        TS_ASSERT(!myIsAdminRccd);
        TS_ASSERT(!ta::isFileExist("master.ini"));
        TS_ASSERT(!ta::isFileExist("master.yaml"));
        TS_ASSERT(ta::isFileExist("user.ini"));
        TS_ASSERT(ta::isFileExist("user.yaml"));
        // just visually inspect YAML configs since we do not have API to read them for proper testing
        traceFileContent("user.yaml");
        TS_ASSERT_EQUALS(Settings::getLatestProvider(), myReq.providerName);
        TS_ASSERT_EQUALS(Settings::getProviders(), list_of(myReq.providerName));
        TS_ASSERT_EQUALS(Settings::getProviderContentVersion(myReq.providerName), myReq.contentVersion);
        TS_ASSERT_EQUALS(Settings::getUserCaName(), "KeyTalk Demo Signing CA");
        TS_ASSERT_EQUALS(Settings::getServerCaName(), "KeyTalk Demo Communication CA");
        TS_ASSERT_EQUALS(Settings::getPrimaryCaName(), "KeyTalk Demo Primary CA");
        TS_ASSERT_EQUALS(Settings::getRootCaName(), "KeyTalk Demo Primary CA");
        TS_ASSERT(Settings::isRootCaExist());
        TS_ASSERT_EQUALS(Settings::getLogLevel(), "DEBUG");
        TS_ASSERT_EQUALS(Settings::getReseptSvrAddress(), myReq.svrAddress);

        TS_ASSERT_EQUALS(Settings::getServices(), list_of(service3.name));
        foreach (const Settings::RccdRequestData::Service service, myReq.services)
        {
            TS_ASSERT(!Settings::isCertChain(myReq.providerName, service.name));
            TS_ASSERT_EQUALS(Settings::getCertValidPercentage(myReq.providerName, service.name), service.certValidityPercentage);
            TS_ASSERT_EQUALS(Settings::getCertFormat(myReq.providerName, service.name), resept::certformatP12);
            TS_ASSERT_EQUALS(Settings::getServiceUri(myReq.providerName, service.name), service.uri);
            TS_ASSERT_EQUALS(Settings::getUsers(myReq.providerName, service.name), service.users);
        }

        TS_TRACE("--- Test generate user configs for invalid input");

         // given (no services)
        Settings::RccdRequestData myBadReq = myReq;
        myBadReq.services.clear();
        // when-then
        TS_ASSERT_THROWS(Settings::generateConfigs(myBadReq, "user.ini", "user.yaml"), std::exception);

        // given (invalid CA)
        myBadReq = myReq;
        myBadReq.commCaPem.clear();
        // when-then
        TS_ASSERT_THROWS(Settings::generateConfigs(myBadReq, "user.ini", "user.yaml"), std::exception);
    }
};

