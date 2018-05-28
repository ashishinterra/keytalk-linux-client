#pragma once

#include "ta/common.h"

#include <vector>
#include <string>
#include <stdexcept>

#ifdef _WIN32
// suppress misleading "not all control paths return a value" warning in boost::property_tree produced by MSVC
#pragma warning (disable: 4715)
#endif
#include "boost/property_tree/json_parser.hpp"
#ifdef _WIN32
#pragma warning (default: 4715)
#endif

namespace ta
{
    struct EncodeError : std::runtime_error
    {
        explicit EncodeError(const std::string& aMessage = "")	: std::runtime_error(aMessage) {}
    };

    struct DecodeError : std::runtime_error
    {
        explicit DecodeError(const std::string& aMessage = "")	: std::runtime_error(aMessage) {}
    };

    namespace EncodingUtils
    {
        /**
          Encode to Base64 format

          @param[in] anSrc String which needs to be encoded
          @param[in] aSingleLine If true, mimics the behavior of 'openssl base64 -e -A' otherwise 'openssl base64 -e'
          @return Encoded string
          @throw EncodeError
         */
        std::string toBase64(const std::vector<unsigned char>& anSrc, bool aSingleLine = false);

        /**
          Decode from Base64 format

          @param[in] anSrc String which needs to be decoded
          @param[in] aSingleLine If true, mimics the behavior of 'openssl base64 -d -A' otherwise 'openssl base64 -d'
          @return Decoded string
          @throw DecodeError
         */
        std::vector<unsigned char> fromBase64(const std::string& anSrc, bool aSingleLine = false);

        /**
          URL-encode the string using the %xx escape. The resulted hex characters are upper-case.
          The following characters are not encoded: 'A'-'Z', 'a'-'z', '0'-'9', '_', '.', '-', '/'
          ' ' gets encoded to '+'
          @param[in] anSrc String which needs to be encoded
          @return encoded string
         */
        std::string urlEncode(const std::string& anSrc);


        /**
          URL-decode the string. Accepts both lower and upper case hex characters.

          @param[in] anSrc String which needs to be decoded. @see urlEncode
          @return encoded string
         */
        std::string urlDecode(const std::string& anSrc);

        /**
          Data-URI encode the PNG image BLOB
         */
        std::string dataUriEncodePngImage(const std::vector<unsigned char>& aPngBlob);


        /**
          Data-URI decode the encoded PMG image
         */
        std::vector<unsigned char> dataUriDecodePngImage(const std::string& anEncodedPng);

        /**
          Encode ASCII-subset of special HTML entities, that is &quot;&amp;&lt;&gt;

          @param[in] anSrc String which needs to be encoded
          @return Encoded string
         */
        std::string htmlEncode(const std::string& anSrc);

        // @throw EncodeError
        std::wstring toWide(const std::string& aStr);

        //@throw EncodeError
        std::string  toMbyte(const std::wstring& aWstr);

        // property tree encoding/decoding routines
        boost::property_tree::ptree toTree(const ta::StringArray& anArray);
        boost::property_tree::ptree toTree(const ta::StringDict& aDict);
        boost::property_tree::ptree toTree(const ta::StringDictArray& aStringDictArray);
        std::vector<std::string> toStringArray(const boost::property_tree::ptree& aTree);
        ta::StringDict toStringDict(const boost::property_tree::ptree& aTree);
        ta::StringDictArray toStringDictArray(const boost::property_tree::ptree& aTree);

        // JSON serialization routines
        // - during serialization forward slashes '/' are escaped as '\/'
        // - in JSON object names (corresponding to std::map keys) dot '.' is interpreted for data member access
        std::string toJson(const ta::StringArray& anArray);
        std::string toJson(const ta::StringDict& aStringDict);
        std::string toJson(const ta::StringDictArray& aStringDictArray);
        std::string toJson(const boost::property_tree::ptree& aTree);
        std::vector<std::string> jsonToStringArray(const std::string& aJson);
        ta::StringDict jsonToStringDict(const std::string& aJson);
        ta::StringDictArray jsonToStringDictArray(const std::string& aJson);
        boost::property_tree::ptree jsonToTree(const std::string& aJson);
    }
}
