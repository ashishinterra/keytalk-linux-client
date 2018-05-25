//----------------------------------------------------------------------------
//
//  Description : RESEPT client settings management implementation.
//
//----------------------------------------------------------------------------
#pragma once

#include "Settings.h"
#include "SettingsDefs.h"
#include "Common.h"
#include "resept/common.h"
#include "ta/process.h"
#include "ta/version.h"
#include "ta/netutils.h"
#include "ta/strings.h"
#include "ta/scopedresource.hpp"
#include "ta/utils.h"
#include "ta/common.h"

// Ignore warnings caused by the usage of exception specification in libconfig
#ifdef _MSC_VER
#pragma warning (disable: 4290)
#endif
#include "libconfig.h++"
#ifdef _MSC_VER
#pragma warning (default: 4290)
#endif

#include <algorithm>
#include <memory>
#include <map>
#include <cassert>
#include <iostream>
#include <fcntl.h>
#include "boost/lexical_cast.hpp"
#include "boost/numeric/conversion/cast.hpp"
#include "boost/static_assert.hpp"
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/convenience.hpp"
#include "boost/filesystem/path.hpp"
#include "boost/assign/list_of.hpp"
#include "boost/algorithm/string.hpp"

#ifdef _WIN32
#include <io.h>
#endif


namespace rclient
{
    namespace Settings
    {
        using namespace ta;
        using namespace std;

        //@note sync setting arrays below with the settings above

        static const string ReseptConfigGlobalSettings[]   = {ConfigVersion,
                                                              ReseptBrokerServicePort,
                                                              ReseptInstallDir,
                                                              ReseptInstalledProviders,
                                                              ReseptInstalledUserCAs,
                                                              ReseptInstalledServerCAs,
                                                              ReseptInstalledPrimaryCAs,
                                                              ReseptInstalledRootCAs
                                                             };

        static const string UserConfigGlobalSettings[]     = {ConfigVersion,
                                                              LatestProvider,
                                                              LatestService
                                                             };

        static const string MasterConfigGlobalSettings[]   = {ConfigVersion};

        static const string UserConfigProviderSettings[]   = {ProviderName,
                                                              ProviderContentVersion,
                                                              ProviderReseptSvrAddress,
                                                              ProviderLogLevel,
                                                              ProviderCaList, // UCA, PCA, SCA [, RCA]
                                                              ProviderLastUserMsgUtc
                                                             };

        static const string MasterConfigProviderSettings[] = {ProviderName,
                                                              ProviderContentVersion,
                                                              ProviderReseptSvrAddress,
                                                              DefProviderReseptSvrAddress, // used only for recovery user user config. MUST STAY UNDER ProviderReseptSvrAddress!
                                                              ProviderLogLevel,
                                                              ProviderCaList, // UCA, PCA, SCA [, RCA]
                                                              DefProviderCaList  // used only for recovery user user config. MUST STAY UNDER ProviderCaList!
                                                             };

        static const string UserConfigServiceSettings[]   =  {ServiceName,
                                                              ServiceUri,
                                                              ServiceDisplayName,
                                                              ServiceCleanupUserCert,
                                                              ServiceCertValidPercent,
                                                              ServiceCertFormat,
                                                              ServiceCertChain,
                                                              ServiceUserList
                                                             };

        static const string MasterConfigServiceSettings[] = { ServiceName,
                                                              DefServiceUri, // used for recovery user user config, not for enforcement
                                                              ServiceDisplayName,
                                                              ServiceCleanupUserCert,
                                                              ServiceCertValidPercent,
                                                              ServiceCertFormat,
                                                              ServiceCertChain,
                                                              ServiceUserList
                                                            };


        namespace SettingsImpl
        {
            namespace
            {
                enum ConfigType
                {
                    reseptConfig, masterConfig, userConfig
                };


                static const ta::version::Version SupportedConfigVersion(1,0,0);

                // Used for test purposes only!
                static string ReseptSettingsPath = "";
                static string ReseptMasterSettingsPath = "";
                static string ReseptUserSettingsPath = "";

                const vector<string> userConfigProviderSettings()
                {
                    return vector<string>(&UserConfigProviderSettings[0],
                                          &UserConfigProviderSettings[0]+sizeof(UserConfigProviderSettings)/sizeof(UserConfigProviderSettings[0]));
                }
                const vector<string> masterConfigProviderSettings()
                {
                    return vector<string>(&MasterConfigProviderSettings[0],
                                          &MasterConfigProviderSettings[0]+sizeof(MasterConfigProviderSettings)/sizeof(MasterConfigProviderSettings[0]));
                }
                const vector<string> userConfigServiceSettings()
                {
                    return vector<string>(&UserConfigServiceSettings[0],
                                          &UserConfigServiceSettings[0]+sizeof(UserConfigServiceSettings)/sizeof(UserConfigServiceSettings[0]));
                }
                const vector<string> masterConfigServiceSettings()
                {
                    return vector<string>(&MasterConfigServiceSettings[0],
                                          &MasterConfigServiceSettings[0]+sizeof(MasterConfigServiceSettings)/sizeof(MasterConfigServiceSettings[0]));
                }

                // Fwd declarations
                auto_ptr<libconfig::Config> updateUserConfigFromMaster();

                string getProviderPath(unsigned int aProviderIdx)
                {
                    return str(boost::format("%s.[%u]") % ProviderList % aProviderIdx);
                }
                string getProviderSettingPath(unsigned int aProviderIdx, const string& aSettingName)
                {
                    return str(boost::format("%s.%s") % getProviderPath(aProviderIdx) % aSettingName);
                }

                string getServicePath(const string& aProviderPath, unsigned int aServiceIdx)
                {
                    return str(boost::format("%s.%s.[%u]") % aProviderPath % ServiceList % aServiceIdx);
                }
                string getServicePath(unsigned int aProviderIdx, unsigned int aServiceIdx)
                {
                    return str(boost::format("%s.[%u]") % getProviderSettingPath(aProviderIdx, ServiceList) % aServiceIdx);
                }
                string getServiceSettingPath(const string& aProviderPath, unsigned int aServiceIdx, const string& aSettingName)
                {
                    return str(boost::format("%s.%s.[%u].%s") % aProviderPath % ServiceList % aServiceIdx % aSettingName);
                }
                string getServiceSettingPath(unsigned int aProviderIdx, unsigned int aServiceIdx, const string& aSettingName)
                {
                    return str(boost::format("%s.%s") % getServicePath(aProviderIdx, aServiceIdx) % aSettingName);
                }

                string getUserPath(unsigned int aProviderIdx, unsigned int aServiceIdx, unsigned int aUserIdx)
                {
                    return str(boost::format("%s.[%u]") % getServiceSettingPath(aProviderIdx, aServiceIdx, ServiceUserList) % aUserIdx);
                }

                unsigned int getListSize(const libconfig::Config& aConfig, const string& aPath)
                {
                    if (!aConfig.exists(aPath))
                        TA_THROW_MSG(SettingsError, boost::format("%s setting does not exist") % aPath);
                    libconfig::Setting& myListSetting = aConfig.lookup(aPath);
                    if (!myListSetting.isList())
                        TA_THROW_MSG(SettingsError, boost::format("%s setting is not a list") % aPath);
                    int myListSize = myListSetting.getLength();
                    if (myListSize < 0)
                        TA_THROW_MSG(SettingsError, boost::format("Negative number of elements in the %s list?!") % aPath);
                    return static_cast<unsigned int>(myListSize);
                }
                unsigned int getArraySize(const libconfig::Config& aConfig, const string& aPath)
                {
                    if (!aConfig.exists(aPath))
                        TA_THROW_MSG(SettingsError, boost::format("%s setting does not exist") % aPath);
                    libconfig::Setting& myArraySetting = aConfig.lookup(aPath);
                    if (!myArraySetting.isArray())
                        TA_THROW_MSG(SettingsError, boost::format("%s setting is not an array") % aPath );
                    int myArraySize = myArraySetting.getLength();
                    if (myArraySize < 0)
                        TA_THROW_MSG(SettingsError, boost::format("Negative number of elements in the %s array?!") % aPath);
                    return static_cast<unsigned int>(myArraySize);
                }

                template <class IterableCollection>
                bool hasDuplicates(const IterableCollection& aCollection)
                {
                    IterableCollection myCollection = aCollection;
                    std::sort(myCollection.begin(), myCollection.end());
                    if (std::unique(myCollection.begin(), myCollection.end()) != myCollection.end())
                        return true;
                    return false;
                }

