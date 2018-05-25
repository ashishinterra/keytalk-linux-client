#include "ReseptIeClient.h"
#include "BrokerProxy.h"
#include "rclient/CommonUtils.h"
#include "rclient/Settings.h"
#include "rclient/NativeCertStore.h"
#include "rclient/Common.h"
#include "resept/common.h"
#include "ta/assert.h"
#include "ta/url.h"
#include "ta/encodingutils.h"
#include "ta/utils.h"
#include "ta/common.h"
#include <vector>
#include <ExDispID.h>

using namespace ta;
using std::string;

const string KeyTalkIeClient::BlankUrl = "about:blank";

KeyTalkIeClient::KeyTalkIeClient()
    :  theBrowserCookie(0)
    ,theIsReseptInialized(false)
    ,theAuthenticatedWithIeClient(false)
{
    if (!checkReseptCustomized())
    {
        showWarningMsg(resept::ProductName + " Installation has not been customized. Please customize " + resept::ProductName + " by running " + resept::ProductName + " Configuration Manager.");
        return;
    }
    try
    {
        theLoggerInitializer.reset(new ReseptProxy::LoggerInitializer());
        theOpenSSLAppPtr.reset(new OpenSSLApp());
        ReseptProxy::logDebug("KeyTalkIeClient c'tor");
    }
    catch (rclient::LoggerInitError& e)
    {
        // we throw technical inforamtion to a user since this is the only way to report it somewhere (logger broken)
        showWarningMsg(str(boost::format("Failed to initialize logger. %s. Please contact %s administrator.") % e.what() % resept::ProductName));
        return;
    }
    catch (BrokerError& e)
    {
        // we throw technical inforamtion to a user since this is the only way to report it somewhere (logger is pronanly not accessible)
        showWarningMsg(str(boost::format("Failed to communicate with IE broker. %s. Please contact %s administrator.") % e.what() % resept::ProductName));
        ReseptProxy::logError(e.what());
        return;
    }
    catch (std::exception& e)
    {
        showWarningMsg("Failed to initialize crypto subsystem. Please contact " + resept::ProductName + " administrator.");
        ReseptProxy::logError(e.what());
        return;
    }
    theIsReseptInialized = true;
}


KeyTalkIeClient::~KeyTalkIeClient()
{
    if (!theIsReseptInialized)
        return;

    ReseptProxy::logDebug("KeyTalkIeClient d'tor");
    try
    {
        if (rclient::Settings::isCleanupUserCert())
        {
            ReseptProxy::logDebug("Cleaning up all " + resept::ProductName + " user certificates");
            ReseptProxy::deleteAllReseptUserCerts();
        }
    }
    catch (std::exception& e)
    {
        ReseptProxy::logWarn(e.what());
    }

}

bool KeyTalkIeClient::checkReseptCustomized()
{
    try
    {
        //@note we do not start CustomizationTool from IE like we do it for ReseptDesktopClient if RESEPT is not customized
        // because it will complicate the things concerning launch permissions when IE protected mode is on.
        // Instead we simply aske the user to do this by hand.
        // Anyway such a behavior seems even more logical from the POV of IE user.
        return rclient::Settings::isCustomized();
    }
    catch (std::exception& e)
    {
        showWarningMsg(e.what());
        return false;
    }
}

HRESULT KeyTalkIeClient::FinalConstruct()
{
    return S_OK;
}

void KeyTalkIeClient::FinalRelease()
{
}

STDMETHODIMP KeyTalkIeClient::SetSite(IUnknown* anUnkSitePtr)
{
    if (!theIsReseptInialized)
        return S_OK;
    if (!anUnkSitePtr)
    {
        ReseptProxy::logWarn("anUnkSitePtr is NULL");
        return S_OK;
    }
    // Query IWebBrowser2
    theWebBrowser = anUnkSitePtr;
    if (!theWebBrowser)
    {
        ReseptProxy::logWarn("QI for IWebBrowser2 failed");
        return S_OK;
    }
    HRESULT hr = ConnectBho2Ie(ConnType_Advise);
    if (FAILED(hr))
    {
        ReseptProxy::logWarn("Failure sinking events from IWebBrowser2");
        return S_OK;
    }
    return S_OK;
}

HRESULT KeyTalkIeClient::ConnectBho2Ie(ConnectType aConnectType)
{
    if (aConnectType == ConnType_Unadvise && theBrowserCookie == 0)
        return S_OK;
    if (!theIsReseptInialized)
        return E_FAIL;
    if (!theWebBrowser)
    {
        ReseptProxy::logError("theWebBrowser is NULL");
        return E_FAIL;
    }
    HRESULT myRetVal = E_FAIL;
    if (aConnectType == ConnType_Advise)
    {
        if (theBrowserCookie != 0)
        {
            ReseptProxy::logError("Trying to connect, but cookie is not 0");
            return E_FAIL;
        }
        myRetVal = AtlAdvise (theWebBrowser, (IDispatch*)this, __uuidof(DWebBrowserEvents2), &theBrowserCookie);
    }
    else
    {
        myRetVal = AtlUnadvise(theWebBrowser, __uuidof(DWebBrowserEvents2), theBrowserCookie);
        theBrowserCookie = 0;
    }
    return myRetVal;
}

