#pragma once

#include "rclient/CRFile.h"
#include "rclient/Common.h"
#include "resept/util.h"
#include "CrTestFile.h"

#include <cxxtest/TestSuite.h>
#include "boost/cstdint.hpp"
#include "boost/assign/list_of.hpp"
#include <string>


const std::string filename = "CR-File";

class CRTest : public CxxTest::TestSuite
{

public:
    static CRTest *createSuite()
    {
        return new CRTest();
    }

    static void destroySuite( CRTest *suite )
    {
        delete suite;
    }

    CRTest():
        testfile(filename)
    {}

    //
    // Test cases
    //
    void test_getResponse()
    {
        rclient::CRFile myFile(filename);

        ta::StringDict  myList = boost::assign::map_list_of ("Challenge", "a43bf18c");

        std::string key = myFile.getResponse(rclient::crfile::ResponseKey, "DemoUser", myList);
        TS_ASSERT_EQUALS(key, "FAB60E96" );
   }

    void test_findResponse()
    {
        rclient::CRFile myFile(filename);
        const std::string user = resept::UmtsUserName;

        ta::StringDict  myList = boost::assign::map_list_of (UmtsAutnChallengeName   , "01010101010101010101010101010101")
                                                            (UmtsRandomChallengeName , "101112131415161718191a1b1c1d1e1f");

        const std::vector<std::string> ResponseNames= boost::assign::list_of ("RES")("IK")("CK");

        ta::StringDict myResponses = calcUmtsResponses(user, myList, ResponseNames);

        std::string key = myFile.getResponse("RES", user, myList);
        TS_ASSERT_EQUALS(key, myResponses.find("RES")->second );

        key = myFile.getResponse("IK", user, myList);
        TS_ASSERT_EQUALS(key, myResponses.find("IK")->second );

        key = myFile.getResponse("CK", user, myList);
        TS_ASSERT_EQUALS(key, myResponses.find("CK")->second );
   }


    void test_findInitialToken()
    {
        rclient::CRFile myFile(filename);

        ta::StringDict myMacthPairList = boost::assign::map_list_of (rclient::crfile::UserKey , "SecuridNewSystemPinUser" );

        std::string key = myFile.getKey(rclient::crfile::InitialTokenKey, myMacthPairList);

        TS_ASSERT_EQUALS(key, "444444" );
     }

private:
    CrTestFile testfile;
};