                string getReseptConfigDir()
                {
                    if (!ReseptSettingsPath.empty())
                    {
                        try
                        {
                            return boost::filesystem::path(ReseptSettingsPath).parent_path().string();
                        }
                        catch (std::exception& e)
                        {
                            TA_THROW_MSG(SettingsOpenError, boost::format("Failed to extract directory path for '%s'. %s") % ReseptSettingsPath % e.what());
                        }
                    }

#ifdef _WIN32
                    try
                    {
                        return Process::getCommonAppDataDir() + "\\" + resept::CompanyName;
                    }
                    catch (std::exception& e)
                    {
                        TA_THROW_MSG(SettingsOpenError, e.what());
                    }
#else
                    return "/etc/" + boost::to_lower_copy(resept::CompanyName);
#endif // _WIN32
                }
                string getReseptConfigFilePath()
                {
                    if (!ReseptSettingsPath.empty())
                        return ReseptSettingsPath;

#ifdef _WIN32
                    try
                    {
                        return str(boost::format("%s\\%s\\%s") % Process::getCommonAppDataDir() % resept::CompanyName % ReseptConfigFileName);
                    }
                    catch (std::exception& e)
                    {
                        TA_THROW_MSG(SettingsOpenError, e.what());
                    }
#else
                    return "/etc/" + boost::to_lower_copy(resept::CompanyName) + "/" + ReseptConfigFileName;
#endif
                }

                string getUserConfigDir()
                {
                    if (!ReseptUserSettingsPath.empty())
                    {
                        try
                        {
                            return boost::filesystem::path(ReseptUserSettingsPath).parent_path().string();
                        }
                        catch (std::exception& e)
                        {
                            TA_THROW_MSG(SettingsOpenError, boost::format("Failed to extract directory path for '%s'. %s") % ReseptUserSettingsPath % e.what());
                        }
                    }

                    try
                    {
#ifdef _WIN32
                        return str(boost::format("%s\\%s") % ta::Process::getUserAppDataDir() % resept::CompanyName);
#else
                        return str(boost::format("%s/.%s") % ta::Process::getUserAppDataDir() % boost::to_lower_copy(resept::CompanyName));
#endif
                    }
                    catch (std::exception& e)
                    {
                        TA_THROW_MSG(SettingsOpenError, e.what());
                    }
                }

                string getUserConfigFilePath()
                {
                    if (!ReseptUserSettingsPath.empty())
                        return ReseptUserSettingsPath;

                    try
                    {
#ifdef _WIN32
                        return str(boost::format("%s\\%s\\%s") % Process::getUserAppDataDir() % resept::CompanyName % UserConfigFileName);
#else
                        return str(boost::format("%s/.%s/%s") % Process::getUserAppDataDir() % boost::to_lower_copy(resept::CompanyName) % UserConfigFileName);
#endif
                    }
                    catch (std::exception& e)
                    {
                        TA_THROW_MSG(SettingsOpenError, e.what());
                    }
                }


                string getMasterConfigDir()
                {
                    if (!ReseptMasterSettingsPath.empty())
                    {
                        try
                        {
                            return boost::filesystem::path(ReseptMasterSettingsPath).parent_path().string();
                        }
                        catch (std::exception& e)
                        {
                            TA_THROW_MSG(SettingsOpenError, boost::format("Failed to extract directory path for '%s'. %s") % ReseptMasterSettingsPath % e.what());
                        }
                    }

#ifdef _WIN32
                    try
                    {
                        return Process::getCommonAppDataDir() + "\\" + resept::CompanyName;
                    }
                    catch (std::exception& e)
                    {
                        TA_THROW_MSG(SettingsOpenError, e.what());
                    }
#else
                    return "/etc/" + boost::to_lower_copy(resept::CompanyName);
#endif // _WIN32
                }

                string getMasterConfigFilePath()
                {
                    if (!ReseptMasterSettingsPath.empty())
                        return ReseptMasterSettingsPath;

#ifdef _WIN32
                    try
                    {
                        return str(boost::format("%s\\%s\\%s") % Process::getCommonAppDataDir() % resept::CompanyName % MasterConfigFileName);
                    }
                    catch (std::exception& e)
                    {
                        TA_THROW_MSG(SettingsOpenError, e.what());
                    }
#else
                    return "/etc/" + boost::to_lower_copy(resept::CompanyName) + "/" + MasterConfigFileName;
#endif // _WIN32
                }

                bool isMasterConfigExist()
                {
                    return ta::isFileExist(getMasterConfigFilePath());
                }


                // Exceptions : throw SettingsError
                ta::version::Version getConfigVersion(const libconfig::Config& aConfig)
                {
                    try
                    {
                        string myVersionStr;
                        if (!aConfig.lookupValue(ConfigVersion, myVersionStr))
                            TA_THROW_MSG(SettingsError, boost::format("%s does not exist") % ConfigVersion);

                        return ta::version::parse(myVersionStr);
                    }
                    catch (SettingsError&)
                    {
                        throw;
                    }
                    catch (std::exception& e)
                    {
                        TA_THROW_MSG(SettingsError, e.what());
                    }
                }

                void skipUtf8Sig(FILE* anFd)
                {
                    if (!anFd)
                        return;
                    static const unsigned char Utf8Sig[] = {0xEF, 0xBB, 0xBF};
                    unsigned char buf[sizeof(Utf8Sig)]  = {};
                    bool myIsSigFound = true;
                    if (fread(buf, 1, sizeof(buf), anFd) == sizeof(buf))
                    {
                        for (size_t i = 0; i < sizeof(buf); ++i)
                        {
                            if (buf[i] != Utf8Sig[i])
                            {
                                myIsSigFound = false;
                                break;
                            }
                        }
                    }
                    if (!myIsSigFound)
                        fseek(anFd, 0, 0);
                }



                // throws SettingsError if the input argument cannot be mapped to any of libconfig scalar types or array of strings
                template <class T>
                libconfig::Setting::Type getLibconfigType(const T& val, string& aFriendlyTypeName)
                {
                    (void)val; // "use" val to suppress warning: Visual studio compiler does not see typeid(val) as usage
                    const type_info& myValType = typeid(val);
                    if (myValType==typeid(int) || myValType==typeid(long) || myValType==typeid(unsigned int) || myValType==typeid(unsigned long))
                    {
                        aFriendlyTypeName = "int";
                        return libconfig::Setting::TypeInt;
                    }
                    if (myValType==typeid(long long) || myValType==typeid(unsigned long long))
                    {
                        aFriendlyTypeName = "int64";
                        return libconfig::Setting::TypeInt64;
                    }
                    if (myValType==typeid(bool))
                    {
                        aFriendlyTypeName = "boolean";
                        return libconfig::Setting::TypeBoolean;
                    }
                    if (myValType==typeid(float) || myValType==typeid(double))
                    {
                        aFriendlyTypeName = "float";
                        return libconfig::Setting::TypeFloat;
                    }
                    if (myValType==typeid(string) || myValType==typeid(char*) || myValType==typeid(const char*))
                    {
                        aFriendlyTypeName = "string";
                        return libconfig::Setting::TypeString;
                    }
                    if (myValType==typeid(vector<string>))
                    {
                        aFriendlyTypeName = "array";
                        return libconfig::Setting::TypeArray;
                    }
                    TA_THROW_MSG(SettingsError, boost::format("Cannot map type %s to libconfig scalar type or array") % myValType.name());
                }

                template <class T>
                libconfig::Setting::Type getLibconfigType(const T& val)
                {
                    string aDummyFriendlyTypeName;
                    return getLibconfigType(val, aDummyFriendlyTypeName);
                }


                template <class T>
                T getArrayValue(libconfig::Config& aConfig, const string& aPath)
                {
                    if (!aConfig.exists(aPath))
                    {
                        TA_THROW_MSG(SettingsError, "Setting "+ aPath +" does not exist");
                    }

                    libconfig::Setting& mySetting = aConfig.lookup(aPath);
                    if (!mySetting.isArray())
                    {
                        TA_THROW_MSG(SettingsError, boost::format("Setting %s is of type %d, array type expected") % aPath % mySetting.getType());
                    }

                    T myRetVal;
                    const unsigned int myNumElems = getArraySize(aConfig, aPath);
                    for (unsigned int iElem=0; iElem < myNumElems; ++iElem)
                    {
                        typename T::value_type myElemVal;
                        const string myPath = str(boost::format("%s.[%u]") % aPath % iElem);
                        if (!aConfig.lookupValue(myPath, myElemVal))
                        {
                            TA_THROW_MSG(SettingsError, boost::format("%s setting not found or it is of a correct type") % myPath);
                        }
                        myRetVal.push_back(myElemVal);
                    }
                    return myRetVal;
                }


