#include "sysinfo.h"

#ifdef _WIN32
#include "ta/registry.h"
#include "ta/osinfoutils.h"
#else
#include "ta/linuxhwutils.h"
#endif //_WIN32
#include "ta/version.h"
#include "ta/timeutils.h"
#include "ta/logger.h"
#include "ta/process.h"
#include "ta/common.h"

#ifdef _WIN32
#include <windows.h>

#define _WIN32_DCOM
#include <comdef.h>
#include <Wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")
#endif

#include <sstream>
#include <iostream>

using std::string;

namespace ta
{
    namespace SysInfo
    {
        //
        // Private API
        //

        namespace
        {
#ifdef _WIN32

            class ScopedComInitializer
            {
            public:
                ScopedComInitializer()
                    : isComInitialized(false)
                {
                    HRESULT hres = ::CoInitialize(NULL);
                    if (SUCCEEDED(hres))
                    {
                        isComInitialized = true;
                    }
                    else
                    {
                        if (hres != RPC_E_CHANGED_MODE)
                        {
                            TA_THROW_MSG(std::runtime_error, boost::format("Failed to initialize COM library. HRESULT 0x%x") % hres);
                        }
                    }
                }
                ~ScopedComInitializer()
                {
                    if (isComInitialized)
                    {
                        ::CoUninitialize();
                    }
                }
            private:
                bool isComInitialized;
            };

            string getBIOSSerialNumberFromWmicApi()
            {
                FUNCLOG;

                // The code is adapted from http://msdn.microsoft.com/en-us/library/aa390423(v=vs.85).aspx

                ScopedComInitializer comInitializer;

                /*HRESULT hres =  CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE,NULL);
                if (FAILED(hres) && hres != RPC_E_TOO_LATE)
                {
                    TA_THROW_MSG(std::runtime_error, boost::format("Failed to initialize security. HRESULT 0x%x") % hres);
                }*/

                // Obtain the initial locator to WMI
                IWbemLocator* pLoc = NULL;
                HRESULT hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*) &pLoc);
                if (FAILED(hres))
                {
                    TA_THROW_MSG(std::runtime_error, boost::format("Failed to create IWbemLocator object. HRESULT 0x%x") % hres);
                }
                if (!pLoc)
                {
                    TA_THROW_MSG(std::runtime_error, "WMI locator is NULL?!");
                }

                // Connect to WMI with current user
                IWbemServices* pSvc = NULL;
                hres = pLoc->ConnectServer(	_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0,	NULL, 0, 0,	&pSvc);

                if (FAILED(hres))
                {
                    pLoc->Release();
                    TA_THROW_MSG(std::runtime_error, boost::format("Failed to connect to WMI. HRESULT 0x%x") % hres);
                }
                if (!pSvc)
                {
                    pLoc->Release();
                    TA_THROW_MSG(std::runtime_error, "WMI connection is NULL?!");
                }

                // Set security levels on the proxy
                hres = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,	NULL, RPC_C_AUTHN_LEVEL_CALL,
                                         RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

                if (FAILED(hres))
                {
                    pSvc->Release();
                    pLoc->Release();
                    TA_THROW_MSG(std::runtime_error, boost::format("Could not set proxy blanket. HRESULT 0x%x") % hres);
                }

                // Get enumerator
                IEnumWbemClassObject* pEnumerator = NULL;
                hres = pSvc->ExecQuery(bstr_t("WQL"), bstr_t("SELECT * FROM Win32_BIOS"),
                                       WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                                       NULL,&pEnumerator);
                if (FAILED(hres))
                {
                    pSvc->Release();
                    pLoc->Release();
                    TA_THROW_MSG(std::runtime_error, boost::format("Query for Win32_BIOS failed. HRESULT 0x%x") % hres);
                }
                if (!pEnumerator)
                {
                    pSvc->Release();
                    pLoc->Release();
                    TA_THROW_MSG(std::runtime_error, "Query for Win32_BIOS succeeded by enumerator is NULL?!");
                }

                IWbemClassObject* pclsObj;
                ULONG uNumObjectsReturned = 0;
                HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uNumObjectsReturned);
                if (FAILED(hres))
                {
                    pEnumerator->Release();
                    pSvc->Release();
                    pLoc->Release();
                    TA_THROW_MSG(std::runtime_error, boost::format("Fetch for Win32_BIOS failed. HRESULT 0x%x") % hres);
                }
                if (uNumObjectsReturned == 0 || !pclsObj)
                {
                    pEnumerator->Release();
                    pSvc->Release();
                    pLoc->Release();
                    TA_THROW_MSG(std::runtime_error, "Win32_BIOS object not found");
                }

                VARIANT vtBiosSerialProperty;
                hr = pclsObj->Get(L"SerialNumber", 0, &vtBiosSerialProperty, 0, 0);
                if (FAILED(hres))
                {
                    pclsObj->Release();
                    pEnumerator->Release();
                    pSvc->Release();
                    pLoc->Release();
                    TA_THROW_MSG(std::runtime_error, boost::format("Fetch for Win32_BIOS::SerialNumber failed. HRESULT 0x%x") % hres);
                }
                //@note to be entirely correct we should have used wstring i.o. string, but I would be surprised this is applicable for BIOS serial
                const string myBiosSerial = (const char*)_bstr_t(V_BSTR(&vtBiosSerialProperty));
                VariantClear(&vtBiosSerialProperty);

                pclsObj->Release();
                pEnumerator->Release();
                pSvc->Release();
                pLoc->Release();

