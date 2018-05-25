#pragma once

#include "rclient/Common.h"
#include "rclient/ContentConfig.h"
#include "rclient/Settings.h"
#include "ta/certutils.h"
#include "ta/opensslapp.h"
#include "cxxtest/TestSuite.h"

#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/convenience.hpp"
#include <string>

class ContentConfigInstallationTest : public CxxTest::TestSuite
{
public:
    static void installCAs(void* aThis, const std::string& aUcaDerPath, const std::string& anScaDerPath, const std::string& aPcaDerPath, const std::string& anRcaDerPath, const std::vector<std::string>& anExtraSigningCAaPaths)
    {
        TS_ASSERT(aThis);
        if (aThis)
        {
            ContentConfigInstallationTest* self = (ContentConfigInstallationTest*)aThis;

            // file paths might not march exactly e.g. ./input/user.ini and input/user.ini
            TS_ASSERT(boost::filesystem::equivalent(aUcaDerPath, InputUcaPath));
            TS_ASSERT(boost::filesystem::equivalent(anScaDerPath, InputScaPath));
            TS_ASSERT(boost::filesystem::equivalent(aPcaDerPath, InputPcaPath));
            if (!anRcaDerPath.empty())
            {
                TS_ASSERT(boost::filesystem::equivalent(anRcaDerPath, InputRcaPath));
            }
            ++self->theInstallKeyTalkCAsCallCounter;

            if (!anExtraSigningCAaPaths.empty())
            {
                TS_ASSERT_EQUALS(anExtraSigningCAaPaths.size(), InputExtraSigningCAsPaths.size());

                size_t index = 0;
                foreach (const std::string& path, anExtraSigningCAaPaths)
                {
                    TS_ASSERT(boost::filesystem::equivalent(path, InputExtraSigningCAsPaths.at(index++)));
                }
                ++self->theInstallExtraSigningCAsCallCounter;
            }
        }
    }
    static void copyFile(const std::string& aSrc, const std::string& aDest)
    {
        boost::filesystem::copy_file(aSrc, aDest, boost::filesystem::copy_option::overwrite_if_exists);
    }
    static void removeFile(const std::string& aFile)
    {
        boost::filesystem::remove(aFile);
    }
    static std::string inputPath(const std::string& aFileName)
    {
        return InputDir + ta::getDirSep() + aFileName;
    }
    static std::string installationPath(const std::string& aFileName)
    {
        return InstallationDir + ta::getDirSep() + aFileName;
    }
    static std::string getInstalledProviderFilePath(const std::string& aFileName)
    {
        return rclient::Settings::getProviderInstallDir() + ta::getDirSep() + aFileName;
    }
    static bool areFilesEqual(const std::string& aFile1Path, const std::string& aFile2Path)
    {
        return ta::HashUtils::getSha256HexFile(aFile1Path) == ta::HashUtils::getSha256HexFile(aFile2Path);
    }

    enum CreateEmptyDirs
    {
        createEmptyDirsYes, createEmptyDirsNo
    };
    void cleanup(const CreateEmptyDirs aCreateEmptyDirs)
    {
        namespace fs = boost::filesystem;

        fs::remove_all(InputDir);
        fs::remove_all(InstallationDir);
        if (aCreateEmptyDirs == createEmptyDirsYes)
        {
            fs::create_directories(InputDir);
            fs::create_directories(InputExtraSigningCAsDir);
            fs::create_directories(InstallationDir);
        }
    }


