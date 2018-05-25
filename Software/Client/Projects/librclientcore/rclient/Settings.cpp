#ifdef _MSC_VER
# pragma warning (disable: 4996) // 'deprecated' API according to MS
#endif

#include "Settings.h"
#include "SettingsImpl.hpp"
#include "ta/certutils.h"
#include "ta/logger.h"
#include "ta/logconfiguration.h"

#include "yaml-cpp/yaml.h"


namespace rclient
{
    namespace Settings
    {
        using namespace ta;
        using std::string;
        using std::vector;
        using SettingsImpl::masterConfig;
        using SettingsImpl::userConfig;
        using SettingsImpl::reseptConfig;

        // Private API
        namespace
        {
            void generateUserLibConfigConfigImpl(const RccdRequestData& aReq, const string& anOutConfPath)
            {
                try
                {
                    libconfig::Config myConfig;
                    libconfig::Setting& myConfigRoot = myConfig.getRoot();

                    // set global settings
                    myConfigRoot.add(ConfigVersion, libconfig::Setting::TypeString) = toStr(SettingsImpl::SupportedConfigVersion);
                    myConfigRoot.add(LatestProvider, libconfig::Setting::TypeString) = aReq.providerName;
                    myConfigRoot.add(LatestService, libconfig::Setting::TypeString) = aReq.services.at(0).name;

                    // set provider settings
                    libconfig::Setting& myProvidersConfig = myConfigRoot.add(ProviderList, libconfig::Setting::TypeList);
                    libconfig::Setting& myProviderConfig = myProvidersConfig.add(libconfig::Setting::TypeGroup);

                    myProviderConfig.add(ProviderName, libconfig::Setting::TypeString) = aReq.providerName;
                    myProviderConfig.add(ProviderContentVersion, libconfig::Setting::TypeInt) = aReq.contentVersion;
                    myProviderConfig.add(ProviderReseptSvrAddress, libconfig::Setting::TypeString) = toString(aReq.svrAddress, DefRcdpV2Port);
                    myProviderConfig.add(ProviderLogLevel, libconfig::Setting::TypeString) = DefLogLevel;
                    // CAs
                    libconfig::Setting& myCasConfig = myProviderConfig.add(ProviderCaList, libconfig::Setting::TypeArray);
                    myCasConfig.add(libconfig::Setting::TypeString) = ta::CertUtils::getCertInfo(aReq.signingCaPem).subjCN;
                    myCasConfig.add(libconfig::Setting::TypeString) = ta::CertUtils::getCertInfo(aReq.commCaPem).subjCN;
                    myCasConfig.add(libconfig::Setting::TypeString) = ta::CertUtils::getCertInfo(aReq.pcaPem).subjCN;
                    if (!aReq.rcaPem.empty())
                    {
                        myCasConfig.add(libconfig::Setting::TypeString) = ta::CertUtils::getCertInfo(aReq.rcaPem).subjCN;
                    }

                    // set service settings
                    libconfig::Setting& myServicesConfig = myProviderConfig.add(ServiceList, libconfig::Setting::TypeList);
                    foreach (const RccdRequestData::Service& service, aReq.services)
                    {
                        libconfig::Setting& myServiceConfig = myServicesConfig.add(libconfig::Setting::TypeGroup);

                        myServiceConfig.add(ServiceName, libconfig::Setting::TypeString) = service.name;
                        myServiceConfig.add(ServiceCertFormat, libconfig::Setting::TypeString) = str(resept::certformatP12);
                        myServiceConfig.add(ServiceCertChain, libconfig::Setting::TypeBoolean) = false;
                        myServiceConfig.add(ServiceUri, libconfig::Setting::TypeString) = service.uri;
                        myServiceConfig.add(ServiceCertValidPercent, libconfig::Setting::TypeInt) = (int)service.certValidityPercentage;

                        libconfig::Setting& myUsersConfig = myServiceConfig.add(ServiceUserList, libconfig::Setting::TypeArray);
                        foreach (const string& user, service.users)
                        {
                            myUsersConfig.add(libconfig::Setting::TypeString) = user;
                        }
                    }

                    // save to file
                    boost::filesystem::remove_all(anOutConfPath);
                    SettingsImpl::save(myConfig, anOutConfPath, true);
                }
                catch (SettingsError&)
                {
                    throw;
                }
                catch (libconfig::SettingException& e)
                {
                    TA_THROW_MSG(SettingsError, boost::format("Failed to generate user config from RCCD signing request. Failed to set setting %s. %s") %
                                 e.getPath() % e.what());
                }
                catch (std::exception& e)
                {
                    TA_THROW_MSG(SettingsError, boost::format("Failed to generate user config from from RCCD signing request. %s.") % e.what());
                }
            }

            void generateUserYamlConfigImpl(const RccdRequestData& aReq, const string& anOutConfPath)
            {
                try
                {
                    YAML::Emitter conf;

                    // populate global settings
                    conf << YAML::BeginMap;
                    conf << YAML::Key << LatestProvider << YAML::Value << aReq.providerName;
                    conf << YAML::Key << LatestService << YAML::Value << aReq.services.at(0).name;
                    conf << YAML::Key << LogLevel << YAML::Value << DefLogLevel;

                    // populate providers
                    conf << YAML::Key << ProviderList;
                    conf << YAML::Value << YAML::BeginSeq;
                    conf << YAML::BeginMap;
                    conf << YAML::Key << ProviderName << YAML::Value << aReq.providerName;
                    conf << YAML::Key << ProviderContentVersion << YAML::Value << aReq.contentVersion;
                    conf << YAML::Key << ProviderReseptSvrAddress << YAML::Value << toString(aReq.svrAddress, DefRcdpV2Port);
                    // CAs
                    conf << YAML::Key << ProviderCaList;
                    conf << YAML::Value<< YAML::BeginMap;
                    conf << YAML::Key << ProviderUserCa << YAML::Value << ta::CertUtils::getCertInfo(aReq.signingCaPem).subjCN;
                    conf << YAML::Key << ProviderServerCa << YAML::Value << ta::CertUtils::getCertInfo(aReq.commCaPem).subjCN;
                    conf << YAML::Key << ProviderPrimaryCa << YAML::Value << ta::CertUtils::getCertInfo(aReq.pcaPem).subjCN;
                    if (!aReq.rcaPem.empty())
                    {
                        conf << YAML::Key << ProviderRootCa << YAML::Value << ta::CertUtils::getCertInfo(aReq.rcaPem).subjCN;
                    }
                    conf << YAML::EndMap; // end CAs

                    // populate services
                    conf << YAML::Key << ServiceList;
                    conf << YAML::Value << YAML::BeginSeq;
                    foreach (const RccdRequestData::Service& service, aReq.services)
                    {
                        conf << YAML::BeginMap;

                        conf << YAML::Key << ServiceName << YAML::Value << service.name;
                        conf << YAML::Key << ServiceCertFormat << YAML::Value << str(resept::certformatP12);
                        conf << YAML::Key << ServiceCertChain << YAML::Value << false;
                        conf << YAML::Key << ServiceUri << YAML::Value << service.uri;
                        conf << YAML::Key << ServiceCertValidPercent << YAML::Value << service.certValidityPercentage;

                        // populate users
                        conf << YAML::Key << ServiceUserList;
                        conf << YAML::Value << YAML::BeginSeq;
                        foreach (const string& user, service.users)
                        {
                            conf << user;
                        }
                        conf << YAML::EndSeq; //users

                        conf << YAML::EndMap; // end per-service settings
                    }
                    conf << YAML::EndSeq; // end services

                    conf << YAML::EndMap; // end per-provider settings
                    conf << YAML::EndSeq; // end providers

                    conf << YAML::EndMap;// end global settings

                    // validate configuration
                    if (!conf.good())
                    {
                        TA_THROW_MSG(SettingsError, boost::format("Failed to generate user YAML config from from RCCD signing request. Failed to emit the document. %s.")  % conf.GetLastError());
                    }

                    // write configuration to file
                    ta::writeData(anOutConfPath, string(conf.c_str()));
                }
                catch (SettingsError&)
                {
                    throw;
                }
                catch (std::exception& e)
                {
                    TA_THROW_MSG(SettingsError, boost::format("Failed to generate user YAML config from from RCCD signing request. %s.") % e.what());
                }
            }

