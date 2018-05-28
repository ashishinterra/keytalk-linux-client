#pragma once

#include "ta/rsautils.h"
#include "ta/strings.h"
#include "ta/process.h"
#include "ta/utils.h"
#include "ta/common.h"

#include "cxxtest/TestSuite.h"
#include <string>
#include <vector>
#include "boost/static_assert.hpp"

using namespace ta;
using namespace std;

class RsaUtilsTest : public CxxTest::TestSuite
{
    std::string theTempFilePath;

public:
    void setUp()
    {
       theTempFilePath = ta::Process::genTempPath();
    }
    void tearDown()
    {
        if (ta::isFileExist(theTempFilePath))
        {
            remove(theTempFilePath.c_str());
        }
    }
    void testGenerateKeyPair()
    {
        const unsigned int myRsakeySize = 512;

        static const int myTransportEncodings[] = {RsaUtils::_firstTransportEncoding, RsaUtils::_lastTransportEncoding};
        static const int myPubKeyEncodings[] = {RsaUtils::_firstPubKeyEncoding, RsaUtils::_lastPubKeyEncoding};

        foreach (int tranportEnc, myTransportEncodings)
        {
            RsaUtils::TransportEncoding myTransportEncoding = static_cast<RsaUtils::TransportEncoding>(tranportEnc);

            foreach (int pubkeyEnc, myPubKeyEncodings)
            {
                RsaUtils::PubKeyEncoding myPubKeyEncoding = static_cast<RsaUtils::PubKeyEncoding>(pubkeyEnc);

                ta::KeyPair myKeyPair1 = RsaUtils::genKeyPair(myRsakeySize, myTransportEncoding, myPubKeyEncoding);
                TS_ASSERT(!myKeyPair1.pubKey.empty());
                TS_ASSERT(!myKeyPair1.privKey.empty());
                TS_ASSERT(RsaUtils::isKeyPair(myKeyPair1, myTransportEncoding, myPubKeyEncoding));

                ta::KeyPair myKeyPair2 = RsaUtils::genKeyPair(myRsakeySize, myTransportEncoding, myPubKeyEncoding);
                TS_ASSERT(!myKeyPair2.pubKey.empty());
                TS_ASSERT(!myKeyPair2.privKey.empty());
                TS_ASSERT_DIFFERS(myKeyPair1.privKey, myKeyPair2.privKey);
                TS_ASSERT_DIFFERS(myKeyPair1.pubKey, myKeyPair2.pubKey);
                TS_ASSERT(RsaUtils::isKeyPair(myKeyPair2, myTransportEncoding, myPubKeyEncoding));

                ta::KeyPair myBadKeyPair(myKeyPair1.privKey, myKeyPair2.pubKey);
                TS_ASSERT(!RsaUtils::isKeyPair(myBadKeyPair, myTransportEncoding, myPubKeyEncoding));

                myBadKeyPair.privKey = myKeyPair2.privKey;
                myBadKeyPair.pubKey = myKeyPair1.pubKey;
                TS_ASSERT(!RsaUtils::isKeyPair(myBadKeyPair, myTransportEncoding, myPubKeyEncoding));
            }
        }
    }

    void testGetKeySize()
    {
        const string my1024BitPrivKeyNoPassName = "CA/certkey.pem";
        const string my1024BitPrivKeyWithPassName = "CA/signingcertkey.pem";
        const string my1024BitPubKeyName = "CA/pubkey.pem";
        const std::vector<unsigned char> my1024BitPrivKeyNoPass = ta::readData(my1024BitPrivKeyNoPassName);
        const std::vector<unsigned char> my1024BitPrivKeyWithPass = ta::readData(my1024BitPrivKeyWithPassName);
        const std::vector<unsigned char> my1024BitPubKey = ta::readData(my1024BitPubKeyName);

        // private key without password
        TS_ASSERT_EQUALS(RsaUtils::getPrivateKeySizeBitsFile(my1024BitPrivKeyNoPassName), 1024U);
        RsaUtils::PrivateKey myPrivKey = RsaUtils::decodePrivateKeyFile(my1024BitPrivKeyNoPassName);
        TS_ASSERT_EQUALS(RsaUtils::getPrivateKeySizeBits(myPrivKey), 1024U);
        TS_ASSERT_EQUALS(RsaUtils::getPrivateKeySizeBits(my1024BitPrivKeyNoPass), 1024U);
        myPrivKey = RsaUtils::decodePrivateKey(my1024BitPrivKeyNoPass);
        TS_ASSERT_EQUALS(RsaUtils::getPrivateKeySizeBits(myPrivKey), 1024U);
        TS_ASSERT_EQUALS(RsaUtils::getKeySizeBits(myPrivKey.n, myPrivKey.e), 1024U);

        // private key with password
        TS_ASSERT_EQUALS(RsaUtils::getPrivateKeySizeBitsFile(my1024BitPrivKeyWithPassName, "kaaskaas"), 1024U);
        myPrivKey = RsaUtils::decodePrivateKeyFile(my1024BitPrivKeyWithPassName, "kaaskaas");
        TS_ASSERT_EQUALS(RsaUtils::getPrivateKeySizeBits(myPrivKey), 1024U);
        TS_ASSERT_EQUALS(RsaUtils::getPrivateKeySizeBits(my1024BitPrivKeyWithPass, "kaaskaas"), 1024U);
        myPrivKey = RsaUtils::decodePrivateKey(my1024BitPrivKeyWithPass, "kaaskaas");
        TS_ASSERT_EQUALS(RsaUtils::getPrivateKeySizeBits(myPrivKey), 1024U);
        TS_ASSERT_EQUALS(RsaUtils::getKeySizeBits(myPrivKey.n, myPrivKey.e), 1024U);

        // public key
        const RsaUtils::PublicKey myPubKey = RsaUtils::decodePublicKeyFile(my1024BitPubKeyName, RsaUtils::pubkeySubjectPublicKeyInfo);
        TS_ASSERT_EQUALS(RsaUtils::getPublicKeySizeBits(myPubKey), 2048U);
        TS_ASSERT_EQUALS(RsaUtils::getKeySizeBits(myPubKey.n, myPubKey.e), 2048U);

        // invalid input
        TS_ASSERT_THROWS(RsaUtils::getPrivateKeySizeBitsFile(my1024BitPrivKeyWithPassName), std::exception); // no password specified
        TS_ASSERT_THROWS(RsaUtils::getPrivateKeySizeBitsFile(my1024BitPrivKeyWithPassName, "invalid_password"), std::exception);
        TS_ASSERT_THROWS(RsaUtils::getPrivateKeySizeBits(my1024BitPrivKeyWithPass), std::exception); // no password specified
        TS_ASSERT_THROWS(RsaUtils::getPrivateKeySizeBits(my1024BitPrivKeyWithPass, "invalid_password"), std::exception);

        TS_ASSERT_THROWS(RsaUtils::getPrivateKeySizeBitsFile("CA/_NIONEXISTING_KEY.pem"), std::exception);
        TS_ASSERT_THROWS(RsaUtils::getPrivateKeySizeBits(std::vector<unsigned char>()), std::exception);
    }

