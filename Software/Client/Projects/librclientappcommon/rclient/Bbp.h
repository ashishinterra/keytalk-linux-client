//----------------------------------------------------------------------------
//
//  Description : BBP protocol for communicating between RESEPT IE BHO and RESEPT Broker
//
//
//  Broker  --> TCP connect  --> BHO
//  Broker: --> Sync         --> BHO
//  BHO:    --> Request      --> Broker
//  Broker: --> BrokerRetVal --> BHO
// [Broker: --> Response     --> BHO]
//
// Message schemas:
//
// Sync: <SID>
//    SID: 4 byte unsigned Session Id
//
// Request: <size><request>
//   size: 4 byte unsigned request size
//   request: <type><body>
//     type: 4 byte unsigned request type (see MessageType)
//   body:
//     type == MessageType::Log: <level><txt>
//       level: 4 byte unsigned logging level
//       txt:   zero-terminated ASCII log string
//     type == MessageType::ValidateCert: nothing
//     type == MessageType::DeleteCert: <deleteInvalidOnly>
//       deleteInvalidOnly: 4 byte unsigned flag indicating whether delete only invalid certs
//     type == MessageType::LoadAuthUi: <clntserviceNums><reqUrl>
//       clntcustNums: serialized vector of client/service pairs
//       reqUrl: zero-terminated ASCII requested URL string
//
// Response: <retval>[<response>]
//   retval: 4 byte unsigned success flag indicating whether the broker succeeded invoking the requested (see BrokerRetVal; do not confuse it with the retval from the requested command itself!)
//           if retval equals Ok , the response follows:
//   For MessageType::Log: <size><retval>
//           size: 4 byte unsigned response size
//           retval: 4 byte unsigned long success flag
//   For MessageType::ValidateCert: <size><retval>
//           size: 4 byte unsigned response size
//           retval: 4 byte signed retval. retval >= 0 indicates is a number of valid certs, otherwise error
//   For MessageType::DeleteCerts: <size><retval>
//           size: 4 byte unsigned response size
//           retval: 4 byte signed retval. retval >= 0 indicates is a number of deleted certs, otherwise error
//   For MessageType::LoadAuthUi: <size><retval>[<url2Go>]
//           size: 4 byte unsigned response size
//           retval: 4 byte unsigned long success flag
//           url2Go: (only if retval is not 0) zero-terminated ASCII URL string for a browser to proceed with
//
//
//----------------------------------------------------------------------------
#ifndef RCLIENT_BBP_H
#define RCLIENT_BBP_H

#include "ta/logappender.h"
#include "boost/cstdint.hpp"
#include <string>
#include <vector>
#include <utility>

namespace rclient
{
    namespace bbp
    {
        namespace MessageType
        {
            const boost::uint32_t Log            = 0;
            const boost::uint32_t ValidateCert   = 1;
            const boost::uint32_t DeleteCert     = 2;
            const boost::uint32_t LoadAuthUi     = 3;
        }
        namespace BrokerRetVal
        {
            const boost::uint32_t Ok           = 0;
            const boost::uint32_t BrokerError  = 1;
            const boost::uint32_t UnsupportedRequest = 2;
        }

        //
        // Serialization/deserialization
        //
        // Usage:
        //   SID:
        //     Broker: serializeSid -> sendAll -- TCP --> receiveAll(4 bytes) -> deserializeSize -> deserializeSid -> deserializeXyzBody
        //
        //   Request:
        //     BHO: serializeXyzRequest -> sendAll -- TCP --> receiveAll(4 bytes) -> deserializeSize -> reseiveAll(size) -> deserializeRequest -> deserializeXyzRequestBody
        //
        //   Response:
        //     Broker: serializeBrokerRetVal ->sendAll -> [if Broker ok then: serializeXyzResponse -> sendAll ] -- TCP --> receiveAll(4 bytes) -> deserializeBrokerRetVal [ if retval os ok: -> deserializeSize() -> receiveAll(size) -> deserializeXyzResponse ]
        //


        std::vector<char> serializeSid(boost::uint32_t aVal);
        std::vector<char> serializeLogRequest(ta::LogLevel::val aLogLevel, const std::string& aLogText);
        std::vector<char> serializeValidateCertRequest();
        std::vector<char> serializeDeleteCertRequest();
        std::vector<char> serializeLoadAuthUiRequest(const std::vector<std::pair<std::string, std::string> >& aProviderServicePairs, const std::string& aReqestedUrl);
        std::vector<char> serializeBrokerRetVal(boost::uint32_t aRetVal);
        std::vector<char> serializeLogResponse(bool aRetVal);
        std::vector<char> serializeValidateCertResponse(int aRetVal);
        std::vector<char> serializeDeleteCertResponse(int aRetVal);
        std::vector<char> serializeLoadAuthUiResponse(bool aRetVal, const std::string& anUrl2Go);


        //
        // All deserialization functions throw std::invalid_argument on error
        //

        boost::uint32_t deserializeSid(const std::vector<char>& aVal);
        boost::uint32_t deserializeSize(const std::vector<char>& aVal);
        boost::uint32_t deserializeBrokerRetVal(const std::vector<char>& aVal);
        boost::uint32_t deserializeRequest(const std::vector<char>& aMessage, std::vector<char>& aBody);
        ta::LogLevel::val deserializeLogRequestBody(const std::vector<char>& aBody, std::string& aText);
        std::string deserializeLoadAuthUiRequestBody(const std::vector<char>& aBody, std::vector<std::pair<std::string, std::string> >& aProviderServicePairs);
        bool deserializeLogResponse(const std::vector<char>& aBody);
        int deserializeValidateCertResponse(const std::vector<char>& aBody);
        int deserializeDeleteCertResponse(const std::vector<char>& aBody);
        bool deserializeLoadAuthUiResponse(const std::vector<char>& aBody, std::string& anUrl2Go);
    }
}

#endif