            void generateMasterLibConfigConfigImpl(const RccdRequestData& aReq, const string& anOutConfPath)
            {
                try
                {
                    if (!aReq.isAdminRccd())
                    {
                        TA_THROW_MSG(SettingsError, "Cannot generate master config because no admin configuration found in the RCCD signing request");
                    }

                    libconfig::Config myConfig;
                    libconfig::Setting& myConfigRoot = myConfig.getRoot();

                    // set global settings
                    myConfigRoot.add(ConfigVersion, libconfig::Setting::TypeString) = toStr(SettingsImpl::SupportedConfigVersion);

                    // set provider settings
                    libconfig::Setting& myProvidersConfig = myConfigRoot.add(ProviderList, libconfig::Setting::TypeList);
                    libconfig::Setting& myProviderConfig = myProvidersConfig.add(libconfig::Setting::TypeGroup);

                    myProviderConfig.add(ProviderName, libconfig::Setting::TypeString) = aReq.providerName;
                    myProviderConfig.add(ProviderContentVersion, libconfig::Setting::TypeInt) = aReq.contentVersion;

                    if (!aReq.allowOverwriteSvrAddress)
                    {
                        myProviderConfig.add(ProviderReseptSvrAddress, libconfig::Setting::TypeString) = toString(aReq.svrAddress, DefRcdpV2Port);
                    }

                    // Add settings for recovery of user config.
                    // There are no sensible compile-time defaults for these settings which could have been used later during recovery
                    // so we ought to supply these defaults upfront when we know them, that is now
                    libconfig::Setting& myDefProviderCasConfig = myProviderConfig.add(DefProviderCaList, libconfig::Setting::TypeArray);
                    myDefProviderCasConfig.add(libconfig::Setting::TypeString) = ta::CertUtils::getCertInfo(aReq.signingCaPem).subjCN;
                    myDefProviderCasConfig.add(libconfig::Setting::TypeString) = ta::CertUtils::getCertInfo(aReq.commCaPem).subjCN;
                    myDefProviderCasConfig.add(libconfig::Setting::TypeString) = ta::CertUtils::getCertInfo(aReq.pcaPem).subjCN;
                    if (!aReq.rcaPem.empty())
                    {
                        myDefProviderCasConfig.add(libconfig::Setting::TypeString) = ta::CertUtils::getCertInfo(aReq.rcaPem).subjCN;
                    }
                    myProviderConfig.add(DefProviderReseptSvrAddress, libconfig::Setting::TypeString) = toString(aReq.svrAddress, DefRcdpV2Port);


                    // set service settings
                    libconfig::Setting& myServicesConfig = myProviderConfig.add(ServiceList, libconfig::Setting::TypeList);
                    foreach (const RccdRequestData::Service& service, aReq.services)
                    {
                        libconfig::Setting& myServiceConfig = myServicesConfig.add(libconfig::Setting::TypeGroup);

                        myServiceConfig.add(ServiceName, libconfig::Setting::TypeString) = service.name;
                        myServiceConfig.add(DefServiceUri, libconfig::Setting::TypeString) = service.uri;

                        if (!service.allowOverwriteCertValidityPercentage)
                        {
                            myServiceConfig.add(ServiceCertValidPercent, libconfig::Setting::TypeInt) = (int)service.certValidityPercentage;
                        }
                        if (!service.allowOverwriteUsers)
                        {
                            libconfig::Setting& myUsersConfig = myServiceConfig.add(ServiceUserList, libconfig::Setting::TypeArray);
                            foreach (const string& user, service.users)
                            {
                                myUsersConfig.add(libconfig::Setting::TypeString) = user;
                            }
                        }
                    }

                    // save to file
                    boost::filesystem::remove_all(anOutConfPath);
                    SettingsImpl::save(myConfig, anOutConfPath, true);
                }
                catch (SettingsError&)
                {
                    throw;
                }
                catch (libconfig::SettingException& e)
                {
                    TA_THROW_MSG(SettingsError, boost::format("Failed to generate master config from RCCD signing request. Failed to set setting %s. %s") %
                                 e.getPath() % e.what());
                }
                catch (std::exception& e)
                {
                    TA_THROW_MSG(SettingsError, boost::format("Failed to generate master config from from RCCD signing request. %s.") % e.what());
                }
            }

            void generateMasterYamlConfigImpl(const RccdRequestData& aReq, const string& anOutConfPath)
            {
                try
                {
                    if (!aReq.isAdminRccd())
                    {
                        TA_THROW_MSG(SettingsError, "Cannot generate master config because no admin configuration found in the RCCD signing request");
                    }

                    YAML::Emitter conf;


                    // populate providers
                    conf << YAML::BeginMap;
                    conf << YAML::Key << ProviderList;
                    conf << YAML::Value << YAML::BeginSeq;
                    conf << YAML::BeginMap;
                    conf << YAML::Key << ProviderName << YAML::Value << aReq.providerName;
                    conf << YAML::Key << ProviderContentVersion << YAML::Value << aReq.contentVersion;
                    if (!aReq.allowOverwriteSvrAddress)
                    {
                        conf << YAML::Key << ProviderReseptSvrAddress << YAML::Value << toString(aReq.svrAddress, DefRcdpV2Port);
                    }


                    // Add settings for recovery of user config.
                    // There are no sensible compile-time defaults for these settings which could have been used later during recovery
                    // so we ought to supply these defaults upfront when we know them, that is now
                    conf << YAML::Key << DefProviderReseptSvrAddress << YAML::Value << toString(aReq.svrAddress, DefRcdpV2Port);
                    // CAs
                    conf << YAML::Key << DefProviderCaList;
                    conf << YAML::Value<< YAML::BeginMap;
                    conf << YAML::Key << ProviderUserCa << YAML::Value << ta::CertUtils::getCertInfo(aReq.signingCaPem).subjCN;
                    conf << YAML::Key << ProviderServerCa << YAML::Value << ta::CertUtils::getCertInfo(aReq.commCaPem).subjCN;
                    conf << YAML::Key << ProviderPrimaryCa << YAML::Value << ta::CertUtils::getCertInfo(aReq.pcaPem).subjCN;
                    if (!aReq.rcaPem.empty())
                    {
                        conf << YAML::Key << ProviderRootCa << YAML::Value << ta::CertUtils::getCertInfo(aReq.rcaPem).subjCN;
                    }
                    conf << YAML::EndMap; // end CAs

                    // populate services
                    conf << YAML::Key << ServiceList;
                    conf << YAML::Value << YAML::BeginSeq;
                    foreach (const RccdRequestData::Service& service, aReq.services)
                    {
                        conf << YAML::BeginMap;

                        conf << YAML::Key << ServiceName << YAML::Value << service.name;
                        conf << YAML::Key << DefServiceUri << YAML::Value << service.uri;
                        if (!service.allowOverwriteCertValidityPercentage)
                        {
                            conf << YAML::Key << ServiceCertValidPercent << YAML::Value << service.certValidityPercentage;
                        }
                        if (!service.allowOverwriteUsers)
                        {
                            // populate users
                            conf << YAML::Key << ServiceUserList;
                            conf << YAML::Value << YAML::BeginSeq;
                            foreach (const string& user, service.users)
                            {
                                conf << user;
                            }
                            conf << YAML::EndSeq; //users
                        }

                        conf << YAML::EndMap; // end per-service settings
                    }
                    conf << YAML::EndSeq; // end services

                    conf << YAML::EndMap; // end per-provider settings
                    conf << YAML::EndSeq; // end providers

                    conf << YAML::EndMap;// end global settings

                    // validate configuration
                    if (!conf.good())
                    {
                        TA_THROW_MSG(SettingsError, boost::format("Failed to generate master YAML config from from RCCD signing request. Failed to emit the document. %s.")  % conf.GetLastError());
                    }
                    // write configuration to file
                    ta::writeData(anOutConfPath, string(conf.c_str()));
                }
                catch (SettingsError&)
                {
                    throw;
                }
                catch (std::exception& e)
                {
                    TA_THROW_MSG(SettingsError, boost::format("Failed to generate master YAML config from from RCCD signing request. %s.") % e.what());
                }
            }