    void testDerEncryptDecryptWithGeneratedKeys()
    {
        const unsigned int myRsakeySize = 512;

        TS_TRACE("Test valid usage (DER)");
        static const int myPubKeyEncodings[] = {RsaUtils::_firstPubKeyEncoding, RsaUtils::_lastPubKeyEncoding};
        foreach (int pubkeyEnc, myPubKeyEncodings)
        {
            RsaUtils::PubKeyEncoding myPubKeyEncoding = static_cast<RsaUtils::PubKeyEncoding>(pubkeyEnc);

            ta::KeyPair myKeyPair = RsaUtils::genKeyPair(myRsakeySize, RsaUtils::encDER, myPubKeyEncoding);
            TS_ASSERT(!myKeyPair.pubKey.empty());
            TS_ASSERT(!myKeyPair.privKey.empty());
            TS_ASSERT(RsaUtils::isKeyPair(myKeyPair, RsaUtils::encDER, myPubKeyEncoding));

            string mySrc = "This is a source buffer smaller than key size";
            vector<unsigned char> myEncrypted = RsaUtils::encrypt(mySrc, myKeyPair.pubKey, RsaUtils::encDER, myPubKeyEncoding);
            size_t myCalcEncryptedSize  = RsaUtils::calcEncryptedSize(mySrc.size(), myKeyPair.pubKey, RsaUtils::encDER, myPubKeyEncoding);
            TS_ASSERT_EQUALS(myCalcEncryptedSize % (myRsakeySize/8), 0U);
            TS_ASSERT_EQUALS(myEncrypted.size(), myCalcEncryptedSize);
            TS_ASSERT_EQUALS(RsaUtils::decrypt(myEncrypted, myKeyPair.privKey, RsaUtils::encDER), mySrc);

            mySrc = "Begin. This is a source buffer is exactly key sizeeeeeeetra End.";
            myEncrypted = RsaUtils::encrypt(mySrc, myKeyPair.pubKey, RsaUtils::encDER, myPubKeyEncoding);
            myCalcEncryptedSize  = RsaUtils::calcEncryptedSize(mySrc.size(), myKeyPair.pubKey, RsaUtils::encDER, myPubKeyEncoding);
            TS_ASSERT_EQUALS(myCalcEncryptedSize % (myRsakeySize/8), 0U);
            TS_ASSERT_EQUALS(myEncrypted.size(), myCalcEncryptedSize);
            TS_ASSERT_EQUALS(RsaUtils::decrypt(myEncrypted, myKeyPair.privKey, RsaUtils::encDER), mySrc);

            mySrc = "Begin: This string is longer than key size. This string is longer than key size.This string is longer than key size.This string is longer than key size.This string is longer than key size. End.";
            myEncrypted = RsaUtils::encrypt(mySrc, myKeyPair.pubKey, RsaUtils::encDER, myPubKeyEncoding);
            myCalcEncryptedSize  = RsaUtils::calcEncryptedSize(mySrc.size(), myKeyPair.pubKey, RsaUtils::encDER, myPubKeyEncoding);
            TS_ASSERT_EQUALS(myCalcEncryptedSize % (myRsakeySize/8), 0U);
            TS_ASSERT_EQUALS(myEncrypted.size(), myCalcEncryptedSize);
            TS_ASSERT_EQUALS(RsaUtils::decrypt(myEncrypted, myKeyPair.privKey, RsaUtils::encDER), mySrc);

            mySrc = "";
            myEncrypted = RsaUtils::encrypt(mySrc, myKeyPair.pubKey, RsaUtils::encDER, myPubKeyEncoding);
            myCalcEncryptedSize  = RsaUtils::calcEncryptedSize(mySrc.size(), myKeyPair.pubKey, RsaUtils::encDER, myPubKeyEncoding);
            TS_ASSERT_EQUALS(myCalcEncryptedSize % (myRsakeySize/8), 0U);
            TS_ASSERT_EQUALS(myEncrypted.size(), myCalcEncryptedSize);
            TS_ASSERT_EQUALS(RsaUtils::decrypt(myEncrypted, myKeyPair.privKey, RsaUtils::encDER), mySrc);
        }

        TS_TRACE("Test invalid usage (DER:PKCS1)");
        ta::KeyPair myKeyPair = RsaUtils::genKeyPair(myRsakeySize, RsaUtils::encDER, RsaUtils::pubkeyPKCS1);
        TS_ASSERT(RsaUtils::isKeyPair(myKeyPair, RsaUtils::encDER, RsaUtils::pubkeyPKCS1));
        TS_ASSERT_THROWS(RsaUtils::encrypt("some text", myKeyPair.pubKey, RsaUtils::encPEM, RsaUtils::pubkeyPKCS1), RsaError); // bad transport encoding
        TS_ASSERT_THROWS(RsaUtils::encrypt("some text", myKeyPair.pubKey, RsaUtils::encDER, RsaUtils::pubkeySubjectPublicKeyInfo), RsaError); // bad pubkey encoding
        TS_ASSERT_THROWS(RsaUtils::encrypt("some text", vector<unsigned char>(), RsaUtils::encDER, RsaUtils::pubkeySubjectPublicKeyInfo), RsaError); // bad key

        vector<unsigned char> myEncrypted = RsaUtils::encrypt("some text", myKeyPair.pubKey, RsaUtils::encDER, RsaUtils::pubkeyPKCS1);
        TS_ASSERT_THROWS(RsaUtils::decrypt(myEncrypted, myKeyPair.privKey, RsaUtils::encPEM), RsaError);  // bad transport encoding
        TS_ASSERT_THROWS(RsaUtils::decrypt(myEncrypted, vector<unsigned char>(), RsaUtils::encDER), RsaError); // bad key

        TS_TRACE("Test invalid usage (DER:SubjectPublicKeyInfo)");
        myKeyPair = RsaUtils::genKeyPair(myRsakeySize, RsaUtils::encDER, RsaUtils::pubkeySubjectPublicKeyInfo);
        TS_ASSERT(RsaUtils::isKeyPair(myKeyPair, RsaUtils::encDER, RsaUtils::pubkeySubjectPublicKeyInfo));
        TS_ASSERT_THROWS(RsaUtils::encrypt("some text", myKeyPair.pubKey, RsaUtils::encPEM, RsaUtils::pubkeySubjectPublicKeyInfo), RsaError); // bad transport encoding
        TS_ASSERT_THROWS(RsaUtils::encrypt("some text", myKeyPair.pubKey, RsaUtils::encDER, RsaUtils::pubkeyPKCS1), RsaError); // bad pubkey encoding
        TS_ASSERT_THROWS(RsaUtils::encrypt("some text", vector<unsigned char>(), RsaUtils::encDER, RsaUtils::pubkeySubjectPublicKeyInfo), RsaError); // bad key

        myEncrypted = RsaUtils::encrypt("some text", myKeyPair.pubKey, RsaUtils::encDER, RsaUtils::pubkeySubjectPublicKeyInfo);
        TS_ASSERT_THROWS(RsaUtils::decrypt(myEncrypted, myKeyPair.privKey, RsaUtils::encPEM), RsaError);  // bad transport encoding
        TS_ASSERT_THROWS(RsaUtils::decrypt(myEncrypted, vector<unsigned char>(), RsaUtils::encDER), RsaError); // bad key
    }

