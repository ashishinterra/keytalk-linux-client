//----------------------------------------------------------------------------
//
//  Name          Bbp.cpp
//  Description : BBP protocol for communicating between RESEPT IE BHO and RESEPT Broker
//
//----------------------------------------------------------------------------
#include "Bbp.h"
#include "ta/common.h"
#include "boost/static_assert.hpp"
#include <stdexcept>

using std::vector;
using std::string;

namespace rclient
{
    //@todo rewrite using boost::serialize
    namespace bbp
    {
        //
        // Private stuff
        //
        namespace
        {
            vector<char> serialize(boost::uint32_t aVal)
            {
                vector<char> mySerializedData(sizeof(boost::uint32_t));
                memcpy(ta::getSafeBuf(mySerializedData), &aVal, sizeof(boost::uint32_t));
                return mySerializedData;
            }

            vector<char> serialize(boost::int32_t aVal)
            {
                vector<char> mySerializedData(sizeof(boost::int32_t));
                memcpy(ta::getSafeBuf(mySerializedData), &aVal, sizeof(boost::int32_t));
                return mySerializedData;
            }

            vector<char> serialize(const string& aVal)
            {
                return vector<char> (aVal.c_str(), aVal.c_str() + aVal.length() + 1);
            }

            vector<char> serialize(const vector<std::pair<string, string> >& aVal)
            {
                typedef std::pair<string, string> ElemType;
                vector<ElemType>::size_type myNumElems = aVal.size();
                BOOST_STATIC_ASSERT(sizeof(myNumElems) <= sizeof(boost::uint32_t));
                vector<char> myRetVal = serialize(static_cast<boost::uint32_t>(myNumElems));
                foreach (ElemType elem, aVal)
                {
                    myRetVal += serialize(elem.first);
                    myRetVal += serialize(elem.second);
                }
                return myRetVal;
            }

            // throw std::invalid_argument on error
            template <class RetType>
            RetType deserialize(const vector<char>& aVal)
            {
                const vector<char>::size_type mySize = aVal.size();
                if (mySize < sizeof(RetType))
                    TA_THROW_MSG(std::invalid_argument, boost::format("Ill-formed size, actual %u, minimal expected %u") % mySize % sizeof(RetType));
                RetType myResult;
                memcpy(&myResult, ta::getSafeBuf(aVal), sizeof(RetType));
                return myResult;
            }


        } // unnamed ns

        //
        // Public  stuff
        //

        vector<char> serializeSid(boost::uint32_t aVal)
        {
            return serialize(aVal);
        }

        vector<char> serializeLogRequest(ta::LogLevel::val aLogLevel, const string& aLogText)
        {
            BOOST_STATIC_ASSERT(sizeof(ta::LogLevel::val) <= sizeof(boost::uint32_t));
            BOOST_STATIC_ASSERT(sizeof(string::size_type) <= sizeof(boost::uint32_t));
            BOOST_STATIC_ASSERT(sizeof(size_t) <= sizeof(boost::uint32_t));
            size_t mySize = sizeof(boost::uint32_t) + sizeof(ta::LogLevel::val) + aLogText.length() + 1;
            vector<char> myRetVal;
            myRetVal += serialize(static_cast<boost::uint32_t>(mySize));
            myRetVal += serialize(MessageType::Log);
            myRetVal += serialize(static_cast<boost::uint32_t>(aLogLevel));
            myRetVal += serialize(aLogText);
            return myRetVal;
        }

        vector<char> serializeValidateCertRequest()
        {
            size_t mySize = sizeof(boost::uint32_t);
            vector<char> myRetVal;
            myRetVal += serialize(static_cast<boost::uint32_t>(mySize));
            myRetVal += serialize(MessageType::ValidateCert);
            return myRetVal;
        }

        vector<char> serializeDeleteCertRequest()
        {
            size_t mySize = sizeof(boost::uint32_t);
            vector<char> myRetVal;
            myRetVal += serialize(static_cast<boost::uint32_t>(mySize));
            myRetVal += serialize(MessageType::DeleteCert);
            return myRetVal;
        }

        vector<char> serializeLoadAuthUiRequest(const vector<std::pair<string, string> >& aProviderServicePairs, const string& aReqestedUrl)
        {
            vector<char> mySerBody;
            mySerBody += serialize(MessageType::LoadAuthUi);
            mySerBody += serialize(aProviderServicePairs);
            mySerBody += serialize(aReqestedUrl);
            vector<char> myRetVal = serialize(static_cast<boost::uint32_t>(mySerBody.size())) + mySerBody;
            return myRetVal;
        }

        vector<char> serializeBrokerRetVal(boost::uint32_t aRetVal)
        {
            return serialize(aRetVal);
        }

