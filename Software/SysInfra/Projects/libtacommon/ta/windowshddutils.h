#pragma once

#ifndef _WIN32
# error "Windows platform required!"
#endif

#include <string>

namespace ta
{
    namespace windhowshddutils
    {
        bool getPrimaryHardDriveSerial(std::string& aSerial);

    }
}
