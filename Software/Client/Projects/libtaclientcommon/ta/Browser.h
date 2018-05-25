#pragma once

#include <stdexcept>
#include <string>
#include <vector>

namespace ta
{
    struct BrowserLaunchError : std::runtime_error
    {
        explicit BrowserLaunchError(const std::string& aMessage = "")	: std::runtime_error(aMessage) {}
    };
    struct BrowserNotSupportedError : BrowserLaunchError
    {
        explicit BrowserNotSupportedError(const std::string& aMessage = "")	: BrowserLaunchError(aMessage) {}
    };

    namespace NetUtils  { struct RemoteAddress; }

    namespace Browser
    {
        enum BrowserType
        {
            Unknown, // Unknown browser
            IE      // Internet Explorer
        };
        std::string toString(const BrowserType aBrowserType);

        //
        // Abstract: open a specified anHttpUrl in a browser specified by the aBrowserType argument
        //
        // Exceptions: throw BrowserLaunchError, BrowserNotSupportedError on error
        //
        void open(const std::string& anHttpUrl, const BrowserType aBrowserType);

        //
        // Abstract: retrieve a default browser type.
        //
        // Errors: return Unknown if the default browser cannot be deduced
        //
        BrowserType getDefault();
    }
}
