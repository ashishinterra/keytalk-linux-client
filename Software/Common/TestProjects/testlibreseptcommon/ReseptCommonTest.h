#pragma once

#include "resept/common.h"
#include "resept/computeruuid.h"
#include "ta/common.h"
#include "cxxtest/TestSuite.h"
#include <string>

class ReseptCommonTest : public CxxTest::TestSuite
{
public:
    void testSignVerifyMsg()
    {
        const std::string myPrivKeyPath = "FIXEDprivkey.pem";
        const std::string myPubKeyPath = "FIXEDpubkey.pem";

        std::string mySignedMsg = resept::rcdpv1::signMsg("this is a test message", myPrivKeyPath);
        TS_ASSERT_EQUALS(resept::rcdpv1::verifySignedMsg(mySignedMsg, myPubKeyPath), "this is a test message");

        TS_ASSERT_THROWS(resept::rcdpv1::signMsg("", myPrivKeyPath), std::exception);
        TS_ASSERT_THROWS(resept::rcdpv1::verifySignedMsg("", myPubKeyPath), std::exception);

        TS_ASSERT_THROWS(resept::rcdpv1::signMsg("this is a test message", "non-existing-key"), std::exception);
        TS_ASSERT_THROWS(resept::rcdpv1::verifySignedMsg("does-not-matter", "non-existing-key"), std::exception);
        TS_ASSERT_THROWS(resept::rcdpv1::verifySignedMsg("invalid-signed-msg", myPubKeyPath), std::exception);
    }

    void testNormalizeHwsig()
    {
        TS_ASSERT_EQUALS(resept::normalizeHwsig(""), "");
        TS_ASSERT_EQUALS(resept::normalizeHwsig("cs-XyZ"), "CS-XYZ");
    }

    void testZeroHwsig()
    {
        TS_ASSERT_EQUALS(resept::ZeroHwsig, resept::normalizeHwsig(resept::ZeroHwsig));
        TS_ASSERT_EQUALS(resept::ZeroHwsig, resept::normalizeHwsig(resept::ComputerUuid::calcCs("0")));
    }
};
