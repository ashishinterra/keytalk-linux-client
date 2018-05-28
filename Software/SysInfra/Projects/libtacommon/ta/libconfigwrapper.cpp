#include "libconfigwrapper.h"
#include "strings.h"
#include "scopedresource.hpp"
#include "utils.h"
#include "common.h"

// Ignore warnings caused by the usage of exception specification in libconfig
#ifdef _MSC_VER
#pragma warning (disable: 4290)
#endif
#include "libconfig.h++"
#ifdef _MSC_VER
#pragma warning (default: 4290)
#endif
#include "boost/algorithm/string.hpp"
#include <vector>
#include <memory>
#include <errno.h>

using std::string;
using std::vector;

namespace ta
{
    // private API
    namespace
    {
        void skipUtf8Bom(FILE* anFd)
        {
            if (!anFd)
            {
                return;
            }

            static const unsigned char Utf8Bom[] = {0xEF, 0xBB, 0xBF};
            unsigned char buf[sizeof(Utf8Bom)]  = {};
            bool myIsBomFound = true;
            if (fread(buf, 1, sizeof(buf), anFd) == sizeof(buf))
            {
                for (size_t i = 0; i < sizeof(buf); ++i)
                {
                    if (buf[i] != Utf8Bom[i])
                    {
                        myIsBomFound = false;
                        break;
                    }
                }
            }
            if (!myIsBomFound)
            {
                fseek(anFd, 0, 0);
            }
        }

        std::auto_ptr<libconfig::Config> load(const string& aConfigFilePath)
        {
            ta::ScopedResource<FILE*> myFd(fopen(aConfigFilePath.c_str(), "rt"), fclose);
            if (!myFd)
            {
                TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to open %s. %s") % aConfigFilePath % strerror(errno));
            }

            //libconfig does not understand UTF-8 BOM, lets help it with it
            skipUtf8Bom(myFd);

            std::auto_ptr<libconfig::Config> myConfigPtr;
            try
            {
                myConfigPtr.reset(new libconfig::Config());
                myConfigPtr->read(myFd);
            }
            catch (libconfig::ParseException& e)
            {
                TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to parse %s. line %d : %s") % aConfigFilePath % e.getLine() % e.getError());
            }
            catch (std::exception& e)
            {
                TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to open %s. %s.") % aConfigFilePath % e.what());
            }

            return myConfigPtr;
        }

        // Add setting creating intermediate group settings if necessary
        //@return reference to this setting in aConfig
        libconfig::Setting& addSetting(libconfig::Config& aConfig, const string& aSettingPath, libconfig::Setting::Type aSettingType, const string& aConfigFilePathHint)
        {
            vector<string> myParts = Strings::split(aSettingPath, '.');
            if (myParts.empty())
            {
                TA_THROW_MSG(LibConfigWrapperError, boost::format("Cannot set empty setting in '%1%'") % aConfigFilePathHint);
            }
            const string mySettingName = myParts.back();
            myParts.pop_back();
            string myPath, myParentPath;
            // add intermediate groups
            foreach (const string& part, myParts)
            {
                myPath = myPath.empty() ? part : myPath + "." + part;
                if (!aConfig.exists(myPath))
                {
                    aConfig.lookup(myParentPath).add(part, libconfig::Setting::TypeGroup);
                }
                myParentPath = myPath;
            }
            return aConfig.lookup(myParentPath).add(mySettingName, aSettingType);
        }