    void testPemEncryptDecryptWithGeneratedKeys()
    {
        const unsigned int myRsakeySize = 512;

        TS_TRACE("Test valid usage (PEM)");
        static const int myPubKeyEncodings[] = {RsaUtils::_firstPubKeyEncoding, RsaUtils::_lastPubKeyEncoding};
        foreach (int pubkeyEnc, myPubKeyEncodings)
        {
            RsaUtils::PubKeyEncoding myPubKeyEncoding = static_cast<RsaUtils::PubKeyEncoding>(pubkeyEnc);

            ta::KeyPair myKeyPair = RsaUtils::genKeyPair(myRsakeySize, RsaUtils::encPEM, myPubKeyEncoding);
            TS_ASSERT(!myKeyPair.pubKey.empty());
            TS_ASSERT(!myKeyPair.privKey.empty());
            TS_ASSERT(RsaUtils::isKeyPair(myKeyPair, RsaUtils::encPEM, myPubKeyEncoding));

            string mySrc = "This is a source buffer smaller than key size";
            vector<unsigned char> myEncrypted = RsaUtils::encrypt(mySrc, myKeyPair.pubKey, RsaUtils::encPEM, myPubKeyEncoding);
            size_t myCalcEncryptedSize  = RsaUtils::calcEncryptedSize(mySrc.size(), myKeyPair.pubKey, RsaUtils::encPEM, myPubKeyEncoding);
            TS_ASSERT_EQUALS(myCalcEncryptedSize % (myRsakeySize/8), 0U);
            TS_ASSERT_EQUALS(myEncrypted.size(), myCalcEncryptedSize);
            TS_ASSERT_EQUALS(RsaUtils::decrypt(myEncrypted, myKeyPair.privKey, RsaUtils::encPEM), mySrc);

            mySrc = "Begin. This is a source buffer is exactly key sizeeeeeeetra End.";
            myEncrypted = RsaUtils::encrypt(mySrc, myKeyPair.pubKey, RsaUtils::encPEM, myPubKeyEncoding);
            myCalcEncryptedSize  = RsaUtils::calcEncryptedSize(mySrc.size(), myKeyPair.pubKey, RsaUtils::encPEM, myPubKeyEncoding);
            TS_ASSERT_EQUALS(myCalcEncryptedSize % (myRsakeySize/8), 0U);
            TS_ASSERT_EQUALS(myEncrypted.size(), myCalcEncryptedSize);
            TS_ASSERT_EQUALS(RsaUtils::decrypt(myEncrypted, myKeyPair.privKey, RsaUtils::encPEM), mySrc);

            mySrc = "Begin: This string is longer than key size. This string is longer than key size.This string is longer than key size.This string is longer than key size.This string is longer than key size. End.";
            myEncrypted = RsaUtils::encrypt(mySrc, myKeyPair.pubKey, RsaUtils::encPEM, myPubKeyEncoding);
            myCalcEncryptedSize  = RsaUtils::calcEncryptedSize(mySrc.size(), myKeyPair.pubKey, RsaUtils::encPEM, myPubKeyEncoding);
            TS_ASSERT_EQUALS(myCalcEncryptedSize % (myRsakeySize/8), 0U);
            TS_ASSERT_EQUALS(myEncrypted.size(), myCalcEncryptedSize);
            TS_ASSERT_EQUALS(RsaUtils::decrypt(myEncrypted, myKeyPair.privKey, RsaUtils::encPEM), mySrc);

            mySrc = "";
            myEncrypted = RsaUtils::encrypt(mySrc, myKeyPair.pubKey, RsaUtils::encPEM, myPubKeyEncoding);
            myCalcEncryptedSize  = RsaUtils::calcEncryptedSize(mySrc.size(), myKeyPair.pubKey, RsaUtils::encPEM, myPubKeyEncoding);
            TS_ASSERT_EQUALS(myCalcEncryptedSize % (myRsakeySize/8), 0U);
            TS_ASSERT_EQUALS(myEncrypted.size(), myCalcEncryptedSize);
            TS_ASSERT_EQUALS(RsaUtils::decrypt(myEncrypted, myKeyPair.privKey, RsaUtils::encPEM), mySrc);
        }

        TS_TRACE("Test invalid usage (PEM:PKCS1)");
        ta::KeyPair myKeyPair = RsaUtils::genKeyPair(myRsakeySize, RsaUtils::encPEM, RsaUtils::pubkeyPKCS1);
        TS_ASSERT(RsaUtils::isKeyPair(myKeyPair, RsaUtils::encPEM, RsaUtils::pubkeyPKCS1));
        TS_ASSERT_THROWS(RsaUtils::encrypt("some text", myKeyPair.pubKey, RsaUtils::encDER, RsaUtils::pubkeyPKCS1), RsaError); // bad transport encoding
        TS_ASSERT_THROWS(RsaUtils::encrypt("some text", myKeyPair.pubKey, RsaUtils::encPEM, RsaUtils::pubkeySubjectPublicKeyInfo), RsaError); // bad pubkey encoding
        TS_ASSERT_THROWS(RsaUtils::encrypt("some text", vector<unsigned char>(), RsaUtils::encPEM, RsaUtils::pubkeySubjectPublicKeyInfo), RsaError); // bad key

        vector<unsigned char> myEncrypted = RsaUtils::encrypt("some text", myKeyPair.pubKey, RsaUtils::encPEM, RsaUtils::pubkeyPKCS1);
        TS_ASSERT_THROWS(RsaUtils::decrypt(myEncrypted, myKeyPair.privKey, RsaUtils::encDER), RsaError);  // bad transport encoding
        TS_ASSERT_THROWS(RsaUtils::decrypt(myEncrypted, vector<unsigned char>(), RsaUtils::encPEM), RsaError); // bad key

        TS_TRACE("Test invalid usage (PEM:SubjectPublicKeyInfo)");
        myKeyPair = RsaUtils::genKeyPair(myRsakeySize, RsaUtils::encPEM, RsaUtils::pubkeySubjectPublicKeyInfo);
        TS_ASSERT(RsaUtils::isKeyPair(myKeyPair, RsaUtils::encPEM, RsaUtils::pubkeySubjectPublicKeyInfo));
        TS_ASSERT_THROWS(RsaUtils::encrypt("some text", myKeyPair.pubKey, RsaUtils::encDER, RsaUtils::pubkeySubjectPublicKeyInfo), RsaError); // bad transport encoding
        TS_ASSERT_THROWS(RsaUtils::encrypt("some text", myKeyPair.pubKey, RsaUtils::encPEM, RsaUtils::pubkeyPKCS1), RsaError); // bad pubkey encoding
        TS_ASSERT_THROWS(RsaUtils::encrypt("some text", vector<unsigned char>(), RsaUtils::encPEM, RsaUtils::pubkeySubjectPublicKeyInfo), RsaError); // bad key

        myEncrypted = RsaUtils::encrypt("some text", myKeyPair.pubKey, RsaUtils::encPEM, RsaUtils::pubkeySubjectPublicKeyInfo);
        TS_ASSERT_THROWS(RsaUtils::decrypt(myEncrypted, myKeyPair.privKey, RsaUtils::encDER), RsaError);  // bad transport encoding
        TS_ASSERT_THROWS(RsaUtils::decrypt(myEncrypted, vector<unsigned char>(), RsaUtils::encPEM), RsaError); // bad key
    }