            void checkProviderAndServiceExist(const string& aProviderName, const string& aServiceName)
            {
                if (!ta::isElemExist(aProviderName, getProviders()))
                    TA_THROW_MSG(SettingsError, boost::format("Provider '%s' does not exist") % aProviderName);
                if (!ta::isElemExist(aServiceName, getServices(aProviderName)))
                    TA_THROW_MSG(SettingsError, boost::format("Service '%s' does not exist for provider '%s'") % aServiceName % aProviderName);
            }

            bool isCertValidPercentageSettingExistInMasterConfig(const string& aProviderName, const string& aServiceName)
            {
                if (SettingsImpl::isServiceValExist<int>(ServiceCertValidPercent, aProviderName, aServiceName, masterConfig))
                {
                    //@note libconfig behavior is that assigning a negative value to an unsigned produces 0,
                    // therefore we retrieve all numbers as signed and then test whether it falls into the desired range
                    int myCertValidityPercentage;
                    const string myPath = SettingsImpl::getServiceScalarVal(ServiceCertValidPercent, aProviderName, aServiceName, masterConfig, myCertValidityPercentage);
                    if (myCertValidityPercentage < 0 || myCertValidityPercentage > 100)
                    {
                        TA_THROW_MSG(SettingsError, boost::format("Invalid value %d for %s setting in the master config. The value should be between 0 and 100") % myCertValidityPercentage % myPath);
                    }
                    return true;
                }
                else
                {
                    return false;
                }
            }

            vector<string> getArraySettingFromReseptConfig(const string& aSettingName)
            {
                std::auto_ptr<libconfig::Config> myConfigPtr = SettingsImpl::load(reseptConfig);
                if (myConfigPtr->exists(aSettingName))
                {
                    return SettingsImpl::getArrayValue<vector<string> >(*myConfigPtr, aSettingName);
                }
                else
                {
                    return vector<string>();
                }
            }

            void addArrayElemToReseptConfig(const string& aVal, const string& aSettingName)
            {
                std::auto_ptr<libconfig::Config> myConfigPtr = SettingsImpl::load(reseptConfig);
                vector<string> myCurrentArrayVal;
                if (myConfigPtr->exists(aSettingName))
                {
                    myCurrentArrayVal = SettingsImpl::getArrayValue<vector<string> >(*myConfigPtr, aSettingName);
                }
                if (!ta::isElemExist(aVal, myCurrentArrayVal))
                {
                    myCurrentArrayVal.push_back(aVal);
                    SettingsImpl::assignArrayValue(*myConfigPtr, "", aSettingName, myCurrentArrayVal, SettingsImpl::settingCreateIfNotExist);
                    SettingsImpl::save(*myConfigPtr, reseptConfig);
                }
            }

        }
        // end of private API

        //
        // Global RESEPT settings
        //

        string getReseptInstallDir()
        {
            auto_ptr<libconfig::Config> myConfigPtr = SettingsImpl::load(reseptConfig);
            string myRetVal;
            if (!myConfigPtr->lookupValue(ReseptInstallDir, myRetVal))
                TA_THROW_MSG(SettingsError, boost::format("%s setting does not exist in the resept config or it is not a string") % ReseptInstallDir);
            return myRetVal;
        }
        void setReseptInstallDir(const string& aDir)
        {
            auto_ptr<libconfig::Config> myConfigPtr = SettingsImpl::load(reseptConfig);
            if (!myConfigPtr->exists(ReseptInstallDir))
                TA_THROW_MSG(SettingsError, boost::format("%s setting does not exist in the resept config") % ReseptInstallDir);
            libconfig::Setting& mySetting = myConfigPtr->lookup(ReseptInstallDir);
            if (mySetting.getType() != libconfig::Setting::TypeString)
                TA_THROW_MSG(SettingsError, boost::format("%s setting in the resept config is not a string") % mySetting.getPath());
            mySetting = aDir;
            SettingsImpl::save(*myConfigPtr, reseptConfig);
        }
        unsigned int getReseptBrokerServicePort()
        {
            std::auto_ptr<libconfig::Config> myConfigPtr = SettingsImpl::load(reseptConfig);
            int myRetVal;
            if (!myConfigPtr->lookupValue(ReseptBrokerServicePort, myRetVal))
                TA_THROW_MSG(SettingsError, boost::format("%s setting does not exist in the resept config or is not a number") % ReseptBrokerServicePort);
            if (myRetVal < 0)
                TA_THROW_MSG(SettingsError, boost::format("%s setting value in the resept config should be a positive number") % ReseptBrokerServicePort);
            return static_cast<unsigned int>(myRetVal);
        }
        void setReseptBrokerServicePort(unsigned int aPort)
        {
            auto_ptr<libconfig::Config> myConfigPtr = SettingsImpl::load(reseptConfig);
            if (!myConfigPtr->exists(ReseptBrokerServicePort))
                TA_THROW_MSG(SettingsError, boost::format("%s setting does not exist in the resept config") % ReseptBrokerServicePort);
            libconfig::Setting& mySetting = myConfigPtr->lookup(ReseptBrokerServicePort);
            if (mySetting.getType() != libconfig::Setting::TypeInt)
                TA_THROW_MSG(SettingsError, boost::format("%s setting in the resept config is not a number") % mySetting.getPath());
            mySetting = static_cast<int>(aPort);
            SettingsImpl::save(*myConfigPtr, reseptConfig);
        }
        vector<string> getInstalledProviders()
        {
            std::auto_ptr<libconfig::Config> myConfigPtr = SettingsImpl::load(reseptConfig);
            return SettingsImpl::getArrayValue<vector<string> >(*myConfigPtr, ReseptInstalledProviders);
        }
        void addInstalledProvider(const string& aProviderName)
        {
            std::auto_ptr<libconfig::Config> myConfigPtr = SettingsImpl::load(reseptConfig);
            vector<string> myInstalledProviders = SettingsImpl::getArrayValue<vector<string> >(*myConfigPtr, ReseptInstalledProviders);
            if (ta::isElemExist(aProviderName, myInstalledProviders))
                return;
            myInstalledProviders.push_back(aProviderName);
            SettingsImpl::assignArrayValue(*myConfigPtr, "", ReseptInstalledProviders, myInstalledProviders);
            SettingsImpl::save(*myConfigPtr, reseptConfig);
        }
        void removeInstalledProvider(const string& aProviderName)
        {
            std::auto_ptr<libconfig::Config> myConfigPtr = SettingsImpl::load(reseptConfig);

            vector<string> myInstalledProviders = SettingsImpl::getArrayValue<vector<string> >(*myConfigPtr, ReseptInstalledProviders);
            vector<string>::iterator it = std::find(myInstalledProviders.begin(), myInstalledProviders.end(), aProviderName);
            if (it != myInstalledProviders.end())
            {
                myInstalledProviders.erase(it);
                SettingsImpl::assignArrayValue(*myConfigPtr, "", ReseptInstalledProviders, myInstalledProviders);
                SettingsImpl::save(*myConfigPtr, reseptConfig);
            }
        }
        vector<string> getInstalledUserCaCNs()
        {
            return getArraySettingFromReseptConfig(ReseptInstalledUserCAs);
        }
        vector<string> getInstalledServerCaCNs()
        {
            return getArraySettingFromReseptConfig(ReseptInstalledServerCAs);
        }
        vector<string> getInstalledPrimaryCaCNs()
        {
            return getArraySettingFromReseptConfig(ReseptInstalledPrimaryCAs);
        }
        vector<string> getInstalledRootCaCNs()
        {
            return getArraySettingFromReseptConfig(ReseptInstalledRootCAs);
        }
        vector<string> getInstalledExtraSigningIntCaSha1Fingerprints()
        {
            return getArraySettingFromReseptConfig(ReseptInstalledExtraSigningIntCAs);
        }
        vector<string> getInstalledExtraSigningRootCaSha1Fingerprints()
        {
            return getArraySettingFromReseptConfig(ReseptInstalledExtraSigningRootCAs);
        }
        void addInstalledUserCA(const string& aCN)
        {
            addArrayElemToReseptConfig(aCN, ReseptInstalledUserCAs);
        }
        void addInstalledServerCA(const string& aCN)
        {
            addArrayElemToReseptConfig(aCN, ReseptInstalledServerCAs);
        }
        void addInstalledPrimaryCA(const string& aCN)
        {
            addArrayElemToReseptConfig(aCN, ReseptInstalledPrimaryCAs);
        }
        void addInstalledRootCA(const string& aCN)
        {
            addArrayElemToReseptConfig(aCN, ReseptInstalledRootCAs);
        }
        void addInstalledExtraSigningIntCA(const string& aSha1Fingerprint)
        {
            addArrayElemToReseptConfig(aSha1Fingerprint, ReseptInstalledExtraSigningIntCAs);
        }
        void addInstalledExtraSigningRootCA(const string& aSha1Fingerprint)
        {
            addArrayElemToReseptConfig(aSha1Fingerprint, ReseptInstalledExtraSigningRootCAs);
        }

