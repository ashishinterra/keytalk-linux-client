#include "ContentConfig.h"
#include "rclient/Common.h"
#include "rclient/Settings.h"
#include "rclient/NativeCertStore.h"
#include "ta/process.h"
#include "ta/strings.h"
#include "ta/logger.h"
#include "ta/utils.h"
#include "ta/certutils.h"
#include "ta/libconfigwrapper.h"
#include "ta/common.h"

#include "boost/algorithm/string.hpp"
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/convenience.hpp"
#include <cassert>

using std::string;
using std::vector;
namespace fs = boost::filesystem;
using ta::version::Version;

namespace
{
    string fixWindowDirSeps(const string& aPath)
    {
#ifndef _WIN32
        return boost::replace_all_copy(aPath, "\\", "/");
#else
        return aPath;
#endif
    }
}

namespace rclient
{
    namespace ContentConfig
    {
        // functions used in RCCD v1.x only
        namespace v1
        {
            string filePath(const ta::LibConfigWrapper& aConfig, const string& aConfigPath, const string& anOptionName)
            {
                string mySourceRelativePath;
                aConfig.getValue(makeSourceOption(anOptionName), mySourceRelativePath, ta::LibConfigWrapper::settingGetFailIfNotExist);
                const string mySourceFilePath = ta::getParentDir(aConfigPath) + ta::getDirSep() + fixWindowDirSeps(mySourceRelativePath);
                if (!ta::isFileExist(mySourceFilePath))
                {
                    TA_THROW_MSG(std::runtime_error, "Source RCCDv1 file " + mySourceFilePath + " does not exist");
                }
                // do not verify checksum any more
                return mySourceFilePath;
            }
        } // v1

        static string filePath(const ta::LibConfigWrapper& aConfig, const string& aConfigPath, const string& anOptionName)
        {
            string mySourceRelativePath;
            aConfig.getValue(anOptionName, mySourceRelativePath, ta::LibConfigWrapper::settingGetFailIfNotExist);
            const string mySourceFilePath = ta::getParentDir(aConfigPath) + ta::getDirSep() + fixWindowDirSeps(mySourceRelativePath);
            if (!ta::isFileExist(mySourceFilePath))
            {
                TA_THROW_MSG(std::runtime_error, "Source RCCD file " + mySourceFilePath + " does not exist");
            }
            return mySourceFilePath;
        }

        static void installFile(const string& aSrcPath, const string& aTargetPath)
        {
            if (!ta::isFileExist(aSrcPath))
            {
                TA_THROW_MSG(std::runtime_error, "RCCD source file " + aSrcPath + " does not exist");
            }
            ta::createParentDir(aTargetPath);
            fs::copy_file(aSrcPath, aTargetPath, fs::copy_option::overwrite_if_exists);
        }

        static vector<string> loadExtraSigningCasPaths(const ta::LibConfigWrapper& aConfig, const string& aConfigPath)
        {
            vector<string> myPaths;

            vector<string> myRelativeCasPaths;
            if (aConfig.getValue(ExtraSigningCAsOption, myRelativeCasPaths, ta::LibConfigWrapper::settingGetTolerateIfNotExist))
            {
                foreach (const string& cas_path, myRelativeCasPaths)
                {
                    const string myPath = ta::getParentDir(aConfigPath) + ta::getDirSep() + fixWindowDirSeps(cas_path);
                    myPaths.push_back(myPath);
                }
            }
            return myPaths;
        }

        static void cleanupDir(const string& aDirPath)
        {
            if (!fs::exists(aDirPath))
            {
                fs::create_directories(aDirPath);
            }
            else
            {
                fs::path dir(aDirPath);
                for (fs::directory_iterator end_dir_it, it(dir); it!=end_dir_it; ++it)
                {
                    fs::remove_all(it->path());
                }
            }
            // @notice: a shortcut by removing and then re-creating aDirPath may fail on Windows with access denied during dir creation (antivirus?)
        }


