#pragma once

#include "rclient/Settings.h"
#include "rclient/Common.h"
#include "resept/common.h"
#include "ta/utils.h"

#include "cxxtest/TestSuite.h"
#include "boost/assign/list_of.hpp"

#include <string>
#include <vector>
#include <utility>
#include <stdio.h>

// Suite to test provider installation as it is performed by the customization tool
class SettingsTestInstallProvider : public CxxTest::TestSuite
{
    static void removeFile(const std::string& aFile)
    {
        remove(aFile.c_str());
        if (ta::isFileExist(aFile))
        {
            TA_THROW_MSG(std::runtime_error, "Failed to remove file " + aFile);
        }
    }

public:
    void setUp()
    {
        CxxTest::setAbortTestOnFail(true);
        rclient::Settings::setConfigsPath("resept.ini", "user.ini", "master.ini");
        removeFile("user.ini");
        removeFile("user.yaml");
        removeFile("user.ini.gen");
        removeFile("user.yaml.gen");
        removeFile("master.ini");
        removeFile("master.yaml");
        removeFile("master.ini.gen");
        removeFile("master.yaml.gen");
        CxxTest::setAbortTestOnFail(false);
    }
    void tearDown()
    {
        try
        {
            rclient::Settings::resetConfigsPath();
            removeFile("user.ini");
            removeFile("user.yaml");
            removeFile("user.ini.gen");
            removeFile("user.yaml.gen");
            removeFile("master.ini");
            removeFile("master.yaml");
            removeFile("master.ini.gen");
            removeFile("master.yaml.gen");
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

    std::vector<std::string> getProviderNames(const std::vector<rclient::Settings::RccdRequestData>& aRequest) const
    {
        std::vector<std::string> myProviderNames;
        foreach (const rclient::Settings::RccdRequestData& req, aRequest)
        {
            myProviderNames.push_back(req.providerName);
        }
        return myProviderNames;
    }

    void verifyInstalledSettings(const std::vector<rclient::Settings::RccdRequestData>& aRequest, const bool anIsAdminInstallation)
    {
        using namespace rclient;
        using boost::assign::list_of;

        foreach (const Settings::RccdRequestData& provider, aRequest)
        {
            TS_ASSERT(Settings::isCustomized());
            TS_ASSERT(ta::isFileExist("user.ini"));
            if (anIsAdminInstallation)
            {
                TS_ASSERT(ta::isFileExist("master.ini"));
            }
            else
            {
                TS_ASSERT(!ta::isFileExist("master.ini"));
            }

            // check provider settings
            TS_TRACE(("Checking provider " + provider.providerName).c_str());
            TS_ASSERT_EQUALS(Settings::getProviders(), getProviderNames(aRequest));
            bool myFromMasterConfig;
            TS_ASSERT_EQUALS(Settings::getProviderContentVersion(provider.providerName), provider.contentVersion);
            TS_ASSERT_EQUALS(Settings::getReseptSvrAddress(provider.providerName, myFromMasterConfig), provider.svrAddress);
            TS_ASSERT_EQUALS(myFromMasterConfig, !provider.allowOverwriteSvrAddress);
            TS_ASSERT_EQUALS(Settings::getUserCaName(provider.providerName), ta::CertUtils::getCertInfo(provider.signingCaPem).subjCN);
            TS_ASSERT_EQUALS(Settings::getServerCaName(provider.providerName), ta::CertUtils::getCertInfo(provider.commCaPem).subjCN);
            TS_ASSERT_EQUALS(Settings::getPrimaryCaName(provider.providerName), ta::CertUtils::getCertInfo(provider.pcaPem).subjCN);
            TS_ASSERT(!Settings::isRootCaExist(provider.providerName));

            // check service settings
            TS_ASSERT_EQUALS(Settings::getServices(provider.providerName), provider.getServiceNames());
            foreach (const Settings::RccdRequestData::Service& service, provider.services)
            {
                TS_TRACE(("Checking service " + service.name + " for provider " + provider.providerName).c_str());
                TS_ASSERT_EQUALS(Settings::getServiceUri(provider.providerName, service.name), service.uri);
                TS_ASSERT_EQUALS(Settings::getCertValidPercentage(provider.providerName, service.name, myFromMasterConfig), service.certValidityPercentage);
                TS_ASSERT_EQUALS(myFromMasterConfig, !service.allowOverwriteCertValidityPercentage);
                TS_ASSERT_EQUALS(Settings::getUsers(provider.providerName, service.name, myFromMasterConfig), service.users);
                TS_ASSERT_EQUALS(myFromMasterConfig, !service.allowOverwriteUsers);
            }
        }
    }

    rclient::Settings::RccdRequestData createRccdRequest(const bool anIsForAdminInstallation)
    {
        using namespace rclient;
        using boost::assign::list_of;

        static const bool AllowOverwriteYes = true;
        static const bool AllowOverwriteNo  = false;

        Settings::RccdRequestData myReq;
        myReq.providerName = "DemoProvider";
        myReq.contentVersion = 2010080401;
        myReq.svrAddress =  ta::NetUtils::RemoteAddress("test.keytalk.com", 443);
        myReq.allowOverwriteSvrAddress = anIsForAdminInstallation ? AllowOverwriteNo : AllowOverwriteYes;
        myReq.signingCaPem = ta::readData("signingcacert.pem");
        myReq.commCaPem = ta::readData("commcacert.pem");
        myReq.pcaPem = ta::readData("pcacert.pem");
        const Settings::RccdRequestData::Service service1("s1",
                                                        "https://s1.com", // uri
                                                         11, anIsForAdminInstallation ? AllowOverwriteNo : AllowOverwriteYes, // cert validity percentage
                                                         list_of("s1u1")("s1u2"), AllowOverwriteYes);
        const Settings::RccdRequestData::Service service2("s2",
                                                        "https://s2.com", // uri
                                                         12, AllowOverwriteYes, // cert validity percentage
                                                         list_of("s2u1"), anIsForAdminInstallation ? AllowOverwriteNo : AllowOverwriteYes);
        myReq.services = list_of(service1)(service2);

        return myReq;
    }


    //
    // Test cases
    //

    void testAdminInstallProviderOnCleanSystem()
    {
        using namespace rclient;
        using boost::assign::list_of;

        TS_TRACE("--- Test admin installation of a new provider on clean (not customized) system");

        // given (create user and master configs)
        static const bool myIsAdminInstallation = true;
        const Settings::RccdRequestData myReq = createRccdRequest(myIsAdminInstallation);
        TS_ASSERT_EQUALS(Settings::generateConfigs(myReq, "user.ini.gen", "user.yaml.gen", "master.ini.gen", "master.yaml.gen"), myIsAdminInstallation);
        TS_ASSERT(!Settings::isCustomized()); // check we are clean
        TS_ASSERT_THROWS(Settings::getProviders(), SettingsError); // check we are clean

        // when
        Settings::adminInstallProvider("user.ini.gen", "master.ini.gen");

        // then
        verifyInstalledSettings(list_of(myReq), myIsAdminInstallation);
    }

    void testUserInstallProviderOnCleanSystem()
    {
        using namespace rclient;
        using boost::assign::list_of;

        TS_TRACE("--- Test user installation of a new provider on clean (not customized) system");

        // given (create user configs)
        static const bool myIsAdminInstallation = false;
        const Settings::RccdRequestData myReq = createRccdRequest(myIsAdminInstallation);
        TS_ASSERT_EQUALS(Settings::generateConfigs(myReq, "user.ini.gen", "user.yaml.gen"), myIsAdminInstallation);
        TS_ASSERT(!Settings::isCustomized()); // check we are clean
        TS_ASSERT_THROWS(Settings::getProviders(), SettingsError); // check we are clean

        // when
        Settings::userInstallProvider("user.ini.gen");

        // then
        verifyInstalledSettings(list_of(myReq), myIsAdminInstallation);
    }

    void testAdminReinstallExistingProvider()
    {
        using namespace rclient;
        using boost::assign::list_of;

        TS_TRACE("--- Test admin reinstallation of the existing provider");

        // given (create user and master configs)
        static const bool myIsAdminInstallation = true;
        const Settings::RccdRequestData myReq = createRccdRequest(myIsAdminInstallation);
        TS_ASSERT_EQUALS(Settings::generateConfigs(myReq, "user.ini.gen", "user.yaml.gen", "master.ini.gen", "master.yaml.gen"), myIsAdminInstallation);
        TS_ASSERT(!Settings::isCustomized()); // check we are clean
        TS_ASSERT_THROWS(Settings::getProviders(), SettingsError); // check we are clean

        // do a fresh install
        Settings::adminInstallProvider("user.ini.gen", "master.ini.gen");
        verifyInstalledSettings(list_of(myReq), myIsAdminInstallation);

        // do some changes to the initial installation
        TS_ASSERT(!Settings::isLastUserMsgUtcExist(myReq.providerName));
        Settings::setLastUserMsgUtc(myReq.providerName, "666");
        TS_ASSERT(Settings::isLastUserMsgUtcExist(myReq.providerName));
        TS_ASSERT_EQUALS(Settings::getLastUserMsgUtc(myReq.providerName), "666");

        const std::string myServiceName = myReq.services.at(0).name;
        TS_ASSERT(!Settings::getUsers(myReq.providerName, myServiceName).empty());
        Settings::removeUsers(myReq.providerName, myServiceName);
        TS_ASSERT(Settings::getUsers(myReq.providerName, myServiceName).empty());

        // when, reinstall the same provider again
        TS_TRACE("    - Reinstall the same provider again");
        Settings::adminInstallProvider("user.ini.gen", "master.ini.gen");

        // then, new settings should be added, existing settings should be replaced
        verifyInstalledSettings(list_of(myReq), myIsAdminInstallation);
    }

    void testAdminInstallAddNewProvider()
    {
        using namespace rclient;
        using boost::assign::list_of;

        TS_TRACE("--- Test admin installation of a new provider on customized system");

        // given
        static const bool myIsAdminInstallation = true;
        const Settings::RccdRequestData myProv1Req = createRccdRequest(myIsAdminInstallation);
        // create another request, make sure it differs from the first one
        Settings::RccdRequestData myProv2Req = myProv1Req;
        myProv2Req.providerName += ".1";
        myProv2Req.contentVersion += 1;
        myProv2Req.svrAddress.port += 1;
        myProv2Req.services.pop_back();

        TS_ASSERT(!Settings::isCustomized()); // check we are clean
        TS_ASSERT_THROWS(Settings::getProviders(), SettingsError); // check we are clean

        // when, install both providers
        TS_ASSERT_EQUALS(Settings::generateConfigs(myProv1Req, "user.ini.gen", "user.yaml.gen", "master.ini.gen", "master.yaml.gen"), myIsAdminInstallation);
        Settings::adminInstallProvider("user.ini.gen", "master.ini.gen");
        TS_ASSERT_EQUALS(Settings::generateConfigs(myProv2Req, "user.ini.gen", "user.yaml.gen", "master.ini.gen", "master.yaml.gen"), myIsAdminInstallation);
        Settings::adminInstallProvider("user.ini.gen", "master.ini.gen");

        // then, both providers should be installed
        verifyInstalledSettings(list_of(myProv1Req)(myProv2Req), myIsAdminInstallation);
    }

    void testUserReinstallExistingProvider()
    {
        using namespace rclient;
        using boost::assign::list_of;

        TS_TRACE("-- Test user reinstallation of the existing provider");

        // given
        static const bool myIsAdminInstallation = false;
        const Settings::RccdRequestData myReq = createRccdRequest(myIsAdminInstallation);
        TS_ASSERT_EQUALS(Settings::generateConfigs(myReq, "user.ini.gen", "user.yaml.gen"), myIsAdminInstallation);
        TS_ASSERT(!Settings::isCustomized()); // check we are clean
        TS_ASSERT_THROWS(Settings::getProviders(), SettingsError); // check we are clean

        // do a fresh install
        Settings::userInstallProvider("user.ini.gen");
        TS_ASSERT(Settings::isCustomized());

        // do some changes to the initial installation
        TS_ASSERT(!Settings::isLastUserMsgUtcExist(myReq.providerName));
        Settings::setLastUserMsgUtc(myReq.providerName, "666");
        TS_ASSERT(Settings::isLastUserMsgUtcExist(myReq.providerName));
        TS_ASSERT_EQUALS(Settings::getLastUserMsgUtc(myReq.providerName), "666");

        const std::string myServiceName = myReq.services.at(0).name;
        TS_ASSERT(!Settings::getUsers(myReq.providerName, myServiceName).empty());
        Settings::removeUsers(myReq.providerName, myServiceName);
        TS_ASSERT(Settings::getUsers(myReq.providerName, myServiceName).empty());

        // when: reinstall the same provider again
        TS_TRACE("    - Reinstall the same provider again");
        Settings::userInstallProvider("user.ini.gen");

        // then, all our changes above should be overwritten
        verifyInstalledSettings(list_of(myReq), myIsAdminInstallation);
    }

    void testUserInstallAddNewProvider()
    {
        using namespace rclient;
        using boost::assign::list_of;

        TS_TRACE("--- Test user installation of a new provider on customized system");

        // given
        static const bool myIsAdminInstallation = false;
        const Settings::RccdRequestData myProv1Req = createRccdRequest(myIsAdminInstallation);
        // create another reqiest, make sure it differs from the first one
        Settings::RccdRequestData myProv2Req = myProv1Req;
        myProv2Req.providerName  += ".1";
        myProv2Req.contentVersion += 1;
        myProv2Req.svrAddress.port += 1;
        myProv2Req.services.pop_back();

        TS_ASSERT(!Settings::isCustomized()); // check we are clean
        TS_ASSERT_THROWS(Settings::getProviders(), SettingsError); // check we are clean

        // when, install both providers
        TS_ASSERT_EQUALS(Settings::generateConfigs(myProv1Req, "user.ini.gen", "user.yaml.gen"), myIsAdminInstallation);
        Settings::userInstallProvider("user.ini.gen");
        TS_ASSERT_EQUALS(Settings::generateConfigs(myProv2Req, "user.ini.gen", "user.yaml.gen"), myIsAdminInstallation);
        Settings::userInstallProvider("user.ini.gen");

        // then,  both providers should be installed
        verifyInstalledSettings(list_of(myProv1Req)(myProv2Req), myIsAdminInstallation);
    }

    void testMixedUserAdminInstall()
    {
        using namespace rclient;
        using boost::assign::list_of;

        // given, perform admin installation
        TS_TRACE("Perform initial admin installation");
        static const bool myIsAdminInstallation = true;
        const Settings::RccdRequestData myAdminReq = createRccdRequest(myIsAdminInstallation);
        Settings::generateConfigs(myAdminReq, "user.ini.gen", "user.yaml.gen", "master.ini.gen", "master.yaml.gen");
        TS_ASSERT(!Settings::isCustomized()); // check we are clean
        TS_ASSERT_THROWS(Settings::getProviders(), SettingsError); // check we are clean
        Settings::adminInstallProvider("user.ini.gen", "master.ini.gen");
        verifyInstalledSettings(list_of(myAdminReq), myIsAdminInstallation);

        // when, perform user installation of the same provider with different settings
        TS_TRACE("Perform user installation of the same provider with different settings");
        Settings::RccdRequestData myUserReq = createRccdRequest(false);
        myUserReq.contentVersion += 1;
        myUserReq.svrAddress.port += 1;
        myUserReq.services.pop_back();
        Settings::generateConfigs(myUserReq, "user.ini.gen", "user.yaml.gen");
        Settings::userInstallProvider("user.ini.gen");

        // then, the existing admin installation should prevent the existing settings from been overwritten
        verifyInstalledSettings(list_of(myAdminReq), myIsAdminInstallation);

        // when, perform user installation of another provider
        TS_TRACE("Perform user installation of another provider");
        myUserReq.providerName += ".1";
        myUserReq.contentVersion += 1;
        myUserReq.svrAddress.port += 1;
        Settings::generateConfigs(myUserReq, "user.ini.gen", "user.yaml.gen");
        Settings::userInstallProvider("user.ini.gen");

        // then, the new provider should be added because it does not conflict with the existing one
        verifyInstalledSettings(list_of(myAdminReq)(myUserReq), myIsAdminInstallation);

         // when, perform admin installation for the existing provider
        TS_TRACE("Perform perform admin installation for the existing provider");
        Settings::RccdRequestData myAdminReq2 = myAdminReq;
        myAdminReq2.providerName = myUserReq.providerName;
        Settings::generateConfigs(myAdminReq2, "user.ini.gen", "user.yaml.gen", "master.ini.gen", "master.yaml.gen");
        Settings::adminInstallProvider("user.ini.gen", "master.ini.gen");

        // then, the new provider should be replace the existing one
        verifyInstalledSettings(list_of(myAdminReq)(myAdminReq2), myIsAdminInstallation);
    }
};