        vector<string> getCustomizedUsers()
        {
            return getArraySettingFromReseptConfig(ReseptCustomizedUsers);
        }
        void addCustomizedUser(const string& aUserName)
        {
            addArrayElemToReseptConfig(aUserName, ReseptCustomizedUsers);
        }

        string getCertValidPercentParamName()
        {
            return ServiceCertValidPercent;
        }


        //
        // Global settings
        //

        string getLatestProvider()
        {
            auto_ptr<libconfig::Config> myConfigPtr = SettingsImpl::load(userConfig);
            string myProviderName;
            if (!myConfigPtr->lookupValue(LatestProvider, myProviderName))
                TA_THROW_MSG(SettingsError, boost::format("%s setting does not exist in the user config or is not a string") % LatestProvider);
            if (!ta::isElemExist(myProviderName, SettingsImpl::getProviders(userConfig)))
                TA_THROW_MSG(SettingsError, boost::format("Latest provider '%s' does not exist in the user config") % myProviderName);
            return myProviderName;
        }
        string getLatestService()
        {
            auto_ptr<libconfig::Config> myConfigPtr = SettingsImpl::load(userConfig);
            string myServiceName;
            if (!myConfigPtr->lookupValue(LatestService, myServiceName))
                TA_THROW_MSG(SettingsError, boost::format("%s setting does not exist in the user config or is not a string") % LatestService);

            const string myLatestProvider = getLatestProvider();
            if (!ta::isElemExist(myServiceName, SettingsImpl::getServices(myLatestProvider, userConfig)))
                TA_THROW_MSG(SettingsError, boost::format("Service '%s' does not exist for provider '%s' in the user config") % myServiceName % myLatestProvider);
            return myServiceName;
        }

        void setLatestProviderService(const string& aProviderName, const string& aServiceName)
        {
            if (!ta::isElemExist(aProviderName, getProviders(userConfig)))
                TA_THROW_MSG(SettingsError, boost::format("Provider '%s' does not exist in the user config") % aProviderName);
            if (!ta::isElemExist(aServiceName, getServices(aProviderName, userConfig)))
                TA_THROW_MSG(SettingsError, boost::format("Service '%s' does not exist for provider '%s' in the user config") % aServiceName % aProviderName);

            auto_ptr<libconfig::Config> myConfigPtr = SettingsImpl::load(userConfig);

            if (!myConfigPtr->exists(LatestProvider))
                myConfigPtr->getRoot().add(LatestProvider, libconfig::Setting::TypeString);
            libconfig::Setting& myLatestProviderSetting = myConfigPtr->lookup(LatestProvider);
            if (myLatestProviderSetting.getType() != libconfig::Setting::TypeString)
                TA_THROW_MSG(SettingsError, boost::format("%s setting in the user config is not a string") % myLatestProviderSetting.getPath());
            myLatestProviderSetting = aProviderName;

            if (!myConfigPtr->exists(LatestService))
                myConfigPtr->getRoot().add(LatestService, libconfig::Setting::TypeString);
            libconfig::Setting& myLatestServiceSetting = myConfigPtr->lookup(LatestService);
            if (myLatestServiceSetting.getType() != libconfig::Setting::TypeString)
                TA_THROW_MSG(SettingsError, boost::format("%s setting in the user config is not a string") % myLatestServiceSetting.getPath());
            myLatestServiceSetting = aServiceName;

            SettingsImpl::save(*myConfigPtr, userConfig);
        }

        vector<pair<string, string> > getProviderServiceForRequestedUri(const string& aRequestedUrl, IsServiceUriFunc anIsServiceUri)
        {
            vector<std::pair<string, string> > myRetVal;
            foreach (const string& provider, getProviders())
            {
                foreach (const string& service, getServices(provider))
                {
                    const string myServiceUri = getServiceUri(provider, service);
                    if (anIsServiceUri(aRequestedUrl, myServiceUri))
                    {
                        myRetVal.push_back(std::make_pair(provider, service));
                    }
                }
            }
            return myRetVal;
        }



        //
        // Provider settings
        //

        vector<string> getProviders()
        {
            vector<string> myProviders = SettingsImpl::getProviders(userConfig);
            if (SettingsImpl::hasDuplicates(myProviders))
                TA_THROW_MSG(SettingsError, "Duplicate provider found");
            return myProviders;
        }

        bool removeProviderFromUserConfig(const string& aProviderName)
        {
            if (SettingsImpl::isMasterConfigExist() && ta::isElemExist(aProviderName, getProviders(masterConfig)))
            {
                TA_THROW_MSG(SettingsError, "Cannot remove provider " + aProviderName + " because it has associtated master settings");
            }

            // when the only provider exists and it matches to be removed, remove the entire user config file
            if (ta::isElemExist(aProviderName, getProviders(userConfig)) && getProviders(userConfig).size() == 1)
            {
                try {
                    boost::filesystem::remove(SettingsImpl::getConfigFilePath(userConfig));
                    return true;
                } catch (std::exception& e) {
                    TA_THROW_MSG(SettingsError, e.what());
                }
            }

            auto_ptr<libconfig::Config> myUserConfigPtr = load(userConfig);

            const unsigned int myNumProviders = SettingsImpl::getListSize(*myUserConfigPtr, ProviderList);
            for (unsigned int i=0; i < myNumProviders; ++i)
            {
                const string myNamePath = SettingsImpl::getProviderSettingPath(i, ProviderName);
                string myProviderName;
                if (!myUserConfigPtr->lookupValue(myNamePath, myProviderName))
                {
                    TA_THROW_MSG(SettingsError, boost::format("%s setting not found in the user config or it is not a string") % myNamePath);
                }

                if (myProviderName == aProviderName)
                {
                    myUserConfigPtr->lookup(ProviderList).remove(i);

                    // if just removed provider was also referred as the latest, reset the latest provider to the first one
                    string myLatestProviderName;
                    if (!myUserConfigPtr->lookupValue(LatestProvider, myLatestProviderName))
                    {
                        TA_THROW_MSG(SettingsError, boost::format("%s setting does not exist in the user config or is not a string") % LatestProvider);
                    }
                    if (myLatestProviderName == aProviderName)
                    {
                        const string myLatestProviderPath = SettingsImpl::getProviderSettingPath(0, ProviderName);
                        if (!myUserConfigPtr->lookupValue(myLatestProviderPath, myLatestProviderName))
                        {
                            TA_THROW_MSG(SettingsError, boost::format("%s setting does not exist in the user config or is not a string") % myLatestProviderPath);
                        }

                        string myLatestServiceName;
                        const string myLatestServicePath = SettingsImpl::getServiceSettingPath(0, 0, ServiceName);
                        if (!myUserConfigPtr->lookupValue(myLatestServicePath, myLatestServiceName))
                        {
                            TA_THROW_MSG(SettingsError, boost::format("%s setting does not exist in the user config or is not a string") % myLatestServicePath);
                        }

                        myUserConfigPtr->lookup(LatestProvider) = myLatestProviderName;
                        myUserConfigPtr->lookup(LatestService) = myLatestServiceName;
                    }

                    SettingsImpl::save(*myUserConfigPtr, userConfig);
                    return true;
                }
            }

            return false; // provider not found
        }

