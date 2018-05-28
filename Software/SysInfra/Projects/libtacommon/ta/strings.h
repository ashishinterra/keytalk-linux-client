#pragma once

#include "ta/common.h"

#include <string>
#include <vector>
#include <set>

size_t strlcpy(char* dst, const char* src, size_t siz);

namespace ta
{
    namespace Strings
    {
        /**
          Convert number to a string
         */
        std::string toString (int aNumber);
        std::string toString (unsigned int aNumber);
        std::string toString (unsigned long aNumber);
        std::string toString (int64_t aNumber);
        std::string toString (double aNumber);

        /**
          Parse string as a target type
          When parsing integer, integer overflow is not allowed i.e. parse<int>("2147483648") will throw exception
          When parsing unsigned integer, neither integer overflow nor integer promotion are allowed i.e. parse<unsigned int>("4294967296") and parse<unsigned int>("-1") will throw exception

          @param[in] aVal string value to be parsed
         */
        template<typename Target>
        Target parse(const std::string& aVal);

        enum CharCase
        {
            caseUpper, caseLower
        };
        /**
          Convert buffer to hex representation

          @param[in] aBuf Hex value to be converted
          @param[in] aLen Length of hex value
          @param[in] aCase desired case of Hex letters
         */
        std::string toHex(const unsigned char* aBuf, size_t aLen, CharCase aCase = caseLower);

        /**
          Convert buffer to hex representation

          @param[in] aBuf Hex value to be converted
          @param[in] aCase desired case of Hex letters
         */
        std::string toHex(const std::vector<unsigned char>& aBuf, CharCase aCase = caseLower);

        /**
          Convert from hex representation

          @param[in] aHexStr String format of hex value
          @return Hex value
          @throw std::invalid_argument if the input is not a correct HEX string
         */
        std::vector<unsigned char> fromHex(const std::string& aHexStr);

        /**
         Split functions. Somewhat more handy than the ones from boost string algorithms
        */
        enum AdjacentSepsMergeMode
        {
            sepsMergeOff,     // split("ab;;cd", ';') -> ["ab", "", "cd"]
            sepsMergeOn       // split("ab;;cd", ';') -> ["ab", "cd"]
        };
        enum EmptyTokensPolicy
        {
            emptyTokensPreserve,  // split(";ab;", ';') -> ["", "ab", ""]
            // split("", ';') -> [""]
            emptyTokensDrop       // split(";ab;", ';') -> ["ab"]
            // split("", ';') -> []
        };
        std::vector<std::string> split(const std::string& anSrc, char aSep,
                                       AdjacentSepsMergeMode anAdjacentSepsMergeMode = sepsMergeOff,
                                       EmptyTokensPolicy anEmptyTokensPolicy = emptyTokensPreserve);
        ///@throw std::logic_error if separators array is empty
        std::vector<std::string> split(const std::string& anSrc, const std::vector<char>& aSeps,
                                       AdjacentSepsMergeMode anAdjacentSepsMergeMode = sepsMergeOff,
                                       EmptyTokensPolicy anEmptyTokensPolicy = emptyTokensPreserve);

        enum EmptyStringsPolicy
        {
            emptyStringsPreserve,  // join(["a", '', "c"], ',') -> "a,,c"
            emptyStringsSkip       // join(["a", '', "c"], ',') -> "a,c"
        };
        std::string join(const std::vector<std::string>& aList, char aSep, EmptyStringsPolicy anEmptyStringsPolicy = emptyStringsPreserve);
        std::string join(const std::vector<std::string>& aList, const std::string& aSep, EmptyStringsPolicy anEmptyStringsPolicy = emptyStringsPreserve);
        std::string join(const std::set<std::string>& aList, char aSep, EmptyStringsPolicy anEmptyStringsPolicy = emptyStringsPreserve);
        std::string join(const std::set<std::string>& aList, const std::string& aSep, EmptyStringsPolicy anEmptyStringsPolicy = emptyStringsPreserve);
        std::string join(const std::vector<int>& aList, char aSep);
        std::string join(const std::vector<int>& aList, const std::string& aSep);
        std::string join(const std::vector<unsigned int>& aList, char aSep);
        std::string join(const std::vector<unsigned int>& aList, const std::string& aSep);

        /**
          Performs template substitution
          @param aTempl Substitute the template like "$(who) likes $(what)"
          @param aMappings mappings between placeholder name and values e.g. {"who":"Jos", "what": "Laphroaig"} will produce "Jos likes Laphroaig"
          Mapping names are case-sensitive.
          To output the template part that occasionally matches one of the mapping keys quote it by doubling the dollar sign like: "$$(what)"
          All placeholders specified in aTempl and missing in aMappings will appear in the resulting string intact
          @pre mapping keys should be ASCII strings consisting of alphanumeric ASCII symbols, "_" or "-"
          @throw std::invalid_argument precondition is not satisfied
        */
        std::string substTemplate(const std::string& aTempl, const StringDict& aMappings);

        /**
           Extract all placeholder strings from template.

           @param aTempl The template to search for placeholders. A placeholder is of the form $([^)]*)
           The $ symbol can be escaped by doubling, e.g. $$(userid).
           @return a set of all placeholder strings excluding the dollar and parentheses.

           Complexity: O(n log(n)), where n = aTempl.size()
         */
        ta::StringSet parseTemplate(const std::string& aTempl);

    }
}

#include "stringsimpl.hpp"
