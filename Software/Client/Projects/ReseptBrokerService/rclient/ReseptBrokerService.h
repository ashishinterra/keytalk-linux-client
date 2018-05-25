#pragma once

#include "rclient/ContentConfig.h"
#include "ta/utils.h"
#include <string>
#include <vector>
#include <sstream>

namespace rclient
{
    namespace ReseptBrokerService
    {
        enum RequestType
        {
            requestInstallSettings,
            requestUninstallSettings
        };
        struct InstallSettingsRequest
        {
            InstallSettingsRequest() {}
            InstallSettingsRequest(const rclient::ContentConfig::Config& aContentConfig, const std::string& aUserConfigPath): contentConfig(aContentConfig), userConfigPath(aUserConfigPath) {};
            rclient::ContentConfig::Config contentConfig;
            std::string userConfigPath;
        };

        struct UninstallSettingsRequest
        {
            UninstallSettingsRequest() {}
            UninstallSettingsRequest(const std::string& aProvider, const std::string& aUserConfigPath): provider(aProvider), userConfigPath(aUserConfigPath) {};
            std::string provider;
            std::string userConfigPath;
        };

        enum ResponseStatus
        {
            responseStatusOk,
            responseStatusError,
            responseStatusUserError,
            responseStatusConfirmation
        };
        struct Response
        {
            Response(): status(responseStatusOk) {}
            Response(ResponseStatus aStatus, const std::string& aText = ""): status(aStatus), text(aText) {}
            ResponseStatus status;
            std::string text; // if status is statusError, text is a developer-oriented error message. if status is statusConfirmation, text is a user confirmation prompt
        };
    }
}

namespace boost
{
    namespace serialization
    {
        template<class Archive>
        void serialize(Archive& ar, rclient::ReseptBrokerService::InstallSettingsRequest& aRequest, const unsigned int UNUSED(version))
        {
            ar & aRequest.contentConfig;
            ar & aRequest.userConfigPath;
        }

        template<class Archive>
        void serialize(Archive& ar, rclient::ReseptBrokerService::UninstallSettingsRequest& aRequest, const unsigned int UNUSED(version))
        {
            ar & aRequest.provider;
            ar & aRequest.userConfigPath;
        }

        template<class Archive>
        void serialize(Archive& ar, rclient::ReseptBrokerService::Response& anResponse, const unsigned int UNUSED(version))
        {
            ar & anResponse.status;
            ar & anResponse.text;
        }
    }
}
