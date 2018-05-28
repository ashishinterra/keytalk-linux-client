#pragma once

#include "ta/signutils.h"
#include "ta/utils.h"
#include "ta/common.h"
#include "cxxtest/TestSuite.h"

class SignUtilsTest : public CxxTest::TestSuite
{
public:
    void testSignVerifyDigest()
    {
        using std::vector;
        using std::string;
		using namespace ta;

        static const string myOrigDataFileName = "CA/orig.data";
        static const string myExpectedSignedSha1DigestFileName = "CA/signed.sha1dgst";
        static const string myExpectedSignedSha256DigestFileName = "CA/signed.sha256dgst";
        static const string myPrivKeyFileNameNoPass = "CA/FIXEDprivkey.pem";
        static const string myPubKeyFileNameNoPass = "CA/FIXEDpubkey.pem";
        static const string myPrivKeyFileNameWithPass = "CA/privkey.pem";
        static const string myPubKeyFileNameWithPass = "CA/pubkey.pem";
        static const std::vector<unsigned char> myPrivKeyNoPass = ta::readData(myPrivKeyFileNameNoPass);
        static const std::vector<unsigned char> myPrivKeyWithPass = ta::readData(myPrivKeyFileNameWithPass);

        const vector<unsigned char> myOrigData = readData(myOrigDataFileName);
        const vector<unsigned char> myPemVerifyPubKey = readData(myPubKeyFileNameNoPass);

        TS_TRACE("--- Test sign-verify with sha1 digest (privkey without password)");
        // Sign: openssl dgst -sha1 -sign CA/FIXEDprivkey.pem -out signed.sha1dgst CA/orig.data
        // Verify: openssl dgst -sha1 -verify CA/FIXEDpubkey.pem -signature signed.sha1dgst CA/orig.data

        vector<unsigned char> mySignedDigestWithKeyFromFile = SignUtils::signDigest(myOrigData, SignUtils::digestSha1, myPrivKeyFileNameNoPass);
        vector<unsigned char> mySignedDigestWithKeyFromBuf = SignUtils::signDigest(myOrigData, SignUtils::digestSha1, myPrivKeyNoPass);
        TS_ASSERT_EQUALS(mySignedDigestWithKeyFromFile, mySignedDigestWithKeyFromBuf);
        vector<unsigned char> myExpectedSignedDigest = readData(myExpectedSignedSha1DigestFileName);
        TS_ASSERT_EQUALS(mySignedDigestWithKeyFromFile, myExpectedSignedDigest);
        TS_ASSERT(SignUtils::verifyDigest(myOrigData, mySignedDigestWithKeyFromFile, SignUtils::digestSha1, myPubKeyFileNameNoPass));
        TS_ASSERT(SignUtils::verifyDigest(myOrigData, mySignedDigestWithKeyFromFile, SignUtils::digestSha1, myPemVerifyPubKey));

        TS_TRACE("--- Test sign-verify with sha1 digest (privkey with password)");
        mySignedDigestWithKeyFromFile = SignUtils::signDigest(myOrigData, SignUtils::digestSha1, myPrivKeyFileNameWithPass, "kaaskaas");
        mySignedDigestWithKeyFromBuf = SignUtils::signDigest(myOrigData, SignUtils::digestSha1, myPrivKeyWithPass, "kaaskaas");
        TS_ASSERT_EQUALS(mySignedDigestWithKeyFromFile, mySignedDigestWithKeyFromBuf);
        TS_ASSERT(SignUtils::verifyDigest(myOrigData, mySignedDigestWithKeyFromFile, SignUtils::digestSha1, myPubKeyFileNameWithPass));
        TS_ASSERT(SignUtils::verifyDigest(myOrigData, mySignedDigestWithKeyFromFile, SignUtils::digestSha1, myPubKeyFileNameWithPass));


        TS_TRACE("--- Test sign-verify with sha256 digest (privkey without password)");
        // Sign: openssl dgst -sha256 -sign CA/FIXEDprivkey.pem -out signed.sha256dgst CA/orig.data
        // Verify: openssl dgst -sha256 -verify CA/FIXEDpubkey.pem -signature signed.sha256dgst CA/orig.data

        mySignedDigestWithKeyFromFile = SignUtils::signDigest(myOrigData, SignUtils::digestSha256, myPrivKeyFileNameNoPass);
        mySignedDigestWithKeyFromBuf = SignUtils::signDigest(myOrigData, SignUtils::digestSha256, myPrivKeyNoPass);
        TS_ASSERT_EQUALS(mySignedDigestWithKeyFromFile, mySignedDigestWithKeyFromBuf);
        myExpectedSignedDigest = readData(myExpectedSignedSha256DigestFileName);
        TS_ASSERT_EQUALS(mySignedDigestWithKeyFromFile, myExpectedSignedDigest);
        TS_ASSERT(SignUtils::verifyDigest(myOrigData, mySignedDigestWithKeyFromFile, SignUtils::digestSha256, myPubKeyFileNameNoPass));
        TS_ASSERT(SignUtils::verifyDigest(myOrigData, mySignedDigestWithKeyFromFile, SignUtils::digestSha256, myPemVerifyPubKey));

        TS_TRACE("--- Test sign-verify with sha256 digest (privkey with password)");
        mySignedDigestWithKeyFromFile = SignUtils::signDigest(myOrigData, SignUtils::digestSha256, myPrivKeyFileNameWithPass, "kaaskaas");
        mySignedDigestWithKeyFromBuf = SignUtils::signDigest(myOrigData, SignUtils::digestSha256, myPrivKeyWithPass, "kaaskaas");
        TS_ASSERT_EQUALS(mySignedDigestWithKeyFromFile, mySignedDigestWithKeyFromBuf)
        TS_ASSERT(SignUtils::verifyDigest(myOrigData, mySignedDigestWithKeyFromFile, SignUtils::digestSha256, myPubKeyFileNameWithPass));
        TS_ASSERT(SignUtils::verifyDigest(myOrigData, mySignedDigestWithKeyFromFile, SignUtils::digestSha256, myPubKeyFileNameWithPass));

        TS_TRACE("--- Test invalid usage");
        TS_ASSERT_THROWS(SignUtils::signDigest(std::vector<unsigned char>(), SignUtils::digestSha1, myPrivKeyFileNameNoPass), SignUtils::SignError);
        TS_ASSERT_THROWS(SignUtils::signDigest(myOrigData, SignUtils::digestSha1, "__UnexistingSignKeyFile__"), SignUtils::SignError);
        TS_ASSERT_THROWS(SignUtils::signDigest(myOrigData, SignUtils::digestSha1, myPrivKeyFileNameWithPass, "invalid password"), SignUtils::SignError);
        TS_ASSERT_THROWS(SignUtils::verifyDigest(myOrigData, myExpectedSignedDigest, SignUtils::digestSha1, "__UnexistingVerifyKeyFile__"), SignUtils::VerifyError);
        TS_ASSERT_THROWS(SignUtils::verifyDigest(myOrigData, myExpectedSignedDigest, SignUtils::digestSha1, std::vector<unsigned char>()), SignUtils::VerifyError);
        TS_ASSERT_THROWS(SignUtils::verifyDigest(std::vector<unsigned char>(), myExpectedSignedDigest, SignUtils::digestSha256, myPubKeyFileNameWithPass), SignUtils::VerifyError);
        TS_ASSERT_THROWS(SignUtils::verifyDigest(myOrigData, std::vector<unsigned char>(), SignUtils::digestSha256, myPubKeyFileNameWithPass), SignUtils::VerifyError);

    }