                enum SettingNotExistPolicy
                {
                    settingCreateIfNotExist, settingFailIfNotExist
                };

                template <class T>
                void assignArrayValue(libconfig::Config& aConfig, const string& aParentPath, const string& aKey, const T& aValue, SettingNotExistPolicy aSettingNotExistPolicy = settingFailIfNotExist)
                {
                    const string mySettingPath = aParentPath.empty() ? aKey : aParentPath + "." + aKey;
                    if (!aConfig.exists(mySettingPath))
                    {
                        if (aSettingNotExistPolicy == settingFailIfNotExist)
                        {
                            TA_THROW_MSG(SettingsError, boost::format("Cannot assign setting %s because it does not exist") % mySettingPath);
                        }
                        else
                        {
                            aConfig.lookup(aParentPath).add(aKey, libconfig::Setting::TypeArray);
                        }
                    }

                    libconfig::Setting& mySetting = aConfig.lookup(mySettingPath);
                    if (!mySetting.isArray())
                    {
                        TA_THROW_MSG(SettingsError, boost::format("Cannot assign setting %s of type %d, array type expected") % mySettingPath % mySetting.getType());
                    }

                    while (mySetting.getLength())
                    {
                        mySetting.remove((unsigned int)0);
                    }

                    foreach (typename T::value_type elem, aValue)
                    {
                        mySetting.add(getLibconfigType(elem)) = elem;
                    }
                }

                // Assign scalar or array setting
                void assignSetting(libconfig::Setting& aTargetSetting, const libconfig::Setting& aSrcSetting)
                {
                    libconfig::Setting::Type mySrcSettingType =  aSrcSetting.getType();
                    if (aTargetSetting.getType() != mySrcSettingType)
                        TA_THROW_MSG(SettingsError, boost::format("Cannot assign setting %s of type %d to the setting %s of type %d") %
                                     aTargetSetting.getPath() % aTargetSetting.getType() % aSrcSetting.getPath() % mySrcSettingType);
                    switch (mySrcSettingType)
                    {
                    case libconfig::Setting::TypeInt:
                        aTargetSetting = (int)aSrcSetting;
                        break;
                    case libconfig::Setting::TypeInt64:
                        aTargetSetting = (long long)aSrcSetting;
                        break;
                    case libconfig::Setting::TypeBoolean:
                        aTargetSetting = (bool)aSrcSetting;
                        break;
                    case libconfig::Setting::TypeFloat:
                        aTargetSetting = (double)aSrcSetting;
                        break;
                    case libconfig::Setting::TypeString:
                        aTargetSetting = (const char*)aSrcSetting;
                        break;
                    case libconfig::Setting::TypeArray:
                    {
                        while (aTargetSetting.getLength())
                        {
                            aTargetSetting.remove((unsigned int)0);
                        }

                        for (int iElem=0; iElem < aSrcSetting.getLength(); ++iElem)
                        {
                            libconfig::Setting& mySrcElemSetting = aSrcSetting[iElem];
                            assignSetting(aTargetSetting.add(mySrcElemSetting.getType()), mySrcElemSetting);
                        }
                        break;
                    }
                    default:
                        TA_THROW_MSG(SettingsError, boost::format("Setting %s has unexpected type %d") % aSrcSetting.getPath() % mySrcSettingType);
                    }
                }


                string getConfigDir(ConfigType aConfigType)
                {
                    switch (aConfigType)
                    {
                    case reseptConfig:
                        return getReseptConfigDir();
                    case masterConfig:
                        return getMasterConfigDir();
                    case userConfig:
                        return getUserConfigDir();
                    default:
                        TA_THROW_MSG(SettingsError, boost::format("Unsupported config type %d") % aConfigType);
                    }
                }
                string getConfigFilePath(ConfigType aConfigType)
                {
                    switch (aConfigType)
                    {
                    case reseptConfig:
                        return getReseptConfigFilePath();
                    case masterConfig:
                        return getMasterConfigFilePath();
                    case userConfig:
                        return getUserConfigFilePath();
                    default:
                        TA_THROW_MSG(SettingsError, boost::format("Unsupported config type %d") % aConfigType);
                    }
                }

                // libconfig can write only to FILE* but we need memory buffer, let's help it bit
                string serializeLibConfig(const libconfig::Config& aConfig)
                {
                    string myRetVal;
                    int myPipe[2] = {0};
#ifdef _WIN32
                    const unsigned int MaxConfigSize = 16*1024; // should suffice
                    if (_pipe(myPipe, MaxConfigSize, O_BINARY|O_NOINHERIT))
                        TA_THROW_MSG(std::runtime_error, boost::format("Failed to open pipe. errno %d") % errno);
#define fdopen _fdopen
#else
                    if (pipe(myPipe) != 0)
                        TA_THROW_MSG(std::runtime_error, boost::format("Failed to open pipe. %s") % strerror(errno));
#endif
                    //@note fclose will also close the associated pipe end

                    {
                        ta::ScopedResource<FILE*> stream(fdopen (myPipe[1], "w"), fclose);
                        aConfig.write(stream);
                    }

                    {
                        ta::ScopedResource<FILE*> stream(fdopen (myPipe[0], "r"), fclose);
                        int c;
                        while ((c = fgetc (stream)) != EOF)
                            myRetVal += (char)c;
                    }
                    return myRetVal;
                }

                // return true if the config aConfig equals to the config at aConfigPath or error occurred (e.g. aConfigPath does not exist), false otherwise
                //@nothrow
                bool equalConfigs(const libconfig::Config& aConfig, const string& aConfigPath)
                {
                    try
                    {
                        string myLhs = serializeLibConfig(aConfig);
                        string myRhs = ta::readData(aConfigPath);
                        // normalize
                        boost::replace_all(myLhs, "\r\n", "\n");
                        boost::replace_all(myRhs, "\r\n", "\n");
                        return myLhs == myRhs;
                    }
                    catch(...)
                    {
                        return false;
                    }
                }

                //
                // Abstract: Save configuration
                //
                // @throws SettingsOpenError, SettingsSaveError
                //
                void save(libconfig::Config& aConfig, const string& myOutFilePath, bool aCreateDirIfNotExist = false)
                {
                    if (aCreateDirIfNotExist)
                    {
                        string myDirName;
                        try {
                            myDirName = boost::filesystem::path(myOutFilePath).parent_path().string();
                        } catch (std::exception& e) {
                            TA_THROW_MSG(SettingsSaveError, boost::format("Failed to extract directory path for '%s'. %s") % myOutFilePath % e.what());
                        }
                        if (!myDirName.empty() && !ta::isDirExist(myDirName))
                        {
                            try {
                                boost::filesystem::create_directories(myDirName);
                            } catch (std::exception& e) {
                                TA_THROW_MSG(SettingsSaveError, boost::format("Failed to create directory '%s'. %s") % myDirName % e.what());
                            }
                        }
                    }

                    // Performance optimization: do not write if nothing changed
                    if (ta::isFileExist(myOutFilePath) && equalConfigs(aConfig, myOutFilePath))
                        return;

                    try {
                        aConfig.writeFile(myOutFilePath.c_str());
                    } catch (libconfig::FileIOException&) {
                        TA_THROW_MSG(SettingsSaveError, "Failed to save to " + myOutFilePath);
                    }
                }
                void save(libconfig::Config& aConfig, ConfigType aConfigType, bool aCreateDirIfNotExist = false)
                {
                    const string myConfigFilePath = getConfigFilePath(aConfigType);
                    save(aConfig, myConfigFilePath, aCreateDirIfNotExist);
                }