    void testDerEncryptDecryptWithPreCalcKeys()
    {
        static const vector<unsigned char> myPubKeys[] = {
            ta::Strings::fromHex("30819f300d06092a864886f70d010101050003818d0030818902818100d2451c07d68d6a7b8e1a4f804161412b17bebc88550f3aff8f193120ea7d578142772d4f03858b6debab826a09613795346c3e7c04d9cf2163c1a603f784aafc77ba679deb96b77015f4753661270c54a6cff9a781241c1884fda6f96ba9dc5fe545e470059f1041672094c0e51e384dd74c1c46ca1fdfa6b7a92fd363795d9f0203010001"),
            ta::Strings::fromHex("30820122300d06092a864886f70d01010105000382010f003082010a0282010100b959589cfae84305a27e0c1faf96c4ca8591d0e6e71327e44cd115e378dc0110d763b8d215aefb01df322ee8870581a6f485162d19c7fcf6b4c8e5fba0bd582927a35897e4c7a383c5e1d9bff999260df51650905f7413dce8cda50c7ffb1a08a3c277799ee30aed43a223e0b9fb51175c52f4edd0b2ba2360209b99222b3104feacd6d65c36558ff917a55dee477b844eb83aeaaca5fcbfb506427c84e8d49d87c8ec3dbb2f32e0d52a806ceedf71e8ed604094ef192d536b7d833a3e78b15128b5cd6c29c02502a3e3b5ec187ebb45588645d47a410ec1790ba34d24ca5a4c84649a39ee8e57afeda376124716a996baa3de0602fbac5ca2a373dc8cdacfb90203010001")
        };
        static const vector<unsigned char> myPrivKeys[] = {
            ta::Strings::fromHex("3082025c02010002818100d2451c07d68d6a7b8e1a4f804161412b17bebc88550f3aff8f193120ea7d578142772d4f03858b6debab826a09613795346c3e7c04d9cf2163c1a603f784aafc77ba679deb96b77015f4753661270c54a6cff9a781241c1884fda6f96ba9dc5fe545e470059f1041672094c0e51e384dd74c1c46ca1fdfa6b7a92fd363795d9f02030100010281806b1b46eefb2c27cc6e131ae202ebae4d6c17fd6318a8dc6da70fb705c44c3a919e30a4c4b5cf85b9652d850dcc5f57f7c9af4598d8c3b60d9509e42deb1c0a180933a60c31cbfae44883cedb53aff504da8fbeefe23b337b36e1d5ed702031cb150a6f20f179ecfc4e558d16af5acd6d26c4cf3292852fb7afe95c8c57a15741024100facd3d2f99de2e8c5b806e7d10a00113949de7cd81c1a6519936e1a6c783fdc692790e868273909fa29ab5ee1f396045e1af6dfd75743bb03d6f399f72f7f63f024100d6a0ceceda3859675b357693a08427b8ac620c5540d7c8a5eb9b7bafe401c826a037d57ad5bb14d66aeaaf973d0d59d8560977aafe008995790b8a52db9880a102403919cb497025c6c14bffe4a7f2c60b18b3287463349cab4a3eb0e11540ad8b74cf5a62753b7426444218293daabc3c700c9f0d52bc90171adc11dbb3b2d043af024100d1b73d7ca783e9eff1127373daa36740aec7fb6f0e360aebfd24e71dbdf7f3bfb24bfc3fd339c329a43cff281352c95876f1374a10792bf6aed914e6d385d721024034242b84c5f79059069549b2d4a5417a1cf7947cf0b92615f70b2c2ccbb79ddbd36d03f031c6abd0cebf841e3a6df08e04620ccf93fa70d7d0ec1db7cbff515d"),
            ta::Strings::fromHex("308204a30201000282010100b959589cfae84305a27e0c1faf96c4ca8591d0e6e71327e44cd115e378dc0110d763b8d215aefb01df322ee8870581a6f485162d19c7fcf6b4c8e5fba0bd582927a35897e4c7a383c5e1d9bff999260df51650905f7413dce8cda50c7ffb1a08a3c277799ee30aed43a223e0b9fb51175c52f4edd0b2ba2360209b99222b3104feacd6d65c36558ff917a55dee477b844eb83aeaaca5fcbfb506427c84e8d49d87c8ec3dbb2f32e0d52a806ceedf71e8ed604094ef192d536b7d833a3e78b15128b5cd6c29c02502a3e3b5ec187ebb45588645d47a410ec1790ba34d24ca5a4c84649a39ee8e57afeda376124716a996baa3de0602fbac5ca2a373dc8cdacfb90203010001028201007415a7503a3d39accfc68e25523de566ca1d376f468e52cfc3e4b806cded4ca595a119624abe5aeb233534f7e188eb58fb9057014c425b06d0d77a630c9b4a1a0c7f5bbf56444afefef2a3047f7911c2701bd7bd746b28fd94b927841aabd3f4514c3cb3e9755da49855768f0b2c81ef559d875562fe6d623694759f852cf3f791dd2f2d9be005ed637884dfe5f557d6ae4c38d6876a18f837c2ae3d1b48048aa0bfe65b71c28c1ffc48f88bbb186d858ba23764f997f6b26cfa0ba0cd577735f6a5ac6a008ddf51e082b7cc7b76e6210b33c5f93f05622b5519ef637c2565fb79745aa01ab6a6d5465f942acb51ba54ee77323de602abce5c76891be0b55e0102818100e172a3fa92c7401b69d87bf4c2dbea8cd3a9068aedb5146769a82cfd55d93bb7cb5e6a216da48299af8e03c056004ecd9c90d7729de4fb085bdfa0c1a36e505f9b582731bfac8e56c52884bf70742c3532f174f16d1596453d80194e27d464bd3d709ad39cbd962eccaaf5a2f589a4dc5f243e1344587adeef45adbc69e792e102818100d2779366a05291e5d022d4e2bbf24c12df517889e7e04d74a48fe785097b98bb3137aedbb34737ed6200e55fff6af6f64943d8e788d19a05e21f2f3467bf1ee928813e32d964c1a52c33cca72a923069836aac0ed0dae1e5a0dca7369c9b892825afc0c0dbcef9964d4287c792ef7062a2f900c68c63b0b7aa2e13a5ce982fd902818100c1272c30a1928e6d2e3928b262447266e76cf84b0d2fc86475547dff2a8f2ca7e25868cb6d19b63f1ff6a7662108b07b5eb985ad10743f53410b8eba78602a024fa0ae2be4515dbe1fbb0048d87adfe9505528e133997c1ea47b5cb2e6530560510469d2058734e5b1ba832b73fcd2bf6fd02852ef8a72f30fc2fdffe02864610281807abbf49f625b046dea2485d4fb195f379a375e7dbd54d69670e8e27078893515e4abbe32aca7997bbb2d1e960ed307d5b3fff6db7eb40c87e5c79db587d2b269f18efd009639e59b36ddb638bde0a68ec16e518c0a4bba74388cb48e6a616168a2218ddc69d297460924051e6317728872d596983282682992fa6fc6bf658c710281806cf332c9e16b222e75f575092de873c18f7d696991724dcc1eabc5e72a0d2a9b3399e4d1d21fcb200837cf7f908e9d2da5cfe3d0da1a9d93fb55f4a2641f8336d9f511689da4d485247a84f82a80ca47f24153d18bf518390d62647d9fb981b8a7136b7eb05bcdfc43dc3d6d53e7486d5d514ba52bac14503d25326ac5da3482")
        };

        static const string mySrcs[] = {
            "Deze text wordt gedecrypt, alleen is het lastig te bepalen hoelang de blocksize moet zijn... maarja... we kijken wel fffff!!" ,
            "Dit is een anders stukje tekstn @%$^&*(*)*()*&*&$^%#$^%#^%#^$^%&(*%^(*(*!",
            "Begin: This string is longer than key size. This string is longer than key size.This string is longer than key size.This string is longer than key size.This string is longer than key size. End."
        };

        BOOST_STATIC_ASSERT(sizeof(myPubKeys)/sizeof(myPubKeys[0]) == sizeof(myPrivKeys)/sizeof(myPrivKeys[0]));

        for (size_t iKey = 0; iKey < sizeof(myPubKeys)/sizeof(myPubKeys[0]); ++iKey)
        {
            for (size_t iSrc = 0; iSrc < sizeof(mySrcs)/sizeof(mySrcs[0]); ++iSrc)
            {
                ta::KeyPair myKeyPair(myPrivKeys[iKey], myPubKeys[iKey]);
                TS_ASSERT(RsaUtils::isKeyPair(myKeyPair, RsaUtils::encDER, RsaUtils::pubkeySubjectPublicKeyInfo));

                vector<unsigned char> myEncrypted = RsaUtils::encrypt(mySrcs[iSrc], myPubKeys[iKey], RsaUtils::encDER, RsaUtils::pubkeySubjectPublicKeyInfo);
                size_t myCalcEncryptedSize  = RsaUtils::calcEncryptedSize(mySrcs[iSrc].size(), myPubKeys[iKey], RsaUtils::encDER, RsaUtils::pubkeySubjectPublicKeyInfo);
                TS_ASSERT_EQUALS(myEncrypted.size(), myCalcEncryptedSize);
                TS_ASSERT_EQUALS(RsaUtils::decrypt(myEncrypted, myPrivKeys[iKey], RsaUtils::encDER), mySrcs[iSrc]);
            }
        }
    }

