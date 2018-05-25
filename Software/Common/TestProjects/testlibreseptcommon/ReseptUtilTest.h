#pragma once

#include "resept/util.h"
#include "resept/common.h"
#include "cxxtest/TestSuite.h"
#include <string>
#include "boost/static_assert.hpp"

class ReseptUtilTest : public CxxTest::TestSuite
{
public:
    void test_providername_validation()
    {
        std::string myErrMsg;

        TS_ASSERT(resept::isValidProviderName("PROVIDER_OCE-VENLO", myErrMsg));

        TS_ASSERT(!resept::isValidProviderName("PROVIDER_$", myErrMsg) && !myErrMsg.empty());
        TS_ASSERT(!resept::isValidProviderName("Antonín Dvořák", myErrMsg) && !myErrMsg.empty());
        TS_ASSERT(!resept::isValidProviderName("", myErrMsg) && !myErrMsg.empty());
        TS_ASSERT(!resept::isValidProviderName(" 	", myErrMsg) && !myErrMsg.empty());
        static const char TooLongProviderName[] = "PROVIDER_OCE-VENLOSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSS";
        BOOST_STATIC_ASSERT(sizeof(TooLongProviderName)-1 > resept::MaxProviderLength);
        TS_ASSERT(!resept::isValidProviderName(TooLongProviderName, myErrMsg) && !myErrMsg.empty());

        // Reserved names
        TS_ASSERT(!resept::isValidProviderName("platforms", myErrMsg) && !myErrMsg.empty());
    }
    void test_servicename_validations()
    {
        std::string myErrMsg;

        TS_ASSERT(resept::isValidServiceName("CUST_RESKEL-PAS", myErrMsg));

        TS_ASSERT(!resept::isValidServiceName("CUST_$", myErrMsg) && !myErrMsg.empty());
        TS_ASSERT(!resept::isValidServiceName("Antonín Dvořák", myErrMsg) && !myErrMsg.empty());
        TS_ASSERT(!resept::isValidServiceName("", myErrMsg) && !myErrMsg.empty());
        TS_ASSERT(!resept::isValidServiceName("     ", myErrMsg) && !myErrMsg.empty());
        static const char TooLongServiceName[] = "CUST_RESKEL_PASSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSS";
        BOOST_STATIC_ASSERT(sizeof(TooLongServiceName)-1 > resept::MaxServiceLength);
        TS_ASSERT(!resept::isValidServiceName(TooLongServiceName, myErrMsg) && !myErrMsg.empty());
    }
    void test_password_validation()
    {
        std::string myErrMsg;

        TS_ASSERT(resept::isValidPassword("password", myErrMsg));
        TS_ASSERT(resept::isValidPassword("", myErrMsg));
        TS_ASSERT(resept::isValidPassword("  ", myErrMsg));

        static const char TooLongPassword[] = "passwordpasswordpasswordpasswordpasswordpasswordpasswordpasswordpassword";
        BOOST_STATIC_ASSERT(sizeof(TooLongPassword)-1 > resept::MaxPasswordLength);
        TS_ASSERT(!resept::isValidPassword(TooLongPassword, myErrMsg) && !myErrMsg.empty());
    }
    void test_response_validation()
    {
        std::string myErrMsg;

        TS_ASSERT(resept::isValidResponse("response", myErrMsg));
        TS_ASSERT(resept::isValidResponse("", myErrMsg));
        TS_ASSERT(resept::isValidResponse("     ", myErrMsg));

        static const char TooLongResponse[] = "responseresponseresponseresponseresponseresponseresponseresponseresponse";
        BOOST_STATIC_ASSERT(sizeof(TooLongResponse)-1 > resept::MaxResponseLength);
        TS_ASSERT(!resept::isValidResponse(TooLongResponse, myErrMsg) && !myErrMsg.empty());
    }
    void test_pincode_validation()
    {
        std::string myErrMsg;

        TS_ASSERT(resept::isValidPincode("1234", myErrMsg));
        TS_ASSERT(resept::isValidPincode("", myErrMsg));
        TS_ASSERT(resept::isValidPincode("  ", myErrMsg));

        static const char TooLongPincode[] = "123412341234123412341234123412341234123412341234123412341234123412341234123412341234123412341234";
        BOOST_STATIC_ASSERT(sizeof(TooLongPincode)-1 > resept::MaxPincodeLength);
        TS_ASSERT(!resept::isValidPincode(TooLongPincode, myErrMsg) && !myErrMsg.empty());
    }
    void test_username_validation()
    {
        std::string myErrMsg;

        TS_ASSERT(resept::isValidUserName("DEMO", myErrMsg));
        TS_ASSERT(resept::isValidUserName("Antonín Dvořák U$eR", myErrMsg));

        TS_ASSERT(!resept::isValidUserName("", myErrMsg)&& !myErrMsg.empty());
        static const char TooLongUserName[] = "DEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMODEMO_";
        BOOST_STATIC_ASSERT(sizeof(TooLongUserName)-1 > resept::MaxUserIdLength);
        TS_ASSERT(!resept::isValidUserName(TooLongUserName, myErrMsg) && !myErrMsg.empty());
    }
    void test_hwsig_formula_validation()
    {
        std::string myErrMsg;

        TS_ASSERT(resept::isValidHwSigFormula("1,2,9,44,1,3,2", myErrMsg));
        TS_ASSERT(resept::isValidHwSigFormula(" 1, 3, 554        , 455, 9 ", myErrMsg));
        TS_ASSERT(resept::isValidHwSigFormula("1", myErrMsg));

        TS_ASSERT(!resept::isValidHwSigFormula("", myErrMsg) && !myErrMsg.empty());
        TS_ASSERT(!resept::isValidHwSigFormula("1,-2,9,44,1,3,2", myErrMsg));
        TS_ASSERT(resept::isValidHwSigFormula("1,0,9,44,1,3,2", myErrMsg));
        TS_ASSERT(!resept::isValidHwSigFormula("1,X,9,44,1,3,2", myErrMsg));
    }
};

