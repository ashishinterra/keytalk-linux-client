#pragma once

#include "ta/common.h"
#include "boost/assign/list_of.hpp"
#include <string>
#include <vector>

namespace resept
{
    /**
       Provider names that have been reserved for internal purposes:
         platforms: Qt Dependencies directory
       */
    const std::string ReservedProviderNames[] = { "platforms" };

    /**
      Sample credentials for client-server testing
      These function is provided for testing purposes only!!
     */
    std::string calcResponse(const std::string& aUserId, const std::string& aChallenge);

    static const std::string Challenge         = "Challenge";
    static const std::string PasswordChallenge = "Password challenge";

    static const std::string DemoUserName = "DemoUser";
    static const std::string DemoUserChallenge = "a43bf18c";

    static const std::string GsmUserName = "GSM_2_354162120787078";
    static const std::string GsmRandomChallengeName = "GSM RANDOM";
    static const std::string GsmRandomChallenge1Value = "101112131415161718191a1b1c1d1e1f";
    static const std::string GsmRandomChallenge2Value = "202122232425262728292a2b2c2d2e2f";
    static const std::string GsmRandomChallenge3Value = "303132333435363738393a3b3c3d3e3f";
    static const std::vector<std::string> GsmResponseNames= boost::assign::list_of ("SRES")("Kc");

    static const std::string UmtsUserName = "UMTS_2_354162120787078";
    static const std::string UmtsRandomChallengeName = "UMTS RANDOM";
    static const std::string UmtsAutnChallengeName = "UMTS AUTN";
    static const std::vector<std::string> UmtsResponseNames= boost::assign::list_of ("RES")("IK")("CK");

    static const std::string UmtsRandomChallengeValue = "101112131415161718191a1b1c1d1e1f";
    static const std::string UmtsAutnChallengeValue   = "01010101010101010101010101010101";

    // @return responses as dict {name : val}
    ta::StringDict calcGsmResponses(const std::string& aUserId, const ta::StringDict& aChallenges, const std::vector<std::string>& aResponseNames);

    // @return responses as dict {name : val}
    ta::StringDict calcUmtsResponses(const std::string& aUserId, const ta::StringDict& aChallenges, const std::vector<std::string>& aResponseNames);

    static const std::string DefOtpInitialSecret = "7f7f7f7f7f7f7f7f";
    static const std::string DefOtpPincode = "1234";
    std::string calcOtp(const std::string& anInitialSecret = DefOtpInitialSecret, const std::string& aPincode = DefOtpPincode);

    namespace securid
    {
        static const std::string SimpleUserName = "SecuridUser";
        static const std::string SimpleUserTokencode = "111111";

        static const std::string NextTokenUserName = "SecuridNextTokenUser";
        static const std::string NextTokenInitialTokenCode = "222222";
        static const std::string NextTokenNewTokenCode = "333333";
        static const std::string NextTokenNewTokenRequest = "Please Enter the Next Code from Your Token:";

        static const std::string NewSystemPinUserName = "SecuridNewSystemPinUser";
        static const std::string NewSystemPinInitialTokenCode = "444444";
        static const std::string NewSystemPinNewPincode = "123456";
        static const std::string NewSystemPinNewTokenCode = "555555";
        static const std::string NewSystemPinNewRequestConfirm = "Are you prepared to accept a new system-generated PIN [y/n]?";
        static const std::string NewSystemPinConfirm           = "Your new PIN is: 123456 Do you accept this [y/n]?";
        static const std::string NewSystemPinNewTokenRequest   = "Pin Accepted. Wait for the code on your card to change, then enter new PIN and TokenCode Enter PASSCODE:";

