#include "util.h"
#include "common.h"
#include "utf8.h"
#include "ta/hashutils.h"
#include "ta/strings.h"
#include "ta/utils.h"

#include "boost/algorithm/string.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/assign/list_of.hpp"
#include <vector>
#include <locale>

namespace resept
{
    std::string calcResponse(const std::string& aUserId, const std::string& aChallenge)
    {
        static const unsigned int Sha1HexLen = 40;
        static const unsigned int ReponseLen = 8;
        BOOST_STATIC_ASSERT(Sha1HexLen <= resept::MaxResponseLength);
        BOOST_STATIC_ASSERT(ReponseLen <= resept::MaxResponseLength);
        std::string myDigest = ta::HashUtils::getSha1Hex(aUserId + aChallenge);
        myDigest = myDigest.substr(0, ReponseLen);
        return boost::algorithm::to_upper_copy(myDigest);
    }

    ta::StringDict calcGsmResponses(const std::string& aUserId, const ta::StringDict& aChallenges, const std::vector<std::string>& aResponseNames)
    {
        const std::string ExpectedUserId = GsmUserName;
        const size_t ExpecteAmountResponses = 2;

        if (aUserId != ExpectedUserId)
            TA_THROW_MSG(std::invalid_argument, boost::format("Invalid user name '%s' supplied for GSM authentication") % aUserId);

        const ta::StringDict::const_iterator myRandChallIt = aChallenges.find(GsmRandomChallengeName);
        if (myRandChallIt == aChallenges.end())
            TA_THROW_MSG(std::invalid_argument, "No \"" + GsmRandomChallengeName + "\" found in GSM challenge");
        if (aResponseNames.size() != ExpecteAmountResponses)
            TA_THROW_MSG(std::invalid_argument, boost::format("Invalid number of responses requested for GSM challenge. Actual %d, expected %d") % aResponseNames.size() % ExpecteAmountResponses);

        const std::string myRandomChallengeVal = myRandChallIt->second;
        if (myRandomChallengeVal.empty())
            TA_THROW_MSG(std::invalid_argument, GsmRandomChallengeName + " challenge cannot be empty");

        if (myRandomChallengeVal == GsmRandomChallenge1Value)
        {
            ta::StringDict myResponses = boost::assign::map_list_of(aResponseNames[0], "d1d2d3d4")(aResponseNames[1], "a0a1a2a3a4a5a6a7");
            return myResponses;
        }
        else if (myRandomChallengeVal == GsmRandomChallenge2Value)
        {
            ta::StringDict myResponses = boost::assign::map_list_of(aResponseNames[0], "e1e2e3e4")(aResponseNames[1], "b0b1b2b3b4b5b6b7");
            return myResponses;
        }
        else if (myRandomChallengeVal == GsmRandomChallenge3Value)
        {
            ta::StringDict myResponses = boost::assign::map_list_of(aResponseNames[0], "f1f2f3f4")(aResponseNames[1], "c0c1c2c3c4c5c6c7");
            return myResponses;
        }
        else
        {
            TA_THROW_MSG(std::invalid_argument, "Unexpected value of \"" + GsmRandomChallengeName + "\" challenge");
        }
    }

    ta::StringDict calcUmtsResponses(const std::string& aUserId, const ta::StringDict& aChallenges, const std::vector<std::string>& aResponseNames)
    {
        const std::string ExpectedUserId = UmtsUserName;
        const size_t ExpecteAmountResponses = 3;

        if (aUserId != ExpectedUserId)
            TA_THROW_MSG(std::invalid_argument, boost::format("Invalid user name '%s' supplied for UMTS authentication") % aUserId);

        const ta::StringDict::const_iterator myRandChallIt = aChallenges.find(UmtsRandomChallengeName);
        if (myRandChallIt == aChallenges.end())
            TA_THROW_MSG(std::invalid_argument, "No \""+UmtsRandomChallengeName+"\" found in UMTS challenge");
        if (aChallenges.find(UmtsAutnChallengeName) == aChallenges.end())
            TA_THROW_MSG(std::invalid_argument, "No \""+UmtsAutnChallengeName+"\" found in UMTS challenge");

        if (aResponseNames.size() != ExpecteAmountResponses)
            TA_THROW_MSG(std::invalid_argument, boost::format("Invalid number of responses requested for UMTS challenge. Actual %d, expected %d") % aResponseNames.size() % ExpecteAmountResponses);

        const std::string myRandomChallengeVal = myRandChallIt->second;
        if (myRandomChallengeVal.empty())
            TA_THROW_MSG(std::invalid_argument, UmtsRandomChallengeName + " challenge cannot be empty");

        if (myRandomChallengeVal == UmtsRandomChallengeValue)
        {
            ta::StringDict myResponses = boost::assign::map_list_of(aResponseNames[0], "02020202020202020202020202020202")
                                         (aResponseNames[1], "03030303030303030303030303030303")
                                         (aResponseNames[2], "04040404040404040404040404040404");
            return myResponses;
        }
        else
        {
            TA_THROW_MSG(std::invalid_argument, "Unexpected value of \""+UmtsRandomChallengeName+"\" challenge");
        }
    }