    void testPemDerEncryptDecrypt()
    {
        vector<unsigned char> myPemPubKey = ta::readData("CA/pubkey2.pem");
        vector<unsigned char> myPemPrivKey = ta::readData("CA/privkey2.pem");
        vector<unsigned char> myDerPubKey = ta::readData("CA/pubkey2.der");
        vector<unsigned char> myDerPrivKey = ta::readData("CA/privkey2.der");
        const string myPemPrivKeyPaasswd = "kaaskaas";

        string mySrc = "This is a source buffer smaller than key size";
        vector<unsigned char> myEncrypted = RsaUtils::encrypt(mySrc, myPemPubKey, RsaUtils::encPEM, RsaUtils::pubkeySubjectPublicKeyInfo);
        TS_ASSERT(myEncrypted.size() >= mySrc.size());
        TS_ASSERT_EQUALS(RsaUtils::decrypt(myEncrypted, myPemPrivKey, RsaUtils::encPEM, myPemPrivKeyPaasswd.c_str()), mySrc);
        TS_ASSERT_EQUALS(RsaUtils::decrypt(myEncrypted, myDerPrivKey, RsaUtils::encDER), mySrc);

        myEncrypted = RsaUtils::encrypt(mySrc, myDerPubKey, RsaUtils::encDER, RsaUtils::pubkeySubjectPublicKeyInfo);
        TS_ASSERT(myEncrypted.size() >= mySrc.size());
        TS_ASSERT_EQUALS(RsaUtils::decrypt(myEncrypted, myPemPrivKey, RsaUtils::encPEM, myPemPrivKeyPaasswd.c_str()), mySrc);
        TS_ASSERT_EQUALS(RsaUtils::decrypt(myEncrypted, myDerPrivKey, RsaUtils::encDER), mySrc);

        mySrc = "Begin. This is a source buffer is exactly key sizeeeeeeetra End.Begin. This is a source buffer is exactly key sizeeeeeeetra End.";
        myEncrypted = RsaUtils::encrypt(mySrc, myPemPubKey, RsaUtils::encPEM, RsaUtils::pubkeySubjectPublicKeyInfo);
        TS_ASSERT(myEncrypted.size() >= mySrc.size());
        TS_ASSERT_EQUALS(RsaUtils::decrypt(myEncrypted, myPemPrivKey, RsaUtils::encPEM, myPemPrivKeyPaasswd.c_str()), mySrc);
        TS_ASSERT_EQUALS(RsaUtils::decrypt(myEncrypted, myDerPrivKey, RsaUtils::encDER), mySrc);

        myEncrypted = RsaUtils::encrypt(mySrc, myDerPubKey, RsaUtils::encDER, RsaUtils::pubkeySubjectPublicKeyInfo);
        TS_ASSERT(myEncrypted.size() >= mySrc.size());
        TS_ASSERT_EQUALS(RsaUtils::decrypt(myEncrypted, myPemPrivKey, RsaUtils::encPEM, myPemPrivKeyPaasswd.c_str()), mySrc);
        TS_ASSERT_EQUALS(RsaUtils::decrypt(myEncrypted, myDerPrivKey, RsaUtils::encDER), mySrc);

        mySrc = "Begin: This string is longer than key size. This string is longer than key size.This string is longer than key size.This string is longer than key size.This string is longer than key size. End.";
        myEncrypted = RsaUtils::encrypt(mySrc, myPemPubKey, RsaUtils::encPEM, RsaUtils::pubkeySubjectPublicKeyInfo);
        TS_ASSERT(myEncrypted.size() >= mySrc.size());
        TS_ASSERT_EQUALS(RsaUtils::decrypt(myEncrypted, myPemPrivKey, RsaUtils::encPEM, myPemPrivKeyPaasswd.c_str()), mySrc);
        TS_ASSERT_EQUALS(RsaUtils::decrypt(myEncrypted, myDerPrivKey, RsaUtils::encDER), mySrc);

        myEncrypted = RsaUtils::encrypt(mySrc, myDerPubKey, RsaUtils::encDER, RsaUtils::pubkeySubjectPublicKeyInfo);
        TS_ASSERT(myEncrypted.size() >= mySrc.size());
        TS_ASSERT_EQUALS(RsaUtils::decrypt(myEncrypted, myPemPrivKey, RsaUtils::encPEM, myPemPrivKeyPaasswd.c_str()), mySrc);
        TS_ASSERT_EQUALS(RsaUtils::decrypt(myEncrypted, myDerPrivKey, RsaUtils::encDER), mySrc);

        mySrc = "";
        myEncrypted = RsaUtils::encrypt(mySrc, myPemPubKey, RsaUtils::encPEM, RsaUtils::pubkeySubjectPublicKeyInfo);
        TS_ASSERT(myEncrypted.size() >= mySrc.size());
        TS_ASSERT_EQUALS(RsaUtils::decrypt(myEncrypted, myPemPrivKey, RsaUtils::encPEM, myPemPrivKeyPaasswd.c_str()), mySrc);
        TS_ASSERT_EQUALS(RsaUtils::decrypt(myEncrypted, myDerPrivKey, RsaUtils::encDER), mySrc);

        myEncrypted = RsaUtils::encrypt(mySrc, myDerPubKey, RsaUtils::encDER, RsaUtils::pubkeySubjectPublicKeyInfo);
        TS_ASSERT(myEncrypted.size() >= mySrc.size());
        TS_ASSERT_EQUALS(RsaUtils::decrypt(myEncrypted, myPemPrivKey, RsaUtils::encPEM, myPemPrivKeyPaasswd.c_str()), mySrc);
        TS_ASSERT_EQUALS(RsaUtils::decrypt(myEncrypted, myDerPrivKey, RsaUtils::encDER), mySrc);

        // Invalid args
        TS_ASSERT_THROWS(RsaUtils::decrypt(myEncrypted, myPemPrivKey, RsaUtils::encPEM), RsaError);
        TS_ASSERT_THROWS(RsaUtils::decrypt(myEncrypted, myPemPrivKey, RsaUtils::encPEM, "--invalid--password--"), RsaError);
        TS_ASSERT_THROWS(RsaUtils::decrypt(myEncrypted, myDerPrivKey, RsaUtils::encPEM), RsaError);
        TS_ASSERT_THROWS(RsaUtils::encrypt(mySrc, myPemPubKey, RsaUtils::encDER, RsaUtils::pubkeySubjectPublicKeyInfo), RsaError);
        TS_ASSERT_THROWS(RsaUtils::encrypt(mySrc, vector<unsigned char>(), RsaUtils::encPEM, RsaUtils::pubkeySubjectPublicKeyInfo), RsaError);
        TS_ASSERT_THROWS(RsaUtils::decrypt(myEncrypted, myPemPrivKey, RsaUtils::encDER), RsaError);
        TS_ASSERT_THROWS(RsaUtils::decrypt(myEncrypted, vector<unsigned char>(), RsaUtils::encPEM), RsaError);
    }

