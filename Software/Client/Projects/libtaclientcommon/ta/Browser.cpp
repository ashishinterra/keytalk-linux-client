//----------------------------------------------------------------------------
//
//  Description : Browser utilites implementation
//
//----------------------------------------------------------------------------
#include "Browser.h"
#include "InternetExplorer.h"
#include "ta/registry.h"
#include "ta/scopedresource.hpp"
#include "ta/logger.h"
#include "ta/assert.h"
#include "ta/utils.h"
#include "ta/common.h"
#include "boost/static_assert.hpp"
#include "boost/algorithm/string.hpp"
#include <cstdlib>
#ifdef _WIN32
# include <windows.h>
# include <shellapi.h>
# include "winhttp.h"
#endif

namespace ta
{
    namespace Browser
    {
        using std::string;

        static const char* BrowserTypesStrings[] = {"Unknown", "IE"};

        void open(const string& anHttpUrl, const BrowserType aBrowserType)
        {
            if (aBrowserType == Unknown)
            {
                TA_THROW_MSG(BrowserNotSupportedError, "Cannot deduce browser type to open URL");
            }
#ifdef _WIN32
            HINSTANCE myBrowserNandle = NULL;
            switch (aBrowserType)
            {
            case IE:
            {
                if (!InternetExplorer::isInstalled())
                {
                    TA_THROW_MSG(BrowserLaunchError, "Internet Explorer is not installed");
                }
                myBrowserNandle = ::ShellExecute(NULL, "open", (InternetExplorer::getInstallDir() + getDirSep() + "iexplore.exe").c_str(), anHttpUrl.c_str(), NULL, SW_SHOWNORMAL);
                break;
            }
            default:
                TA_THROW_MSG(BrowserNotSupportedError, boost::format("Requested browser type: %d") % aBrowserType);
            }
            const size_t myResult = (size_t)myBrowserNandle;
            if (!(myResult > 32))
            {
                TA_THROW_MSG(BrowserLaunchError, boost::format("Failed to launch browser type %d. ::ShellExecute returned %u") % aBrowserType % (unsigned short)myResult);
            }
#else
            TA_THROW_MSG(BrowserNotSupportedError, "Requested browser type: %d", aBrowserType);
#endif
        }

        BrowserType getDefault()
        {
#ifdef _WIN32
            string myAppName;
            try
            {
                Registry::read(HKEY_CURRENT_USER, "Software\\Clients\\StartMenuInternet","", myAppName);
            }
            catch (RegistryError&)
            {
                try
                {
                    Registry::read(HKEY_LOCAL_MACHINE, "Software\\Clients\\StartMenuInternet","", myAppName);
                }
                catch (RegistryError&)
                {
                    return Unknown;
                }
            }
            if (boost::iequals(myAppName, "IExplore.exe"))
            {
                return IE;
            }
            return Unknown;
#else
#error "Not implemented"
#endif
        }

        string toString(const BrowserType aBrowserType)
        {
            return BrowserTypesStrings[aBrowserType];
        }

    }
}
