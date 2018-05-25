#pragma once

#include "rclient/RClientAppCommon.h"
#include "ReseptProxy.h"
#include "ta/opensslapp.h"

#include "stdafx.h"
#include <string>
#include <memory>
#include <comutil.h>
#include "resource.h"

//@note Keep all changes regarding BHO/Broker registration in sync with ReseptClientInstaller wix script

[
    object,
    uuid(IRESEPTLOADER_IID),
    dual,
    pointer_default(unique)
]
__interface IReseptLoader : IDispatch
{
};

//@note class name matters when registering it in the Windows registry
// Disabled warning C4680: 'class' : coclass does not specify a default interface
// According to http://msdn.microsoft.com/en-us/library/3t4k26d0.aspx:
// "If no default interface is specified, the first occurrence of a nonsource interface is used as the default."
// Explicitly specifying a default interface seems to fail, therefore the warning has been locally disabled.
#pragma warning ( disable: 4680 )
[
    coclass,
    threading(apartment),
    aggregatable(never),
    version(1.0),
    uuid(RESEPTBHO_CLSID)
]
class ATL_NO_VTABLE KeyTalkIeClient : public IObjectWithSiteImpl<KeyTalkIeClient>,
    public IDispatchImpl<IReseptLoader, &__uuidof(IReseptLoader)>
{
public:
    KeyTalkIeClient();
    virtual ~KeyTalkIeClient();

    DECLARE_PROTECT_FINAL_CONSTRUCT()
    HRESULT FinalConstruct();
    void FinalRelease();
    // IObjectWithSite method
    STDMETHOD(SetSite)(IUnknown* anUnkSitePtr);
    // IDispatch method
    STDMETHOD(Invoke)(DISPID aDispidMember,REFIID aRiid, LCID aLcid, WORD aFlags, DISPPARAMS* aDispParamsPtr,
                      VARIANT* aVarResultPtr, EXCEPINFO* aExcepInfoPtr, UINT* anArgErrPtr);
private:
    enum ConnectType { ConnType_Advise, ConnType_Unadvise };
    HRESULT ConnectBho2Ie(ConnectType aConnectType);
    void redirect(const std::string& aUrl, DISPPARAMS* aDispParamsPtr);
    static bool checkReseptCustomized();
    static void showWarningMsg(const std::string& aMsg);

private:
    static const std::string BlankUrl;
    DWORD theBrowserCookie;
    CComQIPtr<IWebBrowser2, &IID_IWebBrowser2> theWebBrowser;
    bool theIsReseptInialized;
    bool theAuthenticatedWithIeClient;
    std::auto_ptr<ReseptProxy::LoggerInitializer> theLoggerInitializer;
    std::auto_ptr<ta::OpenSSLApp> theOpenSSLAppPtr;
};