        string getProviderInstallDir()
        {
            return getProviderInstallDir(getLatestProvider());
        }
        string getProviderInstallDir(const string& aProviderName)
        {
#ifdef _WIN32
            return getReseptInstallDir() + "\\" + aProviderName;
#else
            return getReseptConfigDir() + "/" + aProviderName;
#endif
        }

        int getProviderContentVersion(const string& aProviderName)
        {
            bool myDummyFromMasterConfig;
            return getProviderContentVersion(aProviderName, myDummyFromMasterConfig);
        }
        int getProviderContentVersion(const string& aProviderName, bool& aFromMasterConfig)
        {
            if (!ta::isElemExist(aProviderName, getProviders()))
                TA_THROW_MSG(SettingsError, boost::format("Provider '%s' does not exist") % aProviderName);
            aFromMasterConfig = SettingsImpl::isProviderValExist<long>(ProviderContentVersion, aProviderName, masterConfig);
            int myRetVal;
            SettingsImpl::getProviderScalarVal(ProviderContentVersion, aProviderName, userConfig, myRetVal);
            return myRetVal;
        }

        bool isLastUserMsgUtcExist()
        {
            return isLastUserMsgUtcExist(getLatestProvider());
        }
        bool isLastUserMsgUtcExist(const string& aProviderName)
        {
            return SettingsImpl::isProviderValExist<string>(ProviderLastUserMsgUtc, aProviderName, userConfig);
        }
        string getLastUserMsgUtc()
        {
            return getLastUserMsgUtc(getLatestProvider());
        }
        string getLastUserMsgUtc(const string& aProviderName)
        {
            if (!ta::isElemExist(aProviderName, getProviders()))
                TA_THROW_MSG(SettingsError, boost::format("Provider '%s' does not exist") % aProviderName);
            string myRetVal;
            SettingsImpl::getProviderScalarVal(ProviderLastUserMsgUtc, aProviderName, userConfig, myRetVal);
            return myRetVal;
        }
        void setLastUserMsgUtc(const string& anUtc)
        {
            setLastUserMsgUtc(getLatestProvider(), anUtc);
        }
        void setLastUserMsgUtc(const string& aProviderName, const string& anUtc)
        {
            SettingsImpl::setProviderScalarVal(ProviderLastUserMsgUtc, aProviderName,  userConfig, anUtc);
        }

        bool generateConfigs(const RccdRequestData& aReq,
                             const string& anOutUserLibConfigConfPath,
                             const string& anOutUserYamlConfPath,
                             const string& anOutMasterLibConfigConfPath,
                             const string& anOutMasterYamlConfPath)
        {
            generateUserLibConfigConfigImpl(aReq, anOutUserLibConfigConfPath);
            generateUserYamlConfigImpl(aReq, anOutUserYamlConfPath);

            if (aReq.isAdminRccd())
            {
                generateMasterLibConfigConfigImpl(aReq, anOutMasterLibConfigConfPath);
                generateMasterYamlConfigImpl(aReq, anOutMasterYamlConfPath);
                return true;
            }
            else
            {
                return false;
            }
        }


        void installProvider(const string& aUserConfigPath, bool anIsAdminInstall, const string& aMasterConfigPath)
        {
            // Retrieve provider name
            const string myProvider0NameSettingName = SettingsImpl::getProviderSettingPath(0, ProviderName);

            auto_ptr<libconfig::Config> mySrcUserConfigPtr = SettingsImpl::load(aUserConfigPath);
            string myProviderName;
            if (!mySrcUserConfigPtr->lookupValue(myProvider0NameSettingName, myProviderName))
                TA_THROW_MSG(SettingsError, boost::format("No %s setting exist in %s or it is not a string") %  myProvider0NameSettingName % aUserConfigPath);
            if (mySrcUserConfigPtr->exists(SettingsImpl::getProviderPath(1)))
                TA_THROW_MSG(SettingsError, boost::format("More than one provider exists in the source user config %s. Only one provider is allowed.") % aUserConfigPath);

            auto_ptr<libconfig::Config> mySrcMasterConfigPtr;
            if (anIsAdminInstall)
            {
                // check provider name and content version match for user and master configs
                mySrcMasterConfigPtr = SettingsImpl::load(aMasterConfigPath);
                string myMasterProviderName;
                if (!mySrcMasterConfigPtr->lookupValue(myProvider0NameSettingName, myMasterProviderName))
                    TA_THROW_MSG(SettingsError, boost::format("No %s setting exist in %s or it is not a string") %  myProvider0NameSettingName % aMasterConfigPath);
                if (myMasterProviderName != myProviderName)
                    TA_THROW_MSG(SettingsError, boost::format("Source user and master configs have different providers. User config provider: %s, master config provider: %s") % myProviderName % myMasterProviderName);

                int myUserContentVersion, myMasterContentVersion;
                const string myProvider0ContentVersionSettingName = SettingsImpl::getProviderSettingPath(0, ProviderContentVersion);
                if (!mySrcUserConfigPtr->lookupValue(myProvider0ContentVersionSettingName, myUserContentVersion))
                    TA_THROW_MSG(SettingsError, boost::format("No %s setting exist in %s or it is not an integer") %  myProvider0ContentVersionSettingName % aUserConfigPath);
                if (!mySrcMasterConfigPtr->lookupValue(myProvider0ContentVersionSettingName, myMasterContentVersion))
                    TA_THROW_MSG(SettingsError, boost::format("No %s setting exist in %s or it is not an integer") %  myProvider0ContentVersionSettingName % aMasterConfigPath);
                if (myMasterContentVersion != myUserContentVersion)
                    TA_THROW_MSG(SettingsError, boost::format("Source user and master configs have different content version. User config provider: %d, master config provider: %d") % myUserContentVersion % myMasterContentVersion);

                if (mySrcMasterConfigPtr->exists(SettingsImpl::getProviderPath(1)))
                    TA_THROW_MSG(SettingsError, boost::format("More than one provider exists in the source master config %s. Only one provider is allowed.") % aMasterConfigPath);
            }

            // Update user config in memory
            auto_ptr<libconfig::Config> myTargetUserConfigPtr;
            if (ta::isFileExist(getUserConfigPath()))
            {
                myTargetUserConfigPtr = SettingsImpl::load(userConfig);
                if (SettingsImpl::getConfigVersion(*myTargetUserConfigPtr) != SettingsImpl::getConfigVersion(*mySrcUserConfigPtr)) // we do not support config up/downgrading yet, just expect exact version match
                    TA_THROW_MSG(SettingsError, boost::format("Version mismatch for %s user config and %s") % resept::ProductName % aUserConfigPath);

                SettingsImpl::mergeConfigs(SettingsImpl::mergeUserToUser, *mySrcUserConfigPtr, *myTargetUserConfigPtr);
            }
            else
            {
                myTargetUserConfigPtr = mySrcUserConfigPtr;
            }
            assert(myTargetUserConfigPtr.get());

            // Update master config in memory if required
            auto_ptr<libconfig::Config> myTargetMasterConfigPtr;
            if (anIsAdminInstall)
            {
                if (ta::isFileExist(Settings::getMasterConfigPath()))
                {
                    myTargetMasterConfigPtr = SettingsImpl::load(masterConfig);
                    if (SettingsImpl::getConfigVersion(*myTargetMasterConfigPtr) != SettingsImpl::getConfigVersion(*mySrcMasterConfigPtr)) // we do yet not support config up-/downgrading and expect exact version match
                        TA_THROW_MSG(SettingsError, boost::format("Version mismatch for %s master config and %s") % resept::ProductName % aMasterConfigPath);

                    SettingsImpl::mergeConfigs(SettingsImpl::mergeMasterToMaster, *mySrcMasterConfigPtr, *myTargetMasterConfigPtr);
                }
                else
                {
                    myTargetMasterConfigPtr = mySrcMasterConfigPtr;
                }
                assert(myTargetMasterConfigPtr.get());
            }

            // Save the updated configs to disk
            SettingsImpl::save(*myTargetUserConfigPtr, userConfig, true);
            if (anIsAdminInstall)
                SettingsImpl::save(*myTargetMasterConfigPtr, masterConfig, true);
            if (SettingsImpl::isMasterConfigExist())
                SettingsImpl::updateUserConfigFromMaster();
        }


