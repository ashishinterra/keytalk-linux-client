#pragma once

#include "rclient/Bbp.h"
#include "cxxtest/TestSuite.h"
#include "rclient/NativeCertStore.h"
#include <string>
#include <stdexcept>
#include <vector>

using std::string;
using std::vector;

class BbpTest : public CxxTest::TestSuite
{
public:
    void setUp()
    {
        CxxTest::setAbortTestOnFail(true);
    }
    void tearDown()
    {
        CxxTest::setAbortTestOnFail(false);
    }
    void testSimulateSidHandshake()
    {
        using namespace rclient;
        vector<char> myInvalidSid;
        TS_ASSERT_THROWS(bbp::deserializeSid(myInvalidSid), std::invalid_argument);
        myInvalidSid.resize(sizeof(boost::uint32_t)-1);
        TS_ASSERT_THROWS(bbp::deserializeSid(myInvalidSid), std::invalid_argument);

        boost::uint32_t mySidExpected = 1234;
        vector<char> mySerializedSid = bbp::serializeSid(mySidExpected);
        boost::uint32_t mySidActual = bbp::deserializeSid(mySerializedSid);
        TS_ASSERT_EQUALS(mySidExpected, mySidActual);
    }

    void testSimulateLogRequest()
    {
        using namespace rclient;
        //--------
        // Broker
        //--------

        const std::string myLogTextExpected = "This is a message to log";
        ta::LogLevel::val myLogLevelExpected = ta::LogLevel::Info;
        // Serializing the message
        vector<char> mySerializedMsg = bbp::serializeLogRequest(myLogLevelExpected, myLogTextExpected);
        //...
        // Sending over the network ...
        //...

        //--------
        // BHO
        //--------

        TS_ASSERT(mySerializedMsg.size() >= sizeof(boost::uint32_t));
        // Fetch the message size from the socket buffer
        vector<char> myReceivedSerializedSize(mySerializedMsg.begin(), mySerializedMsg.begin() + sizeof(boost::uint32_t));
        // Deserializing size
        boost::uint32_t myReceivedSize = bbp::deserializeSize(myReceivedSerializedSize);
        TS_ASSERT(mySerializedMsg.size() == sizeof(boost::uint32_t) + myReceivedSize);
        // Fetching the rest of the message from the socket buffer
        vector<char> myReceivedSerializedMsg(mySerializedMsg.begin() + sizeof(boost::uint32_t), mySerializedMsg.end());
        // Deserializing the message...
        vector<char> myLogBody;
        boost::uint32_t myType = bbp::deserializeRequest(myReceivedSerializedMsg, myLogBody);
        TS_ASSERT_EQUALS(myType, bbp::MessageType::Log);
        if (myType != bbp::MessageType::Log)
            return;
        std::string myLogTextActual;
        ta::LogLevel::val myLogLevelActual = bbp::deserializeLogRequestBody(myLogBody, myLogTextActual);
        TS_ASSERT_EQUALS(myLogLevelActual, myLogLevelExpected);
        TS_ASSERT_EQUALS(myLogTextActual, myLogTextExpected);
    }