        vector<char> serializeLogResponse(bool aRetVal)
        {
            BOOST_STATIC_ASSERT(sizeof(aRetVal) <= sizeof(boost::uint32_t));
            vector<char> myRetVal = serialize(static_cast<boost::uint32_t>(sizeof(boost::uint32_t)));
            myRetVal += serialize(static_cast<boost::uint32_t>(aRetVal));
            return myRetVal;
        }

        vector<char> serializeValidateCertResponse(int aRetVal)
        {
            BOOST_STATIC_ASSERT(sizeof(aRetVal) <= sizeof(boost::int32_t));
            vector<char> myRetVal = serialize(static_cast<boost::uint32_t>(sizeof(boost::uint32_t)));
            myRetVal += serialize(static_cast<boost::int32_t>(aRetVal));
            return myRetVal;
        }

        vector<char> serializeDeleteCertResponse(int aRetVal)
        {
            BOOST_STATIC_ASSERT(sizeof(aRetVal) <= sizeof(boost::int32_t));
            vector<char> myRetVal = serialize(static_cast<boost::uint32_t>(sizeof(boost::uint32_t)));
            myRetVal += serialize(static_cast<boost::int32_t>(aRetVal));
            return myRetVal;
        }

        vector<char> serializeLoadAuthUiResponse(bool aRetVal, const string& anUrl2Go)
        {
            BOOST_STATIC_ASSERT(sizeof(bool) <= sizeof(boost::int32_t));
            vector<char> myRetVal = serialize(static_cast<boost::uint32_t>(sizeof(boost::uint32_t) + (aRetVal ? anUrl2Go.length() + 1 : 0)));
            myRetVal += serialize(static_cast<boost::int32_t>(aRetVal));
            if (aRetVal)
                myRetVal += serialize(anUrl2Go);
            return myRetVal;
        }

        boost::uint32_t deserializeSid(const vector<char>& aVal)
        {
            return deserialize<boost::uint32_t>(aVal);
        }

        boost::uint32_t deserializeSize(const vector<char>& aVal)
        {
            return deserialize<boost::uint32_t>(aVal);
        }

        boost::uint32_t deserializeBrokerRetVal(const vector<char>& aVal)
        {
            return deserialize<boost::uint32_t>(aVal);
        }

        boost::uint32_t deserializeRequest(const std::vector<char>& aMessage, std::vector<char>& aBody)
        {
            const vector<char>::size_type myMessageSize = aMessage.size();
            static const size_t myMessageTypeSize = sizeof(boost::uint32_t);
            size_t myMinCorrectMessageSize  = myMessageTypeSize;
            if (myMessageSize < myMinCorrectMessageSize)
                TA_THROW_MSG(std::invalid_argument, boost::format("Ill-formed message: size is too small. Actual %u, minimal expected %u") % myMessageSize % myMinCorrectMessageSize);

            boost::uint32_t myMessageType;
            const char* myMessagePtr = ta::getSafeBuf(aMessage);
            memcpy(&myMessageType, myMessagePtr, sizeof(boost::uint32_t));
            myMessagePtr += sizeof(boost::uint32_t);
            aBody.clear();

            switch (myMessageType)
            {
            case MessageType::Log:
            {
                myMinCorrectMessageSize += sizeof(boost::uint32_t);
                if (myMessageSize < myMinCorrectMessageSize)
                    TA_THROW_MSG(std::invalid_argument, boost::format("Ill-formed message: size is too small for the log message. Actual %u, minimal expected %u") % myMessageSize % myMinCorrectMessageSize);
                size_t myLogMsgLen = strlen(myMessagePtr + sizeof(boost::uint32_t));
                myMinCorrectMessageSize += (myLogMsgLen + 1);
                if (myMessageSize < myMinCorrectMessageSize)
                    TA_THROW_MSG(std::invalid_argument, boost::format("Ill-formed message: size is too small for the log message. Actual %u, minimal expected %u") % myMessageSize % myMinCorrectMessageSize);
                aBody.resize(sizeof(boost::uint32_t) + myLogMsgLen + 1);
                memcpy(ta::getSafeBuf(aBody), myMessagePtr, aBody.size());
                return myMessageType;
            }
            case MessageType::ValidateCert:
            {
                return myMessageType;
            }
            case MessageType::DeleteCert:
            {
                return myMessageType;
            }
            case MessageType::LoadAuthUi:
            {
                size_t myNumPairs;
                BOOST_STATIC_ASSERT(sizeof(myNumPairs) <= sizeof(boost::uint32_t));
                memcpy(&myNumPairs, myMessagePtr, sizeof(boost::uint32_t));

                size_t myBodySize = sizeof(boost::uint32_t); // num of pairs;
                const char* p = myMessagePtr + myBodySize;
                for (size_t i=0; i < myNumPairs; ++i)
                {
                    size_t myLen = strlen(p)+1;
                    myBodySize += myLen, p += myLen;
                    myLen = strlen(p)+1;
                    myBodySize += myLen, p += myLen;
                }
                myBodySize += strlen(p)+1;// URL

                aBody.resize(myBodySize);
                memcpy(ta::getSafeBuf(aBody), myMessagePtr, aBody.size());
                return myMessageType;
            }
            default:
                TA_THROW_MSG(std::invalid_argument, boost::format("Unrecognized message type %u") % myMessageType);
            }
        }