        void adminInstallProvider(const string& aUserConfigPath, const string& aMasterConfigPath)
        {
            installProvider(aUserConfigPath, true, aMasterConfigPath);
        }

        void userInstallProvider(const string& aUserConfigPath)
        {
            installProvider(aUserConfigPath, false, "");
        }

        ta::NetUtils::RemoteAddress getReseptSvrAddress()
        {
            return getReseptSvrAddress(getLatestProvider());
        }
        ta::NetUtils::RemoteAddress getReseptSvrAddress(bool& aFromMasterConfig)
        {
            return getReseptSvrAddress(getLatestProvider(), aFromMasterConfig);
        }
        ta::NetUtils::RemoteAddress getReseptSvrAddress(const string& aProviderName)
        {
            bool myDummyFromMasterConfig;
            return getReseptSvrAddress(aProviderName, myDummyFromMasterConfig);
        }
        ta::NetUtils::RemoteAddress getReseptSvrAddress(const string& aProviderName, bool& aFromMasterConfig)
        {
            if (!ta::isElemExist(aProviderName, getProviders()))
                TA_THROW_MSG(SettingsError, boost::format("Provider '%s' does not exist") % aProviderName);
            aFromMasterConfig = SettingsImpl::isProviderValExist<string>(ProviderReseptSvrAddress, aProviderName, masterConfig);
            string myReseptSvrAddr;
            string myPath = SettingsImpl::getProviderScalarVal(ProviderReseptSvrAddress, aProviderName, userConfig, myReseptSvrAddr);
            try  {
                return ta::NetUtils::parseHost(myReseptSvrAddr, DefRcdpV2Port);
            } catch (std::exception& e)  {
                TA_THROW_MSG(SettingsError, boost::format("'%s': %s server address is invalid in user config. %s") % myPath % resept::ProductName % e.what());
            }
        }
        void setReseptSvrAddress(const string& aProviderName, const ta::NetUtils::RemoteAddress& anAddr)
        {
            if (!ta::isElemExist(aProviderName, getProviders()))
                TA_THROW_MSG(SettingsError, boost::format("Provider '%s' does not exist") % aProviderName);
            const string mySvrAddrStr = toString(anAddr, DefRcdpV2Port);
            SettingsImpl::setProviderScalarVal(ProviderReseptSvrAddress, aProviderName, userConfig, mySvrAddrStr);
        }

        vector<string> getCAs(const string& aProviderName)
        {
            vector<string> myRetVal;
            SettingsImpl::getProviderArrayVal<vector<string> >(ProviderCaList, aProviderName, userConfig, myRetVal);
            return myRetVal;
        }
        string getUserCaName(const string& aProviderName)
        {
            if (!ta::isElemExist(aProviderName, getProviders()))
                TA_THROW_MSG(SettingsError, boost::format("Provider '%s' does not exist") % aProviderName);
            const vector<string> myCaNames = getCAs(aProviderName);
            if (myCaNames.size() < 3)
                TA_THROW_MSG(SettingsError, boost::format("CA array should contain at least 3 elements, actual %u") % ((unsigned int)myCaNames.size()));
            return myCaNames[0];
        }
        string getServerCaName(const string& aProviderName)
        {
            if (!ta::isElemExist(aProviderName, getProviders()))
                TA_THROW_MSG(SettingsError, boost::format("Provider '%s' does not exist") % aProviderName);
            const vector<string> myCaNames = getCAs(aProviderName);
            if (myCaNames.size() < 3)
                TA_THROW_MSG(SettingsError, boost::format("CA array should contain at least 3 elements, actual %u") % ((unsigned int)myCaNames.size()));
            return myCaNames[1];
        }
        string getPrimaryCaName(const string& aProviderName)
        {
            if (!ta::isElemExist(aProviderName, getProviders()))
                TA_THROW_MSG(SettingsError, boost::format("Provider '%s' does not exist") % aProviderName);
            const vector<string> myCaNames = getCAs(aProviderName);
            if (myCaNames.size() < 3)
                TA_THROW_MSG(SettingsError, boost::format("CA array should contain at least 3 elements, actual %u") % ((unsigned int)myCaNames.size()));
            return myCaNames[2];
        }
        bool isRootCaExist(const string& aProviderName)
        {
            const vector<string> myCaNames = getCAs(aProviderName);
            if (myCaNames.size() < 3)
                TA_THROW_MSG(SettingsError, boost::format("CA array should contain at least 3 elements, actual %u") % ((unsigned int)myCaNames.size()));
            return (myCaNames.size() >= 4);
        }
        string getRootCaName(const string& aProviderName)
        {
            if (!ta::isElemExist(aProviderName, getProviders()))
                TA_THROW_MSG(SettingsError, boost::format("Provider '%s' does not exist") % aProviderName);
            const vector<string> myCaNames = getCAs(aProviderName);
            if (myCaNames.size() < 4)
                TA_THROW_MSG(SettingsError, boost::format("CA array should contain at least 4 elements, actual %u") % ((unsigned int)myCaNames.size()));
            return myCaNames[3];
        }
        string getUserCaName()
        {
            return getUserCaName(getLatestProvider());
        }
        string getServerCaName()
        {
            return getServerCaName(getLatestProvider());
        }
        string getPrimaryCaName()
        {
            return getPrimaryCaName(getLatestProvider());
        }
        bool isRootCaExist()
        {
            return isRootCaExist(getLatestProvider());
        }
        string getRootCaName()
        {
            return getRootCaName(getLatestProvider());
        }

        string getLogLevel()
        {
            return getLogLevel(getLatestProvider());
        }
        string getLogLevel(bool& aFromMasterConfig)
        {
            return getLogLevel(getLatestProvider(), aFromMasterConfig);
        }
        string getLogLevel(const string& aProviderName)
        {
            bool myDummyFromMasterConfig;
            return getLogLevel(aProviderName, myDummyFromMasterConfig);
        }
        string getLogLevel(const string& aProviderName, bool& aFromMasterConfig)
        {
            if (!ta::isElemExist(aProviderName, getProviders()))
                TA_THROW_MSG(SettingsError, boost::format("Provider '%s' does not exist") % aProviderName);

            aFromMasterConfig = SettingsImpl::isProviderValExist<string>(ProviderLogLevel, aProviderName, masterConfig);
            if (SettingsImpl::isProviderValExist<string>(ProviderLogLevel, aProviderName, userConfig))
            {
                string myLogLevel;
                SettingsImpl::getProviderScalarVal(ProviderLogLevel, aProviderName, userConfig, myLogLevel);
                return myLogLevel;
            }
            else
            {
                return DefLogLevel;
            }
        }
        void setLogLevel(const string& aProviderName, const string& aLogLevel)
        {
            if (!ta::isElemExist(aProviderName, getProviders()))
                TA_THROW_MSG(SettingsError, boost::format("Provider '%s' does not exist") % aProviderName);
            SettingsImpl::setProviderScalarVal(ProviderLogLevel, aProviderName, userConfig, aLogLevel);
        }


