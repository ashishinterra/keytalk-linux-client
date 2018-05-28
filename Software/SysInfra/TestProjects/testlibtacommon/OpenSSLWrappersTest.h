#pragma once

#include "ta/opensslwrappers.h"
#include "ta/utils.h"

#include "cxxtest/TestSuite.h"
#include "boost/assign/list_of.hpp"

class OpenSSLWrappersTest : public CxxTest::TestSuite
{
public:
    void setUp()
    {
        CxxTest::setAbortTestOnFail(true);
    }

    void testCertificateWrapper()
    {
        const std::vector<unsigned char> myCertPem = ta::readData("CA/cert.pem");
        const std::vector<unsigned char> myCertDer = ta::readData("CA/test.cer");
        const std::vector<unsigned char> myCertKeyPem = ta::readData("CA/certkey.pem");
        const std::vector<unsigned char> myKeyPem = ta::readData("CA/privkey3.pem");
        ta::OpenSSLCertificateWrapper myWrapper;

        TS_ASSERT(!(X509*)ta::OpenSSLCertificateWrapper());
        TS_ASSERT((X509*)ta::OpenSSLCertificateWrapper("CA/cert.pem"));
        TS_ASSERT((X509*)ta::OpenSSLCertificateWrapper(myCertPem));
        TS_ASSERT((X509*)ta::OpenSSLCertificateWrapper("CA/certkey.pem"));
        TS_ASSERT((X509*)ta::OpenSSLCertificateWrapper(myCertKeyPem));

        myWrapper.loadFromBuf(myCertPem);
        TS_ASSERT((X509*)myWrapper);
        myWrapper.loadFromFile("CA/cert.pem");
        TS_ASSERT((X509*)myWrapper);
        myWrapper.loadFromBuf(myCertKeyPem);
        TS_ASSERT((X509*)myWrapper);
        myWrapper.loadFromFile("CA/certkey.pem");
        TS_ASSERT((X509*)myWrapper);

        // Invalid inputs
        TS_ASSERT_THROWS(ta::OpenSSLCertificateWrapper cert("CA/test.cer"), std::exception);
        TS_ASSERT_THROWS(ta::OpenSSLCertificateWrapper().loadFromFile("CA/test.cer"), std::exception);
        TS_ASSERT_THROWS(ta::OpenSSLCertificateWrapper cert(myCertDer), std::exception);
        TS_ASSERT_THROWS(ta::OpenSSLCertificateWrapper().loadFromBuf(myCertDer), std::exception);

        TS_ASSERT_THROWS(ta::OpenSSLCertificateWrapper cert("CA/privkey3.pem"), std::exception);
        TS_ASSERT_THROWS(ta::OpenSSLCertificateWrapper().loadFromFile("CA/privkey3.pem"), std::exception);
        TS_ASSERT_THROWS(ta::OpenSSLCertificateWrapper cert(myKeyPem), std::exception);
        TS_ASSERT_THROWS(ta::OpenSSLCertificateWrapper().loadFromBuf(myKeyPem), std::exception);

        TS_ASSERT_THROWS(ta::OpenSSLCertificateWrapper(std::vector<unsigned char>()), std::exception);
        TS_ASSERT_THROWS(ta::OpenSSLCertificateWrapper().loadFromBuf(std::vector<unsigned char>()), std::exception);

        TS_ASSERT_THROWS(ta::OpenSSLCertificateWrapper cert("CA/__nonexisting_cert__.pem"), std::exception);
        TS_ASSERT_THROWS(ta::OpenSSLCertificateWrapper().loadFromFile("CA/__nonexisting_cert__.pem"), std::exception);

        TS_ASSERT_THROWS(ta::OpenSSLCertificateWrapper cert(NULL), std::exception);
    }

