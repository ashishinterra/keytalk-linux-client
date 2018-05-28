#pragma once

#include "ta/ecdhutils.h"
#include "cxxtest/TestSuite.h"
#include <string>

#define ECDHUTILS_TRACE

static const std::string EcDhUtilsTestCurveNames[] = {"prime192v1", "prime256v1", "secp521r1", // prime
                                                      "sect163k1", "sect283k1", "sect571k1"    // binary
													  };

class EcDhUtilsTest : public CxxTest::TestSuite
{
public:
    void testGenerateAliceKeys()
    {
        using namespace ta;
        using std::string;
        foreach (string curveName, EcDhUtilsTestCurveNames)
        {
            string myPubKey, myPrivKey;

            EcDhUtils::EcParams myEcParams = EcDhUtils::generateAliceKeys(curveName, myPubKey, myPrivKey);
            dumpEcParamsKeys(myEcParams, myPubKey, myPrivKey);
            TS_ASSERT_EQUALS(myEcParams.curve_name,curveName);
            TS_ASSERT(myPubKey.size() >= myPrivKey.size());
            TS_ASSERT(!myEcParams.generator.empty());
            TS_ASSERT(!myEcParams.order.empty());
            TS_ASSERT(!myEcParams.cofactor.empty());
            TS_ASSERT(!myEcParams.p.empty());
            TS_ASSERT(!myEcParams.a.empty());
            TS_ASSERT(!myEcParams.b.empty());
        }

        string myPubKey, myPrivKey;
        TS_ASSERT_THROWS(EcDhUtils::generateAliceKeys("--non-existing-curve--", myPubKey, myPrivKey), EcDhError);
    }

	void testCalcSharedKeyFromFixedKeys()
	{
        using namespace ta;
        using std::string;
        static const string myDHmodulus = "E59A9CD4BE6B465294E34FF30F309FD87AE030292C2FBE96F07B8B5B543318B1F99082D6193A6319CB8D8DCD2D8C79E7106172406B10F4F1868E4449E8A5B2AB";
        static const string myPubKey = "D975C8096C3C0A3B6AF97B59C762C9EAD3A94CB1E385F8084AB5E9E952E38DD1A2E12F9FE058791F1CD2677655BD738CBFCEA2405D47292B1817B301F5F68972";
        static const string myPrivKey = "A0C036601BF875AA3E03E5F1AD3C4A44E3A97E0041F20EB57A942844532498775F9B4486B173479A540AE791E6F4BB5AF5967437793C038F490C2A7D4816B528";
        const char* mySalt = "12345678";

        TS_ASSERT_EQUALS(DhUtils::calcSharedKey(myDHmodulus, mySalt, myPubKey, myPrivKey), "91da622519a90df7c5dad81c6915bbdef8554a0d660782bca5c6127883872aca");
	}

	void testKeyExchangeWithGeneratedKeys()
	{
        using namespace ta;

        foreach (string curveName, EcDhUtilsTestCurveNames)
        {
            string myPubKeyAlice, myPrivKeyAlice, myPubKeyBob, myPrivKeyBob;

            // Step 1: Alice generate keys and send them to Bob
            EcDhUtils::EcParams myEcParamsAlice = EcDhUtils::generateAliceKeys(curveName, myPubKeyAlice, myPrivKeyAlice);

            // Step 2: Bob generate keys based on the params got from Alice
            EcDhUtils::generateBobKeys(myEcParamsAlice, myPubKeyBob, myPrivKeyBob);
            dumpKeys(myPubKeyBob, myPrivKeyBob);
            TS_ASSERT(!myPubKeyBob.empty());
            TS_ASSERT(!myPrivKeyBob.empty());

            // Step 3: Bob sends his public key to Alioe and they boh calculate the shared secret key which should be the same
            string mySharedKeyAlice = EcDhUtils::calcSharedKey(myEcParamsAlice, myPubKeyBob, myPrivKeyAlice);
            dumpSharedKey(mySharedKeyAlice, "Alice");
            string mySharedKeyBob   = EcDhUtils::calcSharedKey(myEcParamsAlice, myPubKeyAlice, myPrivKeyBob);
            dumpSharedKey(mySharedKeyBob, "Bob");
            TS_ASSERT_EQUALS(mySharedKeyAlice, mySharedKeyBob);

            // Some invalid inputs
            TS_ASSERT_THROWS(EcDhUtils::calcSharedKey(myEcParamsAlice, "", myPrivKeyAlice), EcDhError);
            TS_ASSERT_THROWS(EcDhUtils::calcSharedKey(myEcParamsAlice, myPubKeyBob, ""), EcDhError);
            clean(myEcParamsAlice);
            TS_ASSERT_THROWS(EcDhUtils::calcSharedKey(myEcParamsAlice, myPubKeyBob, myPrivKeyAlice), EcDhError);
        }

	}

    void dumpEcParamsKeys(const EcDhUtils::EcParams& aEcParams, const std::string& aPubKey, const std::string& aPrivKey)
    {
#ifdef ECDHUTILS_TRACE
        TS_TRACE((boost::format("\nCurve name: %s\nPubkey: %s\nPrivkey: %s\nConversion form: %d\ngenerator: %s\norder: %s\ncofactor: %s\np: %s\na: %s\nb: %s\n")
                 % aEcParams.curve_name % aPubKey % aPrivKey % aEcParams.conversion_form % aEcParams.generator
                 % aEcParams.order % aEcParams.cofactor % aEcParams.p % aEcParams.a % aEcParams.b)
                 .str().c_str());
#endif
    }
    void dumpKeys(const std::string& aPubKey, const std::string& aPrivKey)
    {
#ifdef ECDHUTILS_TRACE
        TS_TRACE((boost::format("\nPubkey: %s\nPrivkey: %s\n") % aPubKey % aPrivKey).str().c_str());
#endif
    }
    void dumpSharedKey(const std::string& aKey, const std::string& anPartyFriendlyName)
    {
#ifdef ECDHUTILS_TRACE
        TS_TRACE((boost::format("\n%s's calculated shared key: %s\n") % anPartyFriendlyName % aKey).str().c_str());
#endif
    }

    void clean(EcDhUtils::EcParams& aEcParams)
    {
        aEcParams.curve_name.clear();
        aEcParams.generator.clear();
        aEcParams.order.clear();
        aEcParams.cofactor.clear();
        aEcParams.p.clear();
        aEcParams.a.clear();
        aEcParams.b.clear();
    }
};