        //
        // Service settings
        //

        vector<string> getServices()
        {
            return getServices(getLatestProvider());
        }
        vector<string> getServices(const string& aProviderName)
        {
            if (!ta::isElemExist(aProviderName, getProviders()))
                TA_THROW_MSG(SettingsError, boost::format("Provider '%s' does not exist") % aProviderName);
            const vector<string> myServices = SettingsImpl::getServices(aProviderName, userConfig);
            if (SettingsImpl::hasDuplicates(myServices))
                TA_THROW_MSG(SettingsError, boost::format("Duplicate service found for provider '%s'") % aProviderName);
            return myServices;
        }

        bool isDisplayServiceName()
        {
            const string myProviderName = getLatestProvider();
            const string myServiceName = getLatestService();
            checkProviderAndServiceExist(myProviderName, myServiceName);

            if (SettingsImpl::isServiceValExist<bool>(ServiceDisplayName, myProviderName, myServiceName, userConfig))
            {
                bool myRetVal;
                SettingsImpl::getServiceScalarVal(ServiceDisplayName, myProviderName, myServiceName, userConfig, myRetVal);
                return myRetVal;
            }
            else
            {
                return DefServiceDisplayName;
            }
        }

        bool isCleanupUserCert(const string& aProviderName, const string& aServiceName)
        {
            checkProviderAndServiceExist(aProviderName, aServiceName);

            if (SettingsImpl::isServiceValExist<bool>(ServiceCleanupUserCert, aProviderName, aServiceName, userConfig))
            {
                bool myRetVal;
                SettingsImpl::getServiceScalarVal(ServiceCleanupUserCert, aProviderName, aServiceName, userConfig, myRetVal);
                return myRetVal;
            }
            else
            {
                return DefServiceCleanupUserCert;
            }
        }
        bool isCleanupUserCert()
        {
            return isCleanupUserCert(getLatestProvider(), getLatestService());
        }

        bool isCertChain(const string& aProviderName, const string& aServiceName)
        {
            checkProviderAndServiceExist(aProviderName, aServiceName);

            if (SettingsImpl::isServiceValExist<bool>(ServiceCertChain, aProviderName, aServiceName, userConfig))
            {
                bool myRetVal;
                SettingsImpl::getServiceScalarVal(ServiceCertChain, aProviderName, aServiceName, userConfig, myRetVal);
                return myRetVal;
            }
            else
            {
                return DefIsCertChain;
            }
        }
        bool isCertChain()
        {
            const string myProviderName = getLatestProvider();
            const string myServiceName = getLatestService();
            return isCertChain(myProviderName, myServiceName);
        }

        unsigned int getCertValidPercentage(const string& aProviderName, const string& aServiceName, bool& aFromMasterConfig)
        {
            checkProviderAndServiceExist(aProviderName, aServiceName);

            aFromMasterConfig = isCertValidPercentageSettingExistInMasterConfig(aProviderName, aServiceName);

            if (SettingsImpl::isServiceValExist<int>(ServiceCertValidPercent, aProviderName, aServiceName, userConfig))
            {
                //@note libconfig behavior is that assigning a negative value to an unsigned produces 0,
                // therefore we retrieve all numbers as signed and then test whether it falls into the desired range
                int myRetVal;
                const string myPath = SettingsImpl::getServiceScalarVal(ServiceCertValidPercent, aProviderName, aServiceName, userConfig, myRetVal);
                if (myRetVal < 0 || myRetVal > 100)
                {
                    TA_THROW_MSG(SettingsError, boost::format("Invalid value %d for %s setting in the user config. The value should be between 0 and 100") % myRetVal % myPath);
                }
                return static_cast<unsigned int>(myRetVal);
            }
            else
            {
                return DefCertValidPercent;
            }
        }
        unsigned int getCertValidPercentage(const string& aProviderName, const string& aServiceName)
        {
            bool myIsFromMasterConfigDummy;
            return getCertValidPercentage(aProviderName, aServiceName, myIsFromMasterConfigDummy);
        }
        unsigned int getCertValidPercentage()
        {
            return getCertValidPercentage(getLatestProvider(), getLatestService());
        }

        resept::CertFormat getCertFormat(const string& aProviderName, const string& aServiceName)
        {
            checkProviderAndServiceExist(aProviderName, aServiceName);

            if (SettingsImpl::isServiceValExist<string>(ServiceCertFormat, aProviderName, aServiceName, userConfig))
            {
                string myCertFormatStr;
                string myPath = SettingsImpl::getServiceScalarVal(ServiceCertFormat, aProviderName, aServiceName, userConfig, myCertFormatStr);
                resept::CertFormat myCertFormat;
                if (!resept::parseCertFormat(myCertFormatStr, myCertFormat))
                {
                    TA_THROW_MSG(SettingsError, boost::format("Error parsing setting %s in the user config.") % myPath);
                }
                return myCertFormat;
            }
            else
            {
                return DefCertFormat;
            }
        }
        resept::CertFormat getCertFormat()
        {
            return getCertFormat(getLatestProvider(), getLatestService());
        }
        void setCertFormat(const string& aProviderName, const string& aServiceName, resept::CertFormat aCertFormat)
        {
            setServiceScalarVal(ServiceCertFormat, aProviderName, aServiceName, userConfig, str(aCertFormat));
        }

        string getServiceUri()
        {
            return getServiceUri(getLatestProvider(), getLatestService());
        }
        string getServiceUri(const string& aProviderName, const string& aServiceName)
        {
            string myRetVal;
            SettingsImpl::getServiceScalarVal(ServiceUri, aProviderName, aServiceName, userConfig, myRetVal);
            return myRetVal;
        }
        void setServiceUri(const string& aServiceUri)
        {
            setServiceScalarVal(ServiceUri, getLatestProvider(), getLatestService(), userConfig, aServiceUri);
        }

        vector<string> getImportedUserCertFingerprints()
        {
            return getImportedUserCertFingerprints(getLatestProvider(), getLatestService());
        }
        vector<string> getImportedUserCertFingerprints(const string& aProviderName, const string& aServiceName)
        {
            vector<string> myRetVal;
            if (SettingsImpl::isServiceValExist<vector<string> >(ServiceImportedUserCerts, aProviderName, aServiceName, userConfig))
            {
                SettingsImpl::getServiceArrayVal<vector<string> >(ServiceImportedUserCerts, aProviderName, aServiceName, userConfig, myRetVal);
            }
            return myRetVal;
        }
        void addImportedUserCertFingerprint(const string& aCertFingerprint)
        {
            vector<string> myFingerprints = getImportedUserCertFingerprints();
            if (!ta::isElemExist(aCertFingerprint, myFingerprints))
            {
                myFingerprints.push_back(aCertFingerprint);
                setServiceArrayVal(ServiceImportedUserCerts, getLatestProvider(), getLatestService(), userConfig, myFingerprints);
            }
        }
        void removeImportedUserCertFingerprints(const vector<string>& aFingerprints)
        {
            vector<string> myRemainingFingerprints;
            foreach(const string& fingerprint, getImportedUserCertFingerprints())
            {
                if (!ta::isElemExist(fingerprint, aFingerprints))
                {
                    myRemainingFingerprints.push_back(fingerprint);
                }
            }
            setServiceArrayVal(ServiceImportedUserCerts, getLatestProvider(), getLatestService(), userConfig, myRemainingFingerprints);
        }


        //
        // User
        //

