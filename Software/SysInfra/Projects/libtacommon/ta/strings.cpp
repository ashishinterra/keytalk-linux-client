#include "strings.h"
#include "utils.h"
#include "common.h"

#include <sstream>
#include <stdexcept>
#include <cassert>
#ifdef WIN32
#include <cfloat>
#include <string.h>
#else
#include <cmath>
#endif
#include "boost/cstdint.hpp"
#include "boost/regex.hpp"
#include "boost/algorithm/string.hpp"

#ifdef _WIN32
#define snprintf _snprintf
#endif

using namespace std;


size_t strlcpy(char* dst, const char* src, size_t siz)
{
    char* d = dst;
    const char* s = src;
    size_t n = siz;

    /* Copy as many bytes as will fit */
    if (n != 0) {
        while (--n != 0) {
            if ((*d++ = *s++) == '\0')
                break;
        }
    }

    /* Not enough room in dst, add NUL and traverse rest of src */
    if (n == 0) {
        if (siz != 0)
            *d = '\0';		/* NUL-terminate dst */
        while (*s++)
            ;
    }

    return(s - src - 1);	/* count does not include NUL */
}

namespace ta
{
    namespace Strings
    {

        string toString (int aNumber)
        {
            ostringstream myOs;
            myOs << aNumber;
            return myOs.str();
        }
        string toString (unsigned int aNumber)
        {
            ostringstream myOs;
            myOs << aNumber;
            return myOs.str();
        }
        string toString (unsigned long aNumber)
        {
            ostringstream myOs;
            myOs << aNumber;
            return myOs.str();
        }
        string toString (int64_t aNumber)
        {
            ostringstream myOs;
            myOs << aNumber;
            return myOs.str();
        }
        string toString (double aNumber)
        {
            ostringstream myOs;
#ifdef WIN32
            if (_isnan(aNumber))
#else
            if (isnan(aNumber))
#endif
                myOs << 0;
            else
                myOs << aNumber;
            return myOs.str();
        }

        string toHex(const unsigned char* aBuf, size_t aLen, CharCase aCase)
        {
            if (!aBuf || !aLen)
                return string();
            char* myBuf = new char[2*aLen+1];
            char* myPtr = myBuf;
            for(size_t i = 0; i<aLen; ++i)
            {
                if (aCase == caseLower)
                    snprintf(myPtr, 3, "%02x", aBuf[i]);
                else
                    snprintf(myPtr, 3, "%02X", aBuf[i]);
                myPtr += 2;
            }
            string myRetVal(myBuf, 2*aLen);
            delete []myBuf;
            return myRetVal;
        }
        string toHex(const vector<unsigned char>& aBuf, CharCase aCase)
        {
            return toHex(getSafeBuf(aBuf), aBuf.size(), aCase);
        }

        vector<unsigned char> fromHex(const string& aHexStr)
        {
            vector<unsigned char> myRetVal;
            if (aHexStr.empty())
                return myRetVal;
            if (aHexStr.size() % 2)
                TA_THROW_MSG(std::invalid_argument, boost::format("Odd number of bytes in hex string '%1%'") % aHexStr );

            static const char hexSmall[] = "0123456789abcdef";
            static const char hexBig  [] = "0123456789ABCDEF";
            myRetVal.reserve(aHexStr.size() / 2);
            const char* myHexBufPtr = aHexStr.c_str();
            while (*myHexBufPtr)
            {
                boost::int8_t myHiWord;
                const char* myHiWordPtr = strchr(hexSmall, *myHexBufPtr);
                if (myHiWordPtr)
                {
                    myHiWord = (boost::int8_t)(myHiWordPtr - hexSmall);
                }
                else
                {
                    myHiWordPtr = strchr(hexBig, *myHexBufPtr);
                    if (!myHiWordPtr)
                        TA_THROW_MSG(std::invalid_argument, boost::format("Invalid hex string: '%1%'") % aHexStr);
                    myHiWord = (boost::int8_t)(myHiWordPtr - hexBig);
                }
                boost::int8_t myLoWord;
                const char* myLoWordPtr = strchr(hexSmall, *(myHexBufPtr+1));
                if (myLoWordPtr)
                {
                    myLoWord = (boost::int8_t)(myLoWordPtr - hexSmall);
                }
                else
                {
                    myLoWordPtr = strchr(hexBig, *(myHexBufPtr+1));
                    if (!myLoWordPtr)
                        TA_THROW_MSG(std::invalid_argument, boost::format("Invalid hex string: %1%") % aHexStr);
                    myLoWord = (boost::int8_t)(myLoWordPtr - hexBig);
                }
                myRetVal.push_back((myHiWord << 4) | myLoWord);
                myHexBufPtr += 2;
            }
            assert(myRetVal.size() == aHexStr.size() / 2);
            return myRetVal;
        }

