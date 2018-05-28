#pragma once

#include <string>
#include <vector>
#include <stdexcept>

namespace ta
{
    struct LibConfigWrapperError : std::runtime_error
    {
        explicit LibConfigWrapperError (const std::string& aMessage = "") : std::runtime_error(aMessage) {}
    };

    class LibConfigWrapper
    {
    public:
        enum StripWs
        {
            wsStripYes, wsStripNo
        };
        enum FileCreationPolicy
        {
            fileFailIfNotExist, fileCreateIfNotExist
        };
        enum SettingGetPolicy
        {
            settingGetTolerateIfNotExist, settingGetFailIfNotExist
        };
        enum SettingSetPolicy
        {
            settingSetCreateIfNotExist, settingSetFailIfNotExist
        };

        /**
          Constructor
          @throw LibConfigWrapperError
        */
        LibConfigWrapper(const std::string& aConfigFilePath, const FileCreationPolicy aFileCreationPolicy = fileFailIfNotExist);


        /**
          Get a value from the config file

          @param[in] aSettingPath String containing path of config setting to read
          @param[out] aSettingValue setting value
          @param[in] aSettingGetPolicy controls the function behavior when the setting does not exist: if aSettingGetPolicy is tolerateIfNotExist the function return false,
          otherwise the function throws LibConfigWrapperError
          @param[in] aStripWs whether to strip leading and trailing whitespace from the parsed value
          @return whether the value has been found
          @throw LibConfigWrapperError
        */
        bool getValue(const std::string& aSettingPath, std::string& aSettingValue, SettingGetPolicy aSettingGetPolicy, StripWs aStripWs = wsStripNo) const;
        bool getValue(const std::string& aSettingPath, int& aSettingValue, SettingGetPolicy aSettingGetPolicy) const;
        bool getValue(const std::string& aSettingPath, unsigned int& aSettingValue, SettingGetPolicy aSettingGetPolicy) const;
        bool getValue(const std::string& aSettingPath, bool& aSettingValue, SettingGetPolicy aSettingGetPolicy) const;
        bool getValue(const std::string& aSettingPath, std::vector<std::string>& aSettingValue, SettingGetPolicy aSettingGetPolicy) const;

        // Convenience methods
        bool isStringSettingExist(const std::string& aSettingPath) const;
        bool isIntSettingExist(const std::string& aSettingPath) const;
        bool isUintSettingExist(const std::string& aSettingPath) const;
        bool isBoolSettingExist(const std::string& aSettingPath) const;
        bool isStringArraySettingExist(const std::string& aSettingPath) const;
        bool isGroupSettingExist(const std::string& aSettingPath) const;

        /**
          Check if the setting is list and, if so, retrieve the number of items in it

          @param[in] aSettingPath path to the list/array setting to test
          @param[out] aLength If the function return true, contains a number of items in list/array
          @return whether the given option represents list
          @throw LibConfigWrapperError
        */
        bool getListInfo(const std::string& aSettingPath, size_t& aLength) const;

        /**
          Change or add a value to the config file

          @param[in] aSettingPath path of config setting
          @param[in] aSettingValue value of config setting
          @param[in] aSettingGetPolicy controls the function behavior when the setting does not exist
          @throw LibConfigWrapperError
        */
        void setValue(const std::string& aSettingPath, const char* aSettingValue, SettingSetPolicy aSettingSetPolicy);
        void setValue(const std::string& aSettingPath, const std::string& aSettingValue, SettingSetPolicy aSettingSetPolicy);
        void setValue(const std::string& aSettingPath, int aSettingValue, SettingSetPolicy aSettingSetPolicy);
        void setValue(const std::string& aSettingPath, bool aSettingValue, SettingSetPolicy aSettingSetPolicy);
        void setValue(const std::string& aSettingPath, const std::vector<std::string>& aSettingValue, SettingSetPolicy aSettingSetPolicy);

        /**
          Remove a given setting, provided it exists, from the config file

          @param[in] aSettingPath String containing path of config setting to remove
          @throw LibConfigWrapperError on error
        */
        void removeSetting(const std::string& aSettingPath);

        std::string getConfigFilePath() const;

    private:
        std::string theConfigFilePath;

    };
}