                //
                // Open and load the settings file from the specified location
                // @throw SettingsOpenError, SettingsError
                //
                auto_ptr<libconfig::Config> load(const string& aConfigFilePath)
                {
                    ta::ScopedResource<FILE*> myFd(fopen(aConfigFilePath.c_str(), "rt"), fclose);
                    if (!myFd)
                    {
                        TA_THROW_MSG(SettingsOpenError, boost::format("Failed to open config file at %s. %s") % aConfigFilePath % strerror(errno));
                    }

                    //libconfig does not understand UTF-8 signatures, lets help it with it
                    skipUtf8Sig(myFd);

                    auto_ptr<libconfig::Config> myConfigPtr;
                    try
                    {
                        myConfigPtr.reset(new libconfig::Config());
                        myConfigPtr->read(myFd);
                    }
                    catch (libconfig::ParseException& e)
                    {
                        string message = str(boost::format("Failed to parse %s. line %d : %s") % aConfigFilePath % e.getLine() % e.getError());
                        throw SettingsOpenParseError(message, e.getLine());
                    }
                    catch (std::exception& e)
                    {
                        TA_THROW_MSG(SettingsOpenError, boost::format("Failed to open %s. %s.") % aConfigFilePath % e.what());
                    }

                    const ta::version::Version myVersion = getConfigVersion(*myConfigPtr);
                    if (myVersion != SupportedConfigVersion)
                    {
                        TA_THROW_MSG(SettingsError, boost::format("Failed to parse %s because the config version %s is not supported by the parser. Supported version is %s") %
                                     aConfigFilePath % ta::version::toStr(myVersion) % ta::version::toStr(SupportedConfigVersion));
                    }
                    return myConfigPtr;
                }

                // If the given config is a user config and the master config exists, the following occurs prior to loading of the user config:
                // 1. If the aConfigFilePath does not exist or cannot be parsed, it will be recovered from the master config
                // 2. All providers/services of the master config which do not exist in user config will be added to the user config
                // 3. All settings which exist in the master config will overwrite the ones in the user config except for 'defaultUri' which will be copied to 'uri' of the one does not exist in the user config.
                auto_ptr<libconfig::Config> load(ConfigType aConfigType)
                {
                    if (aConfigType == userConfig && isMasterConfigExist())
                        return updateUserConfigFromMaster();
                    return load(getConfigFilePath(aConfigType));
                }



                // @throw SettingsError or SettingsOpenError
                vector<string> getProviders(ConfigType aConfigType)
                {
                    if (aConfigType != userConfig && aConfigType != masterConfig)
                        TA_THROW_MSG(SettingsError, boost::format("Unexpected confguration type %d") % aConfigType);

                    auto_ptr<libconfig::Config> myConfigPtr = load(aConfigType);
                    const unsigned int myNumProviders = getListSize(*myConfigPtr, ProviderList);

                    vector<string> myProviders;
                    for (unsigned int i=0; i < myNumProviders; ++i)
                    {
                        const string myNamePath = getProviderSettingPath(i, ProviderName);
                        string myProviderName;
                        if (!myConfigPtr->lookupValue(myNamePath, myProviderName))
                            TA_THROW_MSG(SettingsError, boost::format("%s setting not found in the %s config or it is not a string") % myNamePath % (aConfigType==userConfig?"user":"master"));
                        myProviders.push_back(myProviderName);
                    }
                    return myProviders;
                }
                //@return map<provider-name, provider-path>
                map<string,string> getProviders(const libconfig::Config& aConfig)
                {
                    const unsigned int myNumProviders = getListSize(aConfig, ProviderList);

                    map<string,string> myProviders;
                    for (unsigned int i=0; i < myNumProviders; ++i)
                    {
                        const string myNamePath = getProviderSettingPath(i, ProviderName);
                        string myName;
                        if (!aConfig.lookupValue(myNamePath, myName))
                            TA_THROW_MSG(SettingsError, boost::format("%s setting not found or it is not a string") % myName);
                        myProviders[myName] = getProviderPath(i);
                    }
                    if (myProviders.size() != myNumProviders)
                        TA_THROW_MSG(SettingsError, "Duplicate provider found");
                    return myProviders;
                }

                // @throw SettingsError or SettingsOpenError
                template <class T>
                string getProviderScalarVal(const string& aProviderKey, const string& aProviderName, ConfigType aConfigType, T& aVal)
                {
                    if (aConfigType != userConfig && aConfigType != masterConfig)
                        TA_THROW_MSG(SettingsError, boost::format("Unexpected confguration type %d") % aConfigType);

                    auto_ptr<libconfig::Config> myConfigPtr = load(aConfigType);
                    const unsigned int myNumProviders = getListSize(*myConfigPtr, ProviderList);
                    if (myNumProviders == 0)
                        TA_THROW_MSG(SettingsError, boost::format("No providers exist in the %s config") % (aConfigType==userConfig?"user":"master"));

                    for (unsigned int i=0; i < myNumProviders; ++i)
                    {
                        const string myNamePath = getProviderSettingPath(i, ProviderName);
                        string myProviderName;
                        if (!myConfigPtr->lookupValue(myNamePath, myProviderName))
                            TA_THROW_MSG(SettingsError, boost::format("%s setting not found in the %s config or it is not a string") % myNamePath % (aConfigType==userConfig?"user":"master"));
                        if (myProviderName == aProviderName)
                        {
                            const string myKeyPath = getProviderSettingPath(i, aProviderKey);
                            if (!myConfigPtr->lookupValue(myKeyPath, aVal))
                                TA_THROW_MSG(SettingsError, boost::format("Setting %s in the %s config does not exist or has unexpected type") % myKeyPath % (aConfigType==userConfig?"user":"master"));
                            return myKeyPath;
                        }
                    }
                    TA_THROW_MSG(SettingsError, boost::format("%s setting not found for provider %s in the %s config")  % aProviderKey % aProviderName % (aConfigType==userConfig?"user":"master"));
                }
                template <class T>
                string getProviderArrayVal(const string& aProviderKey, const string& aProviderName, ConfigType aConfigType, T& aVal)
                {
                    if (aConfigType != userConfig && aConfigType != masterConfig)
                        TA_THROW_MSG(SettingsError, boost::format("Unexpected confguration type %d") % aConfigType);

                    auto_ptr<libconfig::Config> myConfigPtr = load(aConfigType);
                    const unsigned int myNumProviders = getListSize(*myConfigPtr, ProviderList);
                    if (myNumProviders == 0)
                        TA_THROW_MSG(SettingsError, boost::format("No providers exist in the %s config") % (aConfigType==userConfig?"user":"master"));

                    for (unsigned int i=0; i < myNumProviders; ++i)
                    {
                        const string myNamePath = getProviderSettingPath(i, ProviderName);
                        string myProviderName;
                        if (!myConfigPtr->lookupValue(myNamePath, myProviderName))
                            TA_THROW_MSG(SettingsError, boost::format("%s setting not found in the %s config or it is not a string") % myNamePath % (aConfigType==userConfig?"user":"master"));
                        if (myProviderName == aProviderName)
                        {
                            const string myKeyPath = getProviderSettingPath(i, aProviderKey);
                            aVal = getArrayValue<T>(*myConfigPtr, myKeyPath);
                            return myKeyPath;
                        }
                    }
                    TA_THROW_MSG(SettingsError, boost::format("%s setting not found for provider %s in the %s config")  % aProviderKey % aProviderName % (aConfigType==userConfig?"user":"master"));
                }

                // @throw SettingsError or SettingsOpenError
                template <class T>
                bool isProviderValExist(const string& aProviderKey, const string& aProviderName, ConfigType aConfigType)
                {
                    if (aConfigType != userConfig && aConfigType != masterConfig)
                        TA_THROW_MSG(SettingsError, boost::format("Unexpected confguration type %d") % aConfigType);

                    // master config is optional
                    if (aConfigType == masterConfig && !isMasterConfigExist())
                        return false;

                    auto_ptr<libconfig::Config> myConfigPtr = load(aConfigType);
                    const unsigned int myNumProviders = getListSize(*myConfigPtr, ProviderList);

                    for (unsigned int i=0; i < myNumProviders; ++i)
                    {
                        const string myNamePath = getProviderSettingPath(i, ProviderName);
                        string myProviderName;
                        if (!myConfigPtr->lookupValue(myNamePath, myProviderName))
                            TA_THROW_MSG(SettingsError, boost::format("%s setting not found in the %s config or it is not a string") % myNamePath % (aConfigType==userConfig?"user":"master"));
                        if (myProviderName == aProviderName)
                        {
                            const string myKeyPath = getProviderSettingPath(i, aProviderKey);
                            if (!myConfigPtr->exists(myKeyPath))
                                return false;
                            string myFriendlyTypeName;
                            if (myConfigPtr->lookup(myKeyPath).getType() != getLibconfigType(T(), myFriendlyTypeName))
                            {
                                TA_THROW_MSG(SettingsError, boost::format("%s setting in the %s config has invalid type. %s expected") %
                                             myKeyPath % (aConfigType==userConfig?"user":"master") % myFriendlyTypeName);
                            }
                            return true;
                        }
                    }
                    return false;
                }

