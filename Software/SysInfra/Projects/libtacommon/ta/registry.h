//----------------------------------------------------------------------------
//
//  Description : Windows Registry utility
//
//----------------------------------------------------------------------------
#pragma once

#ifndef _WIN32
#error "Only Windows platform is supported"
#endif

#include <stdexcept>
#include <string>
#include <windows.h>

namespace ta
{
    struct RegistryError : std::runtime_error
    {
        explicit RegistryError(const std::string& aMessage = "") : std::runtime_error(aMessage) {}
    };

    namespace Registry
    {
        bool isExist(HKEY aBaseKey, const std::string& aKey, const std::string& aValName, bool anIsKey64bit = false);

        // @throw RegistryError
        void read(HKEY aBaseKey, const std::string& aKey, const std::string& aValName, std::string& aValVal, bool anIsKey64bit = false);

        // @throw RegistryError
        void read(HKEY aBaseKey, const std::string& aKey, const std::string& aValName, DWORD& aValVal, bool anIsKey64bit = false);

        // @throw RegistryError
        void write(HKEY aBaseKey, const std::string& aKey, const std::string& aValName, const std::string& aValVal, bool anIsKey64bit = false);
    }
}
