#pragma once

#include <string>
#include <vector>
#include "rclient/Common.h"

namespace LoadSettingsBL
{
    typedef bool (*ConfirmationPromptCallback)(const std::string& aMsgText, void* aCookie);

    // Load RCCD from URL into the memory buffer
    // The function only loads RCCD in the given memory buffer without installing it. Call installRccd() to install the loaded RCCD.
    //@nothrow
    //@return success flag. If return value is false, anErrorMsg contains user-oriented error message.
    //                      If the function return true, anErrorMsg is not affected
    bool loadRccdFromUrl(const std::string& anUrl,
                         std::vector<unsigned char>& aBlob,
                         std::string& anErrorMsg);

    // Load RCCD from file into the memory buffer
    // The function only loads RCCD in the given memory buffer without installing it. Call installRccd() to install the loaded RCCD.
    //@nothrow
    //@return success flag. If return value is false, anErrorMsg contains user-oriented error message.
    //                      If the function return true, anErrorMsg is not affected
    bool loadRccdFromFile(const std::string& aPath, std::vector<unsigned char>& aBlob, std::string& anErrorMsg);

    //
    // Installs RCCD from the memory buffer. The RCCD memory is typically created with loadRccdFromUrl() or loadRccdFromFile()
    //
    //@nothrow
    //@return success flag. If return value is false, anErrorMsg contains user-oriented error message.
    //                      If the function return true, anErrorMsg is not affected
    bool installRccd(const std::vector<unsigned char>& anRccdBlob,
                     const std::string& anUrlHint,
                     ConfirmationPromptCallback aConfirmationPromptCallback,
                     void* aConfirmationPromptCallbackCookie,
                     std::string& anErrorMsg);

#ifdef _WIN32
    //
    // Uninstalls user settings for all providers having name the same as the given name case-insensitive.
    // Only user settings can be uninstalled, if master settings exist for the given provider the function will fail.
    //
    //@nothrow
    //@return success flag. If return value is false, anErrorMsg contains user-oriented error message.
    //                      If the function return true, anErrorMsg is not affected
    bool uninstallUserSettings(const std::string& aProvider, std::string& anErrorMsg);
#endif
}