    void testExistingPrivateKeyDecode()
    {
        // when
        const RsaUtils::PrivateKey myRsaPrivateKey = RsaUtils::decodePrivateKeyFile("CA/privkey2.pem", "kaaskaas");

        // then
        TS_ASSERT(!myRsaPrivateKey.n.empty());
        TS_ASSERT(!myRsaPrivateKey.e.empty());
        TS_ASSERT(!myRsaPrivateKey.d.empty());
        TS_ASSERT(!myRsaPrivateKey.p.empty());
        TS_ASSERT(!myRsaPrivateKey.q.empty());
        TS_ASSERT(!myRsaPrivateKey.dmp1.empty());
        TS_ASSERT(!myRsaPrivateKey.dmq1.empty());
        TS_ASSERT(!myRsaPrivateKey.iqmp.empty());
        TS_ASSERT_EQUALS(RsaUtils::getPrivateKeySizeBits(myRsaPrivateKey), 1024U);

        // invalid input
        TS_ASSERT_THROWS(RsaUtils::decodePrivateKey(std::vector<unsigned char>()), std::exception);
        TS_ASSERT_THROWS(RsaUtils::decodePrivateKeyFile("non-existing-file"), std::exception);
        TS_ASSERT_THROWS(RsaUtils::decodePrivateKeyFile("CA/privkey2.pem", "invalid-password"), std::exception);
        TS_ASSERT_THROWS(RsaUtils::decodePrivateKeyFile("CA/privkey2.pem"), std::exception); // no password
}