                //
                // Abstract   : set or add provider setting in the specified config
                //
                // Exceptions : throw SettingsError, SettingsOpenError, SettingsSaveError
                //
                template <class T>
                void setProviderScalarVal(const string& aProviderKey, const string& aProviderName, ConfigType aConfigType, const T& aVal)
                {
                    if (aConfigType != userConfig && aConfigType != masterConfig)
                        TA_THROW_MSG(SettingsError, boost::format("Unexpected confguration type %d") % aConfigType);

                    auto_ptr<libconfig::Config> myConfigPtr = load(aConfigType);
                    const unsigned int myNumProviders = getListSize(*myConfigPtr, ProviderList);
                    if (myNumProviders == 0)
                        TA_THROW_MSG(SettingsError, boost::format("No providers exist in the %s config")  % (aConfigType==userConfig?"user":"master"));

                    string myExpectedTypeName;
                    libconfig::Setting::Type myExpectedType = getLibconfigType(aVal, myExpectedTypeName);

                    for (unsigned int i=0; i < myNumProviders; ++i)
                    {
                        const string myNamePath = getProviderSettingPath(i, ProviderName);
                        string myProviderName;
                        if (!myConfigPtr->lookupValue(myNamePath, myProviderName))
                            TA_THROW_MSG(SettingsError, boost::format("%s setting not found in the %s config or it is not a string") % myNamePath % (aConfigType==userConfig?"user":"master"));
                        if (myProviderName == aProviderName)
                        {
                            libconfig::Setting& myProviderSetting = myConfigPtr->lookup(getProviderPath(i));
                            if (!myProviderSetting.exists(aProviderKey))
                                myProviderSetting.add(aProviderKey, myExpectedType);
                            libconfig::Setting& myKeySetting = myConfigPtr->lookup(getProviderSettingPath(i, aProviderKey));
                            if (myKeySetting.getType() != myExpectedType)
                                TA_THROW_MSG(SettingsError, boost::format("%s setting in the %s config is not of %s type") % myKeySetting.getPath() % (aConfigType==userConfig?"user":"master") % myExpectedTypeName);
                            myKeySetting = aVal;
                            save(*myConfigPtr, aConfigType);
                            return;
                        }
                    }
                    TA_THROW_MSG(SettingsError, boost::format("%s setting not found for provider %s in the %s config")  % aProviderKey % aProviderName % (aConfigType==userConfig?"user":"master"));
                }



                vector<string> getServices(const string& aProviderName, ConfigType aConfigType)
                {
                    if (aConfigType != userConfig && aConfigType != masterConfig)
                        TA_THROW_MSG(SettingsError, boost::format("Unexpected confguration type %d") % aConfigType);

                    auto_ptr<libconfig::Config> myConfigPtr = load(aConfigType);

                    const vector<string> myProviders = getProviders(aConfigType);
                    vector<string> myServices;
                    for (unsigned int iProvider=0; iProvider < myProviders.size(); ++iProvider)
                    {
                        if (myProviders[iProvider] == aProviderName)
                        {
                            const unsigned int myNumServices = getListSize(*myConfigPtr, getProviderSettingPath(iProvider, ServiceList));
                            for (unsigned int iService=0; iService < myNumServices; ++iService)
                            {
                                const string myServiceNamePath = getServiceSettingPath(iProvider, iService, ServiceName);
                                string myServiceName;
                                if (!myConfigPtr->lookupValue(myServiceNamePath, myServiceName))
                                    TA_THROW_MSG(SettingsError, boost::format("%s setting not found in the %s config or it is not a string") % myServiceNamePath % (aConfigType==userConfig?"user":"master"));
                                myServices.push_back(myServiceName);
                            }
                        }
                    }
                    return myServices;
                }
                //@return map<service-name, service-path>
                map<string,string> getServices(const libconfig::Config& aConfig, const string& aProviderPath)
                {
                    const unsigned int myNumServices = getListSize(aConfig, aProviderPath+"."+ServiceList);

                    map<string,string> myServices;
                    for (unsigned int i=0; i < myNumServices; ++i)
                    {
                        const string myNamePath = getServiceSettingPath(aProviderPath, i, ProviderName);
                        string myName;
                        if (!aConfig.lookupValue(myNamePath, myName))
                            TA_THROW_MSG(SettingsError, boost::format("%s setting not found or it is not a string") % myName);
                        myServices[myName] = getServicePath(aProviderPath, i);
                    }
                    if (myServices.size() != myNumServices)
                        TA_THROW_MSG(SettingsError, "Duplicate service found in " + aProviderPath);
                    return myServices;
                }

                //
                // Abstract   : retrieve the value of the service setting
                //
                // Return     : path to the setting
                //
                // Exceptions : throw SettingsError, SettingsOpenError
                //
                template <class T>
                string getServiceScalarVal(const string& aServiceKey, const string& aProviderName, const string& aServiceName, ConfigType aConfigType, T& aVal)
                {
                    if (aConfigType != userConfig && aConfigType != masterConfig)
                        TA_THROW_MSG(SettingsError, boost::format("Unexpected confguration type %d") % aConfigType);

                    auto_ptr<libconfig::Config> myConfigPtr = load(aConfigType);

                    const unsigned int myNumProviders = getListSize(*myConfigPtr, ProviderList);
                    if (myNumProviders == 0)
                        TA_THROW_MSG(SettingsError, boost::format("No providers exist in the %s config")  % (aConfigType==userConfig?"user":"master"));

                    for (unsigned int iProvider=0; iProvider < myNumProviders; ++iProvider)
                    {
                        const string myProviderNamePath = getProviderSettingPath(iProvider, ProviderName);
                        string myProviderName;
                        if (!myConfigPtr->lookupValue(myProviderNamePath, myProviderName))
                            TA_THROW_MSG(SettingsError, boost::format("%s setting not found in the %s config or it is not a string") % myProviderNamePath % (aConfigType==userConfig?"user":"master"));

                        if (myProviderName == aProviderName)
                        {
                            const unsigned int myNumServices = getListSize(*myConfigPtr, getProviderSettingPath(iProvider, ServiceList));
                            if (myNumServices == 0)
                                TA_THROW_MSG(SettingsError, boost::format("No services exist for provider %s in the %s config")  % aProviderName % (aConfigType==userConfig?"user":"master"));
                            for (unsigned int iService=0; iService < myNumServices; ++iService)
                            {
                                const string myServiceNamePath = getServiceSettingPath(iProvider, iService, ServiceName);
                                string myServiceName;
                                if (!myConfigPtr->lookupValue(myServiceNamePath, myServiceName))
                                    TA_THROW_MSG(SettingsError, boost::format("%s setting not found in the %s config or it is not a string") % myServiceNamePath % (aConfigType==userConfig?"user":"master"));
                                if (myServiceName == aServiceName)
                                {
                                    const string myKeyPath = getServiceSettingPath(iProvider, iService, aServiceKey);
                                    if (!myConfigPtr->lookupValue(myKeyPath, aVal))
                                        TA_THROW_MSG(SettingsError, boost::format("Setting %s in the %s config does not exist or is not of a correct type") % myKeyPath % (aConfigType==userConfig?"user":"master"));
                                    return myKeyPath;
                                }
                            }
                        }
                    }
                    TA_THROW_MSG(SettingsError, boost::format("%s setting not found for provider %s, service %s in the %s config")  % aServiceKey % aProviderName % aServiceName % (aConfigType==userConfig?"user":"master"));
                }