        ta::LogLevel::val deserializeLogRequestBody(const vector<char>& aBody, string& aText)
        {
            const vector<char>::size_type myBodySize = aBody.size();
            size_t myMinCorrectBodySize = sizeof(boost::uint32_t);
            if (myBodySize < myMinCorrectBodySize)
                TA_THROW_MSG(std::invalid_argument, boost::format("Ill-formed log body: size is too small. Actual %u, minimal expected %u") % myBodySize % myMinCorrectBodySize);

            boost::uint32_t myLogLevel;
            const char* myBodyPtr = ta::getSafeBuf(aBody);
            memcpy(&myLogLevel, myBodyPtr, sizeof(boost::uint32_t));
            if (!ta::LogLevel::isLogLevel(myLogLevel))
                TA_THROW_MSG(std::invalid_argument, boost::format("Unrecognised log level %u") % myLogLevel);

            myBodyPtr += sizeof(boost::uint32_t);
            size_t myLogTextLen = strlen(myBodyPtr);
            myMinCorrectBodySize += (myLogTextLen + 1);
            if (myBodySize < myMinCorrectBodySize)
                TA_THROW_MSG(std::invalid_argument, boost::format("Ill-formed log body: size is too small. Actual (%u), minimal expected %u") % myBodySize % myMinCorrectBodySize);
            aText.assign(myBodyPtr, myLogTextLen);

            return static_cast<ta::LogLevel::val>(myLogLevel);
        }

        string deserializeLoadAuthUiRequestBody(const vector<char>& aBody, vector<std::pair<string, string> >& aProviderServicePairs)
        {
            typedef vector<std::pair<string, string> > RetType;
            const RetType::size_type mySerSize = aBody.size();
            if (mySerSize < sizeof(boost::uint32_t))
                TA_THROW_MSG(std::invalid_argument, boost::format("Ill-formed size, actual %u, minimal expected %u") % mySerSize % sizeof(boost::uint32_t));

            RetType::size_type myNumPairs;
            BOOST_STATIC_ASSERT(sizeof(myNumPairs) <= sizeof(boost::uint32_t));
            const char* myValPtr = ta::getSafeBuf(aBody);
            memcpy(&myNumPairs, myValPtr, sizeof(boost::uint32_t));
            myValPtr += sizeof(boost::uint32_t);

            string myUrl;
            aProviderServicePairs.clear();

            RetType myRetVal;
            for (RetType::size_type i = 0; i < myNumPairs; ++i)
            {
                string first(myValPtr);
                myValPtr += first.length()+1;
                string second(myValPtr);
                myValPtr += second.length()+1;
                aProviderServicePairs.push_back(std::pair<string, string>(first, second));
            }
            myUrl.assign(myValPtr);
            return myUrl;
        }

        bool deserializeLogResponse(const vector<char>& aBody)
        {
            BOOST_STATIC_ASSERT(sizeof(bool) <= sizeof(boost::uint32_t));
            return !!deserialize<boost::uint32_t>(aBody);
        }

        int deserializeValidateCertResponse(const vector<char>& aBody)
        {
            BOOST_STATIC_ASSERT(sizeof(int) <= sizeof(boost::int32_t));
            return deserialize<boost::int32_t>(aBody);
        }

        int deserializeDeleteCertResponse(const vector<char>& aBody)
        {
            BOOST_STATIC_ASSERT(sizeof(int) <= sizeof(boost::int32_t));
            return deserialize<boost::int32_t>(aBody);
        }

        bool deserializeLoadAuthUiResponse(const vector<char>& aBody, string& anUrl2Go)
        {
            BOOST_STATIC_ASSERT(sizeof(bool) <= sizeof(boost::int32_t));
            bool myRetVal = !!deserialize<boost::int32_t>(aBody);
            if (myRetVal)
                anUrl2Go.assign(&aBody[sizeof(boost::int32_t)]);
            return myRetVal;
        }
    }
}
