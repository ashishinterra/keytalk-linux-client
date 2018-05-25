//----------------------------------------------------------------------------
//
//  Name          BrokerProxy.h
//  Description : Proxy class for the IE Broker declaration
//
//----------------------------------------------------------------------------
#ifndef BROKERPROXY_H
#define BROKERPROXY_H

#include "ta/logappender.h"

#include "boost/utility.hpp"
#include "boost/cstdint.hpp"
#include "boost/thread/thread.hpp"
#include "boost/thread/recursive_mutex.hpp"
#include "boost/thread/condition.hpp"
#include <string>
#include <stdexcept>
#include <vector>

struct BrokerError : std::runtime_error
{
    explicit BrokerError(const std::string& aMessage = "")	: std::runtime_error(aMessage) {}
};

struct BrokerProxyError : BrokerError
{
    explicit BrokerProxyError(const std::string& aMessage = "")	: BrokerError(aMessage) {}
};

class BrokerProxy : private boost::noncopyable
{
    enum State { ExitSuccess, ExitError, Listening, None };

    class IeBhoTcpSvrEngine
    {
    public:
        IeBhoTcpSvrEngine(unsigned int& aPort, boost::uint32_t anSid, const std::vector<char>& aSerializedReq, std::vector<char>& aSerializedResp, State& aState, boost::recursive_mutex& aStateMtx, boost::condition& aStateCv);
        void operator()();
    private:
        void notifyListening();
        void notifyExitSuccess();
        void notifyExitError();
    private:
        unsigned int& thePort;
        boost::uint32_t theSid;
        std::vector<char> theSerializedReq;
        std::vector<char>& theSerializedResp;
        State& theState;
        boost::recursive_mutex& theStateMtx;
        boost::condition& theStateCv;
    }; // IeBhoTcpSvrEngine

public:
    BrokerProxy();
    ~BrokerProxy();

    //
    //  Start the broker and submit log job to it
    //
    //  Return: log success flag
    //
    //  throw BrokerError on error
    bool log(ta::LogLevel::val aLogLevel, const std::string& aMsg);

    //
    //  Start the broker and submit log job to it
    //
    //  Return: number of valid certs
    //
    //  throw BrokerError, std::exception on error
    unsigned int validateCert();

    //
    //  Start the broker and submit log job to it
    //
    //  Return: number of deleted certs
    //
    //  throw BrokerError, std::exception on error
    unsigned int deleteAllReseptUserCerts();

    //
    //  Start the broker and submit LoadAuthUi job to it
    //
    //  See ::loadBrowserReseptClientAuthUI spec
    //
    //  throw BrokerError on error
    bool loadReseptClientAuthUi(const std::vector<std::pair<std::string, std::string> >& aProviderServicePairs, const std::string& aReqestedUrl, std::string& anUrl2Go);

private:
    bool waitForListening(boost::thread& aTcpServerThread);
    bool waitForExit(boost::thread& aTcpServerThread);
    static void initLogger();
    static void deinitLogger();
    void invoke(const std::vector<char>& aSerReq, std::vector<char>& aSerResp);
private:
    State theState;
    boost::recursive_mutex theStateMtx;
    boost::condition theStateCv;
};

#endif