        vector<string> split(const string& anSrc, char aSep, AdjacentSepsMergeMode anAdjacentSepsMergeMode, EmptyTokensPolicy anEmptyTokensPolicy)
        {
            return split(anSrc, vector<char>(1, aSep), anAdjacentSepsMergeMode, anEmptyTokensPolicy);
        }

        vector<string> split(const std::string& anSrc, const std::vector<char>& aSeps, AdjacentSepsMergeMode anAdjacentSepsMergeMode, EmptyTokensPolicy anEmptyTokenPolicy)
        {
            if (aSeps.empty())
            {
                TA_THROW_MSG(std::logic_error, "At least one separator required");
            }

            vector<string> myParts;
            const string mySepsStr = vec2Str(aSeps);
            string::size_type myFrom = 0, mySepPos = 0;
            while ((mySepPos = anSrc.find_first_of(mySepsStr, myFrom)) != string::npos)
            {
                if (anAdjacentSepsMergeMode == sepsMergeOff || mySepPos != myFrom)
                {
                    myParts.push_back(anSrc.substr(myFrom, mySepPos-myFrom));
                }
                myFrom = mySepPos+1;
            }
            myParts.push_back(anSrc.substr(myFrom));

            if (anEmptyTokenPolicy == emptyTokensDrop)
            {
                myParts = filterOutWhen(&string::empty, myParts);
            }

            return myParts;
        }

        string join(const vector<string>& aList, char aSep, EmptyStringsPolicy anEmptyStringsPolicy)
        {
            return join(aList, string(1, aSep), anEmptyStringsPolicy );
        }

        string join(const vector<string>& aList, const string& aSep, EmptyStringsPolicy anEmptyStringsPolicy)
        {
            string myRetVal;
            bool isPopulated = false;

            foreach (const string& elem, aList)
            {
                if (elem.empty() && anEmptyStringsPolicy == emptyStringsSkip)
                {
                    continue;
                }

                if (!isPopulated)
                {
                    myRetVal = elem;
                    isPopulated = true;
                }
                else
                {
                    myRetVal += aSep + elem;
                }
            }
            return myRetVal;
        }

        string join(const set<string>& aList, char aSep, EmptyStringsPolicy anEmptyStringsPolicy)
        {
            return join(ta::set2Vec(aList), aSep, anEmptyStringsPolicy);
        }

        string join(const set<string>& aList, const string& aSep, EmptyStringsPolicy anEmptyStringsPolicy)
        {
            return join(ta::set2Vec(aList), aSep, anEmptyStringsPolicy);
        }

        std::string join(const std::vector<int>& aList, char aSep)
        {
            return join(aList, string(1, aSep) );
        }

        std::string join(const std::vector<int>& aList, const std::string& aSep)
        {
            string myRetVal;
            foreach (int elem, aList)
            {
                if (myRetVal.empty())
                    myRetVal = toString(elem);
                else
                    myRetVal += aSep + toString(elem);
            }
            return myRetVal;
        }

        std::string join(const std::vector<unsigned int>& aList, char aSep)
        {
            return join(aList, string(1, aSep) );
        }

        std::string join(const std::vector<unsigned int>& aList, const std::string& aSep)
        {
            string myRetVal;
            foreach (unsigned int elem, aList)
            {
                if (myRetVal.empty())
                    myRetVal = toString(elem);
                else
                    myRetVal += aSep + toString(elem);
            }
            return myRetVal;
        }



        namespace
        {
            struct TemplatePart
            {
                TemplatePart(bool aQuoted, const string& aVal): quoted(aQuoted), val(aVal) {}
                bool quoted; // whether the value is quoted as $${...} i.e. should not be substituted
                std::string val;
            };