    std::string calcOtp(const std::string& anInitialSecret, const std::string& aPincode)
    {
        const long myUtcDecaSeconds = std::ldiv((long)time(NULL), 10).quot;
        const std::string myOtp = str(boost::format("%lu%s%s")% myUtcDecaSeconds % anInitialSecret % aPincode);
        return ta::HashUtils::getMd5Hex(myOtp).substr(0, 6);
    }

    bool isValidProviderName(const std::string& aProviderName, std::string& anErrorMsg, Capitalize aCapitalizeErrorMsg)
    {
        if (boost::trim_copy(aProviderName).empty())
        {
            anErrorMsg = "Provider name cannot be empty";
            if (aCapitalizeErrorMsg == capitalizeYes)
                boost::to_upper(anErrorMsg);
            return false;
        }
        if (aProviderName.length() > MaxProviderLength)
        {
            anErrorMsg = str(boost::format("Provider name can contain at most %u characters") % MaxProviderLength);
            if (aCapitalizeErrorMsg == capitalizeYes)
                boost::to_upper(anErrorMsg);
            return false;
        }
        static const std::locale& loc = std::locale::classic();
        foreach (char ch, aProviderName)
        {
            if (!std::isalnum(ch, loc) && ch != '_' && ch != '-')
            {
                anErrorMsg = "Provider may only contain English alphanumeric characters, '_' or '-'";
                if (aCapitalizeErrorMsg == capitalizeYes)
                    boost::to_upper(anErrorMsg);
                return false;
            }
        }
        foreach(const std::string& reservedName, ReservedProviderNames)
        {
            if (boost::trim_copy(aProviderName) == reservedName)
            {
                anErrorMsg = str(boost::format("Provider name may not be the reserved word '%s'") % reservedName);
                return false;
            }
        }
        return true;
    }

    bool isValidServiceName(const std::string& aServiceName, std::string& anErrorMsg, Capitalize aCapitalizeErrorMsg)
    {
        if (boost::trim_copy(aServiceName).empty())
        {
            anErrorMsg = "Service name cannot be empty";
            if (aCapitalizeErrorMsg == capitalizeYes)
                boost::to_upper(anErrorMsg);
            return false;
        }
        if (aServiceName.length() > MaxServiceLength)
        {
            anErrorMsg = str(boost::format("Service name can contain at most %u characters") % MaxServiceLength);
            if (aCapitalizeErrorMsg == capitalizeYes)
                boost::to_upper(anErrorMsg);
            return false;
        }
        static const std::locale& loc = std::locale::classic();
        foreach (char ch, aServiceName)
        {
            if (!std::isalnum(ch, loc) && ch != '_' && ch != '-')
            {
                anErrorMsg = "Service may only contain English alphanumeric characters, '_' or '-'";
                if (aCapitalizeErrorMsg == capitalizeYes)
                    boost::to_upper(anErrorMsg);
                return false;
            }
        }
        return true;
    }

    bool isValidPassword(const std::string& anUtf8Password, std::string& anErrorMsg, Capitalize aCapitalizeErrorMsg)
    {
        try
        {
            if (static_cast<size_t>(utf8::distance(anUtf8Password.begin(), anUtf8Password.end())) > MaxPasswordLength)
            {
                anErrorMsg = str(boost::format("Password can contain at most %u characters") % MaxPasswordLength);
                if (aCapitalizeErrorMsg == capitalizeYes)
                    boost::to_upper(anErrorMsg);
                return false;
            }
        }
        catch (std::exception&)
        {
            // utf8::distance may fail if the given string is not a valid UTF-8 encoded string e.g. some binary garbage
            if (aCapitalizeErrorMsg == capitalizeYes)
                boost::to_upper(anErrorMsg);
            anErrorMsg = "Password should contain only printable characters";
            return false;
        }
        return true;
    }