        static const std::string NewUserPinUserName = "SecuridNewUserPinUser";
        static const std::string NewUserPinInitialTokenCode = "666666";
        static const std::string NewUserPinNewPincode = "234567";
        static const std::string NewUserPinNewTokenCode = "777777";
        static const std::string NewUserPinInitialTokenRequest = "Enter your new PIN of 4 to 8 digits, or <Ctrl-D> to cancel the New PIN procedure:";
        static const std::string NewUserPinReEnterRequest = "Please re-enter new PIN:";
        static const std::string NewUserPinNewTokenRequest = "Wait for the code on your card to change, then enter new PIN and TokenCode Enter PASSCODE:";

        static const std::string YesResponse = "y";
        static const std::string NoResponse = "n";
    }


    enum Capitalize
    {
        capitalizeNo, capitalizeYes
    };

    /**
      Valid provider name consists of alphanumeric ASCII chars , '_' or '-' MaxProviderLength length at most. Cannot be empty or whitespace-only.

      @param[in] aProviderName provider name
      @param[out] anErrorMsg if the function return false, contains user-friendly error message
      @return whether the given provider name is valid
     */
    bool isValidProviderName(const std::string& aProviderName, std::string& anErrorMsg, Capitalize aCapitalizeErrorMsg = capitalizeNo);

    /**
      Valid service name consists of alphanumeric ASCII chars, '_' or '-' MaxServiceLength length at most. Cannot be empty or whitespace-only.

      @param[in] aServiceName service name
      @param[out] anErrorMsg if the function return false, contains user-friendly error message
      @return whether the given service name is valid
     */
    bool isValidServiceName(const std::string& aServiceName, std::string& anErrorMsg, Capitalize aCapitalizeErrorMsg = capitalizeNo);

    /**
      Valid password has length <= MaxPasswordLength unicode codepoints

      @param[in] anUtf8Password UTF-8 encoded password
      @param[out] anErrorMsg if the function return false, contains user-friendly error message
      @return whether the given password is valid
     */
    bool isValidPassword(const std::string& anUtf8Password, std::string& anErrorMsg, Capitalize aCapitalizeErrorMsg = capitalizeNo);

    /**
      Valid response has length <= MaxResponseLength unicode codepoints

      @param[in] anUtf8Response UTF-8 encoded repoonse
      @param[out] anErrorMsg if the function return false, contains user-friendly error message
      @return whether the given response is valid
     */
    bool isValidResponse(const std::string& anUtf8Response, std::string& anErrorMsg, Capitalize aCapitalizeErrorMsg = capitalizeNo);

    /**
      Valid pincode has length <= MaxPincodeLength one-byte characters

      @param[in] aPincode pincode ASCII string
      @param[out] anErrorMsg if the function return false, contains user-friendly error message
      @return whether the given pincode is valid
     */
    bool isValidPincode(const std::string& aPincode, std::string& anErrorMsg, Capitalize aCapitalizeErrorMsg = capitalizeNo);

    /**
     Valid user name consists at most MaxUserNameLength unicode codepoints. Cannot be empty or whitespace-only.

     @param[in] anUtf8UserName UTF-8 encoded user name
     @param[out] anErrorMsg if the function return false, contains user-friendly error message
     @return whether the given user name is valid
    */
    bool isValidUserName(const std::string& anUtf8UserName, std::string& anErrorMsg, Capitalize aCapitalizeErrorMsg = capitalizeNo);

    /**
     @param[in] aKeySize key size
     @param[out] anErrorMsg if the function return false, contains user-friendly error message
     @return whether the given key size is valid
    */
    bool isValidRsaKeyBitLen(const int aKeySizeBit, std::string& anErrorMsg, Capitalize aCapitalizeErrorMsg = capitalizeNo);

    /**
     Valid hardware signature formula is a comma-separated list of numbers

     @param[in] aHwSigFormula hardware signature formula
     @param[out] anErrorMsg if the function return false, contains user-friendly error message
     @return whether the given key size is valid
    */
    bool isValidHwSigFormula(const std::string& aHwSigFormula, std::string& anErrorMsg, Capitalize aCapitalizeErrorMsg = capitalizeNo);
}