    void testSimulateLogResponse()
    {
        using namespace rclient;
        //--------
        // Broker
        //--------

        boost::uint32_t myBrokerRetVal = bbp::BrokerRetVal::Ok;
        vector<char> mySerializedBrokerRetVal = bbp::serializeBrokerRetVal(myBrokerRetVal);
        //...
        // Sending over the network ...
        //...
        bool myLogRetVal = true;
        vector<char> myLogSerializedResponse = bbp::serializeLogResponse(myLogRetVal);
        //...
        // Sending over the network ...
        //...

        //--------
        // BHO
        //--------

        TS_ASSERT(mySerializedBrokerRetVal.size() == sizeof(boost::uint32_t));
        // Fetch the broker retval from the socket buffer
        vector<char> myReceivedSerializedBrokerRetVal = mySerializedBrokerRetVal;
        // Deserializing broker retval
        boost::uint32_t myReceivedBrokerRetVal = bbp::deserializeBrokerRetVal(myReceivedSerializedBrokerRetVal);
        TS_ASSERT_EQUALS(myReceivedBrokerRetVal, myBrokerRetVal);

        TS_ASSERT(myLogSerializedResponse.size() >= sizeof(boost::uint32_t));
        // Fetch the log response size from the socket buffer
        vector<char> myReceivedResponseSize(myLogSerializedResponse.begin(), myLogSerializedResponse.begin() + sizeof(boost::uint32_t));
        boost::uint32_t mySize = bbp::deserializeSize(myReceivedResponseSize);
        TS_ASSERT(mySize >= sizeof(boost::uint32_t));
        // Fetch the log response from the socket buffer
        vector<char> myReceivedResponse(myLogSerializedResponse.begin() + sizeof(boost::uint32_t), myLogSerializedResponse.begin() + sizeof(boost::uint32_t) + mySize) ;
        bool myReceivedRetVal = bbp::deserializeLogResponse(myReceivedResponse);
        TS_ASSERT_EQUALS(myReceivedRetVal, myLogRetVal);
    }

    void testSimulateValidateCertRequest()
    {
        using namespace rclient;
        //--------
        // Broker
        //--------

        // Serializing the message
        vector<char> mySerializedMsg = bbp::serializeValidateCertRequest();
        //...
        // Sending over the network ...
        //...

        //--------
        // BHO
        //--------

        TS_ASSERT(mySerializedMsg.size() >= sizeof(boost::uint32_t));
        // Fetch the message size from the socket buffer
        vector<char> myReceivedSerializedSize(mySerializedMsg.begin(), mySerializedMsg.begin() + sizeof(boost::uint32_t));
        // Deserializing size
        boost::uint32_t myReceivedSize = bbp::deserializeSize(myReceivedSerializedSize);
        TS_ASSERT(mySerializedMsg.size() == sizeof(boost::uint32_t) + myReceivedSize);
        // Fetching the rest of the message from the socket buffer
        vector<char> myReceivedSerializedMsg(mySerializedMsg.begin() + sizeof(boost::uint32_t), mySerializedMsg.end());
        // Deserializing the message
        vector<char> myBody;
        boost::uint32_t myType = bbp::deserializeRequest(myReceivedSerializedMsg, myBody);
        TS_ASSERT_EQUALS(myType, bbp::MessageType::ValidateCert);
    }

    void testSimulateValidateCertResponse()
    {
        using namespace rclient;
        //--------
        // Broker
        //--------

        boost::uint32_t myBrokerRetVal = bbp::BrokerRetVal::Ok;
        vector<char> mySerializedBrokerRetVal = bbp::serializeBrokerRetVal(myBrokerRetVal);
        //...
        // Sending over the network ...
        //...
        int myNumOfValidCerts = 2;
        vector<char> mySerializedResponse = bbp::serializeValidateCertResponse(myNumOfValidCerts);
        //...
        // Sending over the network ...
        //...

        //--------
        // BHO
        //--------

        TS_ASSERT(mySerializedBrokerRetVal.size() == sizeof(boost::uint32_t));
        // Fetch the broker retval from the socket buffer
        vector<char> myReceivedSerializedBrokerRetVal = mySerializedBrokerRetVal;
        // Deserializing broker retval
        boost::uint32_t myReceivedBrokerRetVal = bbp::deserializeBrokerRetVal(myReceivedSerializedBrokerRetVal);
        TS_ASSERT_EQUALS(myReceivedBrokerRetVal, myBrokerRetVal);

        TS_ASSERT(mySerializedResponse.size() >= sizeof(boost::uint32_t));
        // Fetch the response size from the socket buffer
        vector<char> myReceivedResponseSize(mySerializedResponse.begin(), mySerializedResponse.begin() + sizeof(boost::uint32_t));
        boost::uint32_t mySize = bbp::deserializeSize(myReceivedResponseSize);
        TS_ASSERT(mySize >= sizeof(boost::uint32_t));
        // Fetch the response from the socket buffer
        vector<char> myReceivedResponse(mySerializedResponse.begin() + sizeof(boost::uint32_t), mySerializedResponse.begin() + sizeof(boost::uint32_t) + mySize) ;
        int myReceivedRetVal = bbp::deserializeValidateCertResponse(myReceivedResponse);
        TS_ASSERT_EQUALS(myReceivedRetVal, myNumOfValidCerts);
    }