            void verifyMappingKeys(const StringDict& aMappings)
            {
                static const std::locale& loc = std::locale::classic();
                foreach (const StringDict::value_type& mapping, aMappings)
                {
                    const std::string myKey = mapping.first;
                    foreach (char ch, myKey)
                    {
                        if (!std::isalnum(ch, loc) && ch != '_' && ch != '-')
                            TA_THROW_MSG(std::invalid_argument, boost::format("Invalid template key %s. Template keys can only contain alphanumeric characters, '_' or '-'") % myKey);
                    }
                }
            }

            std::string buildOrRegexStrFromKeys(const StringDict& aMappings)
            {
                std::string myRegexStr;
                foreach (const StringDict::value_type& mapping, aMappings)
                {
                    if (!myRegexStr.empty())
                        myRegexStr += "|";
                    myRegexStr += regexEscapeStr(mapping.first);
                }
                return myRegexStr;
            }


            struct SubstPlaceholderCb
            {
                SubstPlaceholderCb(const StringDict& aMappings): mappings(aMappings) {}

                std::string operator()(boost::match_results<std::string::const_iterator> aMatch)
                {
                    const string myPlaceholder = aMatch[0];
                    foreach (const StringDict::value_type& mapping, mappings)
                    {
                        if (myPlaceholder == "$(" + mapping.first + ")")
                            return mapping.second;
                    }
                    return myPlaceholder; // no-op
                }
                const StringDict mappings;
            };

        } // unnamed ns

        std::string substTemplate(const std::string& aTempl, const StringDict& aMappings)
        {
            if (aMappings.empty())
                return aTempl;
            verifyMappingKeys(aMappings);

            // Split template strings on $${mapping-name}
            std::vector<TemplatePart> mySplitTemplate;
            const std::string myKeysOrRegexStr = buildOrRegexStrFromKeys(aMappings);
            const std::string mySplitRegExStr = str(boost::format("(.*?)(\\$\\$\\((?:%s)\\))") % myKeysOrRegexStr);
            boost::regex mySplitRegEx(mySplitRegExStr);
            boost::match_results<string::const_iterator> myMatch;
            std::string::const_iterator myBeg = aTempl.begin(), myEnd = aTempl.end();
            while (regex_search(myBeg, myEnd, myMatch, mySplitRegEx))
            {
                assert(myMatch.size() == 3);
                const std::string myNonQuoted = myMatch[1];
                const std::string myQuoted = myMatch[2];
                if (!myNonQuoted.empty())
                    mySplitTemplate.push_back(TemplatePart(false, myNonQuoted));
                mySplitTemplate.push_back(TemplatePart(true, myQuoted));
                myBeg = myMatch[0].second;
            }
            if (myBeg != myEnd)
            {
                const std::string myRemaining = aTempl.substr(std::distance(aTempl.begin(), myBeg));
                mySplitTemplate.push_back(TemplatePart(false, myRemaining));
            }

            // Perform substitution for each split part and join the substituted parts back
            std::string myRetVal;
            foreach (TemplatePart& part, mySplitTemplate)
            {
                if (part.quoted)
                {
                    // Drop leading '$'
                    assert(!part.val.empty() && part.val[0] == '$');
                    myRetVal += part.val.substr(1);
                }
                else
                {
                    // Just do normal substitution
                    SubstPlaceholderCb mySubstPlaceholderCb(aMappings);
                    const std::string myRegExStr = str(boost::format("\\$\\((?:%s)\\)") % myKeysOrRegexStr);
                    const boost::regex myRegEx(myRegExStr);
                    myRetVal += regex_replace(part.val, myRegEx, mySubstPlaceholderCb, boost::format_all);
                }
            }
            return myRetVal;
        }

        StringSet parseTemplate(const std::string& aTempl)
        {
            StringSet result;

            bool myReadPattern = false;
            size_t i = 0;
            while (i < aTempl.size())
            {
                switch ( aTempl[i] )
                {
                case '$':
                    myReadPattern = !myReadPattern;
                    break;
                case '(':
                    if (myReadPattern)
                    {
                        size_t myEnd = aTempl.find(')', i+1);
                        if (myEnd == string::npos)
                            i = aTempl.size();
                        else
                        {
                            result.insert(aTempl.substr(i+1, myEnd-i-1));
                            i = myEnd;
                        }
                    }
                // intentional fall-through to reset myReadPattern
                default:
                    myReadPattern = false;
                }
                i++;
            }
            return result;
        }

    }// namespace Strings
} // namespace ta
