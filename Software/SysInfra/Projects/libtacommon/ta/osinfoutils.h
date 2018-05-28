#pragma once

#include <string>

namespace ta
{
    namespace OsInfoUtils
    {
        struct Version
        {
            std::string name;
            std::string ver;
        };

        /**
          Retrieve OS version

          @return OS version number
          @throw std::runtime_error on error
        */
        Version getVersion();

        // Return short platform name such as "Windows", "Linux" or "OpenBSD"
        //@nothrow
        std::string getPlatformShortName();

        // The best-effort judgement that we run Raspberry Pi
        bool isRaspberryPi();
    }
}