    void setUp()
    {
        using namespace rclient;
        using boost::assign::list_of;

        CxxTest::setAbortTestOnFail(true);

        static ta::OpenSSLApp myOpenSSL;

        cleanup(createEmptyDirsYes);

        // prepare input files aka they are extracted from RCCD
        copyFile("../../../CertKeys/CommunicationAndSigning/signingcacert.pem", InputUcaPath);
        copyFile("../../../CertKeys/CommunicationAndSigning/commcacert.pem", InputScaPath);
        copyFile("../../../CertKeys/CommunicationAndSigning/pcacert.pem", InputPcaPath);
        copyFile("../../../CertKeys/CommunicationAndSigning/pcacert.pem", InputRcaPath); // hack but will work as long as it is not checked ;)
        copyFile("../Common/CA/globalsign_orgca.pem", InputExtraSigningCAsPaths.at(0));
        copyFile("../Common/CA/globalsign_evca.pem", InputExtraSigningCAsPaths.at(1));
        copyFile("../testlibrclientappcommon/logo.png", InputLogoPath);
        // Create resept.ini as done by KeyTalk installer
        copyFile("../../Projects/ReseptInstaller/" + std::string(rclient::Settings::ReseptConfigFileName), InstalledReseptConfigPath);
        Settings::setConfigsPath(InstalledReseptConfigPath, InstalledUserConfigPath, InstalledMasterConfigPath);
        Settings::setReseptInstallDir(InstallationDir);

        theInstallKeyTalkCAsCallCounter = 0;
        theInstallExtraSigningCAsCallCounter = 0;

        CxxTest::setAbortTestOnFail(false);
    }
    void tearDown()
    {
        try
        {
            rclient::Settings::resetConfigsPath();
            cleanup(createEmptyDirsNo);
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

    rclient::Settings::RccdRequestData createRccdRequest(const bool anIsForAdminInstallation, const bool aWithRca)
    {
        using namespace rclient;
        using boost::assign::list_of;

        static const bool AllowOverwriteYes = true;
        static const bool AllowOverwriteNo  = false;

        Settings::RccdRequestData myReq;
        myReq.providerName = "DemoProvider";
        myReq.contentVersion = 2016121301;
        myReq.svrAddress =  ta::NetUtils::RemoteAddress("test.keytalk.com", 80);
        myReq.allowOverwriteSvrAddress = anIsForAdminInstallation ? AllowOverwriteNo : AllowOverwriteYes;
        myReq.signingCaPem = ta::readData(InputUcaPath);
        myReq.commCaPem = ta::readData(InputScaPath);
        myReq.pcaPem = ta::readData(InputPcaPath);
        if (aWithRca)
        {
            myReq.rcaPem = ta::readData(InputRcaPath);
        }
        const Settings::RccdRequestData::Service service1("s1",
                                                        "https://s1.com",
                                                         11, AllowOverwriteYes,
                                                         list_of("s1u1")("s1u2"), AllowOverwriteYes);
        const Settings::RccdRequestData::Service service2("s2",
                                                        "https://s2.com",
                                                         12, AllowOverwriteYes,
                                                         list_of("s2u1"), AllowOverwriteYes);
        myReq.services = list_of(service1)(service2);

        return myReq;
    }

    //@return path to the created index file
    std::string createRccdIndexFile(const rclient::Settings::RccdRequestData& aReq, const bool aWithExtraSigningCAs = false)
    {
        using namespace rclient;

        static const ta::LibConfigWrapper::SettingSetPolicy mySetPolicy = ta::LibConfigWrapper::settingSetCreateIfNotExist;
        removeFile(ContentConfig::IndexFileName);
        ta::LibConfigWrapper myIndex(ContentConfig::IndexFileName, ta::LibConfigWrapper::fileCreateIfNotExist);

        myIndex.setValue(ContentConfig::ConfigVersionOption, toStr(ta::version::Version(2,0)), mySetPolicy);
        myIndex.setValue(ContentConfig::ContentVersionOption, aReq.contentVersion, mySetPolicy);
        myIndex.setValue(ContentConfig::ProviderNameOption, aReq.providerName, mySetPolicy);

        myIndex.setValue(ContentConfig::UserConfigOption, InputUserConfigPath, mySetPolicy);
        if (aReq.isAdminRccd())
        {
            myIndex.setValue(ContentConfig::MasterConfigOption, InputMasterConfigPath, mySetPolicy);
        }
        myIndex.setValue(ContentConfig::ScaOption, InputScaPath, mySetPolicy);
        myIndex.setValue(ContentConfig::UcaOption, InputUcaPath, mySetPolicy);
        myIndex.setValue(ContentConfig::PcaOption, InputPcaPath, mySetPolicy);
        if (!aReq.rcaPem.empty())
        {
            myIndex.setValue(ContentConfig::RcaOption, InputRcaPath, mySetPolicy);
        }
        myIndex.setValue(ContentConfig::LogoOption, InputLogoPath, mySetPolicy);
        if (aWithExtraSigningCAs)
        {
            myIndex.setValue(ContentConfig::ExtraSigningCAsOption, InputExtraSigningCAsPaths, mySetPolicy);
        }

        return myIndex.getConfigFilePath();
    }

    void verifyNotCustomized()
    {
        TS_ASSERT(!rclient::Settings::isCustomized());
        TS_ASSERT(!ta::isFileExist(InstalledUserConfigPath));
        TS_ASSERT(!ta::isFileExist(InstalledMasterConfigPath));
        TS_ASSERT_EQUALS(theInstallKeyTalkCAsCallCounter, 0);
    }

    void verifyCustomized(const rclient::Settings::RccdRequestData& aReq, const bool aWithExtraSigningCAs = false)
    {
        using namespace rclient;
        using ta::version::Version;

        // quick check settings
        TS_ASSERT(Settings::isCustomized());
        TS_ASSERT(ta::isFileExist(InstalledUserConfigPath));
        if (aReq.isAdminRccd())
        {
            TS_ASSERT(ta::isFileExist(InstalledMasterConfigPath));
        }
        else
        {
            TS_ASSERT(!ta::isFileExist(InstalledMasterConfigPath));
        }

        const std::string myProviderName = aReq.providerName;
        TS_ASSERT_EQUALS(Settings::getProviders().size(), 1);
        TS_ASSERT_EQUALS(Settings::getProviders().at(0), myProviderName);
        bool myFromMasterConfig;
        TS_ASSERT_EQUALS(Settings::getProviderContentVersion(myProviderName), aReq.contentVersion);
        TS_ASSERT_EQUALS(Settings::getReseptSvrAddress(myProviderName, myFromMasterConfig), aReq.svrAddress);
        TS_ASSERT_EQUALS(myFromMasterConfig, !aReq.allowOverwriteSvrAddress);
        TS_ASSERT_EQUALS(Settings::getUserCaName(), ta::CertUtils::getCertInfo(aReq.signingCaPem).subjCN);
        TS_ASSERT_EQUALS(Settings::getServerCaName(), ta::CertUtils::getCertInfo(aReq.commCaPem).subjCN);
        TS_ASSERT_EQUALS(Settings::getPrimaryCaName(), ta::CertUtils::getCertInfo(aReq.pcaPem).subjCN);
        if (!aReq.rcaPem.empty())
        {
            TS_ASSERT(Settings::isRootCaExist(myProviderName));
        }
        else
        {
            TS_ASSERT(!Settings::isRootCaExist(myProviderName));
        }
        TS_ASSERT_EQUALS(Settings::getServices(), aReq.getServiceNames());
        TS_ASSERT(ta::isFileExist(getInstalledProviderFilePath(rclient::LogoV20ImageName)));
        TS_ASSERT(areFilesEqual(getInstalledProviderFilePath(rclient::LogoV20ImageName), InputLogoPath));

        // check right CAs are "imported"
        TS_ASSERT_EQUALS(theInstallKeyTalkCAsCallCounter, 1);
        if (aWithExtraSigningCAs)
        {
            TS_ASSERT_EQUALS(theInstallExtraSigningCAsCallCounter, 1);
        }
        else
        {
            TS_ASSERT_EQUALS(theInstallExtraSigningCAsCallCounter, 0);
        }
    }


    //
    // Test cases
    //

    void test_that_user_rccd_without_rca_can_be_installed()
    {
        using namespace rclient;

        // given (create files&settings that are normally extracted from RCCD)
        const Settings::RccdRequestData myReq = createRccdRequest(AdminInstallationNo, WithRcaNo);
        Settings::generateConfigs(myReq, InputUserConfigPath, inputPath("user.yaml"));
        const ContentConfig::Config myConfig(createRccdIndexFile(myReq));
        verifyNotCustomized();

        // when
        ContentConfig::install(myConfig, &installCAs, this);

        // then
        verifyCustomized(myReq);
    }

    void test_that_user_rccd_with_rca_can_be_installed()
    {
        using namespace rclient;

        // given (create files&settings that are normally extracted from RCCD)
        const Settings::RccdRequestData myReq = createRccdRequest(AdminInstallationNo, WithRcaYes);
        Settings::generateConfigs(myReq, InputUserConfigPath, inputPath("user.yaml"));
        const ContentConfig::Config myConfig(createRccdIndexFile(myReq));
        verifyNotCustomized();

        // when
        ContentConfig::install(myConfig, &installCAs, this);

        // then
        verifyCustomized(myReq);
    }

    void test_that_admin_rccd_without_rca_can_be_installed()
    {
        using namespace rclient;

        // given (create files&settings that are normally extracted from RCCD)
        const Settings::RccdRequestData myReq = createRccdRequest(AdminInstallationYes, WithRcaNo);
        Settings::generateConfigs(myReq, InputUserConfigPath, inputPath("user.yaml"), InputMasterConfigPath, inputPath("master.yaml"));
        const ContentConfig::Config myConfig(createRccdIndexFile(myReq));
        verifyNotCustomized();

        // when
        ContentConfig::install(myConfig, &installCAs, this);

        // then
        verifyCustomized(myReq);
    }

    void test_that_admin_rccd_with_rca_can_be_installed()
    {
        using namespace rclient;

        // given (create files&settings that are normally extracted from RCCD)
        const Settings::RccdRequestData myReq = createRccdRequest(AdminInstallationYes, WithRcaYes);
        Settings::generateConfigs(myReq, InputUserConfigPath, inputPath("user.yaml"), InputMasterConfigPath, inputPath("master.yaml"));
        const ContentConfig::Config myConfig(createRccdIndexFile(myReq));
        verifyNotCustomized();

        // when
        ContentConfig::install(myConfig, &installCAs, this);

        // then
        verifyCustomized(myReq);
    }

    void test_that_rccd_with_extra_cas_can_be_installed()
    {
        using namespace rclient;

        // given (create files&settings that are normally extracted from RCCD)
        static const bool myWithExtraSigningCAs = true;
        const Settings::RccdRequestData myReq = createRccdRequest(AdminInstallationNo, WithRcaNo);
        Settings::generateConfigs(myReq, InputUserConfigPath, inputPath("user.yaml"));
        const ContentConfig::Config myConfig(createRccdIndexFile(myReq, myWithExtraSigningCAs));
        verifyNotCustomized();

        // when
        ContentConfig::install(myConfig, &installCAs, this);

        // then
        verifyCustomized(myReq, myWithExtraSigningCAs);
    }

private:
    int theInstallKeyTalkCAsCallCounter;
    int theInstallExtraSigningCAsCallCounter;

    static const std::string InputDir; // we take input installation files from here
    static const std::string InputUserConfigPath;
    static const std::string InputMasterConfigPath;
    static const std::string InputUcaPath;
    static const std::vector<std::string> InputExtraSigningCAsPaths;
    static const std::string InputScaPath;
    static const std::string InputPcaPath;
    static const std::string InputRcaPath;
    static const std::string InputLogoPath;
    static const std::string InputExtraSigningCAsDir;

    static const std::string InstallationDir;// we are going to install all settings here (both user files and app files)
    static const std::string InstalledReseptConfigPath;
    static const std::string InstalledUserConfigPath;
    static const std::string InstalledMasterConfigPath;

    static const bool AdminInstallationYes = true;
    static const bool AdminInstallationNo = false;
    static const bool WithRcaYes = true;
    static const bool WithRcaNo = false;
};

const std::string ContentConfigInstallationTest::InputDir = "input";
const std::string ContentConfigInstallationTest::InputUserConfigPath = inputPath("user.ini");
const std::string ContentConfigInstallationTest::InputMasterConfigPath = inputPath("master.ini");
const std::string ContentConfigInstallationTest::InputUcaPath = inputPath("signingcacert.pem");
const std::vector<std::string> ContentConfigInstallationTest::InputExtraSigningCAsPaths = boost::assign::list_of(inputPath("extra_signing_ca_1.pem"))(inputPath("extra_signing_ca_2.pem"));
const std::string ContentConfigInstallationTest::InputScaPath = inputPath("commcacert.pem");
const std::string ContentConfigInstallationTest::InputPcaPath = inputPath("pcacert.pem");
const std::string ContentConfigInstallationTest::InputRcaPath = inputPath("rcacert.pem");
const std::string ContentConfigInstallationTest::InputLogoPath = inputPath("logo.png");
const std::string ContentConfigInstallationTest::InputExtraSigningCAsDir = inputPath("extra_signing_cas");

const std::string ContentConfigInstallationTest::InstallationDir = "install";
const std::string ContentConfigInstallationTest::InstalledReseptConfigPath = installationPath(rclient::Settings::ReseptConfigFileName);
const std::string ContentConfigInstallationTest::InstalledUserConfigPath = installationPath(rclient::Settings::UserConfigFileName);
const std::string ContentConfigInstallationTest::InstalledMasterConfigPath = installationPath(rclient::Settings::MasterConfigFileName);