        Config::Config()
            : theContentVersion(0), theIsMasterSettingsExist(false), theIsRcaExist(false) //@note is is essential to initialize bools otherwise they will get undefined values most likely other than 0 or 1 which might (depending on stdlib implementation) cause further error during deserialization because of incorrect stream read
        {}

        Config::Config(const string& aConfigPath)
            : theContentVersion(0), theIsMasterSettingsExist(false), theIsRcaExist(false)
        {
            initFromFile(aConfigPath);
        }

        void Config::initFromFile(const string& aConfigPath)
        {
            ta::LibConfigWrapper myConfig(aConfigPath);

            string myVersionStr;
            myConfig.getValue(ConfigVersionOption, myVersionStr, ta::LibConfigWrapper::settingGetFailIfNotExist);
            try {
                theConfigVersion = ta::version::parse(myVersionStr);
            } catch (...) {
                TA_THROW_MSG(std::runtime_error, boost::format("Cannot parse %s option in %s") % ConfigVersionOption % aConfigPath);
            }

            myConfig.getValue(ContentVersionOption, (unsigned int&)theContentVersion, ta::LibConfigWrapper::settingGetFailIfNotExist);
            myConfig.getValue(ProviderNameOption, theProviderName, ta::LibConfigWrapper::settingGetFailIfNotExist);

            // Parse the rest of the options based on the config version
            if (theConfigVersion >= Version(2,0) && theConfigVersion < Version(2,1))
            {
                theUserSettingsSourcePath = filePath(myConfig, aConfigPath, UserConfigOption);
                theIsMasterSettingsExist = myConfig.isStringSettingExist(MasterConfigOption);
                if (theIsMasterSettingsExist)
                {
                    theMasterSettingsSourcePath = filePath(myConfig, aConfigPath, MasterConfigOption);
                }

                theLogoSourcePath = filePath(myConfig, aConfigPath, LogoOption);

                theUcaSourcePath = filePath(myConfig, aConfigPath, UcaOption);
                theScaSourcePath = filePath(myConfig, aConfigPath, ScaOption);
                thePcaSourcePath = filePath(myConfig, aConfigPath, PcaOption);
                theIsRcaExist = myConfig.isStringSettingExist(RcaOption);
                if (theIsRcaExist)
                {
                    theRcaSourcePath = filePath(myConfig, aConfigPath, RcaOption);
                }

                theExtraSigningCasPaths = loadExtraSigningCasPaths(myConfig, aConfigPath);
            }
            else if (theConfigVersion == Version(1,0) || theConfigVersion == Version(1,1))
            {
                theUserSettingsSourcePath = v1::filePath(myConfig, aConfigPath, UserConfigOption);
                theIsMasterSettingsExist = myConfig.isGroupSettingExist(MasterConfigOption);
                if (theIsMasterSettingsExist)
                {
                    theMasterSettingsSourcePath = v1::filePath(myConfig, aConfigPath, MasterConfigOption);
                }

                theIconV10SourcePath = v1::filePath(myConfig, aConfigPath, v1::IconOption);
                theLogoV10SourcePath = v1::filePath(myConfig, aConfigPath, v1::LogoOption);
                if (theConfigVersion == Version(1,1))
                {
                    theLogoSourcePath = v1::filePath(myConfig, aConfigPath, v1::LogoV11Option);
                }

                theUcaSourcePath = v1::filePath(myConfig, aConfigPath, UcaOption);
                theScaSourcePath = v1::filePath(myConfig, aConfigPath, ScaOption);
                thePcaSourcePath = v1::filePath(myConfig, aConfigPath, PcaOption);
                theIsRcaExist = myConfig.isGroupSettingExist(RcaOption);
                if (theIsRcaExist)
                {
                    theRcaSourcePath = v1::filePath(myConfig, aConfigPath, RcaOption);
                }
            }
            else
            {
                TA_THROW_MSG(std::runtime_error, "Unsupported RCCD version " + toStr(theConfigVersion));
            }
        }