    void testSimulateDeleteCertRequest()
    {
        using namespace rclient;
        //--------
        // Broker
        //--------

        // Serializing the message
        vector<char> mySerializedMsg = bbp::serializeDeleteCertRequest();
        //...
        // Sending over the network ...
        //...

        //--------
        // BHO
        //--------

        TS_ASSERT(mySerializedMsg.size() >= sizeof(boost::uint32_t));
        // Fetch the message size from the socket buffer
        vector<char> myReceivedSerializedSize(mySerializedMsg.begin(), mySerializedMsg.begin() + sizeof(boost::uint32_t));
        // Deserializing size
        boost::uint32_t myReceivedSize = bbp::deserializeSize(myReceivedSerializedSize);
        TS_ASSERT(mySerializedMsg.size() == sizeof(boost::uint32_t) + myReceivedSize);
        // Fetching the rest of the message from the socket buffer
        vector<char> myReceivedSerializedMsg(mySerializedMsg.begin() + sizeof(boost::uint32_t), mySerializedMsg.end());
        // Deserializing the message
        vector<char> myBody;
        boost::uint32_t myType = bbp::deserializeRequest(myReceivedSerializedMsg, myBody);
        TS_ASSERT_EQUALS(myType, bbp::MessageType::DeleteCert);
    }

    void testSimulateDeleteCertResponse()
    {
        using namespace rclient;
        //--------
        // Broker
        //--------

        boost::uint32_t myBrokerRetVal = bbp::BrokerRetVal::Ok;
        vector<char> mySerializedBrokerRetVal = bbp::serializeBrokerRetVal(myBrokerRetVal);
        //...
        // Sending over the network ...
        //...
        int myNumOfDeletedCerts = 2;
        vector<char> mySerializedResponse = bbp::serializeDeleteCertResponse(myNumOfDeletedCerts);
        //...
        // Sending over the network ...
        //...

        //--------
        // BHO
        //--------

        TS_ASSERT(mySerializedBrokerRetVal.size() == sizeof(boost::uint32_t));
        // Fetch the broker retval from the socket buffer
        vector<char> myReceivedSerializedBrokerRetVal = mySerializedBrokerRetVal;
        // Deserializing broker retval
        boost::uint32_t myReceivedBrokerRetVal = bbp::deserializeBrokerRetVal(myReceivedSerializedBrokerRetVal);
        TS_ASSERT_EQUALS(myReceivedBrokerRetVal, myBrokerRetVal);

        TS_ASSERT(mySerializedResponse.size() >= sizeof(boost::uint32_t));
        // Fetch the response size from the socket buffer
        vector<char> myReceivedResponseSize(mySerializedResponse.begin(), mySerializedResponse.begin() + sizeof(boost::uint32_t));
        boost::uint32_t mySize = bbp::deserializeSize(myReceivedResponseSize);
        TS_ASSERT(mySize >= sizeof(boost::uint32_t));
        // Fetch the response from the socket buffer
        vector<char> myReceivedResponse(mySerializedResponse.begin() + sizeof(boost::uint32_t), mySerializedResponse.begin() + sizeof(boost::uint32_t) + mySize) ;
        int myReceivedRetVal = bbp::deserializeDeleteCertResponse(myReceivedResponse);
        TS_ASSERT_EQUALS(myReceivedRetVal, myNumOfDeletedCerts);
    }