        Users getUsers()
        {
            bool myDummyFromMasterConfig;
            return getUsers(getLatestProvider(), getLatestService(), myDummyFromMasterConfig);
        }
        Users getUsers(bool& aFromMasterConfig)
        {
            return getUsers(getLatestProvider(), getLatestService(), aFromMasterConfig);
        }
        Users getUsers(const string& aProviderName, const string& aServiceName)
        {
            bool myDummyFromMasterConfig;
            return getUsers(aProviderName, aServiceName, myDummyFromMasterConfig);
        }

        Users getUsers(const string& aProviderName, const string& aServiceName, bool& aFromMasterConfig)
        {
            const vector<string> myProviders = getProviders(userConfig);
            const vector<string> myServices = getServices(aProviderName, userConfig);

            if (!ta::isElemExist(aProviderName, myProviders))
                TA_THROW_MSG(SettingsError, boost::format("Provider '%s' does not exist") % aProviderName);
            if (!ta::isElemExist(aServiceName, myServices))
                TA_THROW_MSG(SettingsError, boost::format("Service '%s' does not exist for provider '%s'") % aServiceName % aProviderName);

            aFromMasterConfig = SettingsImpl::isServiceValExist<vector<string> >(ServiceUserList, aProviderName, aServiceName, masterConfig);
            auto_ptr<libconfig::Config> myConfigPtr = SettingsImpl::load(userConfig);

            vector<string> myUsers;
            for (unsigned int iProvider=0; iProvider < myProviders.size(); ++iProvider)
            {
                if (myProviders[iProvider] == aProviderName)
                {
                    for (unsigned int iService=0; iService < myServices.size(); ++iService)
                    {
                        const string myUserListPath = SettingsImpl::getServiceSettingPath(iProvider, iService, ServiceUserList);
                        if (myServices[iService] == aServiceName && myConfigPtr->exists(myUserListPath))
                        {
                            const unsigned int myNumUsers = SettingsImpl::getArraySize(*myConfigPtr, myUserListPath);
                            for (unsigned int iUser=0; iUser < myNumUsers; ++iUser)
                            {
                                const string myUserNamePath = SettingsImpl::getUserPath(iProvider, iService, iUser);
                                string myUserName;
                                if (!myConfigPtr->lookupValue(myUserNamePath, myUserName))
                                    TA_THROW_MSG(SettingsError, boost::format("%s setting not found in the user config or it is not a string") % myUserNamePath);
                                myUsers.push_back(myUserName);
                            }
                        }
                    }
                }
            }
            if (SettingsImpl::hasDuplicates(myUsers))
                TA_THROW_MSG(SettingsError, boost::format("Duplicate user found in the user config for provider '%s', service '%s'") % aProviderName % aServiceName);
            return myUsers;
        }

        void addUser(const string& aUserName)
        {
            addUser(getLatestProvider(), getLatestService(), aUserName);
        }
        void addUser(const string& aProviderName, const string& aServiceName, const string& aUserName)
        {
            const vector<string> myProviders = SettingsImpl::getProviders(userConfig);
            vector<string>::const_iterator myProviderIt = std::find(myProviders.begin(), myProviders.end(), aProviderName);
            if (myProviderIt == myProviders.end())
                TA_THROW_MSG(SettingsError, boost::format("Provider '%s' does not exist in the user config") % aProviderName);
            const unsigned int myProviderIdx = myProviderIt - myProviders.begin();

            const vector<string> myServices = SettingsImpl::getServices(aProviderName, userConfig);
            vector<string>::const_iterator myServiceIt = std::find(myServices.begin(), myServices.end(), aServiceName);
            if (myServiceIt == myServices.end())
                TA_THROW_MSG(SettingsError, boost::format("Service '%s' does not exist for provider '%s' in the user config") % aServiceName % aProviderName);
            const unsigned int myServiceIdx = myServiceIt - myServices.begin();

            if (ta::isElemExist(aUserName, getUsers(aProviderName, aServiceName)))
                TA_THROW_MSG(SettingsError, boost::format("User '%s' already exists in the user config at for provider '%s service '%s'") % aUserName % aProviderName % aServiceName);

            auto_ptr<libconfig::Config> myConfigPtr = SettingsImpl::load(userConfig);
            const string myUserListPath = SettingsImpl::getServiceSettingPath(myProviderIdx, myServiceIdx, ServiceUserList);
            libconfig::Setting& myUserListSetting = myConfigPtr->exists(myUserListPath) ? myConfigPtr->lookup(myUserListPath)
                                                    : myConfigPtr->lookup(SettingsImpl::getServicePath(myProviderIdx, myServiceIdx)).add(ServiceUserList, libconfig::Setting::TypeArray);
            myUserListSetting.add(libconfig::Setting::TypeString) = aUserName;
            SettingsImpl::save(*myConfigPtr, userConfig);
        }


        void removeUsers()
        {
            removeUsers(getLatestProvider(), getLatestService());
        }
        void removeUsers(const string& aProviderName, const string& aServiceName)
        {
            const vector<string> myProviders = SettingsImpl::getProviders(userConfig);
            vector<string>::const_iterator myProviderIt = std::find(myProviders.begin(), myProviders.end(), aProviderName);
            if (myProviderIt == myProviders.end())
                TA_THROW_MSG(SettingsError, boost::format("Provider '%s' does not exist in the user config") % aProviderName);
            const unsigned int myProviderIdx = myProviderIt - myProviders.begin();

            const vector<string> myServices = SettingsImpl::getServices(aProviderName, userConfig);
            vector<string>::const_iterator myServiceIt = std::find(myServices.begin(), myServices.end(), aServiceName);
            if (myServiceIt == myServices.end())
                TA_THROW_MSG(SettingsError, boost::format("Service '%s' does not exist for provider '%s' in the user config") % aServiceName % aProviderName);
            const unsigned int myServiceIdx = myServiceIt - myServices.begin();

            auto_ptr<libconfig::Config> myConfigPtr = SettingsImpl::load(userConfig);
            const string myUserListPath = SettingsImpl::getServiceSettingPath(myProviderIdx, myServiceIdx, ServiceUserList);
            if (!myConfigPtr->exists(myUserListPath))
                return;
            libconfig::Setting& myUserListSetting = myConfigPtr->lookup(myUserListPath);
            while (myUserListSetting.getLength())
                myUserListSetting.remove((unsigned int)0);

            SettingsImpl::save(*myConfigPtr, userConfig);
        }


        string getReseptConfigDir()
        {
            return SettingsImpl::getConfigDir(reseptConfig);
        }
        string getReseptConfigPath()
        {
            return SettingsImpl::getConfigFilePath(reseptConfig);
        }
        string getUserConfigDir()
        {
            return SettingsImpl::getConfigDir(userConfig);
        }
        string getUserConfigPath()
        {
            return SettingsImpl::getConfigFilePath(userConfig);
        }
        string getMasterConfigDir()
        {
            return SettingsImpl::getConfigDir(masterConfig);
        }
        string getMasterConfigPath()
        {
            return SettingsImpl::getConfigFilePath(masterConfig);
        }
        bool isCustomized()
        {
            try {
                return !SettingsImpl::getProviders(userConfig).empty();
            } catch (...) {
                return false;
            }
        }

        void setConfigsPath(const string& aReseptConfigPath, const string& aUserConfigPath, const string& aMasterConfigPath)
        {
            SettingsImpl::ReseptSettingsPath = aReseptConfigPath;
            SettingsImpl::ReseptUserSettingsPath = aUserConfigPath;
            SettingsImpl::ReseptMasterSettingsPath = aMasterConfigPath;
        }
        void setUserConfigPath(const string& aUserConfigPath)
        {
            SettingsImpl::ReseptUserSettingsPath = aUserConfigPath;
        }
        void resetConfigsPath()
        {
            SettingsImpl::ReseptSettingsPath.clear();
            SettingsImpl::ReseptUserSettingsPath.clear();
            SettingsImpl::ReseptMasterSettingsPath.clear();
        }

    } // namespace Settings
} // namespace resept