STDMETHODIMP KeyTalkIeClient::Invoke(DISPID aDispidMember, REFIID UNUSED(aRiid), LCID UNUSED(aLcid), WORD UNUSED(aFlags),
                                     DISPPARAMS* aDispParamsPtr, VARIANT* UNUSED(aVarResultPtr),
                                     EXCEPINFO*  UNUSED(aExcepInfoPtr), UINT* UNUSED(anArgErrPtr))
{
    USES_CONVERSION;

    if (!theIsReseptInialized || !aDispParamsPtr)
    {
        return E_INVALIDARG;
    }

    switch (aDispidMember)
    {
    case DISPID_BEFORENAVIGATE2:
    {
        _bstr_t bstrUrl(aDispParamsPtr->rgvarg[5].pvarVal->bstrVal);
        const string myRequestedUri((const char*)bstrUrl);

        if (myRequestedUri == BlankUrl)
        {
            ReseptProxy::logDebug("Blank page requested, just let it go");
            return S_OK;
        }

        try
        {
            if (theAuthenticatedWithIeClient)
            {
                // because the URL is authenticated by IE client, provider and service are already selected, so we can directly check the cert validity
                if (ReseptProxy::validateReseptUserCert() > 0)
                {
                    ReseptProxy::logDebug(str(boost::format("Found valid user certificate for URI %s and service %s. Let the URL go since it is already authenticated by IE client") % myRequestedUri % rclient::Settings::getLatestService()));
                    return S_OK;
                }
                ReseptProxy::logDebug("No valid user certificates found for the previously authenticated URI " + myRequestedUri + " and service " + rclient::Settings::getLatestService());
                theAuthenticatedWithIeClient = false;
            }
            else
            {
                ReseptProxy::logDebug("URI " + myRequestedUri + " is not yet authenticated by IE client");
            }

            std::vector<std::pair<string, string> > myProviderServicePairs;
            myProviderServicePairs = rclient::Settings::getProviderServiceForRequestedUri(myRequestedUri, rclient::isServiceUri);
            if (myProviderServicePairs.empty())
            {
                ReseptProxy::logDebug(str(boost::format("URL (%s) is not service URI. Ignoring...") % myRequestedUri));
                return S_OK;
            }

            string myUri2Go;
            if (!ReseptProxy::loadBrowserReseptClientAuthUI(myProviderServicePairs, myRequestedUri, myUri2Go))
            {
                // Do not pop-up message box since the user could just cancelled the authentication
                // @todo separate when the dialog is needed and when not?
                ReseptProxy::logError("loadBrowserReseptClientAuthUI failed, redirecting to a blank page");
                theAuthenticatedWithIeClient = false;
                redirect(BlankUrl, aDispParamsPtr);
                return S_OK;
            }
            theAuthenticatedWithIeClient = true;
            if (!myUri2Go.empty() && url::normalize(myRequestedUri) !=  url::normalize(myUri2Go))
            {
                ReseptProxy::logDebug("loadBrowserReseptClientAuthUI succeeded, redirecting to " + myUri2Go);
                redirect(myUri2Go, aDispParamsPtr);
                return S_OK;
            }
            ReseptProxy::logDebug("loadBrowserReseptClientAuthUI succeeded, proceeding with " + myRequestedUri);
            return S_OK;
        }
        catch (std::exception& e)
        {
            showWarningMsg("Authentication failed. Please contact " + resept::ProductName + " administrator.");
            ReseptProxy::logError(e.what());
        }
        catch (...)
        {
            showWarningMsg("Authentication failed. Please contact " + resept::ProductName + " administrator.");
            ReseptProxy::logError("Unknown exception is caught");
        }
        theAuthenticatedWithIeClient = false;
        redirect(BlankUrl, aDispParamsPtr);
        return S_OK;
    }
    case DISPID_NAVIGATECOMPLETE2:
        break;
    case DISPID_ONQUIT:
        ConnectBho2Ie(ConnType_Unadvise);
        break;
    default:
        break;
    }
    return S_OK;
}

void KeyTalkIeClient::redirect(const string& aUrl, DISPPARAMS* aDispParamsPtr)
{
    CComPtr<IWebBrowser2> myBrowserPtr;
    CComPtr<IDispatch> spDisp = (aDispParamsPtr->rgvarg)[6].pdispVal;
    HRESULT hr = spDisp->QueryInterface(IID_IWebBrowser2, (void**)&myBrowserPtr);
    if (FAILED(hr))
    {
        showWarningMsg("Authentication failed. Please contact " + resept::ProductName + " administrator.");
        ReseptProxy::logError(str(boost::format("Failed to query IID_IWebBrowser2 interface. HRESULT: %d") % hr));
        return;
    }

    myBrowserPtr->Stop();

    CComBSTR newURL = ta::EncodingUtils::toWide(aUrl).c_str();
    myBrowserPtr->Navigate(newURL, NULL, NULL, NULL, NULL); //@todo why losing POST and other headers data by passing all NULLs?
    (aDispParamsPtr->rgvarg)[0].boolVal = VARIANT_TRUE;
}

void KeyTalkIeClient::showWarningMsg(const std::string& aMsg)
{
    ::MessageBox(NULL, aMsg.c_str(), resept::ProductName.c_str(), MB_ICONWARNING | MB_TOPMOST);
}