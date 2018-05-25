#ifndef TA_INTERNET_EXPLORER_H
#define TA_INTERNET_EXPLORER_H

#ifndef _WIN32
#error "Only Windows platform is supported"
#endif

#include <string>

namespace ta
{
    namespace InternetExplorer
    {
        bool isInstalled();

        struct Version
        {
            unsigned long major;
            unsigned long minor;
            unsigned long subminor;
            unsigned long revision;
        };

        // Exceptions: throw std::runtime_error on error e.g. if IE is not installed
        Version getVersion();

        // Exceptions: throw std::runtime_error on error e.g. if IE is not installed
        std::string getInstallDir();

        enum ProtectedMode
        {
            protectedModeOn,          // IE protected mode is On
            protectedModeOff,         // IE protected mode is Off. @note turning off UAC disables protected mode for all users of the computer no matter if IE has "Protected mode" tickbox set
            protectedModeNotIeProcess // The function is not called from IE process
        };
        // throw std::runtime_error
        ProtectedMode getProtectedMode();

        // throws std::runtime_error if protected mode is not On
        std::string getProtectedModeTempDir();

        // Restarts ieuser.exe process.
        // This is normally needed to "clean cache" and allow IE to pickup some broker-related Registry changes.
        // throws std::runtime_error
        void restartIeUser();
    }
}

#endif
