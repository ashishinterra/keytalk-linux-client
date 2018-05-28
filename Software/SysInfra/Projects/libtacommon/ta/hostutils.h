#pragma once

#ifdef _WIN32
#error "Unsupported platform"
#else
#include <string>

namespace ta
{
    namespace HostUtils
    {
        namespace hostname
        {
            std::string get();
            void set(const std::string& aHostName);
        }

    }
}
#endif