        template <class T>
        void setScalarValue(const string& aConfigFilePath, const string& aSettingPath, const T& aSettingValue, libconfig::Setting::Type aSettingType, LibConfigWrapper::SettingSetPolicy aSettingSetPolicy)
        {
            try
            {
                std::auto_ptr<libconfig::Config> myConfig = load(aConfigFilePath);

                if (myConfig->exists(aSettingPath))
                {
                    libconfig::Setting& mySetting = myConfig->lookup(aSettingPath);
                    mySetting = aSettingValue;
                }
                else
                {
                    if (aSettingSetPolicy == LibConfigWrapper::settingSetFailIfNotExist)
                    {
                        TA_THROW_MSG(LibConfigWrapperError, boost::format("Cannot set '%1%' in '%2%' because the setting does not exist") % aSettingPath % aConfigFilePath);
                    }

                    libconfig::Setting& mySetting = addSetting(*myConfig, aSettingPath, aSettingType, aConfigFilePath);
                    mySetting = aSettingValue;
                }

                myConfig->writeFile(aConfigFilePath.c_str());
            }
            catch (LibConfigWrapperError&)
            {
                throw;
            }
            catch (libconfig::FileIOException&)
            {
                TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to save to config file '%1%'.") % aConfigFilePath);
            }
            catch (libconfig::ParseException& e)
            {
                TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to parse setting at line %1% in '%2%': %3%") % e.getLine() % aConfigFilePath % e.getError());
            }
            catch (libconfig::SettingNotFoundException& e)
            {
                TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to locate setting '%1%' in '%2%': %3%") % e.getPath() % aConfigFilePath % e.what());
            }
            catch (libconfig::SettingTypeException& e)
            {
                TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to set '%1%' in '%2%': %3%") % e.getPath() % aConfigFilePath % e.what());
            }
            catch (libconfig::SettingException& e)
            {
                TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to parse setting '%1%' in '%2%': %3%") % e.getPath() % aConfigFilePath % e.what());
            }
            catch (std::exception& e)
            {
                TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to parse '%1%' in '%2%': %3%") % aSettingPath % aConfigFilePath % e.what());
            }
            catch (...)
            {
                TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to parse '%1%' in '%2%': unknown error.") % aSettingPath % aConfigFilePath);
            }
        }

    } // end of private API


    //
    // Public API
    //

    LibConfigWrapper::LibConfigWrapper(const string& aConfigFilePath, const LibConfigWrapper::FileCreationPolicy aFileCreationPolicy)
        : theConfigFilePath(aConfigFilePath)
    {
        if (!ta::isFileExist(aConfigFilePath))
        {
            if (aFileCreationPolicy == fileFailIfNotExist)
            {
                TA_THROW_MSG(LibConfigWrapperError, "Configuration file " + aConfigFilePath + " does not exist");
            }
            else
            {
                ta::ScopedResource<FILE*> myFd(fopen(aConfigFilePath.c_str(), "w"), fclose);
                if (!myFd)
                {
                    TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to create configuration file %s. %s.") % aConfigFilePath % strerror(errno));
                }
            }
        }
    }

    //
    // Getters
    //