    void testSignVerifyPkcs7()
    {
        using std::vector;
        using std::string;
		using namespace ta;

        static const string myOrigDataFileName                     = "CA/orig.data";
        static const string myResultedSignedPkcs7SmimeFileName     = "signed.pkcs7.smime";
        static const string myResultedSignedPkcs7NoSmimeFileName   = "signed.pkcs7.nosmime";
        static const string mySigningCertPass                      = "kaaskaas";
        static const string mySigningCertWithPrivKey               = "CA/signingcertkey.pem";
        static const string myVerificationCa                       = "CA/signingcertissuercert.pem";
        static const string otherVerificationCa                    = "CA/cert.pem";

        const string myOrigData = readData(myOrigDataFileName);

        TS_TRACE("Test sign-verify with SMIME headers");
        // Sign: openssl smime -sign -in CA/orig.data -signer CA/signingcertkey.pem -inkey CA/signingcertkey.pem -out signed.pkcs7.smime
        // Verify: openssl smime -verify -in signed.pkcs7.smime -CAfile CA/signingcertissuercert.pem

        // @note we do not compare signing results of file- and buffer- versions of signPKCS7() because signing may add some random noise (e.g. depends on time)

        SignUtils::signPKCS7(myOrigDataFileName, myResultedSignedPkcs7SmimeFileName, mySigningCertPass, mySigningCertWithPrivKey, true);
        TS_ASSERT_EQUALS(SignUtils::loadNotVerifyPKCS7WithSMIME(myResultedSignedPkcs7SmimeFileName), myOrigData);
        TS_ASSERT_EQUALS(SignUtils::verifyPKCS7(myResultedSignedPkcs7SmimeFileName, myVerificationCa, true), myOrigData);
        TS_ASSERT_THROWS(SignUtils::verifyPKCS7(myResultedSignedPkcs7SmimeFileName, myVerificationCa, false), SignUtils::VerifyError);

        vector<unsigned char> myActualSignedData = SignUtils::signPKCS7(ta::str2Vec<unsigned char>(myOrigData), mySigningCertPass, mySigningCertWithPrivKey, true);
        TS_ASSERT_EQUALS(SignUtils::verifyPKCS7(myActualSignedData, myVerificationCa, true), myOrigData);
        TS_ASSERT_THROWS(SignUtils::verifyPKCS7(myActualSignedData, myVerificationCa, false), SignUtils::VerifyError);

        TS_TRACE("Test sign-verify without SMIME headers (PKCS#7)");
        // Sign: openssl smime -sign -outform PEM -in CA/orig.data -signer CA/signingcertkey.pem -inkey CA/signingcertkey.pem -out signed.pkcs7.nosmime
        // Verify: openssl smime -verify -inform PEM -in signed.pkcs7.nosmime -CAfile CA/signingcertissuercert.pem

        SignUtils::signPKCS7(myOrigDataFileName, myResultedSignedPkcs7NoSmimeFileName, mySigningCertPass, mySigningCertWithPrivKey, false);
        TS_ASSERT_EQUALS(SignUtils::verifyPKCS7(myResultedSignedPkcs7NoSmimeFileName, myVerificationCa, false), myOrigData);
        TS_ASSERT_THROWS(SignUtils::verifyPKCS7(myResultedSignedPkcs7NoSmimeFileName, myVerificationCa, true), SignUtils::VerifyError);
        TS_ASSERT_THROWS(SignUtils::loadNotVerifyPKCS7WithSMIME(myResultedSignedPkcs7NoSmimeFileName), std::exception);

        myActualSignedData = SignUtils::signPKCS7(ta::str2Vec<unsigned char>(myOrigData), mySigningCertPass, mySigningCertWithPrivKey, false);
        TS_ASSERT_EQUALS(SignUtils::verifyPKCS7(myActualSignedData, myVerificationCa, false), myOrigData);
        TS_ASSERT_THROWS(SignUtils::verifyPKCS7(myActualSignedData, myVerificationCa, true), SignUtils::VerifyError);

        TS_TRACE("Test with invalid input");
        //
        TS_ASSERT_THROWS(SignUtils::signPKCS7("__bad_data_file_name_", myResultedSignedPkcs7SmimeFileName, mySigningCertPass, mySigningCertWithPrivKey, true), SignUtils::SignError);
        TS_ASSERT_THROWS(SignUtils::signPKCS7(myOrigDataFileName, myResultedSignedPkcs7SmimeFileName, "_bad_password_", mySigningCertWithPrivKey, true), SignUtils::SignError);
        TS_ASSERT_THROWS(SignUtils::signPKCS7(myOrigDataFileName, myResultedSignedPkcs7SmimeFileName, mySigningCertPass, "__bad_cert_file_", true), SignUtils::SignError);
        TS_ASSERT_THROWS(SignUtils::signPKCS7(vector<unsigned char>(), mySigningCertPass, mySigningCertWithPrivKey, true), SignUtils::SignError);

        TS_ASSERT_THROWS(SignUtils::signPKCS7("__bad_data_file_name_", myResultedSignedPkcs7NoSmimeFileName, mySigningCertPass, mySigningCertWithPrivKey, false), SignUtils::SignError);
        TS_ASSERT_THROWS(SignUtils::signPKCS7(myOrigDataFileName, myResultedSignedPkcs7NoSmimeFileName, "_bad_password_", mySigningCertWithPrivKey, false), SignUtils::SignError);
        TS_ASSERT_THROWS(SignUtils::signPKCS7(myOrigDataFileName, myResultedSignedPkcs7NoSmimeFileName, mySigningCertPass, "__bad_cert_file_", false), SignUtils::SignError);
        TS_ASSERT_THROWS(SignUtils::signPKCS7(vector<unsigned char>(), mySigningCertPass, mySigningCertWithPrivKey, false), SignUtils::SignError);

        TS_ASSERT_THROWS(SignUtils::verifyPKCS7("__bad_input_file_", myVerificationCa, true), SignUtils::VerifyError);
        TS_ASSERT_THROWS(SignUtils::loadNotVerifyPKCS7WithSMIME("__bad_input_file_"), std::exception);
        TS_ASSERT_THROWS(SignUtils::verifyPKCS7(myResultedSignedPkcs7SmimeFileName, "bad_ca_name", true), SignUtils::VerifyError);
        TS_ASSERT_THROWS(SignUtils::verifyPKCS7(vector<unsigned char>(), "bad_ca_name", true), SignUtils::VerifyError);

        TS_ASSERT_THROWS(SignUtils::verifyPKCS7("__bad_input_file_", myVerificationCa, false), SignUtils::VerifyError);
        TS_ASSERT_THROWS(SignUtils::verifyPKCS7(myResultedSignedPkcs7NoSmimeFileName, "bad_ca_name", false), SignUtils::VerifyError);
        TS_ASSERT_THROWS(SignUtils::verifyPKCS7(vector<unsigned char>(), "bad_ca_name", false), SignUtils::VerifyError);

        // Check input file that has different signature.
        TS_ASSERT_THROWS(SignUtils::verifyPKCS7(myResultedSignedPkcs7SmimeFileName, otherVerificationCa, true), SignUtils::SignatureVerifyError);
        myActualSignedData = SignUtils::signPKCS7(ta::str2Vec<unsigned char>(myOrigData), mySigningCertPass, mySigningCertWithPrivKey, false);
        TS_ASSERT_THROWS(SignUtils::verifyPKCS7(myActualSignedData, otherVerificationCa, false), SignUtils::SignatureVerifyError);
    }
};
