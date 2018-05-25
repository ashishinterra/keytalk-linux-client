//----------------------------------------------------------------------------
//
//  Name          ReseptProxy.h
//  Description : Declaration of utilites to call RESEPT API from IE BHO.
//
//                The proxy is required because when IE runs in protected mode (introduced in Windows Vista),
//                the IE BHO runs in low integrity level which prevents it from changing RESEPT configuration,
//                logging and accessing the Certificate Store. The solution is to delegate these tasks to a separate executable (broker).
//                The broker will be given all necessary permissions to do these tasks (either during instalation or explicitly by the user).
//                The proxy will then communicate with the broker transparently, thus BHO will not notice a difference
//                whether its request to the proxy is executed in place or by the broker.
//
//----------------------------------------------------------------------------
#pragma once

#include <string>
#include <utility>
#include <vector>
#include <stdexcept>

namespace rclient { struct LoggerInitializer; };

namespace ReseptProxy
{

    struct LoggerInitializer
    {
        //@throw LoggerInitError
        LoggerInitializer();
        ~LoggerInitializer();

        rclient::LoggerInitializer* theLoggerInitializer;
    };

    //
    //   Abstract:
    //             Load resept client UI in order to authenticate and get session certificate
    //
    //   Arguments:
    //             aProviderServicePairs [in] client/customer pairs (at least 1 pair) whose service URI matches aReqestedUri
    //             aReqestedUri  [in] Requested URI
    //             anUri2Go      [out] URI to proceed with
    //
    //   Return:
    //          true if succeeded (cert. loaded or is still valid wrt the OS native cert store), otherwise false
    //
    //   Post:
    //         if succeeded, anUrl2Go contains URL to proceed with, Settings::getLatestProviderId() and Settings::getLatestServicNum() contain selected provider/service number
    //         anUri2Go is calculated using the following algorithm:
    //                  if cert is still valid, anUri2Go equals aReqestedUri
    //                  if aReqestedUri is an extracted service URI, anUri2Go equals aReqestedUri
    //                  if aReqestedUri is not an extracted service URI, anUri2Go equals the extracted service URI
    //
    //
    //  Errors:
    //         throw BrokerError on error
    //
    bool loadBrowserReseptClientAuthUI(const std::vector<std::pair<std::string, std::string> >& aProviderServicePairs, const std::string& aReqestedUri, std::string& anUri2Go);


    //@nothrow
    bool logDebug(const std::string& aMsg);
    bool logInfo (const std::string& aMsg);
    bool logWarn (const std::string& aMsg);
    bool logError(const std::string& aMsg);

    //  throw rclient::NativeCertStoreValidateError, BrokerError on error
    unsigned int validateReseptUserCert();

    //  throw rclient::NativeCertStoreDaleteError, BrokerError on error
    unsigned int deleteAllReseptUserCerts();

}