                return myBiosSerial;
            }


            string getBIOSSerialNumberFromWmicShell()
            {
                FUNCLOG;
                string commandStdout;
                string commandStderr;

                int executionResultCode = ta::Process::shellExecSync("wmic bios get serialnumber", commandStdout, commandStderr);

                if (executionResultCode != 0)
                {
                    TA_THROW_MSG(std::runtime_error,
                                 boost::format("Failed to execute 'wmic' to get BIOS serial number. Command finished with code %d. Stderr: '%s'") % executionResultCode % commandStderr);
                }

                std::istringstream textStream(commandStdout);
                string biosSerialNumberText;
                if ( !textStream.eof() )
                {
                    // Skip first line
                    string propertyText;
                    std::getline(textStream, propertyText);
                    if ( !textStream.eof() )
                    {
                        getline(textStream, biosSerialNumberText);
                        boost::trim(biosSerialNumberText);
                    }
                }
                if ( biosSerialNumberText.empty() )
                {
                    TA_THROW_MSG(std::runtime_error, "Could not get BIOS serial number.");
                }

                return biosSerialNumberText;
            }
#endif //_WIN32

            size_t getDefPasswordExpirationDays()
            {
#ifdef _WIN32
                size_t myDefPasswordExpirationDays = 5;
                try
                {
                    if (version::parse(OsInfoUtils::getVersion().ver) >= version::Version(6,1))
                    {
                        myDefPasswordExpirationDays = 5; // Win7, Win2008 R2 and higher
                    }
                    else
                    {
                        myDefPasswordExpirationDays = 14;
                    }
                }
                catch (std::exception& e)
                {
                    WARNLOG(boost::format("Failed to determine OS version, falling back to default password expiration to %d days. %s") % myDefPasswordExpirationDays % e.what());
                }
#else
                const size_t myDefPasswordExpirationDays = 7;
#endif //_WIN32
                return myDefPasswordExpirationDays;
            }

        }// private API

        //
        // Public API
        //
#ifdef _WIN32
        bool isWow64()
        {
            typedef BOOL (APIENTRY *LPFN_ISWOW64PROCESS) (HANDLE hProcess,PBOOL Wow64Process);
            LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS)::GetProcAddress(GetModuleHandle("kernel32"),"IsWow64Process");
            if (!fnIsWow64Process)
                return false;
            BOOL bIsWow64 = FALSE;
            if (!fnIsWow64Process(::GetCurrentProcess(),&bIsWow64))
                TA_THROW_MSG(std::runtime_error, boost::format("GetProcAddress(IsWow64Process) failed. Last Error: %d") % ::GetLastError());
            return !!bIsWow64;
        }

        string getWinProductId()
        {
            string myOsProductId;
            Registry::read(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows NT\\CurrentVersion", "ProductId", myOsProductId, isWow64());
            return myOsProductId;
        }

        string getWinRegisteredOwner()
        {
            string myOsProductOwner;
            Registry::read(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows NT\\CurrentVersion", "RegisteredOwner", myOsProductOwner, isWow64());
            return myOsProductOwner;
        }

        string getBIOSSerialNumber()
        {
            // First try retrieving serial from WMIC API. This is way faster than retrieving it from WMIC shell.
            try
            {
                return getBIOSSerialNumberFromWmicApi();
            }
            catch (std::exception& e)
            {
                WARNLOG(boost::format("Failed to retrieve BIOS serial from WMIC API. %s. Falling back to WMIC shell") % e.what());
                try
                {
                    return getBIOSSerialNumberFromWmicShell();
                }
                catch (std::exception& e)
                {
                    WARNLOG(boost::format("Failed to retrieve BIOS serial from WMIC Shell. %s.") % e.what());
                    throw;
                }
            }
        }
#endif // _WIN32

        string getSerialNumber()
        {
#ifdef _WIN32
            static const string mySerial = getBIOSSerialNumber();// static for performance reasons
            return mySerial;
#else
            return ta::linuxhwutils::getSerialNum();
#endif // _WIN32
        }


        string getHardwareDescription()
        {
            string osName = "<unknown OS>";
            string serialNumber = "<unknown S/N>";

#ifdef _WIN32

            try
            {
                osName = OsInfoUtils::getVersion().name;
            }
            catch (...)
            {}

            try
            {
                serialNumber = getSerialNumber();
            }
            catch (...)
            {}
#else
            string myStdErr;

            if (ta::Process::shellExecSync("uname", osName, myStdErr) == 0)
            {
                // remove trailing endlines
                boost::trim(osName);
            }

            try
            {
                serialNumber = ta::linuxhwutils::getSerialNum();
            }
            catch (...)
            {}
#endif

            return osName + ", BIOS s/n " + serialNumber;

        }

        bool isUserPasswordExpiring(size_t aRemainingPasswordValidityInSeconds)
        {
            size_t myConfiguredPasswordExpirationInSeconds = getDefPasswordExpirationDays() * ta::TimeUtils::SecondsInDay;
#ifdef _WIN32
            static const string myPwdExpiryTimeRegKey("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon");
            static const string myPwdExpiryTimeRegItem("PasswordExpiryWarning");

            if (Registry::isExist(HKEY_LOCAL_MACHINE, myPwdExpiryTimeRegKey, myPwdExpiryTimeRegItem))
            {
                DWORD myConfiguredPasswordExpirationInDays = 0;
                ta::Registry::read(HKEY_LOCAL_MACHINE, myPwdExpiryTimeRegKey, myPwdExpiryTimeRegItem, myConfiguredPasswordExpirationInDays);
                myConfiguredPasswordExpirationInSeconds = myConfiguredPasswordExpirationInDays * ta::TimeUtils::SecondsInDay;
            }
#endif // _WIN32
            return aRemainingPasswordValidityInSeconds < myConfiguredPasswordExpirationInSeconds;
        }

    } // SysInfo
} // ta
