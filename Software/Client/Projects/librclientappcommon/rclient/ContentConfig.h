#pragma once

#include "ta/version.h"
#include <string>
#include <vector>
#include "boost/archive/text_oarchive.hpp"
#include "boost/archive/text_iarchive.hpp"
#include "boost/serialization/vector.hpp"

namespace rclient
{
    namespace ContentConfig
    {
        //
        // RCCD format version history
        //
        // RCCD version is written in the index files of RCCD package
        //
        // version 1.0 - initial version
        //
        // version 1.1 (July 2013; KeyTalk-4.3; backward compatible with RCCD-1.0)
        //     - added "LogoV11" option to the index holding the location of 110x110 logo in PNG format. Old 55x55 BMP logo is still preserved in "Logo" section for compatibility
        //
        // version 2.0 (Jan 2017; supported by KeyTalk-5.x; not compatible with RCCD-1.x)
        //     - index files are not signed any more and renamed to index.conf and index.yaml
        //     - as a result of giving up signing checksums are not added to the index any more
        //     - extra signing CAs can be added to RCCD ("ExtraSigningCAs" settings in the index)
        //     - removed signed communication key ("SignedSvrCommPubKey" index file setting stored in SignedSvrCommPubKey.smime file) since it is not required for RCDPv2
        //     - use only one logo in png format ("Logo"). Got rid of v10 and v11 logo/icons
        //     - removed KeyAgreement setting from user.ini as we dropped support for RCDPv1
        //
        // version 2.0.1 (Sep 2017; supported by KeyTalk-5.x; fully compatible with the previous version)
        //     - removed 'ProxySettings' from user.ini and master.ini as http proxy does not make sense for https connection used by KeyTalk

        static const ta::version::Version LatestVersion = ta::version::Version(2,0,1);

        static const std::string IndexFileName     = "index.conf";
        static const std::string YamlIndexFileName = "index.yaml";

        static const std::string ConfigVersionOption   = "ConfigVersion";
        static const std::string ContentVersionOption  = "ContentVersion";
        static const std::string ProviderNameOption    = "ProviderName";
        static const std::string ContentDir            = "content";
        static const std::string MasterConfigOption    = "MasterConfig";
        static const std::string UserConfigOption      = "UserConfig";
        static const std::string LogoOption            = "Logo";
        static const std::string UcaOption             = "UCA";
        static const std::string ScaOption             = "SCA";
        static const std::string PcaOption             = "PCA";
        static const std::string RcaOption             = "RCA";
        static const std::string ExtraSigningCAsOption = "ExtraSigningCAs";


        // settings in this namespace are used in RCCD v1.x only
        namespace v1
        {
            static const std::string IndexFileName             = "content.conf.signed";

            static const std::string IconOption                = "Icon";
            static const std::string LogoOption                = "Logo";
            static const std::string LogoV11Option             = "LogoV11";

            inline std::string makeSourceOption(const std::string& anOptionBaseName) { return anOptionBaseName + ".Source"; }
        }

        class Config
        {
        public:
            Config();
            Config(const std::string& aConfigPath);

            //
            // Getters
            //
            inline ta::version::Version getConfigVersion() const { return theConfigVersion; }
            inline int getContentVersion() const { return theContentVersion; }
            inline std::string getProviderName() const { return theProviderName; }
            // All path-related settings are returned with checksums verified
            inline bool isMasterSettingsExist() const { return theIsMasterSettingsExist; }
            inline std::string getMasterSettingsPath() const { return theMasterSettingsSourcePath; }
            inline std::string getUserSettingsPath() const { return theUserSettingsSourcePath; }
            inline std::string getIconV10Path() const  { return theIconV10SourcePath; } // for RCCD-1.0 only
            inline std::string getLogoV10Path() const  { return theLogoV10SourcePath; }// for RCCD-1.0 only
            inline std::string getLogoPath() const  { return theLogoSourcePath; } // since RCCD-1.1
            inline std::string getUcaPath() const { return theUcaSourcePath; }
            inline std::string getScaPath() const { return theScaSourcePath; }
            inline std::string getPcaPath() const { return thePcaSourcePath; }
            inline bool isRcaExist() const { return theIsRcaExist; }
            inline std::string getRcaPath() const { return theRcaSourcePath; }
            inline std::vector<std::string> getExtraSigningCasPaths() const { return theExtraSigningCasPaths; } // since RCCD-2.0

        private:
            void initFromFile(const std::string& aConfigPath);

            friend class boost::serialization::access;

            template<class Archive>
            void serialize(Archive& ar, const unsigned int UNUSED(version))
            {
                ar & theConfigVersion;
                ar & theContentVersion;
                ar & theProviderName;
                ar & theIsMasterSettingsExist;
                ar & theMasterSettingsSourcePath;
                ar & theUserSettingsSourcePath;
                ar & theIconV10SourcePath;
                ar & theLogoV10SourcePath;
                ar & theLogoSourcePath;
                ar & theUcaSourcePath;
                ar & theScaSourcePath;
                ar & thePcaSourcePath;
                ar & theIsRcaExist;
                ar & theRcaSourcePath;
                ar & theExtraSigningCasPaths;
            }
        private:
            ta::version::Version theConfigVersion;
            int theContentVersion;
            std::string theProviderName;
            bool theIsMasterSettingsExist;
            std::string theMasterSettingsSourcePath;
            std::string theUserSettingsSourcePath;
            std::string theIconV10SourcePath;
            std::string theLogoV10SourcePath;
            std::string theLogoSourcePath;
            std::string theUcaSourcePath;
            std::string theScaSourcePath;
            std::string thePcaSourcePath;
            bool theIsRcaExist;
            std::string theRcaSourcePath;
            std::vector<std::string> theExtraSigningCasPaths;
        };


        typedef void (*InstallCAsCb)(void* aCookie, const std::string& aUcaDerPath, const std::string& anScaDerPath, const std::string& aPcaDerPath, const std::string& anRcaDerPath, const std::vector<std::string>& anExtraSigningCAaPemPaths);
        //
        // Install RCCD content given its configuration
        // param [in] anInstallCAsCustomCb / aCbCookie custom callback with a cookie for installation of CAs. Must be NULL for production code, use it ONLY for test purposes
        //
        void install(const Config& aConfig, InstallCAsCb anInstallCAsCustomCb = NULL, void* aCbCookie = NULL);
    }
}

