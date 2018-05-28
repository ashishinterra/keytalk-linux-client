#pragma once

#include "ta/ecutils.h"
#include "cxxtest/TestSuite.h"
#include <string>
#include <vector>

using namespace ta;
using namespace std;

class EcUtilsTest : public CxxTest::TestSuite
{
public:
    void testGenerateKeyPair()
    {
        const unsigned int myEckeySize = 256;

        TS_TRACE("Test valid usage");
        ta::KeyPair myKeyPair = EcUtils::genKeyPair(myEckeySize);
        TS_ASSERT(!myKeyPair.pubKey.empty());
        TS_ASSERT(!myKeyPair.privKey.empty());
        // TS_TRACE(ta::vec2Str(myKeyPair.pubKey));
        // TS_TRACE(ta::vec2Str(myKeyPair.privKey));

        TS_TRACE("Test invalid usage");
        TS_ASSERT_THROWS(EcUtils::genKeyPair(0), EcError);
    }


};