    void testPrivateKeyWrapper()
    {
        const std::vector<unsigned char> myEncryptedKeyPem = ta::readData("CA/privkey.pem");
        const std::vector<unsigned char> myKeyPem = ta::readData("CA/privkey3.pem");
        const std::vector<unsigned char> myCertKeyPem = ta::readData("CA/certkey.pem");
        const std::vector<unsigned char> myPubKeyPem = ta::readData("CA/pubkey.pem");
        const std::vector<unsigned char> myCertPem = ta::readData("CA/cert.pem");
        ta::OpenSSLPrivateKeyWrapper myWrapper;

        TS_ASSERT(!(EVP_PKEY*)ta::OpenSSLPrivateKeyWrapper());
        TS_ASSERT((EVP_PKEY*)ta::OpenSSLPrivateKeyWrapper("CA/privkey3.pem"));
        TS_ASSERT((EVP_PKEY*)ta::OpenSSLPrivateKeyWrapper(myKeyPem));
        TS_ASSERT((EVP_PKEY*)ta::OpenSSLPrivateKeyWrapper("CA/privkey.pem", "kaaskaas"));
        TS_ASSERT((EVP_PKEY*)ta::OpenSSLPrivateKeyWrapper(myEncryptedKeyPem, "kaaskaas"));
        TS_ASSERT((EVP_PKEY*)ta::OpenSSLPrivateKeyWrapper(myCertKeyPem));

        myWrapper.loadFromBuf(myKeyPem);
        TS_ASSERT((EVP_PKEY*)myWrapper);
        myWrapper.loadFromBuf(myEncryptedKeyPem, "kaaskaas");
        TS_ASSERT((EVP_PKEY*)myWrapper);
        myWrapper.loadFromFile("CA/privkey3.pem");
        TS_ASSERT((EVP_PKEY*)myWrapper);
        myWrapper.loadFromFile("CA/privkey.pem", "kaaskaas");
        TS_ASSERT((EVP_PKEY*)myWrapper);

        // Invalid inputs
        TS_ASSERT_THROWS(ta::OpenSSLPrivateKeyWrapper key("CA/privkey.pem"), std::exception); // no password
        TS_ASSERT_THROWS(ta::OpenSSLPrivateKeyWrapper key(myEncryptedKeyPem), std::exception); // no password
        TS_ASSERT_THROWS(ta::OpenSSLPrivateKeyWrapper key("CA/privkey.pem", "invalid password"), std::exception);
        TS_ASSERT_THROWS(ta::OpenSSLPrivateKeyWrapper key(myEncryptedKeyPem, "invalid password"), std::exception); // no password
        TS_ASSERT_THROWS(myWrapper.loadFromFile("CA/privkey.pem"), std::exception); // no password
        TS_ASSERT_THROWS(myWrapper.loadFromBuf(myEncryptedKeyPem), std::exception); // no password
        TS_ASSERT_THROWS(myWrapper.loadFromFile("CA/privkey.pem", "invalid password"), std::exception);
        TS_ASSERT_THROWS(myWrapper.loadFromBuf(myEncryptedKeyPem, "invalid password"), std::exception);

        TS_ASSERT_THROWS(ta::OpenSSLPrivateKeyWrapper key("CA/pubkey.pem"), std::exception);
        TS_ASSERT_THROWS(myWrapper.loadFromFile("CA/pubkey.pem"), std::exception);
        TS_ASSERT_THROWS(ta::OpenSSLPrivateKeyWrapper key(myPubKeyPem), std::exception);
        TS_ASSERT_THROWS(myWrapper.loadFromBuf(myPubKeyPem), std::exception);

        TS_ASSERT_THROWS(ta::OpenSSLPrivateKeyWrapper key("CA/cert.pem"), std::exception);
        TS_ASSERT_THROWS(myWrapper.loadFromFile("CA/cert.pem"), std::exception);
        TS_ASSERT_THROWS(ta::OpenSSLPrivateKeyWrapper key(myCertPem), std::exception);
        TS_ASSERT_THROWS(myWrapper.loadFromBuf(myCertPem), std::exception);

        TS_ASSERT_THROWS(ta::OpenSSLPrivateKeyWrapper(std::vector<unsigned char>()), std::exception);
        TS_ASSERT_THROWS(myWrapper.loadFromBuf(std::vector<unsigned char>()), std::exception);

        TS_ASSERT_THROWS(myWrapper.loadFromFile("CA/__nonexisting_cert__.pem"), std::exception);
        TS_ASSERT_THROWS(ta::OpenSSLPrivateKeyWrapper key("CA/__nonexisting_cert__.pem"), std::exception);
    }

