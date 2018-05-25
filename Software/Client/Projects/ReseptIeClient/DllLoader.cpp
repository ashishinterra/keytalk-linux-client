//----------------------------------------------------------------------------
//
//  Name          DllLoader.cpp
//  Description : Resept IE BHO application entry point
//
//----------------------------------------------------------------------------
#include "stdafx.h"
#include "resource.h"
#include "rclient/RClientAppCommon.h"
#include "rclient/Common.h"
#include "ta/InternetExplorer.h"
#include "ta/common.h"
#include <comdef.h>
#include <string>
#include <vector>

//@note Keep all changes regarding BHO/Broker registration in sync with ReseptClientInstaller wix script

[ module(dll,
         uuid = RESEPTBHO_TYPELIBID,
         name = "ReseptIeBHO",
         helpstring = "ReseptIeBHO 1.0 Type Library")]
class CIeDllLoader
{
public:
    //
    // 1. Registers the library as COM inproc server
    // 2. Registers the library as BHO (HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\Browser Helper Objects)
    // 3. Elevates ReseptIEBroker and ReseptConfigManager tools to medium integrity level (HKLM\SOFTWARE\Microsoft\Internet Explorer\Low Rights\ElevationPolicy)
    // 4. Restart IE broker (ieuser.exe) to make it re-read the elevation information from the Registry
    //
    HRESULT DllRegisterServer(BOOL bRegTypeLib = TRUE) throw()
    {
        HRESULT hr = __super::DllRegisterServer(bRegTypeLib);
        if (FAILED(hr))
            return hr;
        hr = __super::UpdateRegistryFromResource(IDR_REGISTRY1, TRUE);
        if (FAILED(hr))
            return hr;
        try { ta::InternetExplorer::restartIeUser(); }
        catch (...) { return E_FAIL;}
        return S_OK;
    }

    //
    // Clean-up COM, BHO and IE broker registry entries
    //
    HRESULT DllUnregisterServer(BOOL bUnRegTypeLib = TRUE) throw()
    {
        HRESULT hr = __super::DllUnregisterServer(bUnRegTypeLib);
        if (SUCCEEDED(hr))
            hr = __super::UpdateRegistryFromResource(IDR_REGISTRY1, FALSE);
        return hr;
    }

    //
    //  Add parameter expansion mappings to be processed by the registrar script and used to register the IE broker
    //  The parameter will be expanded to the directory containing our library
    //
    HRESULT AddCommonRGSReplacements(IRegistrarBase* pRegistrar) throw()
    {
        HRESULT hr = __super::AddCommonRGSReplacements(pRegistrar);
        if (SUCCEEDED(hr))
        {
            char szModule[MAX_PATH+1] = {};
            if (!::GetModuleFileName(_AtlComModule.m_hInstTypeLib, szModule, sizeof(szModule)-1) || !(*szModule))
                return E_FAIL;
            std::string myModuleName(szModule);
            std::string::size_type myPos = myModuleName.find_last_of('\\');
            if (myPos == std::string::npos)
                return E_FAIL;
            std::string myModuleDir = myModuleName.substr(0, myPos);
            pRegistrar->AddReplacement(L"PRODUCT_NAME", bstr_t(resept::ProductName.c_str()).copy());
            pRegistrar->AddReplacement(L"BROKER_NAME", bstr_t(rclient::ReseptIeBroker.c_str()).copy());
            pRegistrar->AddReplacement(L"RESEPT_INSTALL_DIR", bstr_t(myModuleDir.c_str()).copy());

        }
        return hr;
    }
};
