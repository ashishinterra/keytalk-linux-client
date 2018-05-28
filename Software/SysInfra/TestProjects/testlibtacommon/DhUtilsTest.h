#pragma once

#include "ta/dhutils.h"
#include "cxxtest/TestSuite.h"
#include <string>


class DhUtilsTest : public CxxTest::TestSuite
{
public:
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


	void verifyGeneratedKeySizesAgainstExpectedSizes(const string& DHPrime, const unsigned int expectedPublicKeySizeInBits, const unsigned int expectedPrivateKeySizeInBits)
	{
        using namespace ta;
        using std::string;

        string DHGenerator = "5";
        string generatedPubKey;
        string generatedPrivKey;
        char* mySalt = const_cast<char *>("12345678");
        int SHA256OutputSizeInBits = 256;

        TS_ASSERT_EQUALS(DHPrime.size() * 4, expectedPublicKeySizeInBits);
        DhUtils::generateKeys(DHPrime, DHGenerator, generatedPubKey, generatedPrivKey);
        TS_ASSERT((generatedPubKey.size() * 4) <= expectedPublicKeySizeInBits);
        TS_ASSERT((generatedPubKey.size() * 4) >= (expectedPublicKeySizeInBits - 8));

        TS_ASSERT((generatedPrivKey.size() * 4) <= expectedPrivateKeySizeInBits);
        TS_ASSERT((generatedPrivKey.size() * 4) >= (expectedPrivateKeySizeInBits - 8));

        TS_ASSERT_EQUALS(DhUtils::determineGeneratedKeySizeInBits(generatedPubKey), (generatedPubKey.size() * 4));
        TS_ASSERT_EQUALS(DhUtils::determineGeneratedKeySizeInBits(generatedPrivKey), (generatedPrivKey.size() * 4));

        string mySessionKey = DhUtils::calcSharedKey(DHPrime, mySalt, generatedPubKey, generatedPrivKey);
        TS_ASSERT_EQUALS(mySessionKey.size() * 4, SHA256OutputSizeInBits);
        TS_ASSERT_EQUALS(DhUtils::determineSharedKeySizeInBits(mySessionKey), SHA256OutputSizeInBits);

        TS_ASSERT_THROWS(DhUtils::calcSharedKey(DHPrime, mySalt, generatedPubKey, ""), DhError);
        TS_ASSERT_THROWS(DhUtils::calcSharedKey(DHPrime, mySalt, "", generatedPrivKey), DhError);
        TS_ASSERT_THROWS(DhUtils::calcSharedKey("", mySalt, generatedPubKey, generatedPrivKey), DhError);
	}


    void testCalc512BitsSharedKeyFromGeneratedKeysUsing512BitsDHPrime()
	{
        using namespace ta;
        using std::string;

        const unsigned int ExpectedSizeInBits = 512;
        const string preGenerated512BitsDHPrime = "E59A9CD4BE6B465294E34FF30F309FD87AE030292C2FBE96F07B8B5B543318B1F99082D6193A6319CB8D8DCD2D8C79E7106172406B10F4F1868E4449E8A5B2AB";

        verifyGeneratedKeySizesAgainstExpectedSizes(preGenerated512BitsDHPrime, ExpectedSizeInBits, ExpectedSizeInBits);
	}


	void testCalc2048BitsSharedKeyFromGeneratedKeysUsing2048BitsDHPrime()
	{
        using namespace ta;
        using std::string;

        const unsigned int ExpectedSizeInBits = 2048;
        const string preGenerated2048BitsDHPrime = "CC926ABA601C2C045E90A744491B82D33196197BAC8480E8DB5D4268EFAFE3276204939280565662F90862BCCC33CCBBF198DA3750C7B6FA1663B502BFE968C998195520A539229E618A64BFFB3B48CF8AA32B1B8F9C7A34B5911A964EACBA7002F5C0C4B2612D45A151D2D3953CD79F9C70D92C3CBBA4A1DABCB482B0D3EF2AF0DD271EA8917D023C101BD4E2650E14C254755ED9ABEE96FA738856F9ED67A4F98F76476D24E8154822849AA12AEA7F4C3C591F208F995AABAAFFD61B423007AADEA0AD6F2B2C84F5F9318626D8F51745C19C8B3CA46D3EE053081EC15684C7AE12291E01C356BF8E4AC82F188D4C4B84C6D21185DEF3CE0D6D7BE1C7B4A757";

        verifyGeneratedKeySizesAgainstExpectedSizes(preGenerated2048BitsDHPrime, ExpectedSizeInBits, ExpectedSizeInBits);
	}


    void testWeakParams()
    {
        using namespace ta;

        TS_ASSERT(!DhUtils::isAliceParamWeak("234", "5", "123"));
        TS_ASSERT(!DhUtils::isAliceParamWeak("0100", "2", "123"));
        TS_ASSERT(DhUtils::isAliceParamWeak("234", "5", ""));
        TS_ASSERT(DhUtils::isAliceParamWeak("0001", "5", "123"));
        TS_ASSERT(DhUtils::isAliceParamWeak("234", " 1  ", "123"));
        TS_ASSERT(DhUtils::isAliceParamWeak("0000", " ", "123"));

        TS_ASSERT(!DhUtils::isBobPubkeyWeak("0010"));
        TS_ASSERT(DhUtils::isBobPubkeyWeak("0"));
        TS_ASSERT(DhUtils::isBobPubkeyWeak("1"));
        TS_ASSERT(DhUtils::isBobPubkeyWeak("0000 "));
        TS_ASSERT(DhUtils::isBobPubkeyWeak("0001"));
        TS_ASSERT(DhUtils::isBobPubkeyWeak("   "));
        TS_ASSERT(DhUtils::isBobPubkeyWeak(" 1"));

    }
};