    void testGeneratedKeyPairEncodeDecode()
    {
        static const int myPubKeyEncodings[] = {RsaUtils::_firstPubKeyEncoding, RsaUtils::_lastPubKeyEncoding};
        static const int myRsakeySize = 2048;

        foreach (int pubkeyEnc, myPubKeyEncodings)
        {
            RsaUtils::PubKeyEncoding myPubKeyEncoding = static_cast<RsaUtils::PubKeyEncoding>(pubkeyEnc);

            TS_TRACE(str(boost::format("Test encode/decode private keys for public key encoding %d") % myPubKeyEncoding).c_str());

            // given
            const ta::KeyPair myKeyPair = RsaUtils::genKeyPair(myRsakeySize, ta::RsaUtils::encPEM, myPubKeyEncoding);
            ta::writeData(theTempFilePath, myKeyPair.privKey);

            // when: decode the same private key from memory buffer and from file
            const RsaUtils::PrivateKey myRsaPrivateKey = RsaUtils::decodePrivateKey(myKeyPair.privKey);
            const RsaUtils::PrivateKey myRsaPrivateKey2 = RsaUtils::decodePrivateKeyFile(theTempFilePath);

            // then
            TS_ASSERT_EQUALS(myRsaPrivateKey2, myRsaPrivateKey);
            TS_ASSERT(!myRsaPrivateKey.n.empty());
            TS_ASSERT(!myRsaPrivateKey.e.empty());
            TS_ASSERT(!myRsaPrivateKey.d.empty());
            TS_ASSERT(!myRsaPrivateKey.p.empty());
            TS_ASSERT(!myRsaPrivateKey.q.empty());
            TS_ASSERT(!myRsaPrivateKey.dmp1.empty());
            TS_ASSERT(!myRsaPrivateKey.dmq1.empty());
            TS_ASSERT(!myRsaPrivateKey.iqmp.empty());
            TS_ASSERT_EQUALS(RsaUtils::getPrivateKeySizeBits(myRsaPrivateKey), myRsakeySize);


            // when
            const ta::KeyPair myKeyPair2 = RsaUtils::encodePrivateKey(myRsaPrivateKey, myPubKeyEncoding);

            // then, after subsequent decoding and encoding we get the original keypair
            TS_ASSERT_EQUALS(myKeyPair2.pubKey, myKeyPair.pubKey);
            TS_ASSERT_EQUALS(myKeyPair2.privKey, myKeyPair.privKey);

            TS_TRACE(str(boost::format("Test encode/decode public keys for public key encoding %d") % myPubKeyEncoding).c_str());

            // given
            ta::writeData(theTempFilePath, myKeyPair.pubKey);

            // when: decode the same public key from memory buffer and from file
            const RsaUtils::PublicKey myRsaPublicKey = RsaUtils::decodePublicKey(myKeyPair.pubKey, myPubKeyEncoding);
            const RsaUtils::PublicKey myRsaPublicKey2 = RsaUtils::decodePublicKeyFile(theTempFilePath, myPubKeyEncoding);

            // then
            TS_ASSERT_EQUALS(myRsaPublicKey, myRsaPublicKey2);
            TS_ASSERT(!myRsaPublicKey.n.empty());
            TS_ASSERT(!myRsaPublicKey.e.empty());
            TS_ASSERT_EQUALS(RsaUtils::getPublicKeySizeBits(myRsaPublicKey), myRsakeySize);

            // when
            const std::vector<unsigned char> mPubKey = RsaUtils::encodePublicKey(myRsaPublicKey, myPubKeyEncoding);

            // then, after subsequent decoding and encoding we get the original keypair
            TS_ASSERT_EQUALS(mPubKey, myKeyPair.pubKey);


            // invalid input
            TS_TRACE(str(boost::format("Test misuse of encode/decode keys for public key encoding %d") % myPubKeyEncoding).c_str());
            TS_ASSERT_THROWS(RsaUtils::decodePrivateKey(myKeyPair.pubKey), std::exception);
            TS_ASSERT_THROWS(RsaUtils::decodePrivateKeyFile("non-existing-file"), std::exception);
            TS_ASSERT_THROWS(RsaUtils::decodePublicKey(myKeyPair.privKey, myPubKeyEncoding), std::exception);
            TS_ASSERT_THROWS(RsaUtils::decodePublicKeyFile("non-existing-file", myPubKeyEncoding), std::exception);
            TS_ASSERT_THROWS(RsaUtils::encodePrivateKey(RsaUtils::PrivateKey(), myPubKeyEncoding), std::exception);
        }
    }