                template <class T>
                string getServiceArrayVal(const string& aServiceKey, const string& aProviderName, const string& aServiceName, ConfigType aConfigType, T& aVal)
                {
                    if (aConfigType != userConfig && aConfigType != masterConfig)
                        TA_THROW_MSG(SettingsError, boost::format("Unexpected confguration type %d") % aConfigType);

                    auto_ptr<libconfig::Config> myConfigPtr = load(aConfigType);

                    const unsigned int myNumProviders = getListSize(*myConfigPtr, ProviderList);
                    if (myNumProviders == 0)
                        TA_THROW_MSG(SettingsError, boost::format("No providers exist in the %s config")  % (aConfigType==userConfig?"user":"master"));

                    for (unsigned int iProvider=0; iProvider < myNumProviders; ++iProvider)
                    {
                        const string myProviderNamePath = getProviderSettingPath(iProvider, ProviderName);
                        string myProviderName;
                        if (!myConfigPtr->lookupValue(myProviderNamePath, myProviderName))
                            TA_THROW_MSG(SettingsError, boost::format("%s setting not found in the %s config or it is not a string") % myProviderNamePath % (aConfigType==userConfig?"user":"master"));

                        if (myProviderName == aProviderName)
                        {
                            const unsigned int myNumServices = getListSize(*myConfigPtr, getProviderSettingPath(iProvider, ServiceList));
                            if (myNumServices == 0)
                                TA_THROW_MSG(SettingsError, boost::format("No services exist for provider %s in the %s config")  % aProviderName % (aConfigType==userConfig?"user":"master"));
                            for (unsigned int iService=0; iService < myNumServices; ++iService)
                            {
                                const string myServiceNamePath = getServiceSettingPath(iProvider, iService, ServiceName);
                                string myServiceName;
                                if (!myConfigPtr->lookupValue(myServiceNamePath, myServiceName))
                                    TA_THROW_MSG(SettingsError, boost::format("%s setting not found in the %s config or it is not a string") % myServiceNamePath % (aConfigType==userConfig?"user":"master"));
                                if (myServiceName == aServiceName)
                                {
                                    const string myKeyPath = getServiceSettingPath(iProvider, iService, aServiceKey);
                                    aVal = getArrayValue<T>(*myConfigPtr, myKeyPath);
                                    return myKeyPath;
                                }
                            }
                        }
                    }
                    TA_THROW_MSG(SettingsError, boost::format("%s setting not found for provider %s, service %s in the %s config")  % aServiceKey % aProviderName % aServiceName % (aConfigType==userConfig?"user":"master"));
                }

                //
                // T is needed to specify expected value type
                //
                // Exceptions : throw SettingsError, SettingsOpenError
                //
                template <class T>
                bool isServiceValExist(const string& aServiceKey, const string& aProviderName, const string& aServiceName, ConfigType aConfigType)
                {
                    if (aConfigType != userConfig && aConfigType != masterConfig)
                        TA_THROW_MSG(SettingsError, boost::format("Unexpected confguration type %d") % aConfigType);

                    // master config is optional
                    if (aConfigType == masterConfig && !isMasterConfigExist())
                        return false;

                    auto_ptr<libconfig::Config> myConfigPtr = load(aConfigType);
                    const unsigned int myNumProviders = getListSize(*myConfigPtr, ProviderList);

                    for (unsigned int iProvider=0; iProvider < myNumProviders; ++iProvider)
                    {
                        const string myProviderNamePath = getProviderSettingPath(iProvider, ProviderName);
                        string myProviderName;
                        if (!myConfigPtr->lookupValue(myProviderNamePath, myProviderName))
                            TA_THROW_MSG(SettingsError, boost::format("%s setting not found in the %s config or it is not a string") % myProviderNamePath % (aConfigType==userConfig?"user":"master"));

                        if (myProviderName == aProviderName)
                        {
                            const unsigned int myNumServices = getListSize(*myConfigPtr, getProviderSettingPath(iProvider, ServiceList));
                            if (myNumServices == 0)
                                return false;
                            for (unsigned int iService=0; iService < myNumServices; ++iService)
                            {
                                const string myServiceNamePath = getServiceSettingPath(iProvider, iService, ServiceName);
                                string myServiceName;
                                if (!myConfigPtr->lookupValue(myServiceNamePath, myServiceName))
                                    TA_THROW_MSG(SettingsError, boost::format("%s setting not found in the %s config or it is not a string") % myServiceNamePath % (aConfigType==userConfig?"user":"master"));
                                if (myServiceName == aServiceName)
                                {
                                    const string myKeyPath = getServiceSettingPath(iProvider, iService, aServiceKey);
                                    if (!myConfigPtr->exists(myKeyPath))
                                        return false;
                                    string myFriendlyTypeName;
                                    if (myConfigPtr->lookup(myKeyPath).getType() != getLibconfigType(T(), myFriendlyTypeName))
                                    {
                                        TA_THROW_MSG(SettingsError, boost::format("%s setting in the %s config has invalid type. %s expected") %
                                                     myKeyPath % (aConfigType==userConfig?"user":"master") % myFriendlyTypeName);
                                    }
                                    return true;
                                }
                            }
                        }
                    }
                    return false;
                }

                //
                // Abstract   : set service value in the specified config
                //
                // Exceptions : throw SettingsError, SettingsOpenError, SettingsSaveError
                //
                template <class T>
                void setServiceScalarVal(const string& aServiceKey, const string& aProviderName, const string& aServiceName, ConfigType aConfigType, const T& aVal)
                {
                    if (aConfigType != userConfig && aConfigType != masterConfig)
                        TA_THROW_MSG(SettingsError, boost::format("Unexpected confguration type %d") % aConfigType);

                    auto_ptr<libconfig::Config> myConfigPtr = load(aConfigType);
                    const unsigned int myNumProviders = getListSize(*myConfigPtr, ProviderList);
                    if (myNumProviders == 0)
                        TA_THROW_MSG(SettingsError, boost::format("No providers exist in the %s config")  % (aConfigType==userConfig?"user":"master"));

                    string myExpectedTypeName;
                    libconfig::Setting::Type myExpectedType = getLibconfigType(aVal, myExpectedTypeName);

                    for (unsigned int iProvider=0; iProvider < myNumProviders; ++iProvider)
                    {
                        const string myProviderNamePath = getProviderSettingPath(iProvider, ProviderName);
                        string myProviderName;
                        if (!myConfigPtr->lookupValue(myProviderNamePath, myProviderName))
                            TA_THROW_MSG(SettingsError, boost::format("%s setting not found in the %s config or it is not a string") % myProviderNamePath % (aConfigType==userConfig?"user":"master"));

                        if (myProviderName == aProviderName)
                        {
                            const unsigned int myNumServices = getListSize(*myConfigPtr, getProviderSettingPath(iProvider, ServiceList));
                            if (myNumServices == 0)
                                TA_THROW_MSG(SettingsError, boost::format("No services exist for provider %s in the %s config")  % aProviderName % (aConfigType==userConfig?"user":"master"));
                            for (unsigned int iService=0; iService < myNumServices; ++iService)
                            {
                                const string myServiceNamePath = getServiceSettingPath(iProvider, iService, ServiceName);
                                string myServiceName;
                                if (!myConfigPtr->lookupValue(myServiceNamePath, myServiceName))
                                    TA_THROW_MSG(SettingsError, boost::format("%s setting not found in the %s config or it is not a string") % myServiceNamePath % (aConfigType==userConfig?"user":"master"));
                                if (myServiceName == aServiceName)
                                {
                                    libconfig::Setting& myServiceSetting = myConfigPtr->lookup(getServicePath(iProvider, iService));
                                    if (!myServiceSetting.exists(aServiceKey))
                                        myServiceSetting.add(aServiceKey, myExpectedType);
                                    libconfig::Setting& myKeySetting = myConfigPtr->lookup(getServiceSettingPath(iProvider, iService, aServiceKey));
                                    if (myKeySetting.getType() != myExpectedType)
                                        TA_THROW_MSG(SettingsError, boost::format("%s setting in the %s config is not of %s type") % myKeySetting.getPath() % (aConfigType==userConfig?"user":"master") % myExpectedTypeName);
                                    myKeySetting = aVal;
                                    save(*myConfigPtr, aConfigType);
                                    return;
                                }
                            }
                        }
                    }
                    TA_THROW_MSG(SettingsError, boost::format("%s setting not found for provider %s and service %s in the %s config")  % aServiceKey % aProviderName % aServiceName % (aConfigType==userConfig?"user":"master"));
                }