    bool isValidResponse(const std::string& anUtf8Response, std::string& anErrorMsg, Capitalize aCapitalizeErrorMsg)
    {
        try
        {
            if (static_cast<size_t>(utf8::distance(anUtf8Response.begin(), anUtf8Response.end())) > MaxResponseLength)
            {
                anErrorMsg = str(boost::format("Response can contain at most %u characters") % MaxResponseLength);
                if (aCapitalizeErrorMsg == capitalizeYes)
                    boost::to_upper(anErrorMsg);
                return false;
            }
        }
        catch (std::exception&)
        {
            // utf8::distance may fail if the given string is not a valid UTF-8 encoded string e.g. some binary garbage
            anErrorMsg = "Response should contain only printable characters";
            if (aCapitalizeErrorMsg == capitalizeYes)
                boost::to_upper(anErrorMsg);
            return false;
        }
        return true;
    }

    bool isValidPincode(const std::string& aPincode, std::string& anErrorMsg, Capitalize aCapitalizeErrorMsg)
    {
        if (aPincode.length() > MaxPincodeLength)
        {
            anErrorMsg = str(boost::format("Pincode can contain at most %u characters") % MaxPincodeLength);
            if (aCapitalizeErrorMsg == capitalizeYes)
                boost::to_upper(anErrorMsg);
            return false;
        }
        return true;
    }

    bool isValidUserName(const std::string& anUtf8UserName, std::string& anErrorMsg, Capitalize aCapitalizeErrorMsg)
    {
        if (boost::trim_copy(anUtf8UserName).empty())
        {
            anErrorMsg = "User name cannot be empty";
            if (aCapitalizeErrorMsg == capitalizeYes)
                boost::to_upper(anErrorMsg);
            return false;
        }
        try
        {
            if (static_cast<size_t>(utf8::distance(anUtf8UserName.begin(), anUtf8UserName.end())) > MaxUserIdLength)
            {
                anErrorMsg = str(boost::format("User name can contain at most %u characters") % MaxUserIdLength);
                if (aCapitalizeErrorMsg == capitalizeYes)
                    boost::to_upper(anErrorMsg);
                return false;
            }
        }
        catch (std::exception&)
        {
            // utf8::distance may fail if the given string is not a valid UTF-8 encoded string e.g. some binary garbage
            anErrorMsg = "User name should contain only printable characters";
            if (aCapitalizeErrorMsg == capitalizeYes)
                boost::to_upper(anErrorMsg);
            return false;
        }
        return true;
    }

    bool isValidRsaKeyBitLen(const int aKeySizeBit, std::string& anErrorMsg, Capitalize aCapitalizeErrorMsg)
    {
        if (aKeySizeBit <= 0 || (aKeySizeBit % 1024) != 0)
        {
            anErrorMsg = "Invalid RSA key bit length (multiple of 1024 expected)";
            if (aCapitalizeErrorMsg == capitalizeYes)
                boost::to_upper(anErrorMsg);
            return false;
        }
        return true;
    }

    bool isValidHwSigFormula(const std::string& aHwSigFormula, std::string& anErrorMsg, Capitalize aCapitalizeErrorMsg)
    {
        std::vector<std::string> myTokens = ta::Strings::split(aHwSigFormula, ',');
        if (myTokens.empty())
        {
            if (aCapitalizeErrorMsg == capitalizeYes)
                anErrorMsg = str(boost::format("INVALID HWSIG FORMULA '%s'. COMMA-SEPARATED LIST OF POSITIVE NUMBERS EXPECTED.") % aHwSigFormula);
            else
                anErrorMsg = str(boost::format("Invalid HWSIG formula '%s'. Comma-separated list of positive numbers expected.") % aHwSigFormula);
            return false;
        }
        foreach (std::string tok, myTokens)
        {
            try
            {
                int myComponentId = ta::Strings::parse<int>(tok);
                if (myComponentId < 0)
                {
                    if (aCapitalizeErrorMsg == capitalizeYes)
                        anErrorMsg = str(boost::format("INVALID HWSIG FORMULA '%s'. COMMA-SEPARATED LIST OF POSITIVE NUMBERS EXPECTED.") % aHwSigFormula);
                    else
                        anErrorMsg = str(boost::format("Invalid HWSIG formula '%s'. Comma-separated list of positive numbers expected.") % aHwSigFormula);
                    return false;
                }
            }
            catch (...)
            {
                if (aCapitalizeErrorMsg == capitalizeYes)
                    anErrorMsg = str(boost::format("INVALID HWSIG FORMULA '%s'. COMMA-SEPARATED LIST OF POSITIVE NUMBERS EXPECTED.") % aHwSigFormula);
                else
                    anErrorMsg = str(boost::format("Invalid HWSIG formula '%s'. Comma-separated list of positive numbers expected.") % aHwSigFormula);
                return false;
            }
        }
        return true;
    }

}
