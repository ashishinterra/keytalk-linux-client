//----------------------------------------------------------------------------
//
//  Name          ReseptIeBroker.cpp
//  Description : RESEPT Broker for Internet Explorer
//
// RESEPT Client IE BHO performs some tasks for which it has insufficient permissions on behalf of the broker. The communication scheme is the following:
// 1. RESEPT IE BHO launches the broker passing port and session id cookie as command-line arguments
// 2. The broker connects back to the BHO using the port and  session id supplied
// 3. IE BHO submits jobs to the broker. The broker does the jobs, reports the results back to IE BHO and exits.
//
// Usage: ReseptIeBroker <Port> <SID>
//
// Port - local TCP port the caller is listening on.
// SID  - session id the broker shell use to be granted to talk to the caller
//
// Return values:
// 0 if succeeded, <>0 otherwise
//
// The ReseptIeBroker has not been made as Windows Service (which initially seemed as more simple alternative) because
// it would make the log location behavior too complex (Windows Service is executed in the system cobntext while ReseptIeBroker is executed in the user's context)
//
//
//----------------------------------------------------------------------------
#include "rclient/CommonUtils.h"
#include "rclient/Bbp.h"
#include "rclient/NativeCertStore.h"
#include "ta/netutils.h"
#include "ta/logger.h"
#include "ta/tcpclient.h"
#include "ta/timeutils.h"
#include "ta/strings.h"
#include "ta/opensslapp.h"
#include "ta/assert.h"
#include "ta/common.h"

#include <windows.h>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include "boost/cstdint.hpp"
#include "boost/tokenizer.hpp"

using std::string;
using std::vector;

namespace
{
    // throw std::exception on error
    void parseArgs(const string& aCmdLine, unsigned int& aPort, boost::uint32_t& anSid)
    {
        typedef boost::tokenizer<boost::char_separator<char> > Tokenizer;
        boost::char_separator<char> mySep(" \t\n");
        Tokenizer myTokens(aCmdLine, mySep);
        size_t myNumArgs = std::distance(myTokens.begin(), myTokens.end());
        if (myNumArgs != 2)
            TA_THROW_MSG(std::logic_error, boost::format("Expected %d args, actual %d.") % 3 % myNumArgs);
        Tokenizer::iterator myTok = myTokens.begin();
        if (!ta::NetUtils::isValidPort(*myTok, &aPort))
            TA_THROW_MSG(std::logic_error, (*myTok) + " is not a valid port number");
        ++myTok;
        anSid = ta::Strings::parse<boost::uint32_t>(*myTok);
    }

