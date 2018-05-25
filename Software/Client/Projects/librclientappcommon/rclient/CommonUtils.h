#pragma once

#include <string>
#include <stdexcept>
#include <vector>
#include <utility>

namespace rclient
{
    struct CertStillValidException : std::exception
    {};
    struct UserLockedError : std::exception
    {};
    struct AuthCancelledException : std::exception
    {};


    struct LoggerInitError : std::runtime_error
    {
        explicit LoggerInitError(const std::string& aMessage = "") : std::runtime_error(aMessage) {}
    };


    // RAII-class for logger.
    struct LoggerInitializer
    {
        //@throw LoggerInitError
        LoggerInitializer();
        ~LoggerInitializer();
    };

    //
    // Matching criteria between the given user requested URI and Service URI
    // The function has the following semantics:
    // 1. Both URIs are parsed according to the RFC3986 and the following parts are extracted:
    //    scheme, user, password, host, port, path, query and fragment.
    // 2. If scheme, host and  port parts are equal and path part of the requested URI
    //    resides in or equals to the path part of the service URI, the function return true, otherwise false
    // 3. If host starts with '*.' it is treated as a domain wildcard. E.g. https://login.example.com will match https://*.example.com
    // 4. Otherwise or if any of URLs are not well-formed URLs, the function return false
    //
    // @nothrow
    bool isServiceUri(const std::string& aRequestedUri, const std::string& aServiceUri);

#ifdef _WIN32

    /**
       Load browser RESEPT client UI to authenticate and retrieve session certificate

       @param[in] aProviderServicePairs provider/service pairs (at least 1 pair) whose service URI matches aReqestedUri
       @param[in] aReqestedUri  Requested URI
       @param[out] anUri2Go    URI to proceed with
       @return true if succeeded (cert. loaded or is still valid wrt the OS native cert store), otherwise false
       @post if succeeded, anUri2Go contains URI to proceed with, Settings::getLatestProvider() and Settings::getLatestService() contain selected provider/service number
             anUri2Go is calculated using the following algorithm:
             if cert is still valid, anUri2Go equals aReqestedUri
             if aReqestedUri is a service URI, anUri2Go equals aReqestedUri
             if aReqestedUri is not a service URI, anUri2Go equals extracted URI
      @throw nothing
    */
    bool loadBrowserReseptClientAuthUI(const std::vector<std::pair<std::string, std::string> >& aProviderServicePairs, const std::string& aReqestedUri, std::string& anUri2Go);

    // Returns true if IE and RESEPT IE add-on are installed, false otherwise
    bool isReseptIeAddonInstalled();
#endif

}