    void testSimulateLoadAuthUiRequestNonEmptyProviderServicePairsList()
    {
        using namespace rclient;
        //--------
        // Broker
        //--------

        // Serializing the message
        std::string myReqUrl = "https://bank.com";
        vector<std::pair<string, string> > myProviderServicePairs;
        myProviderServicePairs.push_back(std::pair<string, string>("Provider1","Service1"));
        myProviderServicePairs.push_back(std::pair<string, string>("Provider1","Service2"));
        myProviderServicePairs.push_back(std::pair<string, string>("Provider2","Service1"));
        vector<char> mySerializedMsg = bbp::serializeLoadAuthUiRequest(myProviderServicePairs, myReqUrl);
        //...
        // Sending over the network ...
        //...

        //--------
        // BHO
        //--------

        TS_ASSERT(mySerializedMsg.size() >= sizeof(boost::uint32_t));
        // Fetch the message size from the socket buffer
        vector<char> myReceivedSerializedSize(mySerializedMsg.begin(), mySerializedMsg.begin() + sizeof(boost::uint32_t));
        // Deserializing size
        boost::uint32_t myReceivedSize = bbp::deserializeSize(myReceivedSerializedSize);
        TS_ASSERT(mySerializedMsg.size() == sizeof(boost::uint32_t) + myReceivedSize);
        // Fetching the rest of the message from the socket buffer
        vector<char> myReceivedSerializedMsg(mySerializedMsg.begin() + sizeof(boost::uint32_t), mySerializedMsg.end());
        // Deserializing the message
        vector<char> myBody;
        boost::uint32_t myType = bbp::deserializeRequest(myReceivedSerializedMsg, myBody);
        TS_ASSERT_EQUALS(myType, bbp::MessageType::LoadAuthUi);
        vector<std::pair<string, string> > myReceivedProviderServicePairs;
        std::string myReceivedReqUrl = bbp::deserializeLoadAuthUiRequestBody(myBody, myReceivedProviderServicePairs);
        TS_ASSERT_EQUALS(myReceivedReqUrl, myReqUrl);
        TS_ASSERT_EQUALS(myReceivedProviderServicePairs, myProviderServicePairs);
    }

    void testSimulateLoadAuthUiRequestEmptyProviderServicePairsList()
    {
        using namespace rclient;
        //--------
        // Broker
        //--------

        // Serializing the message
        std::string myReqUrl = "https://bank.com";
        vector<std::pair<string, string> > myProviderServicePairs;
        vector<char> mySerializedMsg = bbp::serializeLoadAuthUiRequest(myProviderServicePairs, myReqUrl);
        //...
        // Sending over the network ...
        //...

        //--------
        // BHO
        //--------

        TS_ASSERT(mySerializedMsg.size() >= sizeof(boost::uint32_t));
        // Fetch the message size from the socket buffer
        vector<char> myReceivedSerializedSize(mySerializedMsg.begin(), mySerializedMsg.begin() + sizeof(boost::uint32_t));
        // Deserializing size
        boost::uint32_t myReceivedSize = bbp::deserializeSize(myReceivedSerializedSize);
        TS_ASSERT(mySerializedMsg.size() == sizeof(boost::uint32_t) + myReceivedSize);
        // Fetching the rest of the message from the socket buffer
        vector<char> myReceivedSerializedMsg(mySerializedMsg.begin() + sizeof(boost::uint32_t), mySerializedMsg.end());
        // Deserializing the message
        vector<char> myBody;
        boost::uint32_t myType = bbp::deserializeRequest(myReceivedSerializedMsg, myBody);
        TS_ASSERT_EQUALS(myType, bbp::MessageType::LoadAuthUi);
        vector<std::pair<string, string> > myReceivedProviderServicePairs;
        std::string myReceivedReqUrl = bbp::deserializeLoadAuthUiRequestBody(myBody, myReceivedProviderServicePairs);
        TS_ASSERT_EQUALS(myReceivedReqUrl, myReqUrl);
        TS_ASSERT_EQUALS(myReceivedProviderServicePairs, myProviderServicePairs);
    }