    void testUnwrapPrivateKey()
    {
        const string myWrappedKeyPath = "CA/privkey2.pem";
        const string myNotWrappedKeyPath = "CA/FIXEDprivkey.pem";
        const string myWrappedKeyPassword = "kaaskaas";
        const vector<unsigned char> myWrappedKeyBuf = ta::readData(myWrappedKeyPath);
        const vector<unsigned char> myNotWrappedKeyBuf = ta::readData(myNotWrappedKeyPath);

        // when
        std::vector<unsigned char> myUnwrappedKey = RsaUtils::unwrapPrivateKeyFile(myWrappedKeyPath, myWrappedKeyPassword);
        // then
        TS_ASSERT_EQUALS(RsaUtils::decodePrivateKey(myUnwrappedKey), RsaUtils::decodePrivateKeyFile(myWrappedKeyPath, myWrappedKeyPassword.c_str()));
        TS_ASSERT_EQUALS(RsaUtils::unwrapPrivateKey(myWrappedKeyBuf, myWrappedKeyPassword), myUnwrappedKey);

        // when: test that password should not matter for not password-protected key
        myUnwrappedKey = RsaUtils::unwrapPrivateKeyFile(myNotWrappedKeyPath, "whatever");
        // then
        TS_ASSERT_EQUALS(RsaUtils::decodePrivateKey(myUnwrappedKey), RsaUtils::decodePrivateKeyFile(myNotWrappedKeyPath, "whatever"));
        TS_ASSERT_EQUALS(RsaUtils::unwrapPrivateKey(myNotWrappedKeyBuf, "whatever"), myUnwrappedKey);

        // Invalid usage
        TS_ASSERT_THROWS(RsaUtils::unwrapPrivateKeyFile(myWrappedKeyPath, myWrappedKeyPassword + "_invalid"), std::exception);
        TS_ASSERT_THROWS(RsaUtils::unwrapPrivateKey(myWrappedKeyBuf, myWrappedKeyPassword + "_invalid"), std::exception);
        TS_ASSERT_THROWS(RsaUtils::unwrapPrivateKeyFile("non-existing-file", myWrappedKeyPassword ), std::exception);
        TS_ASSERT_THROWS(RsaUtils::unwrapPrivateKey(vector<unsigned char>(), myWrappedKeyPassword), std::exception);
    }

    void testConvertPrivateKeyToPkcs8()
    {
        const vector<unsigned char> myNotWrappedKey = ta::readData("CA/FIXEDprivkey.pem");
        const vector<unsigned char> myWrappedKey = ta::readData("CA/privkey2.pem");

        TS_ASSERT(!RsaUtils::convPrivateKey2Pkcs8Der(myNotWrappedKey).empty());
        TS_ASSERT_THROWS(RsaUtils::convPrivateKey2Pkcs8Der(myWrappedKey), std::exception);
    }
};