    bool LibConfigWrapper::getValue(const string& aSettingPath, string& aSettingValue, SettingGetPolicy aSettingGetPolicy, StripWs aStripWs) const
    {
        try
        {
            std::auto_ptr<libconfig::Config> myConfig = load(theConfigFilePath);

            if (!myConfig->lookupValue(aSettingPath, aSettingValue))
            {
                if (aSettingGetPolicy == settingGetFailIfNotExist)
                {
                    TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to get string setting '%1%' in '%2%' because the setting does not exist or is not string type") % aSettingPath % theConfigFilePath);
                }
                else
                {
                    return false;
                }
            }
            if (aStripWs == wsStripYes)
            {
                boost::algorithm::trim(aSettingValue);
            }
            return true;
        }
        catch (LibConfigWrapperError&)
        {
            throw;
        }
        catch (libconfig::FileIOException&)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to open config file '%1%'.") % theConfigFilePath);
        }
        catch (std::exception& e)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to parse '%1%' in '%2%': %3%") % aSettingPath % theConfigFilePath % e.what());
        }
        catch (...)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to parse '%1%' in '%2%': unknown error.") % aSettingPath % theConfigFilePath);
        }
    }

    bool LibConfigWrapper::getValue(const string& aSettingPath, int& aSettingValue, SettingGetPolicy aSettingGetPolicy) const
    {
        try
        {
            std::auto_ptr<libconfig::Config> myConfig = load(theConfigFilePath);

            if (!myConfig->lookupValue(aSettingPath, aSettingValue))
            {
                if (aSettingGetPolicy == settingGetFailIfNotExist)
                    TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to get integer setting '%1%' in '%2%' because the setting does not exist or is not integer type") % aSettingPath % theConfigFilePath);
                return false;
            }
            return true;
        }
        catch (LibConfigWrapperError&)
        {
            throw;
        }
        catch (libconfig::FileIOException&)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to open config file '%1%'.") % theConfigFilePath);
        }
        catch (std::exception& e)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to parse '%1%' in '%2%': %3%") % aSettingPath % theConfigFilePath % e.what());
        }
        catch (...)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to parse '%1%' in '%2%': unknown error.") % aSettingPath % theConfigFilePath);
        }
    }

    bool LibConfigWrapper::getValue(const string& aSettingPath, unsigned int& aSettingValue, SettingGetPolicy aSettingGetPolicy) const
    {
        int mySettingValue;
        bool myRetVal = getValue(aSettingPath, mySettingValue, aSettingGetPolicy);
        if (myRetVal)
        {
            if (mySettingValue < 0)
                TA_THROW_MSG(LibConfigWrapperError, boost::format("Invalid value for '%1%' in '%2%': the value should be non-negative integer.") % aSettingPath % theConfigFilePath);
            aSettingValue = mySettingValue;
        }
        return myRetVal;
    }

    bool LibConfigWrapper::getValue(const string& aSettingPath, bool& aSettingValue, SettingGetPolicy aSettingGetPolicy) const
    {
        try
        {
            std::auto_ptr<libconfig::Config> myConfig = load(theConfigFilePath);

            if (!myConfig->lookupValue(aSettingPath, aSettingValue))
            {
                if (aSettingGetPolicy == settingGetFailIfNotExist)
                    TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to get boolean setting '%1%' in '%2%' because the setting does not exist or is not boolean type") % aSettingPath % theConfigFilePath);
                return false;
            }
            return true;
        }
        catch (LibConfigWrapperError&)
        {
            throw;
        }
        catch (libconfig::FileIOException&)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to open config file '%1%'.") % theConfigFilePath);
        }
        catch (std::exception& e)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to parse '%1%' in '%2%': %3%") % aSettingPath % theConfigFilePath % e.what());
        }
        catch (...)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to parse '%1%' in '%2%': unknown error.") % aSettingPath % theConfigFilePath);
        }
    }

    bool LibConfigWrapper::getValue(const string& aSettingPath, vector<string>& aSettingValue, SettingGetPolicy aSettingGetPolicy) const
    {
        try
        {
            std::auto_ptr<libconfig::Config> myConfig = load(theConfigFilePath);

            if (!myConfig->exists(aSettingPath))
            {
                if (aSettingGetPolicy == settingGetFailIfNotExist)
                {
                    TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to get setting '%1%' in '%2%' because it does not exist") % aSettingPath % theConfigFilePath);
                }
                else
                {
                    return false;
                }
            }

            libconfig::Setting& mySetting = myConfig->lookup(aSettingPath);
            if (!mySetting.isArray())
            {
                TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to get array setting '%1%' in '%2%' because the setting is not of array type") % aSettingPath % theConfigFilePath);
            }

            aSettingValue.clear();
            const unsigned int myNumElems = mySetting.getLength();
            for (unsigned int iElem=0; iElem < myNumElems; ++iElem)
            {
                string myElemVal;
                const string myPath = str(boost::format("%s.[%u]") % aSettingPath % iElem);
                if (!myConfig->lookupValue(myPath, myElemVal))
                {
                    TA_THROW_MSG(LibConfigWrapperError, boost::format("%s setting not found or it is not a string") % myPath);
                }
                aSettingValue.push_back(myElemVal);
            }
            return true;
        }
        catch (LibConfigWrapperError&)
        {
            throw;
        }
        catch (libconfig::FileIOException&)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to open config file '%1%'.") % theConfigFilePath);
        }
        catch (std::exception& e)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to parse '%1%' in '%2%': %3%") % aSettingPath % theConfigFilePath % e.what());
        }
        catch (...)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to parse '%1%' in '%2%': unknown error.") % aSettingPath % theConfigFilePath);
        }
    }

    bool LibConfigWrapper::isStringSettingExist(const string& aSettingPath) const
    {
        string dummy;
        return getValue(aSettingPath, dummy, settingGetTolerateIfNotExist);
    }

    bool LibConfigWrapper::isIntSettingExist(const string& aSettingPath) const
    {
        int dummy;
        return getValue(aSettingPath, dummy, settingGetTolerateIfNotExist);
    }

    bool LibConfigWrapper::isUintSettingExist(const string& aSettingPath) const
    {
        unsigned int dummy;
        return getValue(aSettingPath, dummy, settingGetTolerateIfNotExist);
    }

    bool LibConfigWrapper::isBoolSettingExist(const string& aSettingPath) const
    {
        bool dummy;
        return getValue(aSettingPath, dummy, settingGetTolerateIfNotExist);
    }

    bool LibConfigWrapper::isStringArraySettingExist(const string& aSettingPath) const
    {
        vector<string> dummy;
        return getValue(aSettingPath, dummy, settingGetTolerateIfNotExist);
    }
    bool LibConfigWrapper::isGroupSettingExist(const std::string& aSettingPath) const
    {
        try
        {
            std::auto_ptr<libconfig::Config> myConfig = load(theConfigFilePath);

            if (!myConfig->exists(aSettingPath))
            {
                return false;
            }

            libconfig::Setting& mySetting = myConfig->lookup(aSettingPath);
            if (!mySetting.isGroup())
            {
                TA_THROW_MSG(LibConfigWrapperError, boost::format("Setting '%1%' in '%2%' is not of group type") % aSettingPath % theConfigFilePath);
            }
            return true;
        }
        catch (LibConfigWrapperError&)
        {
            throw;
        }
        catch (libconfig::FileIOException&)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to open config file '%1%'.") % theConfigFilePath);
        }
        catch (std::exception& e)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to parse '%1%' in '%2%': %3%") % aSettingPath % theConfigFilePath % e.what());
        }
        catch (...)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to parse '%1%' in '%2%': unknown error.") % aSettingPath % theConfigFilePath);
        }
    }

    bool LibConfigWrapper::getListInfo(const string& aSettingPath, size_t& aLength) const
    {
        try
        {
            std::auto_ptr<libconfig::Config> myConfig = load(theConfigFilePath);

            libconfig::Setting& mySetting = myConfig->lookup(aSettingPath);
            if (!mySetting.isList())
                return false;
            int myLength = mySetting.getLength();
            if (myLength < 0)
                TA_THROW_MSG(LibConfigWrapperError, boost::format("Negative number of elements in the list at %s in %s") % aSettingPath % theConfigFilePath);
            aLength = myLength;
            return true;
        }
        catch (LibConfigWrapperError&)
        {
            throw;
        }
        catch (libconfig::FileIOException&)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to open config file '%1%'.") % theConfigFilePath);
        }
        catch (libconfig::SettingNotFoundException& e)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to locate setting '%1%' in '%2%': %3%") % e.getPath() % theConfigFilePath % e.what());
        }
        catch (std::exception& e)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to parse '%1%' in '%2%': %3%") % aSettingPath % theConfigFilePath % e.what());
        }
        catch (...)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to parse '%1%' in '%2%': unknown error.") % aSettingPath % theConfigFilePath);
        }
    }



    //
    // Setters
    //

    void LibConfigWrapper::setValue(const string& aSettingPath, const char* aSettingValue, SettingSetPolicy aSettingSetPolicy)
    {
        if (!aSettingValue)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Cannot set setting value '%1%' in '%2%' because it is NULL") % aSettingPath % theConfigFilePath);
        }
        setScalarValue(theConfigFilePath, aSettingPath, string(aSettingValue), libconfig::Setting::TypeString, aSettingSetPolicy);
    }

    void LibConfigWrapper::setValue(const string& aSettingPath, const string& aSettingValue, SettingSetPolicy aSettingSetPolicy)
    {
        setScalarValue(theConfigFilePath, aSettingPath, aSettingValue, libconfig::Setting::TypeString, aSettingSetPolicy);
    }

    void LibConfigWrapper::setValue(const string& aSettingPath, int aSettingValue, SettingSetPolicy aSettingSetPolicy)
    {
        setScalarValue(theConfigFilePath, aSettingPath, aSettingValue, libconfig::Setting::TypeInt, aSettingSetPolicy);
    }

    void LibConfigWrapper::setValue(const string& aSettingPath, bool aSettingValue, SettingSetPolicy aSettingSetPolicy)
    {
        setScalarValue(theConfigFilePath, aSettingPath, aSettingValue, libconfig::Setting::TypeBoolean, aSettingSetPolicy);
    }

    void LibConfigWrapper::setValue(const string& aSettingPath, const vector<string>& aSettingValue, SettingSetPolicy aSettingSetPolicy)
    {
        try
        {
            std::auto_ptr<libconfig::Config> myConfig = load(theConfigFilePath);

            if (myConfig->exists(aSettingPath))
            {
                // remove this setting
                const size_t myLeafSepPos = aSettingPath.rfind('.');
                const string mySettingName = myLeafSepPos != string::npos ? aSettingPath.substr(myLeafSepPos+1) : aSettingPath;
                const string mySettingParentPath = myLeafSepPos != string::npos ? aSettingPath.substr(0, myLeafSepPos) : "";
                myConfig->lookup(mySettingParentPath).remove(mySettingName);
            }
            else
            {
                if (aSettingSetPolicy == settingSetFailIfNotExist)
                {
                    TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to set setting '%1%' in '%2%' because the setting does not exist") % aSettingPath % theConfigFilePath);
                }
            }

            libconfig::Setting& mySetting = addSetting(*myConfig, aSettingPath, libconfig::Setting::TypeArray, theConfigFilePath);
            foreach (const string& elem, aSettingValue)
            {
                mySetting.add(libconfig::Setting::TypeString) = elem;
            }

            myConfig->writeFile(theConfigFilePath.c_str());

        }
        catch (LibConfigWrapperError&)
        {
            throw;
        }
        catch (libconfig::FileIOException&)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to save to config file '%1%'.") % theConfigFilePath);
        }
        catch (libconfig::ParseException& e)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to parse setting at line %1% in '%2%': %3%") % e.getLine() % theConfigFilePath % e.getError());
        }
        catch (libconfig::SettingNotFoundException& e)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to locate setting '%1%' in '%2%': %3%") % e.getPath() % theConfigFilePath % e.what());
        }
        catch (libconfig::SettingTypeException& e)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to set '%1%' in '%2%': %3%") % e.getPath() % theConfigFilePath % e.what());
        }
        catch (libconfig::SettingException& e)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to parse setting '%1%' in '%2%': %3%") % e.getPath() % theConfigFilePath % e.what());
        }
        catch (std::exception& e)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to parse '%1%' in '%2%': %3%") % aSettingPath % theConfigFilePath % e.what());
        }
        catch (...)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to parse '%1%' in '%2%': unknown error.") % aSettingPath % theConfigFilePath);
        }
    }


    void LibConfigWrapper::removeSetting(const string& aSettingPath)
    {
        try
        {
            std::auto_ptr<libconfig::Config> myConfig = load(theConfigFilePath);

            if (!myConfig->exists(aSettingPath))
                return;

            const size_t myLeafSepPos = aSettingPath.rfind('.');
            const string mySettingName = myLeafSepPos != string::npos ? aSettingPath.substr(myLeafSepPos+1) : aSettingPath;
            const string mySettingParentPath = myLeafSepPos != string::npos ? aSettingPath.substr(0, myLeafSepPos) : "";

            libconfig::Setting& mySetting = myConfig->lookup(mySettingParentPath);
            mySetting.remove(mySettingName);
            myConfig->writeFile(theConfigFilePath.c_str());
        }
        catch (LibConfigWrapperError&)
        {
            throw;
        }
        catch (libconfig::FileIOException&)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to save to config file '%1%'.") % theConfigFilePath);
        }
        catch (libconfig::ParseException& e)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to parse setting at line %1% in '%2%': %3%") % e.getLine() % theConfigFilePath % e.getError());
        }
        catch (libconfig::SettingNotFoundException& e)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to locate setting '%1%' in '%2%': %3%") % e.getPath() % theConfigFilePath % e.what());
        }
        catch (libconfig::SettingTypeException& e)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to set '%1%' in '%2%': %3%") % e.getPath() % theConfigFilePath % e.what());
        }
        catch (libconfig::SettingException& e)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to parse setting '%1%' in '%2%': %3%") % e.getPath() % theConfigFilePath % e.what());
        }
        catch (std::exception& e)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to parse '%1%' in '%2%': %3%") % aSettingPath % theConfigFilePath % e.what());
        }
        catch (...)
        {
            TA_THROW_MSG(LibConfigWrapperError, boost::format("Failed to parse '%1%' in '%2%': unknown error.") % aSettingPath % theConfigFilePath);
        }
    }

    string LibConfigWrapper::getConfigFilePath() const
    {
        return theConfigFilePath;
    }

}