                template <class T>
                void setServiceArrayVal(const string& aServiceKey, const string& aProviderName, const string& aServiceName, ConfigType aConfigType, const T& aVal)
                {
                    if (aConfigType != userConfig && aConfigType != masterConfig)
                        TA_THROW_MSG(SettingsError, boost::format("Unexpected confguration type %d") % aConfigType);

                    auto_ptr<libconfig::Config> myConfigPtr = load(aConfigType);
                    const unsigned int myNumProviders = getListSize(*myConfigPtr, ProviderList);
                    if (myNumProviders == 0)
                    {
                        TA_THROW_MSG(SettingsError, boost::format("No providers exist in the %s config")  % (aConfigType==userConfig?"user":"master"));
                    }

                    for (unsigned int iProvider=0; iProvider < myNumProviders; ++iProvider)
                    {
                        const string myProviderNamePath = getProviderSettingPath(iProvider, ProviderName);
                        string myProviderName;
                        if (!myConfigPtr->lookupValue(myProviderNamePath, myProviderName))
                        {
                            TA_THROW_MSG(SettingsError, boost::format("%s setting not found in the %s config or it is not a string") % myProviderNamePath % (aConfigType==userConfig?"user":"master"));
                        }

                        if (myProviderName == aProviderName)
                        {
                            const unsigned int myNumServices = getListSize(*myConfigPtr, getProviderSettingPath(iProvider, ServiceList));
                            if (myNumServices == 0)
                            {
                                TA_THROW_MSG(SettingsError, boost::format("No services exist for provider %s in the %s config")  % aProviderName % (aConfigType==userConfig?"user":"master"));
                            }
                            for (unsigned int iService=0; iService < myNumServices; ++iService)
                            {
                                const string myServiceNamePath = getServiceSettingPath(iProvider, iService, ServiceName);
                                string myServiceName;
                                if (!myConfigPtr->lookupValue(myServiceNamePath, myServiceName))
                                {
                                    TA_THROW_MSG(SettingsError, boost::format("%s setting not found in the %s config or it is not a string") % myServiceNamePath % (aConfigType==userConfig?"user":"master"));
                                }
                                if (myServiceName == aServiceName)
                                {
                                    assignArrayValue(*myConfigPtr, getServicePath(iProvider, iService), aServiceKey, aVal, SettingsImpl::settingCreateIfNotExist);
                                    save(*myConfigPtr, aConfigType);
                                    return;
                                }
                            }
                        }
                    }
                    TA_THROW_MSG(SettingsError, boost::format("Cannot assign %s setting because no service %s found for provider %s in the %s config")  % aServiceKey % aServiceName % aProviderName % (aConfigType==userConfig?"user":"master"));
                }

                enum MergeType
                {
                    mergeUserToUser, mergeMasterToMaster, mergeMasterToUser
                };

                void addService(MergeType aMergeType, const libconfig::Config& aSourceConfig, const string& aSourceServicePath, libconfig::Config& aTargetConfig, const string& aTargetServiceListPath)
                {
                    if (aMergeType != mergeUserToUser && aMergeType != mergeMasterToMaster && aMergeType != mergeMasterToUser)
                        TA_THROW_MSG(SettingsError, boost::format("Unexpected merge type %d") % aMergeType);

                    libconfig::Setting& myTargetServiceSetting = aTargetConfig.lookup(aTargetServiceListPath).add(libconfig::Setting::TypeGroup);
                    const vector<string> myServiceSettingNames = (aMergeType == mergeUserToUser) ? userConfigServiceSettings()
                            : masterConfigServiceSettings();
                    foreach (const string& sourceSettingName, myServiceSettingNames)
                    {
                        const string mySourceSettingPath = aSourceServicePath + "." + sourceSettingName;
                        if (aSourceConfig.exists(mySourceSettingPath))
                        {
                            libconfig::Setting& mySourceSetting = aSourceConfig.lookup(mySourceSettingPath);
                            const string myTargetSettingName = (sourceSettingName == DefServiceUri && aMergeType == mergeMasterToUser) ? ServiceUri : sourceSettingName;
                            libconfig::Setting& myTargetSetting = myTargetServiceSetting.add(myTargetSettingName, mySourceSetting.getType());
                            assignSetting(myTargetSetting, mySourceSetting);
                        }
                    }
                }

                void replaceService(MergeType aMergeType, const libconfig::Config& aSourceConfig, const string& aSourceServicePath, libconfig::Config& aTargetConfig, const string& aTargetServicePath)
                {
                    if (aMergeType != mergeUserToUser && aMergeType != mergeMasterToMaster && aMergeType != mergeMasterToUser)
                        TA_THROW_MSG(SettingsError, boost::format("Unexpected merge type %d") % aMergeType);

                    libconfig::Setting& myTargetServiceSetting = aTargetConfig.lookup(aTargetServicePath);
                    const vector<string> myServiceSettingNames = (aMergeType == mergeUserToUser) ? userConfigServiceSettings()
                            : masterConfigServiceSettings();
                    foreach (const string& sourceSettingName, myServiceSettingNames)
                    {
                        const string mySourceSettingPath = aSourceServicePath + "." + sourceSettingName;
                        if (aSourceConfig.exists(mySourceSettingPath))
                        {
                            const string myTargetSettingName = (sourceSettingName == DefServiceUri && aMergeType == mergeMasterToUser) ? ServiceUri
                                                               : sourceSettingName;
                            const string myTargetSettingPath = aTargetServicePath + "." + myTargetSettingName;

                            libconfig::Setting& mySourceSetting = aSourceConfig.lookup(mySourceSettingPath);
                            // recover missing service URI in user config from master
                            if (sourceSettingName == DefServiceUri && aMergeType == mergeMasterToUser)
                            {
                                if (aTargetConfig.exists(myTargetSettingPath))
                                    continue;
                                libconfig::Setting& myTargetSetting = myTargetServiceSetting.add(myTargetSettingName, mySourceSetting.getType());
                                assignSetting(myTargetSetting, mySourceSetting);
                                continue;
                            }
                            libconfig::Setting& myTargetSetting = aTargetConfig.exists(myTargetSettingPath) ? aTargetConfig.lookup(myTargetSettingPath)
                                                                  : myTargetServiceSetting.add(myTargetSettingName, mySourceSetting.getType());
                            assignSetting(myTargetSetting, mySourceSetting);
                        }
                    }
                }

                void addProvider(MergeType aMergeType, const libconfig::Config& aSourceConfig, const string& aSourceProviderPath, libconfig::Config& aTargetConfig)
                {
                    if (aMergeType != mergeUserToUser && aMergeType != mergeMasterToMaster && aMergeType != mergeMasterToUser)
                        TA_THROW_MSG(SettingsError, boost::format("Unexpected merge type %d") % aMergeType);

                    if (!aTargetConfig.exists(ProviderList))
                    {
                        aTargetConfig.getRoot().add(ProviderList, libconfig::Setting::TypeList);
                    }
                    libconfig::Setting& myTargetProviderSetting = aTargetConfig.lookup(ProviderList).add(libconfig::Setting::TypeGroup);

                    const vector<string> mySourceProviderSettingNames = (aMergeType == mergeUserToUser) ? userConfigProviderSettings()
                            : masterConfigProviderSettings();

                    // Add providers settings without diving into services
                    foreach (const string& sourceSettingName, mySourceProviderSettingNames)
                    {
                        const string mySourceSettingPath = aSourceProviderPath + "." + sourceSettingName;
                        if (aSourceConfig.exists(mySourceSettingPath))
                        {
                            string myTargetSettingName = sourceSettingName;

                            if (aMergeType == mergeMasterToUser)
                            {
                                if (sourceSettingName == DefProviderReseptSvrAddress)
                                {
                                    if (aTargetConfig.exists(myTargetProviderSetting.getPath() + "." + ProviderReseptSvrAddress)) {
                                        // enforcement setting counterpart already exists, do not overwrite it (notice that the enforcement counterpart should PRECEDE its default counterpart in the list of setting names)
                                        continue;
                                    }
                                    else {
                                        // create target setting from its default counterpart
                                        myTargetSettingName = ProviderReseptSvrAddress;
                                    }
                                }
                                else if (sourceSettingName == DefProviderCaList)
                                {
                                    if (aTargetConfig.exists(myTargetProviderSetting.getPath() + "." + ProviderCaList)) {
                                        // enforcement setting counterpart already exists, do not overwrite it (notice that the enforcement counterpart should PRECEDE its default counterpart in the list of setting names)
                                        continue;
                                    }
                                    else {
                                        // create target setting from its default counterpart
                                        myTargetSettingName = ProviderCaList;
                                    }
                                }
                            }
                            libconfig::Setting& mySrcSetting = aSourceConfig.lookup(mySourceSettingPath);
                            libconfig::Setting& myTargetSetting = myTargetProviderSetting.add(myTargetSettingName, mySrcSetting.getType());
                            assignSetting(myTargetSetting, mySrcSetting);
                        }
                    }

                    // Add services
                    libconfig::Setting& myTargetServiceListSetting = myTargetProviderSetting.add(ServiceList, libconfig::Setting::TypeList);
                    const string myTargetServiceListPath = myTargetServiceListSetting.getPath();
                    foreach (ta::StringPair sourceService, getServices(aSourceConfig, aSourceProviderPath))
                    {
                        addService(aMergeType, aSourceConfig, sourceService.second, aTargetConfig, myTargetServiceListPath);
                    }
                }

