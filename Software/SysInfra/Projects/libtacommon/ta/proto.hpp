/**
@brief Contains trivial request-response protocol to communicate over TCP and uses boost::serialization to serialize the data.
*/
#pragma once

#include "ta/tcpclient.h"
#include "ta/tcpserver.h"
#include "ta/common.h"
#include "ta/boost_serialize_helper.hpp"

#include <vector>

namespace ta
{
    namespace proto
    {
        // The protocol is trivial:
        //
        // Request:
        // <request-type><request-size><request>
        // Response:
        // <response-size><response>

        const char PadSymbol = '\0';
        const size_t SizeOfSerializedSize = 128;

        inline void pad(std::vector<char>& aVec, size_t aSize, char aCh)
        {
            aVec.insert(aVec.end(), aSize-aVec.size(), aCh);
        }
        inline void unPad(std::vector<char>& aVec, char aCh)
        {
            if (!aVec.empty())
            {
                std::vector<char>::size_type myVecSize = aVec.size();
                for (std::vector<char>::size_type i = myVecSize-1; i != 0 && aVec[i] == aCh; --i)
                    --myVecSize;
                aVec.resize(myVecSize);
            }
        }

        // Use this function to send the message when the receiver can expect several message types
        template <class T, class MsgT>
        void send(ta::TcpClient& aTcpClient, MsgT aMsgType, const T& aMsg)
        {
            std::vector<char> mySerializedMsg = ta::str2Vec<char>(boost_serialize(aMsg));
            std::vector<char> mySerializedMsgType = ta::str2Vec<char>(boost_serialize(aMsgType));
            pad(mySerializedMsgType, SizeOfSerializedSize, PadSymbol);
            std::vector<char> mySerializedMsgSize = ta::str2Vec<char>(boost_serialize(mySerializedMsg.size()));
            pad(mySerializedMsgSize, SizeOfSerializedSize, PadSymbol);
            mySerializedMsg = mySerializedMsgType + mySerializedMsgSize + mySerializedMsg;

            aTcpClient.sendAll(mySerializedMsg);
        }

        // Use this function to send the message when the receiver can expect only one message type
        template <class T>
        void send(ta::TcpClient& aTcpClient, const T& aMsg)
        {
            std::vector<char> mySerializedMsg = ta::str2Vec<char>(boost_serialize(aMsg));
            std::vector<char> mySerializedMsgSize = ta::str2Vec<char>(boost_serialize(mySerializedMsg.size()));
            pad(mySerializedMsgSize, SizeOfSerializedSize, PadSymbol);
            mySerializedMsg = mySerializedMsgSize + mySerializedMsg;

            aTcpClient.sendAll(mySerializedMsg);
        }

        // Use this function to receive the message type when it is now known apriori
        template <class MsgT>
        MsgT receiveMsgType(ta::TcpClient& aTcpClient)
        {
            std::vector<char> mySerializedMsgType = aTcpClient.receiveAll(SizeOfSerializedSize);
            unPad(mySerializedMsgType, PadSymbol);
            return boost_deserialize<MsgT>(ta::vec2Str(mySerializedMsgType));
        }

        // Use this function to receive the message when the message type is known (either in advance or because receiveMsgType() has just been called)
        template <class T>
        T receive(ta::TcpClient& aTcpClient)
        {
            std::vector<char> mySerializedMsgSize = aTcpClient.receiveAll(SizeOfSerializedSize);
            unPad(mySerializedMsgSize, PadSymbol);
            const size_t myMsgSize =  boost_deserialize<size_t>(ta::vec2Str(mySerializedMsgSize));
            const std::vector<char> mySerializedMsg = aTcpClient.receiveAll(myMsgSize);
            return boost_deserialize<T>(ta::vec2Str(mySerializedMsg));
        }
    }
}