    // throw std::runtime_error on error
    void handleBhoRequest(ta::TcpClient& aTcpClient)
    {
        boost::uint32_t myBrokerRetVal = rclient::bbp::BrokerRetVal::BrokerError;
        vector<char> mySerializedResponse;
        try
        {
            vector<char> myReqSizeSer = aTcpClient.receiveAll(sizeof(boost::uint32_t));
            boost::uint32_t myReqSize = rclient::bbp::deserializeSize(myReqSizeSer);
            vector<char> myReqSer = aTcpClient.receiveAll(myReqSize);
            vector<char> myReqBodySer;
            boost::uint32_t myReqType = rclient::bbp::deserializeRequest(myReqSer, myReqBodySer);

            switch (myReqType)
            {
            case rclient::bbp::MessageType::Log:
            {
                DEBUGLOG("Log requested");
                string myLogText;
                ta::LogLevel::val myLogLevel = rclient::bbp::deserializeLogRequestBody(myReqBodySer, myLogText);
                myLogText = "BHO> " + myLogText;
                switch (myLogLevel)
                {
                case ta::LogLevel::Debug:
                    DEBUGLOG(myLogText);
                    myBrokerRetVal = rclient::bbp::BrokerRetVal::Ok;
                    break;
                case ta::LogLevel::Info:
                    INFOLOG(myLogText);
                    myBrokerRetVal = rclient::bbp::BrokerRetVal::Ok;
                    break;
                case ta::LogLevel::Warn:
                    WARNLOG(myLogText);
                    myBrokerRetVal = rclient::bbp::BrokerRetVal::Ok;
                    break;
                case ta::LogLevel::Error:
                    ERRORLOG(myLogText);
                    myBrokerRetVal = rclient::bbp::BrokerRetVal::Ok;
                    break;
                default:
                    ERRORLOG(boost::format("Unsupported log level %u") % myLogLevel);
                    break;
                }
                if (myBrokerRetVal == rclient::bbp::BrokerRetVal::Ok)
                    mySerializedResponse =  rclient::bbp::serializeLogResponse(true);
                break;
            }
            case rclient::bbp::MessageType::ValidateCert:
            {
                DEBUGLOG("ValidateCert requested");
                int myValidateCertRetVal = -1;
                ta::OpenSSLApp myOpenSSLApp;
                try
                {
                    myValidateCertRetVal = rclient::NativeCertStore::validateReseptUserCert();

                }
                catch (rclient::NativeCertStoreError& e)
                {
                    ERRORLOG2("Error validating certificate", e.what());
                }
                mySerializedResponse =  rclient::bbp::serializeValidateCertResponse(myValidateCertRetVal);
                myBrokerRetVal = rclient::bbp::BrokerRetVal::Ok;
                break;
            }
            case rclient::bbp::MessageType::DeleteCert:
            {
                DEBUGLOG("DeleteCert requested");
                int myDeleteCertsRetVal = -1;
                ta::OpenSSLApp myOpenSSLApp;
                try
                {
                    myDeleteCertsRetVal = rclient::NativeCertStore::deleteAllReseptUserCerts();
                }
                catch (rclient::NativeCertStoreError& e)
                {
                    ERRORLOG2("Error deleting certificates", e.what());
                }
                mySerializedResponse =  rclient::bbp::serializeDeleteCertResponse(myDeleteCertsRetVal);
                myBrokerRetVal = rclient::bbp::BrokerRetVal::Ok;
                break;
            }
            case rclient::bbp::MessageType::LoadAuthUi:
            {
                DEBUGLOG("Load auth UI requested");
                ta::OpenSSLApp myOpenSSLApp;
                string myUri2Go;
                vector<std::pair<string, string> > myReceivedProviderServicePairs;
                string myReceivedReqUrl = rclient::bbp::deserializeLoadAuthUiRequestBody(myReqBodySer, myReceivedProviderServicePairs);
                bool  myLoadAuthUiRetVal = rclient::loadBrowserReseptClientAuthUI(myReceivedProviderServicePairs, myReceivedReqUrl, myUri2Go);
                mySerializedResponse =  rclient::bbp::serializeLoadAuthUiResponse(myLoadAuthUiRetVal, myUri2Go);
                myBrokerRetVal = rclient::bbp::BrokerRetVal::Ok;
                break;
            }
            default:
            {
                myBrokerRetVal = rclient::bbp::BrokerRetVal::UnsupportedRequest;
                ERRORLOG2("Unsupported request", boost::format("Unsupported request type %u") % myReqType);
                break;
            }
            }
        }
        catch (std::exception& e)
        {
            ERRORLOG2("Broker error", boost::format("Broker error occurred: %s") % e.what());
        }
        aTcpClient.sendAll(rclient::bbp::serializeBrokerRetVal(myBrokerRetVal));
        if (myBrokerRetVal ==  rclient::bbp::BrokerRetVal::Ok)
        {
            TA_ASSERT(!mySerializedResponse.empty());
            aTcpClient.sendAll(mySerializedResponse);
        }
        else
        {
            TA_THROW(std::runtime_error);
        }
    }

    const string BhoIp = "127.0.0.1";
    const unsigned int myNumConnectionAttempts = 3;
}

int APIENTRY WinMain(HINSTANCE UNUSED(hInstance), HINSTANCE UNUSED(hPrevInstance), LPSTR  aCmdLine, int  UNUSED(nCmdShow))
{
    if (!aCmdLine || !(*aCmdLine))
        return -1;
    int myRetCode = -1;
    try
    {
        rclient::LoggerInitializer myLoggerInstance;

        DEBUGLOG("Started with args " + string(aCmdLine));
        unsigned int myPort;
        boost::uint32_t mySid;
        parseArgs(aCmdLine, myPort, mySid);

        ta::TcpClient myClient;
        DEBUGLOG(boost::format("Connecting to BHO at %s:%u...") % BhoIp % myPort);
        for (unsigned int i = 0; i < myNumConnectionAttempts; ++i)
        {
            try
            {
                myClient.open(BhoIp, myPort);
                break;
            }
            catch (ta::TcpClientError& e)
            {
                WARNLOG2("Connection failed. Retrying...", boost::format("Connection failed. %s. Retrying...") % e.what());
                ta::TimeUtils::sleep(300);
            }
        }
        DEBUGLOG(boost::format("Handshaking with SID %u...") % mySid);
        vector<char> mySerializedSid = rclient::bbp::serializeSid(mySid);
        myClient.sendAll(mySerializedSid);

        handleBhoRequest(myClient);

        myRetCode = 0;
    }
    catch (std::exception& e)
    {
        ERRORLOG2("Broker Error", e.what());
        myRetCode = -1;
    }
    DEBUGLOG(boost::format("Finished with retcode %d") % myRetCode);
    return myRetCode;
}

