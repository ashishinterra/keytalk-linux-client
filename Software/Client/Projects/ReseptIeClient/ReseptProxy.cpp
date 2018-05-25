//----------------------------------------------------------------------------
//
//  Name          ReseptProxy.cpp
//  Description : Implementation of utilites to call RESEPT API from IE BHO.
//
//----------------------------------------------------------------------------
#include "ReseptProxy.h"
#include "BrokerProxy.h"
#include "rclient/CommonUtils.h"
#include "rclient/Settings.h"
#include "rclient/NativeCertStore.h"
#include "ta/InternetExplorer.h"
#include "ta/logger.h"
#include "ta/common.h"
#include <vector>

// FORCE_USE_BROKER is used for broker testing only, should not be defined in retail builds
//#define FORCE_USE_BROKER

namespace ReseptProxy
{
    using std::string;
    using std::vector;

    //
    // Private stuff
    //

#define WRITE_LOG_LOCAL(aLogLevel) \
    do {\
    switch (aLogLevel)\
    {\
    case ta::LogLevel::Debug: DEBUGLOG(aMsg); return true; \
    case ta::LogLevel::Info:  INFOLOG(aMsg); return true; \
    case ta::LogLevel::Warn: WARNLOG(aMsg); return true; \
    case ta::LogLevel::Error: ERRORLOG(aMsg); return true; \
    default: return false; \
    }\
    } while (0)

#define WRITE_LOG_BROKER(aLogLevel) \
    do {\
    try \
    { \
    BrokerProxy myBroker; \
    return myBroker.log(aLogLevel, aMsg); \
    } \
    catch (std::exception& e) \
    { \
    ERRORLOG2("Error writing broker log", e.what()); \
    return false; \
    } \
    catch (...) \
    { \
    ERRORLOG("Unknown error"); \
    return false; \
    } \
    } while (0)


    //
    // Public stuff
    //

    LoggerInitializer::LoggerInitializer()
        : theLoggerInitializer(NULL)
    {
#ifndef FORCE_USE_BROKER
        if (ta::InternetExplorer::getProtectedMode() != ta::InternetExplorer::protectedModeOn)
        {
            theLoggerInitializer = new rclient::LoggerInitializer();
            return;
        }
#endif
        // Do nothing since the broker initialises the log himself anyway
    }

    LoggerInitializer::~LoggerInitializer()
    {
        delete theLoggerInitializer;
    }


    bool loadBrowserReseptClientAuthUI(const std::vector<std::pair<string, string> >& aProviderServicePairs, const std::string& aReqestedUri, std::string& anUri2Go)
    {
#ifndef FORCE_USE_BROKER
        if (ta::InternetExplorer::getProtectedMode() != ta::InternetExplorer::protectedModeOn)
            return rclient::loadBrowserReseptClientAuthUI(aProviderServicePairs, aReqestedUri, anUri2Go);
#endif
        BrokerProxy myBroker;
        return myBroker.loadReseptClientAuthUi(aProviderServicePairs, aReqestedUri, anUri2Go);
    }


    bool logDebug(const string& aMsg)
    {
#ifndef FORCE_USE_BROKER
        if (ta::InternetExplorer::getProtectedMode() != ta::InternetExplorer::protectedModeOn)
        {
            WRITE_LOG_LOCAL(ta::LogLevel::Debug);
        }
#endif
        WRITE_LOG_BROKER(ta::LogLevel::Debug);
    }

    bool logInfo (const string& aMsg)
    {
#ifndef FORCE_USE_BROKER
        if (ta::InternetExplorer::getProtectedMode() != ta::InternetExplorer::protectedModeOn)
        {
            WRITE_LOG_LOCAL(ta::LogLevel::Info);
        }
#endif
        WRITE_LOG_BROKER(ta::LogLevel::Info);
    }

    bool logWarn (const string& aMsg)
    {
#ifndef FORCE_USE_BROKER
        if (ta::InternetExplorer::getProtectedMode() != ta::InternetExplorer::protectedModeOn)
        {
            WRITE_LOG_LOCAL(ta::LogLevel::Warn);
        }
#endif
        WRITE_LOG_BROKER(ta::LogLevel::Warn);
    }

    bool logError(const string& aMsg)
    {
#ifndef FORCE_USE_BROKER
        if (ta::InternetExplorer::getProtectedMode() != ta::InternetExplorer::protectedModeOn)
        {
            WRITE_LOG_LOCAL(ta::LogLevel::Error);
        }
#endif
        WRITE_LOG_BROKER(ta::LogLevel::Error);
    }

    unsigned int validateReseptUserCert()
    {
#ifndef FORCE_USE_BROKER
        if (ta::InternetExplorer::getProtectedMode() != ta::InternetExplorer::protectedModeOn)
            return rclient::NativeCertStore::validateReseptUserCert();
#endif
        BrokerProxy myBroker;
        return myBroker.validateCert();
    }

    unsigned int deleteAllReseptUserCerts()
    {
#ifndef FORCE_USE_BROKER
        if (ta::InternetExplorer::getProtectedMode() != ta::InternetExplorer::protectedModeOn)
            return rclient::NativeCertStore::deleteAllReseptUserCerts();
#endif
        BrokerProxy myBroker;
        return myBroker.deleteAllReseptUserCerts();
    }

}
