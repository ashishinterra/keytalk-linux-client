#pragma once

#include "ta/aesutils.h"
#include "ta/strings.h"
#include "ta/common.h"
#include "cxxtest/TestSuite.h"
#include <string>
#include <iostream>

using namespace ta;

class AesUtilsTest : public CxxTest::TestSuite
{
public:
    void testEncryptDecryptGCM()
    {
        using std::string;

        static const size_t myTagSize = 16;
        static const size_t myIvSize = 16;
        static const char* myKeys[] = {"ABCDE56789012345012345678FFFF345", "ABCDE5678901234567890123456FFF0101234567890123456789012345678901"};
        static const char* mySrcBufs[]  = {"Deze text wordt gedecrypt",
                                           "Dit is een test om te kijken of AES encryptie wel werkt.Dit is een test om te kijken of AES encryptie wel werkt.Dit is een test om te kijken of AES encryptie wel werkt.Dit is een test om te kijken of AES encryptie wel werkt.",
                                           "&*^*&%^$$&^&*&^^%$^&^*&(^&%^&%^&%^&%^%^&%^&%&*&))(*)(&(^^ie wel werkt.Dit is een test om te kijken of AES encryptie wel werkt.Dit is een test om te kijken of AES encryptie wel werkt.",
                                           "r",
                                           ""};


        static const char* myExpectedEncryptedHexBufs[] = {
                                                             //
                                                             // key 1
                                                             //
                                                            "d48ca9425a3d86d867d51a44cf0d83e9000000000000000000000000000000008a4a596136dc907d9ba285de778b5cb8c03e52285a8205b139",
                                                            "542a48b1b00fd98213228ede37e7c583000000000000000000000000000000008a4657247fdbd5608aecd2c5609c5cb8c83616395cd017a827ecfa16342eae62018b7779fb2ae80fd544d1ee861c59b46816f0e7dcea5287cfa6e5e93148e7e9278c0484fe7bcace47d556181c6494878e880805829fdd5e2a258e5ff3eedc9158de70963510cb2a47acba6b5ff011221a5927c85896710eddd239f87a03022d105a3b55eb55733bb4b621720937bb7bb50f83dd5624c46ae1d679189f25673a13ecf9fbd2b00cb0e7b1f85723af5b005c920e7a865a17175bd4626b460f31d6e08caefa7f69e436e1d7b9cead6bba5b1f395d34d876e56af9e56554f6af7e6c",
                                                            "dd2708981925301c92ce2b3b1f32d9a000000000000000000000000000000000e8057d2e308dab21cba4ac972fc976c6827f686b67da5ae913a1ba26326496646590027cc061d55b896a83a2c51608f82d1eadab86a70ef7d5a6f4e92f5eabac3587569bef26fa875c981f1f59219a80c4970818d6d0d4134b14b85ffde9d58844c024903610fd0a78aca8604ee91c7c2a5936c846803d4bcfd96be76b5e32640b177252ae107d3cfea9216f5d78b236d43eb5dd5823cd73fdc82d1e9c25511a2cecebf0c3a901eed7b1e9573db917454e995c659707",

                                                            "2d52197c5b95f7b62564b6fdb4525f8900000000000000000000000000000000bc",

                                                            "2d31dac905b6da965c641cf416c231b700000000000000000000000000000000",

                                                            //
                                                            // key 2
                                                            //
                                                            "6c69af22bb76111e942db617ff10045200000000000000000000000000000000e3acfd13290b31658e1dbd3afff2605fea0aefa2bb0026f80b",
                                                            "baeb5c0fe0ff46be33c93f0e101cd86500000000000000000000000000000000e3a0f356600c74789f53ea21e8e5605fe202abb3bd5234e115f8ecb0ba3f9234c8304cd5542c5bc1ecc2afc4fc4a6c56d20656bfb0333af3d50406d1571a14b8ae0411c3f4357614f17f81d7b3b7362be28c144bbc09ac9bf416022fc254b05ad4e571f8fc3d8377d8cec351ea27195983e800024cf4d6a85e7b6397b328a85575ed23e6ad85bab629b5879d8ff5a9ce81e35309381dbfaac1f8a8f6ad3d1cd9e550e8d146d2e790c287ec9377ea6ddf4163215fe6e55f6a4a424bd324e2b9c9defc2a50c5e6aafcf68f49f3058fed2f2320af0801be57ba6a3f95d49991331a",
                                                            "6eb839f8705b645b4d58cad2255171570000000000000000000000000000000081e3d95c2f5a0a39de1b9473a7b04a21a84bd5e1865879a021b5ac80bc75aa32ac2b39d06f676695b0ecfd88bf403d1a970e0bf3ea7e6683cf0417d1490c58fdbc0f43dce568465dea32c8d0f6f2382ca8931456e846a5d69527342fcc53b943c8fb25feff3db557e7ced15afb3e1407b3e8110252e29aed4c703188a275981c6ea06ae1e8c0b4b163aa8780dbbaa083e0d26509361ab6b3dde6fcf0ae3d2af9da50fada57cbeacef287fd9369fc219a53687340f7b8",

                                                            "30a961e02ddc325805ae63e253c5e2d500000000000000000000000000000000d5",

                                                            "a0464f582f3125a317c593411725758000000000000000000000000000000000",
                                                       };

        BOOST_STATIC_ASSERT((sizeof mySrcBufs /sizeof mySrcBufs[0])*(sizeof myKeys /sizeof myKeys[0]) == (sizeof myExpectedEncryptedHexBufs / sizeof myExpectedEncryptedHexBufs[0]));

        size_t i = 0;
        for (size_t iKey = 0; iKey < sizeof(myKeys)/sizeof(myKeys[0]); ++iKey)
        {
            for (size_t iBuf = 0; iBuf < sizeof(mySrcBufs)/sizeof(mySrcBufs[0]); ++iBuf)
            {
                const string mySrcBuf = mySrcBufs[iBuf];
                const string myKey = myKeys[iKey];

                // Random IV
                bool myUseRandomIv = true;
                string myEncRandomIv = AesUtils::encryptGCM(mySrcBuf, myTagSize, myIvSize, myKey, myUseRandomIv);
                TS_ASSERT_EQUALS(myEncRandomIv.size(), getExpectedGsmEncryptedMsgSize(mySrcBuf, myTagSize, myIvSize));
                TS_ASSERT_EQUALS(AesUtils::decryptGCM(myEncRandomIv, myTagSize, myIvSize, myKey), mySrcBuf);

                // Fixed IV (so we have deterministic expectations about the encrypted data that we can assert against)
                myUseRandomIv = false;
                string myEncFixedIv = AesUtils::encryptGCM(mySrcBuf, myTagSize, myIvSize, myKey, myUseRandomIv);
                TS_ASSERT_EQUALS(myEncFixedIv.size(), getExpectedGsmEncryptedMsgSize(mySrcBuf, myTagSize, myIvSize));
                TS_ASSERT_EQUALS(ta::Strings::toHex(str2Vec<unsigned char>(myEncFixedIv)), myExpectedEncryptedHexBufs[i]);
                TS_ASSERT_EQUALS(AesUtils::decryptGCM(myEncFixedIv, myTagSize, myIvSize, myKey), mySrcBuf);

                // empty source buffer
                TS_ASSERT_THROWS(AesUtils::decryptGCM("", myTagSize, myIvSize, myKey), AesDecryptError);

                // bad key
                TS_ASSERT_THROWS(AesUtils::encryptGCM(mySrcBuf, myTagSize, myIvSize, ""), AesEncryptError);
                TS_ASSERT_THROWS(AesUtils::decryptGCM(myEncRandomIv, myTagSize, myIvSize, ""), AesDecryptError);

                // invalid tag size
                TS_ASSERT_THROWS(AesUtils::encryptGCM(mySrcBuf, 0, myIvSize, myKey), AesEncryptError);
                TS_ASSERT_THROWS(AesUtils::decryptGCM(myEncRandomIv, 0, myIvSize, myKey), AesDecryptError);

                // invalid iv size
                TS_ASSERT_THROWS(AesUtils::encryptGCM(mySrcBuf, myTagSize, 0, myKey), AesEncryptError);
                TS_ASSERT_THROWS(AesUtils::decryptGCM(myEncRandomIv, myTagSize, 0, myKey), AesDecryptError);

                ++i;
            }
        }
    }

    size_t getExpectedGsmEncryptedMsgSize(const std::string& anSrcMsg, size_t aTagSize, size_t anIvSize)
    {
        return aTagSize + anIvSize + anSrcMsg.length();
    }
};
