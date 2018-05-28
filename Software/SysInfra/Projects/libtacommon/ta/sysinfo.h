#pragma once

#include <string>

namespace ta
{
    namespace SysInfo
    {
#ifdef _WIN32
        /**
         Determines whether the current process is running under WOW64.

         @throw std::runtime_error
         */
        bool isWow64();

        /**
          Retrieves Windows product ID

          @throw std::runtime_error
          */
        std::string getWinProductId();

        /**
          Retrieves Windows registered owner

          @throw std::runtime_error
          */
        std::string getWinRegisteredOwner();
#endif //_WIN32
        /**
          Retrieves the serial number from BIOS. When BIOS is not available (e.g. on RaspberryPi) return CPU serial number

          @throw std::runtime_error
          */
        std::string getSerialNumber();


        /**
          Get the unique hardware representation text

          @nothrow
        */
        std::string getHardwareDescription();

        bool isUserPasswordExpiring(size_t aRemainingPasswordValidityInSeconds);
    }
}
