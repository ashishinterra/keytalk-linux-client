#pragma once

# ifdef _WIN32

#include <string>

namespace ta
{
    namespace OsUserInfo
    {
        /**
         Retrieve Security Identifier (SID associated with the user of the current process

         @throw std::runtime_error
         */
        std::string getCurentUserSID();
    }
}
# endif