        void install(const Config& aConfig, InstallCAsCb anInstallCAsCustomCb, void* aCbCookie)
        {
            const string myNewProviderName = aConfig.getProviderName();
            const size_t myNewContentVersion = aConfig.getContentVersion();

            // Install settings
            const string myProviderInstallDir = rclient::Settings::getProviderInstallDir(myNewProviderName);
            const bool myIsAdminInstall = aConfig.isMasterSettingsExist();
            const string myUserSettingsSourcePath = aConfig.getUserSettingsPath();
            const string myMasterSettingsSourcePath = myIsAdminInstall ? aConfig.getMasterSettingsPath() : "";
            const string myHomeDir = ta::Process::getUserAppDataDir();
            const string myUserName = ta::getUserName();

            if (myIsAdminInstall)
            {
                DEBUGLOG(boost::format("Performing admin installation of settings version %u for provider %s. User settings from %s, master settings from %s. HOME directory: %s. USER: %s") %
                         (unsigned int)myNewContentVersion % myNewProviderName % myUserSettingsSourcePath % myMasterSettingsSourcePath % myHomeDir % myUserName);
                rclient::Settings::adminInstallProvider(myUserSettingsSourcePath, myMasterSettingsSourcePath);
            }
            else
            {
                DEBUGLOG(boost::format("Performing user installation of settings version %u for provider %s. User settings from %s. HOME directory: %s. USER: %s") %
                         (unsigned int)myNewContentVersion % myNewProviderName % myUserSettingsSourcePath % myHomeDir % myUserName);
                rclient::Settings::userInstallProvider(myUserSettingsSourcePath);
            }

            //
            // Install provider-specific data
            //

            // we don't want a mess of files from different RCCD versions
            cleanupDir(myProviderInstallDir);

            if (aConfig.getConfigVersion() >= Version(2,0))
            {
                const string mySourcePath = aConfig.getLogoPath();
                const string myTargetPath = myProviderInstallDir + ta::getDirSep() + rclient::LogoV20ImageName;
                DEBUGLOG(boost::format("Copying logo from %s to %s") % mySourcePath % myTargetPath);
                installFile(mySourcePath, myTargetPath);
            }
            else
            {

                if (aConfig.getConfigVersion() == Version(1,1))
                {
                    const string mySourcePath = aConfig.getLogoPath();
                    const string myTargetPath = myProviderInstallDir + ta::getDirSep() + rclient::LogoV11ImageName;
                    DEBUGLOG(boost::format("Copying v1.1 logo from %s to %s") % mySourcePath % myTargetPath);
                    installFile(mySourcePath, myTargetPath);
                }
                else if (aConfig.getConfigVersion() == Version(1,0))
                {
                    string mySourcePath = aConfig.getIconV10Path();
                    string myTargetPath = myProviderInstallDir + ta::getDirSep() + rclient::IconV10ImageName;
                    DEBUGLOG(boost::format("Copying v1.0 icon from %s to %s") % mySourcePath % myTargetPath);
                    installFile(mySourcePath, myTargetPath);

                    mySourcePath = aConfig.getLogoV10Path();
                    myTargetPath = myProviderInstallDir + ta::getDirSep() + rclient::LogoV10ImageName;
                    DEBUGLOG(boost::format("Copying v1.0 logo from %s to %s") % mySourcePath % myTargetPath);
                    installFile(mySourcePath, myTargetPath);
                }
            }

            rclient::Settings::addInstalledProvider(myNewProviderName);

            // Install CAs
            if (anInstallCAsCustomCb)
            {
                anInstallCAsCustomCb(aCbCookie,
                                     aConfig.getUcaPath(),
                                     aConfig.getScaPath(),
                                     aConfig.getPcaPath(),
                                     aConfig.isRcaExist() ? aConfig.getRcaPath() : "",
                                     aConfig.getExtraSigningCasPaths());
            }
            else
            {
                rclient::NativeCertStore::installCAs(aConfig.getUcaPath(),
                                                     aConfig.getScaPath(),
                                                     aConfig.getPcaPath(),
                                                     aConfig.isRcaExist() ? aConfig.getRcaPath() : "",
                                                     aConfig.getExtraSigningCasPaths());
            }
        }

    } // namespace ContentConfig
}//namespace rclient