    void testPublicKeyWrapper()
    {
        const std::vector<unsigned char> myPubKeyPem = ta::readData("CA/pubkey.pem");
        const std::vector<unsigned char> myRsaPubKeyPem = ta::readData("CA/rsapubkey.pem");
        const std::vector<unsigned char> myCertKeyPem = ta::readData("CA/certkey.pem");
        const std::vector<unsigned char> myPrivKeyPem = ta::readData("CA/privkey3.pem");
        const std::vector<unsigned char> myCertPem = ta::readData("CA/cert.pem");
        ta::OpenSSLPublicKeyWrapper myWrapper;

        TS_ASSERT(!(EVP_PKEY*)ta::OpenSSLPublicKeyWrapper());
        TS_ASSERT((EVP_PKEY*)ta::OpenSSLPublicKeyWrapper(myPubKeyPem, ta::RsaUtils::pubkeySubjectPublicKeyInfo));
        TS_ASSERT((EVP_PKEY*)ta::OpenSSLPublicKeyWrapper(myRsaPubKeyPem, ta::RsaUtils::pubkeyPKCS1));

        myWrapper.loadFromBuf(myPubKeyPem, ta::RsaUtils::pubkeySubjectPublicKeyInfo);
        TS_ASSERT((EVP_PKEY*)myWrapper);
        myWrapper.loadFromBuf(myRsaPubKeyPem, ta::RsaUtils::pubkeyPKCS1);
        TS_ASSERT((EVP_PKEY*)myWrapper);

        // Mismatched key and encoding
        TS_ASSERT_THROWS(ta::OpenSSLPublicKeyWrapper key(myPubKeyPem, ta::RsaUtils::pubkeyPKCS1), std::exception);
        TS_ASSERT_THROWS(ta::OpenSSLPublicKeyWrapper key(myRsaPubKeyPem, ta::RsaUtils::pubkeySubjectPublicKeyInfo), std::exception);
        TS_ASSERT_THROWS(myWrapper.loadFromBuf(myPubKeyPem, ta::RsaUtils::pubkeyPKCS1), std::exception);
        TS_ASSERT_THROWS(myWrapper.loadFromBuf(myRsaPubKeyPem, ta::RsaUtils::pubkeySubjectPublicKeyInfo), std::exception);

        // Invalid key type
        TS_ASSERT_THROWS(ta::OpenSSLPublicKeyWrapper key(myPrivKeyPem, ta::RsaUtils::pubkeySubjectPublicKeyInfo), std::exception);
        TS_ASSERT_THROWS(ta::OpenSSLPublicKeyWrapper key(myCertKeyPem, ta::RsaUtils::pubkeySubjectPublicKeyInfo), std::exception);
        TS_ASSERT_THROWS(ta::OpenSSLPublicKeyWrapper key(myCertPem, ta::RsaUtils::pubkeySubjectPublicKeyInfo), std::exception);
        TS_ASSERT_THROWS(myWrapper.loadFromBuf(myPrivKeyPem, ta::RsaUtils::pubkeySubjectPublicKeyInfo), std::exception);
        TS_ASSERT_THROWS(myWrapper.loadFromBuf(myCertKeyPem, ta::RsaUtils::pubkeySubjectPublicKeyInfo), std::exception);
        TS_ASSERT_THROWS(myWrapper.loadFromBuf(myCertPem, ta::RsaUtils::pubkeySubjectPublicKeyInfo), std::exception);
    }
};