    void testSimulateLoadAuthUiResponse()
    {
        using namespace rclient;
        //--------
        // Broker
        //--------

        boost::uint32_t myBrokerRetVal = bbp::BrokerRetVal::Ok;
        vector<char> mySerializedBrokerRetVal = bbp::serializeBrokerRetVal(myBrokerRetVal);
        //...
        // Sending over the network ...
        //...
        bool myRetVal = true;
        std::string myUrl2Go = "https://bank.nl";
        vector<char> mySerializedResponse = bbp::serializeLoadAuthUiResponse(myRetVal, myUrl2Go);
        //...
        // Sending over the network ...
        //...

        //--------
        // BHO
        //--------

        TS_ASSERT(mySerializedBrokerRetVal.size() == sizeof(boost::uint32_t));
        // Fetch the broker retval from the socket buffer
        vector<char> myReceivedSerializedBrokerRetVal = mySerializedBrokerRetVal;
        // Deserializing broker retval
        boost::uint32_t myReceivedBrokerRetVal = bbp::deserializeBrokerRetVal(myReceivedSerializedBrokerRetVal);
        TS_ASSERT_EQUALS(myReceivedBrokerRetVal, myBrokerRetVal);

        TS_ASSERT(mySerializedResponse.size() >= sizeof(boost::uint32_t)+1);
        // Fetch the response size from the socket buffer
        vector<char> myReceivedResponseSize(mySerializedResponse.begin(), mySerializedResponse.begin() + sizeof(boost::uint32_t));
        boost::uint32_t mySize = bbp::deserializeSize(myReceivedResponseSize);
        TS_ASSERT(mySize >= sizeof(boost::uint32_t));

        // Fetch the response from the socket buffer
        vector<char> myReceivedResponse(mySerializedResponse.begin() + sizeof(boost::uint32_t), mySerializedResponse.begin() + sizeof(boost::uint32_t) + mySize) ;
        std::string myReceivedUrl2Go;
        bool myReceivedRetVal = bbp::deserializeLoadAuthUiResponse(myReceivedResponse, myReceivedUrl2Go);
        TS_ASSERT_EQUALS(myReceivedRetVal, myRetVal);
        TS_ASSERT_EQUALS(myReceivedUrl2Go, myUrl2Go);
    }

    void testSimulateLoadAuthUiResponse2()
    {
        using namespace rclient;
        //--------
        // Broker
        //--------

        boost::uint32_t myBrokerRetVal = bbp::BrokerRetVal::Ok;
        vector<char> mySerializedBrokerRetVal = bbp::serializeBrokerRetVal(myBrokerRetVal);
        //...
        // Sending over the network ...
        //...
        bool myRetVal = false;
        std::string myUrl2Go = "<whatever>";
        vector<char> mySerializedResponse = bbp::serializeLoadAuthUiResponse(myRetVal, myUrl2Go);
        //...
        // Sending over the network ...
        //...

        //--------
        // BHO
        //--------

        TS_ASSERT(mySerializedBrokerRetVal.size() == sizeof(boost::uint32_t));
        // Fetch the broker retval from the socket buffer
        vector<char> myReceivedSerializedBrokerRetVal = mySerializedBrokerRetVal;
        // Deserializing broker retval
        boost::uint32_t myReceivedBrokerRetVal = bbp::deserializeBrokerRetVal(myReceivedSerializedBrokerRetVal);
        TS_ASSERT_EQUALS(myReceivedBrokerRetVal, myBrokerRetVal);

        TS_ASSERT(mySerializedResponse.size() >= sizeof(boost::uint32_t));
        // Fetch the response size from the socket buffer
        vector<char> myReceivedResponseSize(mySerializedResponse.begin(), mySerializedResponse.begin() + sizeof(boost::uint32_t));
        boost::uint32_t mySize = bbp::deserializeSize(myReceivedResponseSize);
        TS_ASSERT(mySize >= sizeof(boost::uint32_t));

        // Fetch the response from the socket buffer
        vector<char> myReceivedResponse(mySerializedResponse.begin() + sizeof(boost::uint32_t), mySerializedResponse.begin() + sizeof(boost::uint32_t) + mySize) ;
        std::string myReceivedUrl2Go;
        bool myReceivedRetVal = bbp::deserializeLoadAuthUiResponse(myReceivedResponse, myReceivedUrl2Go);
        TS_ASSERT_EQUALS(myReceivedRetVal, myRetVal);
    }
};