                void replaceProvider(MergeType aMergeType, const libconfig::Config& aSourceConfig, const string& aSourceProviderPath, libconfig::Config& aTargetConfig, const string& aTargetProviderPath)
                {
                    if (aMergeType != mergeUserToUser && aMergeType != mergeMasterToMaster && aMergeType != mergeMasterToUser)
                        TA_THROW_MSG(SettingsError, boost::format("Unexpected merge type %d") % aMergeType);

                    libconfig::Setting& myTargetProviderSetting = aTargetConfig.lookup(aTargetProviderPath);

                    const vector<string> mySourceProviderSettingNames = (aMergeType == mergeUserToUser) ? userConfigProviderSettings()
                            : masterConfigProviderSettings();

                    // Add/replace provider settings without diving into services
                    foreach (const string& sourceSettingName, mySourceProviderSettingNames)
                    {
                        const string mySourceSettingPath = aSourceProviderPath + "." + sourceSettingName;
                        if (aSourceConfig.exists(mySourceSettingPath))
                        {
                            string myTargetSettingName = sourceSettingName;

                            if (aMergeType == mergeMasterToUser)
                            {
                                if (sourceSettingName == DefProviderReseptSvrAddress)
                                {
                                    if (aTargetConfig.exists(myTargetProviderSetting.getPath() + "." + ProviderReseptSvrAddress)) {
                                        // enforcement setting counterpart already exists, do not overwrite it (notice that the enforcement counterpart should PRECEDE its default counterpart in the list of setting names)
                                        continue;
                                    }
                                    else {
                                        // create target setting from its default counterpart
                                        myTargetSettingName = ProviderReseptSvrAddress;
                                    }
                                }
                                else if (sourceSettingName == DefProviderCaList)
                                {
                                    if (aTargetConfig.exists(myTargetProviderSetting.getPath() + "." + ProviderCaList)) {
                                        // enforcement setting counterpart already exists, do not overwrite it (notice that the enforcement counterpart should PRECEDE its default counterpart in the list of setting names)
                                        continue;
                                    }
                                    else {
                                        // create target setting from its default counterpart
                                        myTargetSettingName = ProviderCaList;
                                    }
                                }
                            }

                            const string myTargetProviderSettingPath = aTargetProviderPath + "." + myTargetSettingName;
                            libconfig::Setting& mySrcSetting = aSourceConfig.lookup(mySourceSettingPath);
                            libconfig::Setting& myTargetSetting = aTargetConfig.exists(myTargetProviderSettingPath)
                                                                  ? aTargetConfig.lookup(myTargetProviderSettingPath)
                                                                  : myTargetProviderSetting.add(myTargetSettingName, mySrcSetting.getType());
                            assignSetting(myTargetSetting, mySrcSetting);
                        }
                    }

                    // Add/replace services
                    const string myTargetServiceListPath = aTargetProviderPath + "." + ServiceList;
                    const map<string,string> myTargetServices = getServices(aTargetConfig, aTargetProviderPath);
                    foreach (const ta::StringPair& sourceService, getServices(aSourceConfig, aSourceProviderPath))
                    {
                        string myServicePath;
                        if (ta::findValueByKey(sourceService.first, myTargetServices, myServicePath))
                            replaceService(aMergeType, aSourceConfig, sourceService.second, aTargetConfig, myServicePath);
                        else
                            addService(aMergeType, aSourceConfig, sourceService.second, aTargetConfig, myTargetServiceListPath);
                    }

                }

                // Merges two configs overriding settings with the same name and adding settings that do not exist in the target config
                //@pre source config should be well-formed user or maser config. Target config should at least contain version and provider list (maybe empty)
                void mergeConfigs(MergeType aMergeType, const libconfig::Config& aSourceConfig, libconfig::Config& aTargetConfig)
                {
                    if (aMergeType != mergeUserToUser && aMergeType != mergeMasterToMaster && aMergeType != mergeMasterToUser)
                        TA_THROW_MSG(SettingsError, boost::format("Unexpected merge type %d") % aMergeType);

                    // Add/update target config
                    ta::StringDict myTargetProviders = getProviders(aTargetConfig);
                    foreach (const ta::StringPair& sourceProvider, getProviders(aSourceConfig))
                    {
                        string myProviderPath;
                        if (ta::findValueByKey(sourceProvider.first, myTargetProviders, myProviderPath))
                            replaceProvider(aMergeType, aSourceConfig, sourceProvider.second, aTargetConfig, myProviderPath);
                        else
                            addProvider(aMergeType, aSourceConfig, sourceProvider.second, aTargetConfig);
                    }

                    // Verify
                    myTargetProviders = getProviders(aTargetConfig);
                    if (myTargetProviders.empty())
                        TA_THROW_MSG(SettingsError, "No providers exist in the target config");
                    const string myProvider0 = myTargetProviders.begin()->first;
                    map<string,string> myTargetServices = getServices(aTargetConfig, getProviderPath(0));
                    if (myTargetServices.empty())
                        TA_THROW_MSG(SettingsError, "No services exist in the target config for provider " + myProvider0);
                    const string myService0 = myTargetServices.begin()->first;


                    // Add/update latest provider and service
                    if (aMergeType == mergeUserToUser)
                    {
                        string myLatestProvider, myLatestService;
                        if (!aSourceConfig.lookupValue(LatestProvider, myLatestProvider))
                            TA_THROW_MSG(SettingsError, boost::format("No % setting found in source config") % LatestProvider);
                        if (!aSourceConfig.lookupValue(LatestService, myLatestService))
                            TA_THROW_MSG(SettingsError, boost::format("No % setting found in source config") % LatestService);

                        if (aTargetConfig.exists(LatestProvider))
                            aTargetConfig.lookup(LatestProvider) = myLatestProvider;
                        else
                            aTargetConfig.getRoot().add(LatestProvider, libconfig::Setting::TypeString) = myLatestProvider;

                        if (aTargetConfig.exists(LatestService))
                            aTargetConfig.lookup(LatestService) = myLatestService;
                        else
                            aTargetConfig.getRoot().add(LatestService, libconfig::Setting::TypeString) = myLatestService;
                    }
                    else if (aMergeType == mergeMasterToUser)
                    {
                        if (!aTargetConfig.exists(LatestProvider))
                            aTargetConfig.getRoot().add(LatestProvider, libconfig::Setting::TypeString) = myProvider0;
                        if (!aTargetConfig.exists(LatestService))
                            aTargetConfig.getRoot().add(LatestService, libconfig::Setting::TypeString) = myService0;
                    }
                }

                //@return user config
                auto_ptr<libconfig::Config> updateUserConfigFromMaster()
                {
                    // Load/init configs. Notice that we never call load(userConfig) neither explicitly or implicitly to avoid infinite recursion
                    const auto_ptr<libconfig::Config> myMasterConfig = load(masterConfig);
                    auto_ptr<libconfig::Config> myUserConfig;
                    const string myConfigFilePath = getConfigFilePath(userConfig);
                    try
                    {
                        myUserConfig = load(myConfigFilePath);
                    }
                    catch (...)
                    {
                        // User config does not exist or is corrupted. Create minimal workable user config based on the master config.
                        myUserConfig.reset(new libconfig::Config());
                        string myMasterVersionStr;
                        if (!myMasterConfig->lookupValue(ConfigVersion, myMasterVersionStr))
                            TA_THROW_MSG(SettingsError, boost::format("%s does not exist in the master config") % ConfigVersion);
                        myUserConfig->getRoot().add(ConfigVersion, libconfig::Setting::TypeString) = myMasterVersionStr;
                        myUserConfig->getRoot().add(ProviderList, libconfig::Setting::TypeList);
                    }

                    mergeConfigs(mergeMasterToUser, *myMasterConfig, *myUserConfig);

                    save(*myUserConfig, userConfig, true);
                    return myUserConfig;
                }

            } // namespace
        } // namespace SettingsImpl
    }// namespace Settings
} // namespace rclient
