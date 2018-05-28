#pragma once
#include "ta/certutils.h"
#include "ta/rsautils.h"
#include "ta/signutils.h"
#include "ta/process.h"
#include "ta/utils.h"
#include "ta/scopedresource.hpp"
#include "cxxtest/TestSuite.h"
#include "boost/assign/list_of.hpp"
#include "openssl/obj_mac.h" // for NID tests
#include "openssl/x509.h"
#include <vector>

class CertUtilsTest : public CxxTest::TestSuite
{
public:
    void setUp()
    {
        CxxTest::setAbortTestOnFail(true);
    }

    void testGetCertInfo()
    {
        // Read PEM from file
        const ta::CertUtils::CertInfo myCertInfoPemFile = ta::CertUtils::getCertInfoFile("CA/certkey.pem");
        TS_ASSERT_EQUALS(myCertInfoPemFile.subjName,   "/C=NL/ST=Utrecht/L=Utrecht/O=TrustAlert DEMO/OU=Demo Only/CN=cursus3.trustalert.com/emailAddress=prodinfo@trustalert.com");
        TS_ASSERT_EQUALS(myCertInfoPemFile.subjCN,   "cursus3.trustalert.com");
        TS_ASSERT_EQUALS(myCertInfoPemFile.subjO,   "TrustAlert DEMO");
        TS_ASSERT_EQUALS(myCertInfoPemFile.subjOU,   "Demo Only");
        TS_ASSERT_EQUALS(myCertInfoPemFile.issuerName, "/C=NL/ST=Utrecht/L=Utrecht/O=TrustAlert DEMO/OU=Demo Only/CN=RESEPT DEMO SCA/emailAddress=prodinfo@trustalert.com");
        TS_ASSERT_EQUALS(myCertInfoPemFile.issuerCN, "RESEPT DEMO SCA");
        TS_ASSERT_EQUALS(myCertInfoPemFile.serial, "06");
        TS_ASSERT_EQUALS(myCertInfoPemFile.sha1Fingerprint, "54cb78a2c4d1ba31f3abec6385f558e8e57e1429");
        TS_ASSERT(ta::equalIgnoreOrder(myCertInfoPemFile.keyUsage, boost::assign::list_of(ta::CertUtils::keyusageKeyEncipherment)));
        TS_ASSERT_EQUALS(myCertInfoPemFile.extKeyUsage, std::vector<ta::CertUtils::ExtendedKeyUsage>());
        TS_ASSERT_EQUALS(myCertInfoPemFile.signatureAlgorithm.nid, NID_sha1WithRSAEncryption);
        TS_ASSERT_EQUALS(myCertInfoPemFile.signatureAlgorithm.name, "sha1WithRSAEncryption");
        TS_ASSERT_EQUALS(myCertInfoPemFile.pubKeyType, ta::CertUtils::keyRsa);
        TS_ASSERT_EQUALS(myCertInfoPemFile.pubKeyBits, 1024U);
        TS_ASSERT(myCertInfoPemFile.crlDistributinPoints.empty());
        TS_ASSERT(myCertInfoPemFile.ocspUrls.empty());
        TS_ASSERT(!myCertInfoPemFile.basicConstraints.isCA);
        time_t myUtcNotBefore = myCertInfoPemFile.utcNotBefore;
        tm myUtcNotBeforeTm = *gmtime(&myUtcNotBefore);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_isdst, 0);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_year, 108);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_mon, 3);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_mday, 28);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_hour, 11);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_min, 17);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_sec, 26);
        time_t myUtcNotAfter = myCertInfoPemFile.utcNotAfter;
        tm myUtcNotAfterTm = *gmtime(&myUtcNotAfter);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_isdst, 0);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_year, 109);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_mon, 3);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_mday, 28);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_hour, 11);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_min, 17);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_sec, 26);

        std::vector<ta::CertUtils::CertInfo> myCertsInfoPemFile = ta::CertUtils::getPemCertsInfoFile("CA/certkey.pem");
        TS_ASSERT_EQUALS(myCertsInfoPemFile.size(), 1U);
        TS_ASSERT_EQUALS(myCertsInfoPemFile.at(0), myCertInfoPemFile);
        const std::string myCertsInfoPemBuf = ta::readData("CA/certkey.pem");
        TS_ASSERT_EQUALS(ta::CertUtils::getPemCertsInfo(myCertsInfoPemBuf), myCertsInfoPemFile);

        // Read PEM cert from memory buffer
        std::vector<char> myPemMemBuf = ta::readData("CA/certkey.pem");
        TS_ASSERT_EQUALS(ta::CertUtils::getCertInfo(myPemMemBuf), myCertInfoPemFile);

        // Read the same cert as DER from file
        TS_ASSERT_EQUALS(ta::CertUtils::getCertInfoFile("CA/test.cer", ta::CertUtils::DER), myCertInfoPemFile);

        // Read the same cert DER from memory buffer
        std::vector<char> myDerMemBuf = ta::readData("CA/test.cer");
        TS_ASSERT_EQUALS(ta::CertUtils::getCertInfo(myDerMemBuf, ta::CertUtils::DER), myCertInfoPemFile);

        // Read from PEM containing several PEM certs
        std::vector<ta::CertUtils::CertInfo> myCertInfoMultiPemFile = ta::CertUtils::getPemCertsInfoFile("CA/2cert1key.pem");
        TS_ASSERT_EQUALS(myCertInfoMultiPemFile.size(), 2U);
        ta::CertUtils::CertInfo myCertInfo = myCertInfoMultiPemFile.at(0);
        TS_ASSERT_EQUALS(myCertInfo.subjName, "/C=NL/ST=Utrecht/L=Soesterberg/O=Resept Demo/OU=Demo Only/CN=Resept Demo CCA/emailAddress=demo@reseptdemo.com");
        TS_ASSERT_EQUALS(myCertInfo.subjCN,   "Resept Demo CCA");
        TS_ASSERT_EQUALS(myCertInfo.subjO, "Resept Demo");
        TS_ASSERT_EQUALS(myCertInfo.subjOU, "Demo Only");
        TS_ASSERT_EQUALS(myCertInfo.issuerName, "/C=NL/ST=Utrecht/L=Soesterberg/O=Resept Demo/OU=Demo Only/CN=Resept Demo PCA/emailAddress=demo@reseptdemo.com");
        TS_ASSERT_EQUALS(myCertInfo.issuerCN, "Resept Demo PCA");
        TS_ASSERT_EQUALS(myCertInfo.sha1Fingerprint, "9827f63b4ddeb88ce7fa07d0a36134f12cf60537");
        TS_ASSERT(ta::equalIgnoreOrder(myCertInfo.keyUsage, boost::assign::list_of(ta::CertUtils::keyusageCertificateSign)));
        TS_ASSERT_EQUALS(myCertInfo.signatureAlgorithm.nid, NID_sha1WithRSAEncryption);
        TS_ASSERT_EQUALS(myCertInfo.signatureAlgorithm.name, "sha1WithRSAEncryption");
        TS_ASSERT_EQUALS(myCertInfo.pubKeyType, ta::CertUtils::keyRsa);
        TS_ASSERT_EQUALS(myCertInfo.pubKeyBits, 4096U);
        TS_ASSERT(myCertInfo.crlDistributinPoints.empty());
        TS_ASSERT(myCertInfo.ocspUrls.empty());
        TS_ASSERT(myCertInfo.basicConstraints.isCA);
        TS_ASSERT_EQUALS(myCertInfo.basicConstraints.pathLen, ta::CertUtils::PathLenConstraintNone);
        myUtcNotBefore = myCertInfo.utcNotBefore;
        myUtcNotBeforeTm = *gmtime(&myUtcNotBefore);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_isdst, 0);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_year, 111);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_mon, 2);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_mday, 22);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_hour, 13);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_min, 27);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_sec, 19);
        myUtcNotAfter = myCertInfo.utcNotAfter;
        myUtcNotAfterTm = *gmtime(&myUtcNotAfter);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_isdst, 0);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_year, 127);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_mon, 6);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_mday, 6);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_hour, 13);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_min, 27);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_sec, 19);
        const std::string myCertInfoMultiPemBuf = ta::readData("CA/2cert1key.pem");
        TS_ASSERT_EQUALS(ta::CertUtils::getPemCertsInfo(myCertInfoMultiPemBuf), myCertInfoMultiPemFile);

        // parent CA
        myCertInfo = myCertInfoMultiPemFile.at(1);
        TS_ASSERT_EQUALS(myCertInfo.subjName, "/C=NL/ST=Utrecht/L=Soesterberg/O=Resept Demo/OU=Demo Only/CN=Resept Demo PCA/emailAddress=demo@reseptdemo.com");
        TS_ASSERT_EQUALS(myCertInfo.subjCN,   "Resept Demo PCA");
        TS_ASSERT_EQUALS(myCertInfo.subjO, "Resept Demo");
        TS_ASSERT_EQUALS(myCertInfo.subjOU, "Demo Only");
        TS_ASSERT_EQUALS(myCertInfo.issuerName, "/C=NL/ST=Utrecht/L=Soesterberg/O=Resept Demo/OU=Demo Only/CN=Resept Demo PCA/emailAddress=demo@reseptdemo.com");
        TS_ASSERT_EQUALS(myCertInfo.issuerCN, "Resept Demo PCA");
        TS_ASSERT_EQUALS(myCertInfo.sha1Fingerprint, "41bf13190c4bfe0b4a63cd25bab89b57f4edee24");
        TS_ASSERT(ta::equalIgnoreOrder(myCertInfo.keyUsage, boost::assign::list_of(ta::CertUtils::keyusageCertificateSign)));
        TS_ASSERT_EQUALS(myCertInfo.signatureAlgorithm.nid, NID_sha1WithRSAEncryption);
        TS_ASSERT_EQUALS(myCertInfo.signatureAlgorithm.name, "sha1WithRSAEncryption");
        TS_ASSERT_EQUALS(myCertInfo.pubKeyType, ta::CertUtils::keyRsa);
        TS_ASSERT_EQUALS(myCertInfo.pubKeyBits, 4096U);
        TS_ASSERT(myCertInfo.crlDistributinPoints.empty());
        TS_ASSERT(myCertInfo.ocspUrls.empty());
        TS_ASSERT(myCertInfo.basicConstraints.isCA);
        TS_ASSERT_EQUALS(myCertInfo.basicConstraints.pathLen, ta::CertUtils::PathLenConstraintNone);
        myUtcNotBefore = myCertInfo.utcNotBefore;
        myUtcNotBeforeTm = *gmtime(&myUtcNotBefore);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_isdst, 0);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_year, 111);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_mon, 2);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_mday, 22);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_hour, 13);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_min, 22);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_sec, 53);
        myUtcNotAfter = myCertInfo.utcNotAfter;
        myUtcNotAfterTm = *gmtime(&myUtcNotAfter);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_isdst, 0);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_year, 127);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_mon, 7);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_mday, 25);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_hour, 13);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_min, 22);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_sec, 53);

        TS_ASSERT_EQUALS(myCertInfoMultiPemFile.at(0), ta::CertUtils::getCertInfoFile("CA/2cert1key.pem"));

        // Invalid inputs
        TS_ASSERT_THROWS(ta::CertUtils::getCertInfo(std::vector<char>()), std::exception);
        TS_ASSERT_THROWS(ta::CertUtils::getCertInfoFile("CA/test.cer"), std::exception);
        TS_ASSERT_THROWS(ta::CertUtils::getCertInfoFile("CA/test.cer", ta::CertUtils::PEM), std::exception);
        TS_ASSERT_THROWS(ta::CertUtils::getCertInfoFile("CA/certkey.pem", ta::CertUtils::DER), std::exception);
        TS_ASSERT(ta::CertUtils::getPemCertsInfoFile("CA/__nonexisting_cert__.pem").empty());
        TS_ASSERT(ta::CertUtils::getPemCertsInfo("invalid-PEM-buffer").empty());
    }

    // just another cert test for better coverage
    void testGetCertInfo2()
    {
        using boost::assign::list_of;

        const ta::CertUtils::CertInfo myCertInfoPemFile = ta::CertUtils::getCertInfoFile("CA/keytalk.com.cert.pem");
        TS_ASSERT_EQUALS(myCertInfoPemFile.subjName, "/C=NL/ST=Utrecht/L=Utrecht/OU=Operations/O=KeyTalk BV/CN=*.keytalk.com");
        TS_ASSERT_EQUALS(myCertInfoPemFile.subjCN, "*.keytalk.com");
        TS_ASSERT_EQUALS(myCertInfoPemFile.subjO, "KeyTalk BV");
        TS_ASSERT_EQUALS(myCertInfoPemFile.subjOU, "Operations");
        TS_ASSERT_EQUALS(myCertInfoPemFile.issuerName, "/C=BE/O=GlobalSign nv-sa/CN=GlobalSign Organization Validation CA - SHA256 - G2");
        TS_ASSERT_EQUALS(myCertInfoPemFile.issuerCN, "GlobalSign Organization Validation CA - SHA256 - G2");
        TS_ASSERT_EQUALS(myCertInfoPemFile.serial, "11:21:7b:82:ef:53:23:c8:a2:1e:09:6b:6f:d7:4f:91:29:30");
        TS_ASSERT_EQUALS(myCertInfoPemFile.sha1Fingerprint, "d92a56c2bd812fb01e11992a5ea23c38e30a1846");
        TS_ASSERT(ta::equalIgnoreOrder(myCertInfoPemFile.keyUsage, list_of(ta::CertUtils::keyusageDigitalSignature)(ta::CertUtils::keyusageKeyEncipherment)));
        TS_ASSERT(ta::equalIgnoreOrder(myCertInfoPemFile.extKeyUsage, list_of(ta::CertUtils::ekuClientAuth)(ta::CertUtils::ekuServerAuth)));
        TS_ASSERT_EQUALS(myCertInfoPemFile.signatureAlgorithm.nid, NID_sha256WithRSAEncryption);
        TS_ASSERT_EQUALS(myCertInfoPemFile.signatureAlgorithm.name, "sha256WithRSAEncryption");
        TS_ASSERT_EQUALS(myCertInfoPemFile.pubKeyType, ta::CertUtils::keyRsa);
        TS_ASSERT_EQUALS(myCertInfoPemFile.pubKeyBits, 2048U);
        TS_ASSERT(!myCertInfoPemFile.basicConstraints.isCA);
        TS_ASSERT_EQUALS(myCertInfoPemFile.crlDistributinPoints, list_of("http://crl.globalsign.com/gs/gsorganizationvalsha2g2.crl"));
        TS_ASSERT_EQUALS(myCertInfoPemFile.ocspUrls, list_of("http://ocsp2.globalsign.com/gsorganizationvalsha2g2"));
        time_t myUtcNotBefore = myCertInfoPemFile.utcNotBefore;
        tm myUtcNotBeforeTm = *gmtime(&myUtcNotBefore);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_isdst, 0);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_year, 115);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_mon, 4);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_mday, 27);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_hour, 18);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_min, 31);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_sec, 03);
        time_t myUtcNotAfter = myCertInfoPemFile.utcNotAfter;
        tm myUtcNotAfterTm = *gmtime(&myUtcNotAfter);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_isdst, 0);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_year, 117);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_mon, 6);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_mday, 11);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_hour, 15);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_min, 30);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_sec, 03);

        TS_ASSERT_EQUALS(ta::CertUtils::getCertInfoFile("CA/revokedcert.pem").crlDistributinPoints,
                            list_of("http://crl3.digicert.com/ssca-sha2-g5.crl")("http://crl4.digicert.com/ssca-sha2-g5.crl"));
    }

    void test_that_cert_valid_after_2037_can_be_parsed()
    {
#ifdef RESEPT_SERVER
        const ta::CertUtils::CertInfo myCertInfoPemFile = ta::CertUtils::getCertInfoFile("CA/cert_valid_till_2044.pem");
        TS_ASSERT_EQUALS(myCertInfoPemFile.subjName, "/C=NL/ST=Noord Brabant/L=Eindhoven/O=Sioux/OU=Development/CN=test.sioux.eu/emailAddress=test@sioux.eu");
        TS_ASSERT_EQUALS(myCertInfoPemFile.subjCN, "test.sioux.eu");
        TS_ASSERT_EQUALS(myCertInfoPemFile.subjO, "Sioux");
        TS_ASSERT_EQUALS(myCertInfoPemFile.subjOU, "Development");
        TS_ASSERT_EQUALS(myCertInfoPemFile.issuerName, "/C=NL/ST=Noord Brabant/L=Eindhoven/O=Sioux/OU=Development/CN=test.sioux.eu/emailAddress=test@sioux.eu");
        TS_ASSERT_EQUALS(myCertInfoPemFile.issuerCN, "test.sioux.eu");
        TS_ASSERT_EQUALS(myCertInfoPemFile.serial, "b8:d4:8f:62:9c:4f:44:23");
        TS_ASSERT(myCertInfoPemFile.basicConstraints.isCA);
        time_t myUtcNotBefore = myCertInfoPemFile.utcNotBefore;
        tm myUtcNotBeforeTm = *gmtime(&myUtcNotBefore);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_isdst, 0);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_year, 116);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_mon, 9);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_mday, 21);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_hour, 7);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_min, 13);
        TS_ASSERT_EQUALS(myUtcNotBeforeTm.tm_sec, 9);
        time_t myUtcNotAfter = myCertInfoPemFile.utcNotAfter;
        tm myUtcNotAfterTm = *gmtime(&myUtcNotAfter);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_isdst, 0);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_year, 144);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_mon, 2);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_mday, 8);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_hour, 7);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_min, 13);
        TS_ASSERT_EQUALS(myUtcNotAfterTm.tm_sec, 9);
#else
        TS_SKIP("This test is intended for 64-bit platforms only");
#endif
    }

    void test_get_optional_extensions()
    {
        // when
        ta::CertUtils::CertInfo myCertInfo = ta::CertUtils::getCertInfoFile("CA/test-exts.pem");
        // then
        TS_ASSERT_EQUALS(myCertInfo.optionalExtensions["nsBaseUrl"], "CUST_PASSWD_MYSQL");
        TS_ASSERT_EQUALS(myCertInfo.optionalExtensions["subjectAltName"], "DNS:r4webdemo.gotdns.com,IP:192.168.33.111,IP:192.168.33.102");

        // when
        myCertInfo = ta::CertUtils::getCertInfoFile("CA/keytalk.com.cert.pem");
        // then
        TS_ASSERT_EQUALS(myCertInfo.optionalExtensions["subjectAltName"], "DNS:*.keytalk.com,DNS:autodiscover.keytalk.com,DNS:mail.keytalk.com,DNS:owa.keytalk.com,DNS:keytalk.com");
        TS_ASSERT(!ta::isKeyExist("nsBaseUrl", myCertInfo.optionalExtensions));
    }

    void test_get_pathLenConstraint()
    {
        const ta::CertUtils::CertInfo myCertInfo = ta::CertUtils::getCertInfoFile("CA/CA_with_pathLenConstraint.pem");

        TS_ASSERT(myCertInfo.basicConstraints.isCA);
        TS_ASSERT_EQUALS(myCertInfo.basicConstraints.pathLen, 8);
    }

    void test_get_cert_valid_from_1999()
    {
       const  ta::CertUtils::CertInfo myCertInfo = ta::CertUtils::getCertInfoFile("CA/cert_valid_from_1999.pem");

        time_t myUtcNotBefore = myCertInfo.utcNotBefore;
        TS_ASSERT_EQUALS(gmtime(&myUtcNotBefore)->tm_year, 99); // 1999
        time_t myUtcNotAfter = myCertInfo.utcNotAfter;
        TS_ASSERT_EQUALS(gmtime(&myUtcNotAfter)->tm_year, 109); // 2009
    }

    void test_that_sha26RSA_signed_cert_can_be_parsed()
    {
        const ta::CertUtils::CertInfo myCertInfo = ta::CertUtils::getCertInfoFile("CA/cert_sha256RSA.pem");

        TS_ASSERT_EQUALS(myCertInfo.signatureAlgorithm.nid, NID_sha256WithRSAEncryption);
        TS_ASSERT_EQUALS(myCertInfo.signatureAlgorithm.name, "sha256WithRSAEncryption");
    }

    void testThatCertsAreExtractedInTheOrderTheyAppear()
    {
        using namespace ta::CertUtils;

        // when
        std::vector<MemBuffer> myCerts = extractPemCertsFromFile("CA/certkey.pem");
        // then
        TS_ASSERT_EQUALS(myCerts.size(), 1U);
        std::string myCertsBuf = ta::readData("CA/certkey.pem");
        TS_ASSERT_EQUALS(extractPemCerts(myCertsBuf), myCerts);

        // when
        myCerts = extractPemCertsFromFile("CA/2cert1key.pem");
        // then
        TS_ASSERT_EQUALS(myCerts.size(), 2U);
        TS_ASSERT(isCertIssuedBy(myCerts[0], myCerts[1]));
        TS_ASSERT(!isCertIssuedBy(myCerts[1], myCerts[0]));
        myCertsBuf = ta::readData("CA/2cert1key.pem");
        TS_ASSERT_EQUALS(extractPemCerts(myCertsBuf), myCerts);

        // when
        myCerts = extractPemCertsFromFile("CA/3cert.pem");
        // then
        TS_ASSERT_EQUALS(myCerts.size(), 3U);
        TS_ASSERT(isCertIssuedBy(myCerts[0], myCerts[1]));
        TS_ASSERT(isCertIssuedBy(myCerts[1], myCerts[2]));
        TS_ASSERT(!isCertIssuedBy(myCerts[0], myCerts[0]));
        TS_ASSERT(!isCertIssuedBy(myCerts[0], myCerts[2]));
        TS_ASSERT(!isCertIssuedBy(myCerts[2], myCerts[0]));
        myCertsBuf = ta::readData("CA/3cert.pem");
        TS_ASSERT_EQUALS(extractPemCerts(myCertsBuf), myCerts);

        // when-then (invalid input)
        TS_ASSERT_EQUALS(extractPemCertsFromFile("non-existing file").size(), 0U);
        TS_ASSERT_EQUALS(extractPemCertsFromFile("CA/privkey.pem").size(), 0U);
        TS_ASSERT_EQUALS(extractPemCerts("no-certificates-here").size(), 0U);
    }

    void testThatPrivateKeysAreExtractedInTheOrderTheyAppear()
    {
        using namespace ta::CertUtils;

        // when
        std::vector<MemBuffer> myKeys = extractPemPrivKeysFromFile("CA/certkey.pem");
        // then
        TS_ASSERT_EQUALS(myKeys.size(), 1U);
        std::string myKeysBuf = ta::readData("CA/certkey.pem");
        TS_ASSERT_EQUALS(extractPemPrivKeys(myKeysBuf), myKeys);

        // given: CA/3privkeys.pem containing 3 private keys in the following order:
        // 1024-bit non-encrypted
        // 4096-bit non-encrypted
        // 2048-bit encrypted
        // when
        myKeys = extractPemPrivKeysFromFile("CA/3privkeys.pem");
        // then
        TS_ASSERT_EQUALS(myKeys.size(), 3U);
        TS_ASSERT_EQUALS(ta::RsaUtils::getPrivateKeySizeBits(myKeys.at(0)), 1024U);
        TS_ASSERT_EQUALS(ta::RsaUtils::getPrivateKeySizeBits(myKeys.at(1)), 4096U);
        TS_ASSERT_EQUALS(ta::RsaUtils::getPrivateKeySizeBits(myKeys.at(2), "kaaskaas"), 2048U);
        myKeysBuf = ta::readData("CA/3privkeys.pem");
        TS_ASSERT_EQUALS(extractPemPrivKeys(myKeysBuf), myKeys);

        // when
        myKeys = extractPemPrivKeysFromFile("CA/3privkeys.pem", keyFilterEncryptedOnly);
        // then
        TS_ASSERT_EQUALS(myKeys.size(), 1U);
        TS_ASSERT_EQUALS(ta::RsaUtils::getPrivateKeySizeBits(myKeys.at(0), "kaaskaas"), 2048U);
        TS_ASSERT_EQUALS(extractPemPrivKeys(myKeysBuf, keyFilterEncryptedOnly), myKeys);

        // when
        myKeys = extractPemPrivKeysFromFile("CA/3privkeys.pem", keyFilterNotEncryptedOnly);
        // then
        TS_ASSERT_EQUALS(myKeys.size(), 2U);
        TS_ASSERT_EQUALS(ta::RsaUtils::getPrivateKeySizeBits(myKeys.at(0)), 1024U);
        TS_ASSERT_EQUALS(ta::RsaUtils::getPrivateKeySizeBits(myKeys.at(1)), 4096U);
        TS_ASSERT_EQUALS(extractPemPrivKeys(myKeysBuf, keyFilterNotEncryptedOnly), myKeys);

        // when-then (invalid input)
        TS_ASSERT_EQUALS(extractPemPrivKeysFromFile("non-existing file").size(), 0U);
        TS_ASSERT_EQUALS(extractPemPrivKeysFromFile("CA/cert.pem").size(), 0U);
        TS_ASSERT_EQUALS(extractPemPrivKeys("no-keys-here").size(), 0U);
    }

    void testParsePem()
    {
        using std::string;

        const string myPrivKey1 = "-----BEGIN PRIVATE KEY-----\n"
            "MIICdgIBADANBgkqhkiG9w0BAQEFAASCAmAwggJcAgEAAoGBAPbKCm/SzFwlTJ9a\n"
            "jXYaMpKwQklODkxvgGwK79+OsBI5gZ4xCdgBH+po/ZoPzuKbK7dEzy7UBNMCVQsr\n"
            "4tkSRaS2YqWtjMh4P3iCMsJfiqEBALKE2DXKI/jKdV1OkmGljUJ5s0vAwNNFaOvB\n"
            "TLf8PUH6Jr3/fyNsNUSCUEPtNIUrAgMBAAECgYA6bnnsIhAK3TX4IPuPdAJ3Ys4/\n"
            "BwExrdDNRAWpe+JKXl1HNDstbo689KDfAlEsrCq8mMkpQD/oKgwyufBbDw13zEQz\n"
            "+g7KUXcPu1iX8Os8pNXEbCtn4tl2iLQBjimTVhM4K7+ywjc7vghiHObNDjlk49ia\n"
            "rXX22nWQs637jC5p4QJBAPydiXwVsVhfiuPTo0VgNTPlm5+rbiEcQO3AHNO4Bc9Y\n"
            "vRqpAGJLRxFsnAMRdbWlZYJYsUA72kljE//6tsfd/1sCQQD6GIUqeU+1Y+BQgxDj\n"
            "kh5TQtQ2EU9NivQo4xRDK4Ecj2NaUu1IOE7Cjz2XLpsI5k62gXlJX7CrssZKbXQI\n"
            "e8pxAkA3L38ycVa91Jl17e0UIdFdlgJqXjv/1blFdxuVEPWL93nQUi04S/Oplc17\n"
            "Shwp44kKca+/NZQZRiC/Yhj+DrxTAkBJbuey5/nSjefwW/uahm3nDqlW0tBROWql\n"
            "kr+BVYXMiAGpJM5NNVNS51IzUNjuzQjxp6SgObubPpvggLDgSNDRAkEA3kx+H7Ov\n"
            "T7tqIORPQ1ilyqc+/nPqTBoy11O7If1m3FY2csKxf5ukIcfN/jdN5Gc6h0yTfoNA\n"
            "PuxrVIyLktd+jA==\n"
            "-----END PRIVATE KEY-----";

        const string myPrivKey2 = "-----BEGIN EC PRIVATE KEY-----\n"
            "MHcCAQEEIMUlJD8ZAeLxwIgvnkMZXOy1bctaIC2QsJrWS8zIBefHoAoGCCqGSM49\n"
            "AwEHoUQDQgAEksxKRiLAyKrtEnE3wWGIZr6mQJ9ZnwYRL/ixdNuR4FcehnhhwdiQ\n"
            "jz1ReckXfkK580ZbAfMMDXLnPiEhb6GKHw==\n"
            "-----END EC PRIVATE KEY-----";

        const string myPrivKey3 = "-----BEGIN RSA PRIVATE KEY-----\n"
            "Proc-Type: 4,ENCRYPTED\n"
            "DEK-Info: DES-EDE3-CBC,4B3B6EA81646ED11\n"
            "\n"
            "PZeMmwNWlvqZ8Fry4V2vuM2i6l2IMx5W2YbW4CleBXMG2QIJQf88Q67HZMQl6wXf\n"
            "C8YWEh4zVfA0t6xAwzKLzKPECLgIm7bgh3habVSDj8xvF2LqqOpWPEtxXnuWyK2H\n"
            "GSVbJSAWCS+pI/S8d+6WhtpZe6d287HV1HaY8Kla1Eo+/Xi93wmM1qkl7qrQnEDi\n"
            "iOCb435O7IdU2JdwzLc5P8PVDOvMiRaPVH+M/ai2cPNwz7wgNILV3KNdyNzmGgxp\n"
            "N3xb5iKuPsM0L5r4XTFUT80g1yom9Kk9Y4w6xStF6Blpci36a0TEMqIOPozmNX4C\n"
            "tH5g1Vks/cpGq9VLRVXv2C/HNpqh1QAPHVOI5D6kokljqCO/dXYn+tTnLPiwzDIa\n"
            "Fyyy1Un2v9AiZcczmyLgDgauvaintk3XhEyIpwLhAU0Pm+wtC1RZfWlxO96CtjxY\n"
            "sJ3mvl8xK6mfG0YjfMFnrtNlSEy0xEENnKIam3/hqMcU9gOWIYLFX4siI9P4EwoR\n"
            "pAH/31n8fPvhKa8lZi+gG7sjDhac/UXXN6WDF7DQnj7RH/lvsKaFiR4nY8HZeL94\n"
            "nFG1A31IMBhz7vMzQB3BRwPR4hP4uH+o/FkgAs4/2I7Okb6/ivQ66fJ4W/9GSsEa\n"
            "v3Y5VYdxGryRylZOAIL+y3xsy0ahVrK4xTWLZAoScwzLwAv/e2jh4JkiDrJGvoRz\n"
            "FmxOmchLKK6++q+aqHZXURaTwyRCEkXuUuJBuE1TZdhcd2fbRTjkm8fx9usJe27R\n"
            "3X3xIKkc7r/WJGqWvguWRt98o5eEQRzmmA2Hj/TTMRE=\n"
            "-----END RSA PRIVATE KEY-----";

        // the key is invalid because its header and footer are not balanced
        const string myInvalidPrivKey = "-----BEGIN RSA PRIVATE KEY-----\n"
            "Proc-Type: 4,ENCRYPTED\n"
            "DEK-Info: DES-EDE3-CBC,4B3B6EA81646ED11\n"
            "\n"
            "PZeMmwNWlvqZ8Fry4V2vuM2i6l2IMx5W2YbW4CleBXMG2QIJQf88Q67HZMQl6wXf\n"
            "C8YWEh4zVfA0t6xAwzKLzKPECLgIm7bgh3habVSDj8xvF2LqqOpWPEtxXnuWyK2H\n"
            "GSVbJSAWCS+pI/S8d+6WhtpZe6d287HV1HaY8Kla1Eo+/Xi93wmM1qkl7qrQnEDi\n"
            "iOCb435O7IdU2JdwzLc5P8PVDOvMiRaPVH+M/ai2cPNwz7wgNILV3KNdyNzmGgxp\n"
            "N3xb5iKuPsM0L5r4XTFUT80g1yom9Kk9Y4w6xStF6Blpci36a0TEMqIOPozmNX4C\n"
            "tH5g1Vks/cpGq9VLRVXv2C/HNpqh1QAPHVOI5D6kokljqCO/dXYn+tTnLPiwzDIa\n"
            "Fyyy1Un2v9AiZcczmyLgDgauvaintk3XhEyIpwLhAU0Pm+wtC1RZfWlxO96CtjxY\n"
            "sJ3mvl8xK6mfG0YjfMFnrtNlSEy0xEENnKIam3/hqMcU9gOWIYLFX4siI9P4EwoR\n"
            "pAH/31n8fPvhKa8lZi+gG7sjDhac/UXXN6WDF7DQnj7RH/lvsKaFiR4nY8HZeL94\n"
            "nFG1A31IMBhz7vMzQB3BRwPR4hP4uH+o/FkgAs4/2I7Okb6/ivQ66fJ4W/9GSsEa\n"
            "v3Y5VYdxGryRylZOAIL+y3xsy0ahVrK4xTWLZAoScwzLwAv/e2jh4JkiDrJGvoRz\n"
            "FmxOmchLKK6++q+aqHZXURaTwyRCEkXuUuJBuE1TZdhcd2fbRTjkm8fx9usJe27R\n"
            "3X3xIKkc7r/WJGqWvguWRt98o5eEQRzmmA2Hj/TTMRE=\n"
            "-----END PRIVATE KEY-----";

        const string myPubKey1 = "-----BEGIN PUBLIC KEY-----\n"
            "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQD2ygpv0sxcJUyfWo12GjKSsEJJ\n"
            "Tg5Mb4BsCu/fjrASOYGeMQnYAR/qaP2aD87imyu3RM8u1ATTAlULK+LZEkWktmKl\n"
            "rYzIeD94gjLCX4qhAQCyhNg1yiP4ynVdTpJhpY1CebNLwMDTRWjrwUy3/D1B+ia9\n"
            "/38jbDVEglBD7TSFKwIDAQAB\n"
            "-----END PUBLIC KEY-----";

        const string myPubKey2 = "-----BEGIN RSA PUBLIC KEY-----\n"
            "MIIBCgKCAQEA+xGZ/wcz9ugFpP07Nspo6U17l0YhFiFpxxU4pTk3Lifz9R3zsIsu\n"
            "ERwta7+fWIfxOo208ett/jhskiVodSEt3QBGh4XBipyWopKwZ93HHaDVZAALi/2A\n"
            "+xTBtWdEo7XGUujKDvC2/aZKukfjpOiUI8AhLAfjmlcD/UZ1QPh0mHsglRNCmpCw\n"
            "mwSXA9VNmhz+PiB+Dml4WWnKW/VHo2ujTXxq7+efMU4H2fny3Se3KYOsFPFGZ1TN\n"
            "QSYlFuShWrHPtiLmUdPoP6CV2mML1tk+l7DIIqXrQhLUKDACeM5roMx0kLhUWB8P\n"
            "+0uj1CNlNN4JRZlC7xFfqiMbFRU9Z4N6YwIDAQAB\n"
            "-----END RSA PUBLIC KEY-----";


        const string myCert1 = "-----BEGIN CERTIFICATE-----\n"
            "MIIDXDCCAkSgAwIBAgIBBjANBgkqhkiG9w0BAQUFADCBoTELMAkGA1UEBhMCTkwx\n"
            "EDAOBgNVBAgTB1V0cmVjaHQxEDAOBgNVBAcTB1V0cmVjaHQxGDAWBgNVBAoTD1Ry\n"
            "dXN0QWxlcnQgREVNTzESMBAGA1UECxMJRGVtbyBPbmx5MRgwFgYDVQQDEw9SRVNF\n"
            "UFQgREVNTyBTQ0ExJjAkBgkqhkiG9w0BCQEWF3Byb2RpbmZvQHRydXN0YWxlcnQu\n"
            "Y29tMB4XDTA4MDQyODExMTcyNloXDTA5MDQyODExMTcyNlowgagxCzAJBgNVBAYT\n"
            "Ak5MMRAwDgYDVQQIEwdVdHJlY2h0MRAwDgYDVQQHEwdVdHJlY2h0MRgwFgYDVQQK\n"
            "Ew9UcnVzdEFsZXJ0IERFTU8xEjAQBgNVBAsTCURlbW8gT25seTEfMB0GA1UEAxMW\n"
            "Y3Vyc3VzMy50cnVzdGFsZXJ0LmNvbTEmMCQGCSqGSIb3DQEJARYXcHJvZGluZm9A\n"
            "dHJ1c3RhbGVydC5jb20wgZ8wDQYJKoZIhvcNAQEBBQADgY0AMIGJAoGBAL8oLGVY\n"
            "0js+3k+N1o1dWxUwR5WXASTWhPNtIm5bDNpFY3Wp608C7MRfKLw0Ke3Chkiwi4CT\n"
            "qoLaLsjZMSoWWhX8A3H68F4XepVU+gGPVVuppi5HzYVoScZCWF+fwe7/oAfOeOR1\n"
            "cMmOvD0L06KNseRK6RbYnGORp5PjAbCkRUoRAgMBAAGjGjAYMAkGA1UdEwQCMAAw\n"
            "CwYDVR0PBAQDAgUgMA0GCSqGSIb3DQEBBQUAA4IBAQC/1zOn7Er4bBrvjNpClh+1\n"
            "0791a3X/Sk0/ExWix/WonObZjaIOSm3C3uEwDdl0AEW+ge3YMLS3jmqoUSw3Xvi6\n"
            "0h5cYZ9V8eTfv/tptmLVA38Sxkv6bo1nDPsPc7wGxpn6O9HiMHG2bviJvqh+A4U6\n"
            "ZiHIvMXyzSZdvmcufWg2ZQWKWGcvCQUrP8emplqivNFftESfFjOOnJ1g8zULmnBx\n"
            "8E2Jt3sCjafGJVNDjKlb9FNGilpwIgsJJUzt0ulXKGhxqDrWiW72oOcl0LTLmQd6\n"
            "R7dF8D9ANVcp4VS7a+JW3YXNeQ0Z0SgrG/ZckIJEcA6jgseoa5RbGyhGjjePFpd/\n"
            "-----END CERTIFICATE-----";

        const string myCert2 = "-----BEGIN CERTIFICATE-----\n"
            "MIIEATCCA2qgAwIBAgIJALq9MusSiLaBMA0GCSqGSIb3DQEBBQUAMIGyMQswCQYD\n"
            "VQQGEwJOTDEQMA4GA1UECBMHVXRyZWNodDEUMBIGA1UEBxMLU29lc3RlcmJlcmcx\n"
            "GDAWBgNVBAoTD1RydXN0QWxlcnQgREVNTzEYMBYGA1UECxMPVHJ1c3RBbGVydCBE\n"
            "RU1PMSMwIQYDVQQDExpUcnVzdEFsZXJ0IERlbW8gU2lnbmluZyBDQTEiMCAGCSqG\n"
            "SIb3DQEJARYTdGVzdEB0cnVzdGFsZXJ0LmNvbTAeFw0xMDA2MjAwODQzNTlaFw0x\n"
            "OTA2MTgwODQzNTlaMIGyMQswCQYDVQQGEwJOTDEQMA4GA1UECBMHVXRyZWNodDEU\n"
            "MBIGA1UEBxMLU29lc3RlcmJlcmcxGDAWBgNVBAoTD1RydXN0QWxlcnQgREVNTzEY\n"
            "MBYGA1UECxMPVHJ1c3RBbGVydCBERU1PMSMwIQYDVQQDExpUcnVzdEFsZXJ0IERl\n"
            "bW8gU2lnbmluZyBDQTEiMCAGCSqGSIb3DQEJARYTdGVzdEB0cnVzdGFsZXJ0LmNv\n"
            "bTCBnzANBgkqhkiG9w0BAQEFAAOBjQAwgYkCgYEAl7GrmYYYoDJt0ZKXH01iOkHP\n"
            "VD0+p7Pdv25hGmTUIHH4CU7sxQ082Px9M3ZRYQ69mUDh7kGrMeWYGazXJfJhaR9C\n"
            "///WfIlze29N/9nLUExAXxAIANndBIPpoxPBkMuuUk+kB7/a5hA4idLqLX7JNIhD\n"
            "TrkSQ9N8jZaldWdnU70CAwEAAaOCARswggEXMB0GA1UdDgQWBBRzyXZGXUpqGMTi\n"
            "7RMC9d44h0NEoTCB5wYDVR0jBIHfMIHcgBRzyXZGXUpqGMTi7RMC9d44h0NEoaGB\n"
            "uKSBtTCBsjELMAkGA1UEBhMCTkwxEDAOBgNVBAgTB1V0cmVjaHQxFDASBgNVBAcT\n"
            "C1NvZXN0ZXJiZXJnMRgwFgYDVQQKEw9UcnVzdEFsZXJ0IERFTU8xGDAWBgNVBAsT\n"
            "D1RydXN0QWxlcnQgREVNTzEjMCEGA1UEAxMaVHJ1c3RBbGVydCBEZW1vIFNpZ25p\n"
            "bmcgQ0ExIjAgBgkqhkiG9w0BCQEWE3Rlc3RAdHJ1c3RhbGVydC5jb22CCQC6vTLr\n"
            "Eoi2gTAMBgNVHRMEBTADAQH/MA0GCSqGSIb3DQEBBQUAA4GBAFB0BiKmBViIJaDc\n"
            "pfn3JiLWy301IL0T7RZqWyeAAtPBLKFG0lTVUdyqURNzfVGNI0y4f6jdiZTX9aJN\n"
            "xrhD3DgxskIVbkU1MGbD2yYuNOU1lgElLUc8Q+cL9OA2tVzbFZl5iyDX4Ira4jPB\n"
            "bgEDDLEz2zsgyR/B0hmOPX/H8WoP\n"
            "-----END CERTIFICATE-----";

        string myPemBuf = myPrivKey1 + "\n" + myPubKey1+ "\n" + myCert1;
        string myParsedBuf;
        TS_ASSERT(ta::CertUtils::hasPemCert(myPemBuf));
        TS_ASSERT(ta::CertUtils::hasPemCert(myPemBuf, &myParsedBuf));
        TS_ASSERT_EQUALS(myParsedBuf, myCert1 + "\n");
        TS_ASSERT(ta::CertUtils::hasPemPrivKey(myPemBuf));
        TS_ASSERT(ta::CertUtils::hasPemPrivKey(myPemBuf, &myParsedBuf));
        TS_ASSERT_EQUALS(myParsedBuf, myPrivKey1 + "\n");
        TS_ASSERT(ta::CertUtils::hasPemPubKey(myPemBuf));
        TS_ASSERT(ta::CertUtils::hasPemPubKey(myPemBuf, &myParsedBuf));
        TS_ASSERT_EQUALS(myParsedBuf, myPubKey1 + "\n");

        myPemBuf = myCert1 + "\n" + myPrivKey1 + "\n" + myPrivKey2 + myPubKey1 + "\n" + myCert2 + "\n" + myPubKey2 + "\n" + myPrivKey3 + "\n";
        TS_ASSERT(ta::CertUtils::hasPemCert(myPemBuf, &myParsedBuf));
        TS_ASSERT_EQUALS(myParsedBuf, myCert1 + "\n" + myCert2 + "\n");
        TS_ASSERT(ta::CertUtils::hasPemPrivKey(myPemBuf, &myParsedBuf));
        TS_ASSERT_EQUALS(myParsedBuf, myPrivKey1 + "\n" + myPrivKey2 + "\n" + myPrivKey3 + "\n");
        TS_ASSERT(ta::CertUtils::hasPemPubKey(myPemBuf, &myParsedBuf));
        TS_ASSERT_EQUALS(myParsedBuf, myPubKey1 + "\n" + myPubKey2 + "\n");

        myPemBuf = myCert1 + "\r\n" + myPrivKey3 + "\n\n\n" + myCert2 + "\r\n\n" + myPrivKey2 + "\r\n\n";
        TS_ASSERT(ta::CertUtils::hasPemCert(myPemBuf, &myParsedBuf));
        TS_ASSERT_EQUALS(myParsedBuf, myCert1 + "\n" + myCert2 + "\n");
        TS_ASSERT(ta::CertUtils::hasPemPrivKey(myPemBuf, &myParsedBuf));
        TS_ASSERT_EQUALS(myParsedBuf, myPrivKey3 + "\n" + myPrivKey2 + "\n");
        TS_ASSERT(!ta::CertUtils::hasPemPubKey(myPemBuf));
        TS_ASSERT(!ta::CertUtils::hasPemPubKey(myPemBuf, &myParsedBuf));
        TS_ASSERT_EQUALS(myParsedBuf, "");

        TS_ASSERT(!ta::CertUtils::hasPemCert(""));
        TS_ASSERT(!ta::CertUtils::hasPemCert("", &myParsedBuf));
        TS_ASSERT_EQUALS(myParsedBuf, "");
        TS_ASSERT(!ta::CertUtils::hasPemPrivKey(""));
        TS_ASSERT(!ta::CertUtils::hasPemPrivKey("", &myParsedBuf));
        TS_ASSERT_EQUALS(myParsedBuf, "");
        TS_ASSERT(!ta::CertUtils::hasPemPubKey(""));
        TS_ASSERT(!ta::CertUtils::hasPemPubKey("", &myParsedBuf));
        TS_ASSERT_EQUALS(myParsedBuf, "");

        TS_ASSERT(!ta::CertUtils::hasPemPrivKey(myInvalidPrivKey));
    }

    void testParsePemFromFile()
    {
        std::string myParsedBuf;

        // valid input
        //
        TS_ASSERT(ta::CertUtils::fileHasPemCert("CA/cert.pem", &myParsedBuf));
        TS_ASSERT(!myParsedBuf.empty());
        std::vector<X509*> myParsedCerts = ta::CertUtils::getPemCertsX509File("CA/cert.pem");
        TS_ASSERT_EQUALS(myParsedCerts.size(), 1U);
        ta::CertUtils::freeX509Certs(myParsedCerts);
        TS_ASSERT(ta::CertUtils::fileHasPemCert("CA/cert.pem"));
        TS_ASSERT(ta::CertUtils::fileHasPemCert("CA/certkey.pem", &myParsedBuf));
        TS_ASSERT(!myParsedBuf.empty());
        TS_ASSERT(ta::CertUtils::fileHasPemCert("CA/certkey.pem"));

        myParsedCerts = ta::CertUtils::getPemCertsX509File("CA/2cert1key.pem");
        TS_ASSERT_EQUALS(myParsedCerts.size(), 2U);
        ta::CertUtils::freeX509Certs(myParsedCerts);

        TS_ASSERT(ta::CertUtils::fileHasPemPubKey("CA/FIXEDpubkey.pem", &myParsedBuf));
        TS_ASSERT(!myParsedBuf.empty());
        TS_ASSERT(ta::CertUtils::fileHasPemPubKey("CA/FIXEDpubkey.pem"));

        TS_ASSERT(ta::CertUtils::fileHasPemPrivKey("CA/privkey.pem", &myParsedBuf));
        TS_ASSERT(!myParsedBuf.empty());
        TS_ASSERT(ta::CertUtils::fileHasPemEncryptedPrivKey("CA/privkey.pem", &myParsedBuf));
        TS_ASSERT(!myParsedBuf.empty());
        TS_ASSERT(ta::CertUtils::fileHasPemPrivKey("CA/privkey.pem"));
        TS_ASSERT(ta::CertUtils::fileHasPemEncryptedPrivKey("CA/privkey.pem"));
        TS_ASSERT(ta::CertUtils::fileHasPemPrivKey("CA/certkey.pem", &myParsedBuf));
        TS_ASSERT(!myParsedBuf.empty());
        TS_ASSERT(ta::CertUtils::fileHasPemPrivKey("CA/certkey.pem"));
        TS_ASSERT(!ta::CertUtils::fileHasPemEncryptedPrivKey("CA/certkey.pem"));

        // invalid input
        //
        TS_ASSERT(!ta::CertUtils::fileHasPemCert("CA/privkey.pem", &myParsedBuf));
        TS_ASSERT(!ta::CertUtils::fileHasPemCert("CA/privkey.pem"));
        myParsedCerts = ta::CertUtils::getPemCertsX509File("CA/privkey.pem");
        TS_ASSERT_EQUALS(myParsedCerts.size(), 0U);
        ta::CertUtils::freeX509Certs(myParsedCerts);
        TS_ASSERT_THROWS(ta::CertUtils::fileHasPemCert("non_existing_file"), std::exception);

        TS_ASSERT(!ta::CertUtils::fileHasPemPubKey("CA/FIXEDprivkey.pem", &myParsedBuf));
        TS_ASSERT(!ta::CertUtils::fileHasPemPubKey("CA/FIXEDprivkey.pem"));
        TS_ASSERT_THROWS(ta::CertUtils::fileHasPemPubKey("non_existing_file"), std::exception);

        TS_ASSERT(!ta::CertUtils::fileHasPemPrivKey("CA/cert.pem", &myParsedBuf));
        TS_ASSERT(!ta::CertUtils::fileHasPemPrivKey("CA/cert.pem"));
        TS_ASSERT(!ta::CertUtils::fileHasPemEncryptedPrivKey("CA/cert.pem", &myParsedBuf));
        TS_ASSERT(!ta::CertUtils::fileHasPemEncryptedPrivKey("CA/cert.pem"));
        TS_ASSERT_THROWS(ta::CertUtils::fileHasPemPrivKey("non_existing_file"), std::exception);
        TS_ASSERT_THROWS(ta::CertUtils::fileHasPemEncryptedPrivKey("non_existing_file"), std::exception);
    }

    void testHasDerCert()
    {
    	const std::vector<unsigned char> myTestPemCert = ta::readData("CA/cert.pem");
    	const std::vector<unsigned char> myTestDerCert = ta::readData("CA/cert.der");

    	TS_ASSERT(ta::CertUtils::hasDerCert(myTestDerCert));
    	TS_ASSERT(!ta::CertUtils::hasDerCert(myTestPemCert));
    }

    void testConvertDerPem()
    {
    	const std::vector<unsigned char> myTestPemCert = ta::readData("CA/cert.pem");
    	const std::vector<unsigned char> myTestDerCert = ta::readData("CA/cert.der");
    	const std::vector<unsigned char> myOtherDerCert = ta::readData("CA/SCERT.cer");

    	// compare PEM as strings for pretty printing
    	TS_ASSERT_EQUALS(ta::vec2Str(ta::CertUtils::convDer2Pem(myTestDerCert)), ta::vec2Str(myTestPemCert));
    	TS_ASSERT_EQUALS(ta::CertUtils::convPem2Der(myTestPemCert), myTestDerCert);
    	TS_ASSERT_DIFFERS(ta::vec2Str(ta::CertUtils::convDer2Pem(myOtherDerCert)), ta::vec2Str(myTestPemCert));

    	TS_ASSERT_THROWS(ta::CertUtils::convDer2Pem(myTestPemCert), std::exception);
    	TS_ASSERT_THROWS(ta::CertUtils::convPem2Der(myTestDerCert), std::exception);
    	TS_ASSERT_THROWS(ta::CertUtils::convPem2Der(std::vector<unsigned char>()), std::exception);
    	TS_ASSERT_THROWS(ta::CertUtils::convPem2Der(std::vector<unsigned char>()), std::exception);
    }

    void testExtractRSAPubkey()
    {
        TS_ASSERT_THROWS(ta::CertUtils::extractRSAPubKeyFile("CA/__NONEXISTING_SCERT.cer__"), std::exception);
        const std::vector<unsigned char> myPk = ta::CertUtils::extractRSAPubKeyFile("CA/SCERT.cer");
        TS_ASSERT_EQUALS(myPk.size(), 162U);
    }

    void testExtractPemPubKey()
    {
        // openssl x509 -noout -pubkey -in cert.pem
        TS_ASSERT_THROWS(ta::CertUtils::extractPemPubKeyFile("CA/__NONEXISTING_SCERT.cer__"), std::exception);
        const std::vector<unsigned char> myPk = ta::CertUtils::extractPemPubKeyFile("CA/certkey.pem");
        TS_ASSERT(ta::CertUtils::hasPemPubKey(ta::vec2Str(myPk)));
        TS_ASSERT(!ta::CertUtils::hasPemCert(ta::vec2Str(myPk)));
        TS_ASSERT(!ta::CertUtils::hasPemPrivKey(ta::vec2Str(myPk)));
    }

    void testIsKeyPair()
    {
        using std::vector;

        const vector<unsigned char> myBadKeyBuf = ta::readData("CA/privkey3.pem");

        // No password-protected key
        vector<unsigned char> myCertBuf = ta::readData("CA/certkey.pem");
        vector<unsigned char> myKeyBuf = ta::readData("CA/certkey.pem");
        TS_ASSERT(ta::CertUtils::isKeyPairFile("CA/certkey.pem", "CA/certkey.pem"));
        TS_ASSERT(ta::CertUtils::isKeyPair(myCertBuf, myKeyBuf));
        TS_ASSERT(!ta::CertUtils::isKeyPairFile("CA/certkey.pem", "CA/privkey3.pem"));
        TS_ASSERT(!ta::CertUtils::isKeyPair(myCertBuf, myBadKeyBuf));

        // Password-protected key
        myCertBuf = ta::readData("CA/signingcertkey.pem");
        myKeyBuf = ta::readData("CA/signingcertkey.pem");
        TS_ASSERT(ta::CertUtils::isKeyPairFile("CA/signingcertkey.pem", "CA/signingcertkey.pem", "kaaskaas"));
        TS_ASSERT_THROWS(ta::CertUtils::isKeyPairFile("CA/signingcertkey.pem", "CA/signingcertkey.pem", "-invalid-password-"), std::exception);
        TS_ASSERT_THROWS(ta::CertUtils::isKeyPairFile("CA/signingcertkey.pem", "CA/signingcertkey.pem"), std::exception);
        TS_ASSERT(ta::CertUtils::isKeyPair(myCertBuf, myKeyBuf, "kaaskaas"));
        TS_ASSERT(ta::CertUtils::isKeyPair(ta::vec2Str(myCertBuf), ta::vec2Str(myKeyBuf), "kaaskaas"));
        TS_ASSERT(!ta::CertUtils::isKeyPairFile("CA/signingcertkey.pem", "CA/privkey3.pem"));
        TS_ASSERT(!ta::CertUtils::isKeyPair(myCertBuf, myBadKeyBuf));
        TS_ASSERT(!ta::CertUtils::isKeyPair(ta::vec2Str(myCertBuf), ta::vec2Str(myBadKeyBuf)));

        // Non-existing/invalid cert/key
        TS_ASSERT_THROWS(ta::CertUtils::isKeyPairFile("CA/__NONEXISTING_SCERT.pem__", "CA/certkey.pem"), std::exception);
        TS_ASSERT_THROWS(ta::CertUtils::isKeyPairFile("CA/certkey.pem", "CA/__NONEXISTING_KEY__.pem"), std::exception);
        TS_ASSERT_THROWS(ta::CertUtils::isKeyPairFile("CA/__NONEXISTING_SCERT.pem__", "CA/__NONEXISTING_KEY__.pem"), std::exception);
        TS_ASSERT_THROWS(ta::CertUtils::isKeyPair(vector<unsigned char>(), myKeyBuf), std::exception);
        TS_ASSERT_THROWS(ta::CertUtils::isKeyPair("", ta::vec2Str(myKeyBuf)), std::exception);
        TS_ASSERT_THROWS(ta::CertUtils::isKeyPair(myCertBuf, vector<unsigned char>()), std::exception);
        TS_ASSERT_THROWS(ta::CertUtils::isKeyPair(ta::vec2Str(myCertBuf), ""), std::exception);
        TS_ASSERT_THROWS(ta::CertUtils::isKeyPair(vector<unsigned char>(), vector<unsigned char>()), std::exception);
        TS_ASSERT_THROWS(ta::CertUtils::isKeyPair("", ""), std::exception);
    }

    void testIsCertIssuedBy()
    {
        // Issued certificate with self signed parent certificate
        TS_ASSERT(ta::CertUtils::isCertFileIssuedBy("CA/signingcertkey.pem", "CA/signingcertissuercert.pem"));

        // Issued certificate with not self-signed parent certificate
        TS_ASSERT(ta::CertUtils::isCertFileIssuedBy("CA/issuedcertbynotselfsignedcert.pem", "CA/notselfsignedcert.pem"));

        // Not issued certificate
        TS_ASSERT(!ta::CertUtils::isCertFileIssuedBy("CA/signingcertissuercert.pem", "CA/signingcertkey.pem"));

        // Non-existing certificate files
        TS_ASSERT_THROWS(ta::CertUtils::isCertFileIssuedBy("CA/__NONEXISTING_SCERT.cer__", "CA/signingcertkey.pem"), std::exception);
        TS_ASSERT_THROWS(ta::CertUtils::isCertFileIssuedBy("signingcertkey", "CA/__NONEXISTING_SCERT.cer__"), std::exception);

        // Same certificate file: self signed
        TS_ASSERT(ta::CertUtils::isCertFileIssuedBy("CA/selfsigned.pem", "CA/selfsigned.pem"));

        // Same certificate file: not self-signed
        TS_ASSERT(!ta::CertUtils::isCertFileIssuedBy("CA/signingcertkey.pem", "CA/signingcertkey.pem"));
    }

    void test_parse_pfx_without_chain()
    {
        using boost::assign::list_of;
        using ta::CertUtils::parsePfx;
        using ta::CertUtils::convPem2Pfx;
        using ta::CertUtils::convPfx2Pem;

        // given
        std::vector<unsigned char> myPfx = ta::readData("CA/test.p12");
        const std::string myPfxPassword = "182d68595feea39bdd20bd40e97460";
        const std::string myPfxCertCertSubjName = "/emailAddress=testpl@trustalert.com/C=PL/ST=Slupsk/L=Czluchow/O=TrustAlert DemoPL/OU=SiouxPL/CN=DemoUser";
        const std::string myPfxPrivKey = "-----BEGIN PRIVATE KEY-----\n"
        "MIIEvwIBADANBgkqhkiG9w0BAQEFAASCBKkwggSlAgEAAoIBAQDoCnzxi9pK/bEm\n"
        "ReQ1qCmkxmCGnTUQAIDIgq8q2JMC6Dxn/pfEkYyOzSaYXhFgHoL9Isbr0/8nGy8C\n"
        "67hSN1evFbUw2MmCQoUnB31apEUwzgnRm9ufl+w9P4rNdglJeucqcGgG5tMogUiD\n"
        "rr0H3w4bWwtK+rigJoO0SREI6rh6npdTm3obcT+dCkhNFvKlhARxTgYKS4ANP8dq\n"
        "oQuCG4RjzXZthtl5T2vH7To+OaoEiUSdJ+jVOHLyc+e+OexGRtxvqkVpfctbAMv+\n"
        "uevV64wdR9vWIXm4FgHqO/mTMVItbpF2cn7vh+98csR0HLKtQYBAQ/n1JmNbgMg5\n"
        "eJFf7sUbAgMBAAECggEBAIg+ID5zLyj47BrczrHymwD0uZhQledfZD5W/SLbJvZ+\n"
        "BXnKMrJoA2+VZSPxl6IoSCD5WnL9dsMvYtsELKWZeRKEXG7h2u9wv127OA8QhM+z\n"
        "KnC4YqvCOCN+O7GHrMxILZLtjScAQNt3jWxEHRmSDmZhfq/3po/iDVK91BK4Prk/\n"
        "Zp2hz7G28lId79c4fksO5bR/ksIYK1jzrrIvn000vrwB6pIaC9KUQaE58bhW5leF\n"
        "ly5WljQXbU6NuMQBcB+1CkcM0cRaH9TF4M1QTx1h6xS+cbe6Q+yJGGZuPC0E/Mfn\n"
        "oMvfh/6j6MFYVp7NjCPWc1eghqhhJONAf+YcKbrDwoECgYEA+9A5aV68yYUgXH2D\n"
        "ZNF1sBt70g5py9eYE9Dpo0U4UXgjUDOmnCLJrgU8UZoHdx+3/t9xl3d9BHOe2zL2\n"
        "cByBJn4e/Up2eRBYHj9xunrXdNqngRh2G497bdJ5R59ikm44oPrD2MWj3GhwORhB\n"
        "t5q/e3SbdfG41mYdxJxFItxakCcCgYEA6+YbndwP3uGgCQyDjPQCFUwZeQ45UzsX\n"
        "GTdfXgpMmztH9p5UR0KCoFhhzPNVcNrx9asZY0jHPNox7TzaLvultfgz8DCejUT6\n"
        "tewWgbyufHHuVejpqsDyQZnmBHqPIt4oJm1qmuSEHlOM3qhcjs8BLDFk1AAuxCyY\n"
        "iPIQTlJPx+0CgYEA3tWt6IPwlnhbYd2kR7Rf6/72PLWKg9t+dZK7HkOkCdxBi7iZ\n"
        "aLQUOw3hCek/T/her4n2EG6p0CWs3xcjI6Tl2TVkoQqi+pEvGV1VfAs23O6dqk0G\n"
        "P29ib5YAwxPVe/VT4YjnhM2pKuO046GGjj6/0jUnWWEWC4yELTeIqsfK0KMCgYA/\n"
        "g0PaO762Jo89AHaGw8evzj5pfm/mvdkZO/RcUG+Rt+NHPhe42+ppm4IxeS5+5SQS\n"
        "HdAHEDg8ZQ7eLe0cizpxhPMO7zgxFupdgsQIiDF+ZfpifQQ0qiBPk0z/1bOmEqRC\n"
        "vGDgdTgxJhOeezv4YzmQmXfpMGdnAA7NiEbln3nFfQKBgQCHtrwthGjt3N+StKtV\n"
        "uGiObT+iTKFrmT9PZ8EYjm7nUnfviZdd2ubPebq+InFx5UqJJCnGFnBHJFPpvfSd\n"
        "CcYI5UkfvK3uktL4brZMI7gVjGKb+bSlt5tmqJyEKibiTr5MB46+MkmdR5ZB4JOb\n"
        "2ObnqXrWrqZdvezHVkmkE9WspQ==\n"
        "-----END PRIVATE KEY-----";

        std::string myParsedKey, myParsedCert;
        ta::StringArray myParsedCAs;

        // when-then
        TS_ASSERT_EQUALS(parsePfx(myPfx, myPfxPassword, myParsedKey, myParsedCert, myParsedCAs), 1U);
        // then
        TS_ASSERT_EQUALS(boost::trim_copy(myParsedKey), myPfxPrivKey);
        TS_ASSERT_EQUALS(ta::CertUtils::getCertInfo(myParsedCert).subjName, myPfxCertCertSubjName);
        TS_ASSERT_EQUALS(myParsedCAs.size(), 0);

        // when
        const std::string myPem = convPfx2Pem(myPfx, myPfxPassword);
        // then
        const std::vector<ta::CertUtils::MemBuffer> myPrivKeyPem = ta::CertUtils::extractPemPrivKeys(myPem);
        const std::vector<ta::CertUtils::MemBuffer> myCertChainPem = ta::CertUtils::extractPemCerts(myPem);
        TS_ASSERT_EQUALS(myPrivKeyPem.size(), 1);
        TS_ASSERT_EQUALS(boost::trim_copy(ta::vec2Str(myPrivKeyPem.at(0))), myPfxPrivKey);
        TS_ASSERT_EQUALS(myCertChainPem.size(), 1);
        TS_ASSERT_EQUALS(boost::trim_copy(ta::vec2Str(myCertChainPem.at(0))), boost::trim_copy(myParsedCert));

        // when-then
        TS_ASSERT_EQUALS(convPfx2Pem(convPem2Pfx(myPem, myPfxPassword), myPfxPassword), myPem);
        TS_ASSERT_EQUALS(convPfx2Pem(convPem2Pfx(myPem, myPfxPassword, "friendly"), myPfxPassword), myPem);

        // when-then (invalid input)
        myPfx.at(0) += 1;
        TS_ASSERT_THROWS(parsePfx(myPfx, myPfxPassword), std::exception);
        TS_ASSERT_THROWS(convPfx2Pem(myPfx, myPfxPassword), std::exception);
        TS_ASSERT_THROWS(parsePfx(std::vector<unsigned char>(), myPfxPassword), std::exception);
        TS_ASSERT_THROWS(convPfx2Pem(std::vector<unsigned char>(), myPfxPassword), std::exception);
        TS_ASSERT_THROWS(parsePfx(myPfx, myPfxPassword+".invalid"), std::exception);
        TS_ASSERT_THROWS(convPfx2Pem(myPfx, myPfxPassword+".invalid"), std::exception);
        TS_ASSERT_THROWS(convPem2Pfx("invalid-cert", "pass"), std::exception);
    }

    void test_parse_pfx_with_chain()
    {
        using boost::assign::list_of;
        using ta::CertUtils::parsePfx;
        using ta::CertUtils::convPem2Pfx;
        using ta::CertUtils::convPfx2Pem;

        // given
        std::vector<unsigned char> myPfx = ta::readData("CA/test_with_chain.p12");
        const std::string myPfxCertSubjName = "/emailAddress=testui@sioux.eu/C=NL/ST=Noord-Barbant/L=Eindhoven/O=Sioux Group/OU=SES/CN=DemoUser";
        const std::string myPfxCertCaSubjName = "/emailAddress=keytalkdemo@keytalk.com/C=NL/L=Amersfoort/O=KeyTalk for DEMO purposes/OU=DEMO NOT FOR PRODUCTION/CN=KeyTalk Demo Signing CA";
        const std::string myPfxRootCaSubjName = "/emailAddress=keytalkdemo@keytalk.com/C=NL/L=Amersfoort/O=KeyTalk for DEMO purposes/OU=DEMO NOT FOR PRODUCTION/CN=KeyTalk Demo Primary CA";
                                                                 ;
        const std::string myPfxPrivKey = "-----BEGIN PRIVATE KEY-----\n"
        "MIIEvwIBADANBgkqhkiG9w0BAQEFAASCBKkwggSlAgEAAoIBAQDM3967wvasZ0Wq\n"
        "dlumUWF0/rH7v+hT9Pgr0aRu7NpaiibTQ3z+kkeBR8n32IOOeQT1aGC/d+O1LjX1\n"
        "I0otmBOAXqhlaFwEszs2ruLcmsYmf7H3OCLpQAwUoF/liaIIekC0Y+jjMF5MCspI\n"
        "SfAVDkCA5uR0M3yQCMdHPVsY+3yLkQBanQc7zhS9vP+WOLipuThChrIgHokgaXFo\n"
        "PwRTUxGbsw6wQ44CcLtA3wQVwr9srx/yxEvGGekMTXe4d+LVR66Pv0cAQhHaBd6s\n"
        "YJrgA3nf7bZy98tFzPySayLyI81mO6zJiMXgR3Fhli1bm5WTdLGTSiyUKR8IXs5H\n"
        "TgFWhlStAgMBAAECggEARXL+RhfpFrhLXWWR2+dqagaMvxJm5077cZ83ZoLT4i1C\n"
        "zArsrD9aDfEP/fxfXESZ8bbYVQ+HGTv4/ohf9OTAd5ue6gFecBlx0S+np7Cvyw0u\n"
        "fueAriEP904GNAFb9BO9K2lS4PDchlqCB89Im1qhcxDSELIDX7AaHYQnBWj65zQ8\n"
        "6tHyybS3c+AOUR9WBRS4LG/cHZcv0qNaOBYm5eEFc7jB1Oog77daPkVfoUvu6Hhm\n"
        "VP9kzb7xQGkvo6rfdU9hiza+YBp51q6LBd8dmvtR6maFkvko/9A8KiFWuX0baXfR\n"
        "m36egzRV6O1GrHt+XuYgSn/db1jnPN7WSFSLwhzk4QKBgQDn9Y5sSKJqGHM17e29\n"
        "4Hjh30PZT0mqmpZ4IjC99wAn7uHE662jDW8iyets6Zmi0M26YRMkVIVkW5Qtzw4z\n"
        "CfRRicieGEZmnXH8Lgy3viOBf5weZEWa+pVnZwxVXuXWiawi5egQvZd4SMofvpBv\n"
        "QGtm8BXr9HDrGEivArmNFej9qwKBgQDiG7CyC+HP+V4LGIKT2xqSqfed8DqUefzc\n"
        "v6UCplmanf8KXvOC26bctaRtVeE0V5Qoqg4mGAZJg7WCLm5aJsmPDN/HhnDQjvzF\n"
        "xdbX3Ipdh8TrMRiPseXcN6jpGywGFrFV27J57mKdxbNxtPykxpXiXj9Qw/e8NtCr\n"
        "ffqkHL8vBwKBgQCeP5m96EPdy/Z12W/ztXvi5TXdwMkKOlLjfLfyMdwKYl/mv78D\n"
        "WXvzqPwTnr8xI/Dm8alkhMFIeW37XqJaUbU8F5sluHv3L/z+xc/pXy/L/mpdFZ2j\n"
        "IMfi9pukdoypM92bJWyQVzhKWKaEx7a0H6fmX28lPev3h+a2d05toYAF5wKBgQDF\n"
        "bXgzfryFZDABcD1T7RSey1oyxffgfUOVXKEwVyAWKa7v10i5EiH4xYH1Fe+2TGJJ\n"
        "v3LrQD79IcvzB6fDf+quxYcAKRuIf9GmvsCBa0hFsq72zb+seKFEUDVklR0zk8z0\n"
        "Mwj7/nKNYHr25hlTpzJonfKbCwHdjd9WXoHjmlY1MQKBgQCAlgk6GqxxBHSg+H1N\n"
        "Wkugv8funEZas0vDQvceLXgvrJoSw8iM09sMb5TCFVXuccLuWM/lIyr8eQo4dXvE\n"
        "uDg1EGGTiY0D0SPViZc8TiQrDqNSGl9G53hq1PnDSl1ImHPN1lZb62RPm2FyBxdB\n"
        "eFFbuOscOHos3OKvlTX5o4kHrA==\n"
        "-----END PRIVATE KEY-----";
        const std::string myPfxPassword = "f3f1db68f7f82dbb1a9ad820d02514";

        std::string myParsedKey, myParsedCert;
        ta::StringArray myParsedCAs;

        // when-then
        TS_ASSERT_EQUALS(parsePfx(myPfx, myPfxPassword, myParsedKey, myParsedCert, myParsedCAs), 3);
        // then
        TS_ASSERT_EQUALS(boost::trim_copy(myParsedKey), myPfxPrivKey);
        TS_ASSERT_EQUALS(ta::CertUtils::getCertInfo(myParsedCert).subjName, myPfxCertSubjName);
        TS_ASSERT_EQUALS(myParsedCAs.size(), 2);
        TS_ASSERT_EQUALS(ta::CertUtils::getCertInfo(myParsedCAs.at(0)).subjName, myPfxCertCaSubjName);
        TS_ASSERT_EQUALS(ta::CertUtils::getCertInfo(myParsedCAs.at(1)).subjName, myPfxRootCaSubjName);

        // when
        const std::string myPem = convPfx2Pem(myPfx, myPfxPassword);
        // then
        const std::vector<ta::CertUtils::MemBuffer> myPrivKeyPem = ta::CertUtils::extractPemPrivKeys(myPem);
        const std::vector<ta::CertUtils::MemBuffer> myCertChainPem = ta::CertUtils::extractPemCerts(myPem);
        TS_ASSERT_EQUALS(myPrivKeyPem.size(), 1);
        TS_ASSERT_EQUALS(boost::trim_copy(ta::vec2Str(myPrivKeyPem.at(0))), myPfxPrivKey);
        TS_ASSERT_EQUALS(myCertChainPem.size(), 3);
        TS_ASSERT_EQUALS(boost::trim_copy(ta::vec2Str(myCertChainPem.at(0))), boost::trim_copy(myParsedCert));
        TS_ASSERT_EQUALS(boost::trim_copy(ta::vec2Str(myCertChainPem.at(1))), boost::trim_copy(myParsedCAs.at(0)));
        TS_ASSERT_EQUALS(boost::trim_copy(ta::vec2Str(myCertChainPem.at(2))), boost::trim_copy(myParsedCAs.at(1)));

        // when-then
        TS_ASSERT_EQUALS(convPfx2Pem(convPem2Pfx(myPem, myPfxPassword), myPfxPassword), myPem);
        TS_ASSERT_EQUALS(convPfx2Pem(convPem2Pfx(myPem, myPfxPassword, "friendly"), myPfxPassword), myPem);

        // when-then (invalid input)
        myPfx.at(0) += 1;
        TS_ASSERT_THROWS(parsePfx(myPfx, myPfxPassword), std::exception);
        TS_ASSERT_THROWS(convPfx2Pem(myPfx, myPfxPassword), std::exception);
        TS_ASSERT_THROWS(parsePfx(std::vector<unsigned char>(), myPfxPassword), std::exception);
        TS_ASSERT_THROWS(convPfx2Pem(std::vector<unsigned char>(), myPfxPassword), std::exception);
        TS_ASSERT_THROWS(parsePfx(myPfx, myPfxPassword+".invalid"), std::exception);
        TS_ASSERT_THROWS(convPfx2Pem(myPfx, myPfxPassword+".invalid"), std::exception);
        TS_ASSERT_THROWS(convPem2Pfx("invalid-cert", "pass"), std::exception);
    }

    void testx509SerializeKeyUsage()
    {
        using boost::assign::list_of;
        using namespace ta::CertUtils;

        // when-then
        TS_ASSERT_EQUALS(x509SerializeKeyUsage(std::vector<KeyUsage>()), "");
        TS_ASSERT_EQUALS(x509SerializeKeyUsage(ta::StringArray()), "");

        {
            // given
            const std::vector<KeyUsage> myKU = list_of(keyusageDigitalSignature)(keyusageKeyAgreement);
            // when-then
            TS_ASSERT_EQUALS(x509SerializeKeyUsage(myKU),"digitalSignature,keyAgreement");
        }

        {
            // given
            const ta::StringArray myKUStrs = list_of(str(keyusageDigitalSignature))("")(str(keyusageKeyAgreement));
            TS_ASSERT_EQUALS(x509SerializeKeyUsage(myKUStrs), "digitalSignature,keyAgreement");
        }

        {
            // given
            const ta::StringArray myKUStrs = list_of(str(keyusageDigitalSignature))(str(keyusageKeyAgreement));
            // when-then
            TS_ASSERT_EQUALS(x509DeserializeKeyUsage("digitalSignature, ,keyAgreement"), myKUStrs);
        }
    }

    void testx509SerializeBasicConstraints()
    {
        using namespace ta::CertUtils;

        TS_ASSERT_EQUALS(x509SerializeBasicConstraints(BasicConstraints()), "CA:FALSE");
        TS_ASSERT_EQUALS(x509SerializeBasicConstraints(BasicConstraints(caFalse)), "CA:FALSE");
        TS_ASSERT_EQUALS(x509SerializeBasicConstraints(BasicConstraints(caFalse, 5)), "CA:FALSE");
        TS_ASSERT_EQUALS(x509SerializeBasicConstraints(BasicConstraints(caTrue)), "CA:TRUE");
        TS_ASSERT_EQUALS(x509SerializeBasicConstraints(BasicConstraints(caTrue, PathLenConstraintNone)), "CA:TRUE");
        TS_ASSERT_EQUALS(x509SerializeBasicConstraints(BasicConstraints(caTrue, 5)), "CA:TRUE,pathlen:5");
        TS_ASSERT_EQUALS(x509SerializeBasicConstraints(BasicConstraints(caTrue, 0)), "CA:TRUE,pathlen:0");

        TS_ASSERT_EQUALS(x509DeserializeBasicConstraints("CA:FALSE"), BasicConstraints());
        TS_ASSERT_EQUALS(x509DeserializeBasicConstraints("CA:TRUE"), BasicConstraints(caTrue));
        TS_ASSERT_EQUALS(x509DeserializeBasicConstraints("CA:TRUE,pathlen:6"), BasicConstraints(caTrue, 6));
        TS_ASSERT_EQUALS(x509DeserializeBasicConstraints(" CA:TRUE , pathlen:6  "), BasicConstraints(caTrue, 6));
        TS_ASSERT_EQUALS(x509DeserializeBasicConstraints("CA:TRUE,pathlen:0"), BasicConstraints(caTrue, 0));
        TS_ASSERT_EQUALS(x509DeserializeBasicConstraints("CA:TRUE,pathlen:-7"), BasicConstraints(caTrue, 0));

        TS_ASSERT_THROWS(x509SerializeBasicConstraints(BasicConstraints(caTrue, -5)), std::exception);
        TS_ASSERT_THROWS(x509DeserializeBasicConstraints("CA:TRUE,pathlen:NAN"), std::exception);
        TS_ASSERT_THROWS(x509DeserializeBasicConstraints(""), std::exception);
    }

    void testx509SerializeNameConstraints()
    {
        using boost::assign::list_of;
        using namespace ta::CertUtils;
        using std::vector;
        using std::string;

        TS_ASSERT_EQUALS(x509SerializeNameConstraints(NameConstraints()), "");
        TS_ASSERT_EQUALS(x509SerializeNameConstraints(NameConstraints(vector<string>(), vector<string>())), "");

        TS_ASSERT_EQUALS(x509SerializeNameConstraints(NameConstraints(list_of<string>("IP:192.168.0.0/255.255.0.0"), vector<string>())),
                        "permitted;IP:192.168.0.0/255.255.0.0");
        TS_ASSERT_EQUALS(x509SerializeNameConstraints(NameConstraints(list_of<string>("IP:192.168.0.0/255.255.0.0")("")("  email:.somedomain.com")("  "), vector<string>())),
                        "permitted;IP:192.168.0.0/255.255.0.0,permitted;email:.somedomain.com");
        TS_ASSERT_EQUALS(x509SerializeNameConstraints(NameConstraints(vector<string>(), list_of<string>("IP:192.168.0.0/255.255.0.0"))),
                        "excluded;IP:192.168.0.0/255.255.0.0");
        TS_ASSERT_EQUALS(x509SerializeNameConstraints(NameConstraints(vector<string>(), list_of<string>("IP:192.168.0.0/255.255.0.0  ")("email:.somedomain.com")("  "))),
                                                     "excluded;IP:192.168.0.0/255.255.0.0,excluded;email:.somedomain.com");
        TS_ASSERT_EQUALS(x509SerializeNameConstraints(NameConstraints(list_of<string>("")("IP:10.1.1.0/255.255.0.0")("email:.com"), list_of<string>("IP:192.168.0.0/255.255.0.0")("email:.somedomain.com"))),
                        "permitted;IP:10.1.1.0/255.255.0.0,permitted;email:.com,excluded;IP:192.168.0.0/255.255.0.0,excluded;email:.somedomain.com");

        TS_ASSERT_THROWS(x509SerializeNameConstraints(NameConstraints(list_of<string>("email:.some,domain.com"), vector<string>())), std::exception);


        TS_ASSERT_EQUALS(x509DeserializeNameConstraints(""), NameConstraints());
        TS_ASSERT_EQUALS(x509DeserializeNameConstraints("    "), NameConstraints());
        TS_ASSERT_EQUALS(x509DeserializeNameConstraints(" permitted;IP:192.168.0.0/255.255.0.0  "),
                        NameConstraints(list_of<string>("IP:192.168.0.0/255.255.0.0"), vector<string>()));
        TS_ASSERT_EQUALS(x509DeserializeNameConstraints("permitted;IP:192.168.0.0/255.255.0.0,permitted;email:.somedomain.com"),
                        NameConstraints(list_of<string>("IP:192.168.0.0/255.255.0.0")("email:.somedomain.com"), vector<string>()));
        TS_ASSERT_EQUALS(x509DeserializeNameConstraints("excluded;IP:192.168.0.0/255.255.0.0"),
                        NameConstraints(vector<string>(), list_of<string>("IP:192.168.0.0/255.255.0.0")));
        TS_ASSERT_EQUALS(x509DeserializeNameConstraints("excluded;IP:192.168.0.0/255.255.0.0,excluded;email:.somedomain.com"),
                        NameConstraints(vector<string>(), list_of<string>("IP:192.168.0.0/255.255.0.0")("email:.somedomain.com")));
        TS_ASSERT_EQUALS(x509DeserializeNameConstraints("permitted;IP:10.1.1.0/255.255.0.0,permitted;email:.com,excluded;IP:192.168.0.0/255.255.0.0,excluded;email:.somedomain.com  "),
                        NameConstraints(list_of<string>("IP:10.1.1.0/255.255.0.0")("email:.com"), list_of<string>("IP:192.168.0.0/255.255.0.0")("email:.somedomain.com")));

        TS_ASSERT_THROWS(x509DeserializeNameConstraints("IP:10.1.1.0/255.255.0.0"), std::exception);
    }

    void testx509SerializePolicyConstraints()
    {
        using boost::assign::list_of;
        using namespace ta::CertUtils;

        TS_ASSERT_EQUALS(x509SerializePolicyConstraints(PolicyConstraints()), "");
        TS_ASSERT_EQUALS(x509SerializePolicyConstraints(PolicyConstraints(PolicyConstraintNone)), "");
        TS_ASSERT_EQUALS(x509SerializePolicyConstraints(PolicyConstraints(PolicyConstraintNone, PolicyConstraintNone)), "");
        TS_ASSERT_EQUALS(x509SerializePolicyConstraints(PolicyConstraints(1)), "requireExplicitPolicy:1");
        TS_ASSERT_EQUALS(x509SerializePolicyConstraints(PolicyConstraints(PolicyConstraintNone, 2)), "inhibitPolicyMapping:2");
        TS_ASSERT_EQUALS(x509SerializePolicyConstraints(PolicyConstraints(1, 2)), "requireExplicitPolicy:1,inhibitPolicyMapping:2");

        TS_ASSERT_THROWS(x509SerializePolicyConstraints(PolicyConstraints(-112)), std::exception);
        TS_ASSERT_THROWS(x509SerializePolicyConstraints(PolicyConstraints(1, -123)), std::exception);

        TS_ASSERT_EQUALS(x509DeserializePolicyConstraints(""), PolicyConstraints());
        TS_ASSERT_EQUALS(x509DeserializePolicyConstraints("    "), PolicyConstraints());
        TS_ASSERT_EQUALS(x509DeserializePolicyConstraints(" requireExplicitPolicy:1  "), PolicyConstraints(1));
        TS_ASSERT_EQUALS(x509DeserializePolicyConstraints("inhibitPolicyMapping:2"), PolicyConstraints(PolicyConstraintNone, 2));
        TS_ASSERT_EQUALS(x509DeserializePolicyConstraints(" requireExplicitPolicy:1, inhibitPolicyMapping:2 "), PolicyConstraints(1, 2));

        TS_ASSERT_THROWS(x509DeserializePolicyConstraints("requireExplicitPolicy:-1"), std::exception);
        TS_ASSERT_THROWS(x509DeserializePolicyConstraints("inhibitPolicyMapping:-2"), std::exception);
        TS_ASSERT_THROWS(x509DeserializePolicyConstraints("invalid-policy-constraint"), std::exception);
    }

    void test_is_cert_revoked()
    {
        using boost::assign::list_of;
        using namespace ta::CertUtils;

        // given
        const std::string myNonRevokedPemCertPath = "CA/keytalk.com.cert.pem";
        const std::string myNonRevokedDerCertPath = "CA/keytalk.com.cert.der";
        const std::string myRevokedPemCertPath = "CA/revokedcert.pem";
        const std::string myRevokedDerCertPath = "CA/revokedcert.der";
        const std::vector<unsigned char> myNonRevokedCertPathAsnCRL = ta::readData("CA/gsorganizationvalsha2g2.crl");
        const std::vector<unsigned char> myNonRevokedCertPathPemCRL = ta::readData("CA/gsorganizationvalsha2g2.crl.pem");
        const std::vector<std::vector<unsigned char> > myRevokedCertCRLs = list_of(ta::readData("CA/crl3.digicert.com_ssca-sha2-g5.crl"))
                                                                                  (ta::readData("CA/crl4.digicert.com_ssca-sha2-g5.crl"));
        const std::string myInvalidCertPath = ta::readData("CA/privkey.pem");
        const std::vector<unsigned char> myInvalidCRL = ta::readData("CA/cert.pem");

        // when-then
        TS_ASSERT(!isCertFileRevokedForCrl(myNonRevokedPemCertPath, list_of(myNonRevokedCertPathPemCRL)));
        TS_ASSERT(!isCertFileRevokedForCrl(myNonRevokedDerCertPath, list_of(myNonRevokedCertPathPemCRL)));
        TS_ASSERT(!isCertFileRevokedForCrl(myNonRevokedPemCertPath, list_of(myNonRevokedCertPathPemCRL)));
        TS_ASSERT(!isCertFileRevokedForCrl(myNonRevokedDerCertPath, list_of(myNonRevokedCertPathPemCRL)));
        TS_ASSERT(!isCertFileRevokedForCrl(myNonRevokedPemCertPath, list_of(myNonRevokedCertPathPemCRL)(myNonRevokedCertPathAsnCRL)));
        TS_ASSERT(!isCertFileRevokedForCrl(myNonRevokedDerCertPath, list_of(myNonRevokedCertPathPemCRL)(myNonRevokedCertPathAsnCRL)));
        TS_ASSERT(isCertFileRevokedForCrl(myRevokedPemCertPath, myRevokedCertCRLs));
        TS_ASSERT(isCertFileRevokedForCrl(myRevokedDerCertPath, myRevokedCertCRLs));

        // when-then
        TS_ASSERT(!isCertFileRevoked(myNonRevokedPemCertPath));
        TS_ASSERT(!isCertFileRevoked(myNonRevokedDerCertPath));
        TS_ASSERT(isCertFileRevoked(myRevokedPemCertPath));
        TS_ASSERT(isCertFileRevoked(myRevokedDerCertPath));

        // when-then
        TS_ASSERT_THROWS(isCertFileRevokedForCrl(myInvalidCertPath, list_of(myNonRevokedCertPathAsnCRL)), std::exception);
        TS_ASSERT_THROWS(isCertFileRevokedForCrl(myNonRevokedPemCertPath, list_of(myInvalidCRL)), std::exception);
        TS_ASSERT_THROWS(isCertFileRevoked(myInvalidCertPath), std::exception);
    }

    void testx509SerializeCrl()
    {
        using boost::assign::list_of;
        using namespace ta::CertUtils;

        TS_ASSERT_EQUALS(x509SerializeCrlUri(""), "");
        TS_ASSERT_EQUALS(x509SerializeCrlUri("  "), "");
        TS_ASSERT_EQUALS(x509SerializeCrlUri("http://www.nu.nl.crl"), "URI:http://www.nu.nl.crl");
        TS_ASSERT_EQUALS(x509SerializeCrlUri("http://www.nu.nl/test.crl"), "URI:http://www.nu.nl/test.crl");
        TS_ASSERT_EQUALS(x509SerializeCrlUri("  http://www.nu.nl.crl  "), "URI:http://www.nu.nl.crl");
        TS_ASSERT_THROWS(x509SerializeCrlUri("www.nu.nl.crl"), std::exception);

        TS_ASSERT_EQUALS(x509DeserializeCrlUri(""), "");
        TS_ASSERT_EQUALS(x509DeserializeCrlUri("  "), "");
        TS_ASSERT_EQUALS(x509DeserializeCrlUri("URI:http://www.nu.nl.crl"), "http://www.nu.nl.crl");
        TS_ASSERT_EQUALS(x509DeserializeCrlUri("URI:http://www.nu.nl/test.crl"), "http://www.nu.nl/test.crl");
        TS_ASSERT_EQUALS(x509DeserializeCrlUri("  URI: http://www.nu.nl.crl  "), "http://www.nu.nl.crl");
        TS_ASSERT_THROWS(x509DeserializeCrlUri("URI:www.nu.nl.crl"), std::exception);
        TS_ASSERT_THROWS(x509DeserializeCrlUri("http://www.nu.nl.crl"), std::exception);
    }

    void testx509SerializeOcsp()
    {
        using boost::assign::list_of;
        using namespace ta::CertUtils;

        TS_ASSERT_EQUALS(x509SerializeOcspUri(""), "");
        TS_ASSERT_EQUALS(x509SerializeOcspUri("  "), "");
        TS_ASSERT_EQUALS(x509SerializeOcspUri("http://www.nu.nl"), "OCSP;URI:http://www.nu.nl");
        TS_ASSERT_EQUALS(x509SerializeOcspUri("http://www.nu.nl/test"), "OCSP;URI:http://www.nu.nl/test");
        TS_ASSERT_EQUALS(x509SerializeOcspUri("  http://www.nu.nl  "), "OCSP;URI:http://www.nu.nl");
        TS_ASSERT_THROWS(x509SerializeOcspUri("www.nu.nl"), std::exception);

        TS_ASSERT_EQUALS(x509DeserializeOcspUri(""), "");
        TS_ASSERT_EQUALS(x509DeserializeOcspUri("  "), "");
        TS_ASSERT_EQUALS(x509DeserializeOcspUri("OCSP;URI:http://www.nu.nl"), "http://www.nu.nl");
        TS_ASSERT_EQUALS(x509DeserializeOcspUri("  OCSP;URI: http://www.nu.nl  "), "http://www.nu.nl");
        TS_ASSERT_THROWS(x509DeserializeOcspUri("OSCP;URI:www.nu.nl"), std::exception);
        TS_ASSERT_THROWS(x509DeserializeOcspUri("http://www.nu.nl"), std::exception);
    }

    void testx509SerializeCertPolicies()
    {
        using boost::assign::list_of;
        using namespace ta::CertUtils;
        using std::vector;
        using std::string;

        TS_ASSERT_EQUALS(x509SerializeCertPolicies(vector<string>()), "");
        TS_ASSERT_EQUALS(x509SerializeCertPolicies(list_of<string>("1.2.4.5")("")("1.1.3.4")),
                        "1.2.4.5,1.1.3.4");

        TS_ASSERT_EQUALS(x509DeserializeCertPolicies("1.2.4.5, 1.1.3.4"),
                         list_of<string>("1.2.4.5")("1.1.3.4"));
    }

    void testNormalizeExtendedKeyUsages()
    {
        using boost::assign::list_of;
        using namespace ta::CertUtils;
        using std::vector;
        using std::string;

        TS_ASSERT_EQUALS(normalizeExtendedKeyUsages(vector<string>()),
                         vector<string>());

        TS_ASSERT_EQUALS(normalizeExtendedKeyUsages(list_of<string>("1.3.6.1.5.5.7.3.1")),
                         list_of<string>("serverAuth"));
        TS_ASSERT_EQUALS(normalizeExtendedKeyUsages(list_of<string>("1.3.6.1.5.5.7.3.2")),
                         list_of<string>("clientAuth"))
        TS_ASSERT_EQUALS(normalizeExtendedKeyUsages(list_of<string>("1.3.6.1.5.5.7.3.3")),
                         list_of<string>("codeSigning"));
        TS_ASSERT_EQUALS(normalizeExtendedKeyUsages(list_of<string>("1.3.6.1.5.5.7.3.4")),
                         list_of<string>("emailProtection"));
        TS_ASSERT_EQUALS(normalizeExtendedKeyUsages(list_of<string>("1.3.6.1.5.5.7.3.8")),
                         list_of<string>("timeStamping"));

        TS_ASSERT_EQUALS(normalizeExtendedKeyUsages(list_of<string>("1.3.6.1.5.5.7.3.1 ")("  1.3.6.1.5.5.7.3.2")),
                         list_of<string>("clientAuth")("serverAuth"));
        TS_ASSERT_EQUALS(normalizeExtendedKeyUsages(list_of<string>("1.3.6.1.5.5.7.3.1")(" serverAuth")("1.3.6.1.5.5.7.3.2  ")),
                         list_of<string>("clientAuth")("serverAuth"));
        TS_ASSERT_EQUALS(normalizeExtendedKeyUsages(list_of<string>("1.3.6.1.5.5.7.3.1")("  clientAuth")("1.3.6.1.5.5.7.3.2")(" serverAuth")("1.3.6.1.5.5.7.3.1  ")("unknown-usage")("emailProtection")("1.3.6.1.5.5.7.3.999")),
                         list_of<string>("1.3.6.1.5.5.7.3.999")("clientAuth")("emailProtection")("serverAuth")("unknown-usage"));
    }

    void testNormalizeSerializedCertPolicies()
    {
        using boost::assign::list_of;
        using namespace ta::CertUtils;

        TS_ASSERT_EQUALS(normalizeSerializedCertPolicies(""), "");
        TS_ASSERT_EQUALS(normalizeSerializedCertPolicies("1.2.3.4"), "1.2.3.4");
        TS_ASSERT_EQUALS(normalizeSerializedCertPolicies("1.2.3.5,1.2.3.4"), "1.2.3.4,1.2.3.5");
        TS_ASSERT_EQUALS(normalizeSerializedCertPolicies("1.2.3.5,1.2.3.4,1.2.3.4"), "1.2.3.4,1.2.3.5");
        TS_ASSERT_EQUALS(normalizeSerializedCertPolicies(" 1.2.3.5, 1.2.3.4, 1.2.3.4, ,"), "1.2.3.4,1.2.3.5");
        TS_ASSERT_EQUALS(normalizeSerializedCertPolicies(" , 1.2.3.4, 1.2.3.5,, 1.2.3.4, ,"), "1.2.3.4,1.2.3.5");
    }

    void testKeyUsageNames()
    {
        using namespace ta::CertUtils;

        // when-then
        TS_ASSERT_EQUALS(str(keyusageDigitalSignature), "digitalSignature");
        TS_ASSERT_EQUALS(str_long(keyusageDigitalSignature), "Digital Signature");
        TS_ASSERT_EQUALS(str(keyusageCrlSign), "cRLSign");
        TS_ASSERT_EQUALS(str_long(keyusageCrlSign), "CRL Sign");

        // when-then
        TS_ASSERT_EQUALS(parseKeyUsage("digitalSignature"), keyusageDigitalSignature);
        TS_ASSERT_EQUALS(parseKeyUsage("keyCertSign "), keyusageCertificateSign);

        // given
        const std::vector<KeyUsage> myKUs = boost::assign::list_of(keyusageDigitalSignature)(keyusageCertificateSign);
        const ta::StringArray myKUStrs =  boost::assign::list_of("digitalSignature")("keyCertSign");
        // when-then
        TS_ASSERT_EQUALS(strs(myKUs), myKUStrs);
        TS_ASSERT_EQUALS(parseKeyUsages(myKUStrs), myKUs);
    }

    void testCreateCSR()
    {
        using namespace ta::CertUtils;
        using boost::assign::list_of;

        const std::vector<unsigned int> AllKeyBits = list_of(1024)(2048)(3072)(4096);
        const ta::StringArray mySAN = list_of("DNS:www.sioux.eu")("DNS:sioux.eu");

        foreach (unsigned int keyBits, AllKeyBits)
        {
            const ta::KeyPair myKeyPair = ta::RsaUtils::genKeyPair(keyBits, ta::RsaUtils::encPEM, ta::RsaUtils::pubkeyPKCS1);

            {
                TS_TRACE(str(boost::format("Create CSR with %d bit key signed by sha-256") % keyBits).c_str());
                const ta::SignUtils::Digest mySigningAlgorithm = ta::SignUtils::digestSha256;
                // when
                ta::ScopedResource<X509_REQ*> myCsr(createCSR(myKeyPair, Subject("test.keytalk.com"), &mySigningAlgorithm),
                                         X509_REQ_free);
                // then
                TS_ASSERT(myCsr);
                const std::string myCsrPem = convX509_REQ_2Pem(myCsr);
                TS_ASSERT(isCSR(myCsrPem));
                TS_ASSERT_EQUALS(parseCnFromSignedCSR(myCsrPem), "test.keytalk.com");
#ifndef _WIN32
                TS_ASSERT(!doesCsrHaveChallengePassword(myCsrPem));
                TS_ASSERT(!doesCsrHaveSan(myCsrPem));
#endif
            }
            {
                TS_TRACE(str(boost::format("Create CSR with %d bit key signed by sha-1 with SAN") % keyBits).c_str());
                const ta::SignUtils::Digest mySigningAlgorithm = ta::SignUtils::digestSha1;
                // when
                ta::ScopedResource<X509_REQ*> myCsr(createCSR(myKeyPair,
                                                             Subject("test.keytalk.com", "NL", "Noord Brabant", "Eindhoven", "Sioux", "Development", "test@keytalk.com"),
                                                             &mySigningAlgorithm,
                                                             mySAN),
                                         X509_REQ_free);
                // then
                TS_ASSERT(myCsr);
                const std::string myCsrPem = convX509_REQ_2Pem(myCsr);
                TS_ASSERT(isCSR(myCsrPem));
                TS_ASSERT_EQUALS(parseCnFromSignedCSR(myCsrPem), "test.keytalk.com");
#ifndef _WIN32
                TS_ASSERT(!doesCsrHaveChallengePassword(myCsrPem));
                assertCsrSanEquals(myCsrPem, mySAN);
#endif
            }
            {
                TS_TRACE(str(boost::format("Create CSR with %d bit key with SAN and challenge password") % keyBits).c_str());
                const ta::SignUtils::Digest mySigningAlgorithm = ta::SignUtils::digestSha256;
                // when
                ta::ScopedResource<X509_REQ*> myCsr(createCSR(myKeyPair,
                                                              Subject("test.keytalk.com", "NL", "Noord Brabant", "Eindhoven", "Sioux", "Development", "test@keytalk.com"),
                                                              &mySigningAlgorithm,
                                                              mySAN,
                                                              "secret"),
                                         X509_REQ_free);
                // then
                TS_ASSERT(myCsr);
                const std::string myCsrPem = convX509_REQ_2Pem(myCsr);
                TS_ASSERT(isCSR(myCsrPem));
                TS_ASSERT_EQUALS(parseCnFromSignedCSR(myCsrPem), "test.keytalk.com");
#ifndef _WIN32
                assertCsrChallengePasswordEquals(myCsrPem, "secret");
                assertCsrSanEquals(myCsrPem, mySAN);
#endif
            }

            {
                TS_TRACE(str(boost::format("Create non-signed CSR with %d bit key") % keyBits).c_str());
                // when
                ta::ScopedResource<X509_REQ*> myCsr(createCSR(myKeyPair, Subject("test.keytalk.com")),
                                         X509_REQ_free);
                // then
                TS_ASSERT(myCsr);
                const std::string myCsrPem = convX509_REQ_2Pem(myCsr);
                TS_ASSERT(isCSR(myCsrPem));
                TS_ASSERT_THROWS(parseCnFromSignedCSR(myCsrPem), std::exception);
            }
            {
                TS_TRACE(str(boost::format("Create non-signed CSR with %d bit key") % keyBits).c_str());
                // when
                ta::ScopedResource<X509_REQ*> myCsr(createCSR(myKeyPair,
                                                             Subject("test.keytalk.com", "NL", "Noord Brabant", "Eindhoven", "Sioux", "Development", "test@keytalk.com")),
                                         X509_REQ_free);
                // then
                TS_ASSERT(myCsr);
                const std::string myCsrPem = convX509_REQ_2Pem(myCsr);
                TS_ASSERT(isCSR(myCsrPem));
                TS_ASSERT_THROWS(parseCnFromSignedCSR(myCsrPem), std::exception);
            }
        }

        TS_TRACE("Try to sign CSR with invalid key");
        // when-then
        TS_ASSERT_THROWS(createCSR(ta::KeyPair(), Subject("test.keytalk.com")), std::exception);

        TS_TRACE("Try to parse CN from invalid CSR");
        // when-then
        TS_ASSERT_THROWS(parseCnFromSignedCSR("invalid-csr"), std::exception);
        TS_ASSERT_THROWS(parseCnFromSignedCSR(""), std::exception);
    }

    void testValidateSAN()
    {
        using namespace ta::CertUtils;

        TS_ASSERT_THROWS_NOTHING(validateSAN("DNS:mail.keytalk.com"));
        TS_ASSERT_THROWS_NOTHING(validateSAN("DNS:mail.keytalk.com,,IP:192.168.33.1, ,email:copy"));
        TS_ASSERT_THROWS_NOTHING(validateSAN("DNS:mail.keytalk.com, email:key@talk.com,  IP:192.168.44.1 "));
        TS_ASSERT_THROWS_NOTHING(validateSAN(""));

        TS_ASSERT_THROWS(validateSAN("INVALID:mail.keytalk.com"), std::exception);
        TS_ASSERT_THROWS(validateSAN("DNS: "), std::exception);
        TS_ASSERT_THROWS(validateSAN("IP:not-an-ip"), std::exception);
        TS_ASSERT_THROWS(validateSAN("email: "), std::exception);
    }

    void testSerializeSAN()
    {
        using boost::assign::list_of;
        using namespace ta::CertUtils;

        TS_ASSERT_EQUALS(serializeSAN(list_of("DNS:mail.keytalk.com")(" ")("DNS:owa.keytalk.com")), "DNS:mail.keytalk.com,DNS:owa.keytalk.com");
        TS_ASSERT_EQUALS(deserializeSAN("DNS:mail.keytalk.com,,, ,DNS:owa.keytalk.com"), list_of("DNS:mail.keytalk.com")("DNS:owa.keytalk.com"));

        TS_ASSERT_EQUALS(serializeSAN(list_of("DNS:mail.keytalk.com ")("    IP:192.168.44.1 ")), "DNS:mail.keytalk.com,IP:192.168.44.1");
        TS_ASSERT_EQUALS(deserializeSAN("   DNS:mail.keytalk.com,  IP:192.168.44.1 "), list_of("DNS:mail.keytalk.com")("IP:192.168.44.1"));

        TS_ASSERT_EQUALS(serializeSAN(list_of("DNS:mail.keytalk.com ")("    IP:192.168.44.1 ")("email:key@talk.com")), "DNS:mail.keytalk.com,IP:192.168.44.1,email:key@talk.com");
        TS_ASSERT_EQUALS(deserializeSAN("   DNS:mail.keytalk.com,  IP:192.168.44.1  , email:key@talk.com "), list_of("DNS:mail.keytalk.com")("IP:192.168.44.1")("email:key@talk.com"));

        TS_ASSERT_EQUALS(serializeSAN(ta::StringArray()), "");
        TS_ASSERT_EQUALS(deserializeSAN(""), ta::StringArray());

        TS_ASSERT_THROWS(serializeSAN(list_of("IP:mail.keytalk.com ")("DNS:owa.keytalk.com")), std::invalid_argument);
        TS_ASSERT_THROWS(serializeSAN(list_of("DNS: ")("DNS:owa.keytalk.com")), std::invalid_argument);
        TS_ASSERT_THROWS(deserializeSAN("INVALID:mail.keytalk.com,DNS:owa.keytalk.com"), std::invalid_argument);
        TS_ASSERT_THROWS(deserializeSAN("DNS: ,DNS:owa.keytalk.com"), std::invalid_argument);
        TS_ASSERT_THROWS(deserializeSAN("email: ,DNS:owa.keytalk.com"), std::invalid_argument);
    }

    void testExtractSAN_Values()
    {
        using boost::assign::list_of;
        using namespace ta::CertUtils;

        TS_ASSERT_EQUALS(extractSAN_Values(list_of("DNS:mail.keytalk.com")(" ")("IP:192.168.44.1")("DNS:owa.keytalk.com")("email:key@talk.com")),
                        list_of("mail.keytalk.com")("192.168.44.1")("owa.keytalk.com")("key@talk.com"));
        TS_ASSERT_EQUALS(extractSAN_Values(ta::StringArray()), ta::StringArray());

        TS_ASSERT_THROWS(extractSAN_Values(list_of("IP:mail.keytalk.com")("DNS:owa.keytalk.com")), std::invalid_argument);
        TS_ASSERT_THROWS(extractSAN_Values(list_of("IP:192.168.44.1")("INVALID:owa.keytalk.com")), std::invalid_argument);
    }

    void testIsSmimeCert()
    {
        // Test correct
        std::vector<unsigned char> myCertBufCorrect = ta::readData("CA/smime_cert_possibly_correct.pem");
        TS_ASSERT(ta::CertUtils::isSmimeCert(ta::vec2Str(myCertBufCorrect)));

        // Test no pub key
        std::vector<unsigned char> myCertBufNoPub = ta::readData("CA/smime_cert_no_pub.pem");
        TS_ASSERT_THROWS_ANYTHING(ta::CertUtils::isSmimeCert(ta::vec2Str(myCertBufNoPub)));
        // Test email address
        std::vector<unsigned char> myCertBufNoEmail = ta::readData("CA/smime_cert_no_email.pem");
        TS_ASSERT(!ta::CertUtils::isSmimeCert(ta::vec2Str(myCertBufNoEmail)));
        // Test no (email address in) SAN (but email address present in subject)
        std::vector<unsigned char> myCertBufNoSAN = ta::readData("CA/smime_cert_globalsign_no_san.pem");
        TS_ASSERT(!ta::CertUtils::isSmimeCert(ta::vec2Str(myCertBufNoSAN)));
        // Test key usage
        std::vector<unsigned char> myCertBufNoKeyUsage = ta::readData("CA/smime_cert_no_keyusage.pem");
        TS_ASSERT(!ta::CertUtils::isSmimeCert(ta::vec2Str(myCertBufNoKeyUsage)));
        // Test extended key usage
        std::vector<unsigned char> myCertBufNoExtendedKeyUsage = ta::readData("CA/smime_cert_no_extendedkeyusage.pem");
        TS_ASSERT(!ta::CertUtils::isSmimeCert(ta::vec2Str(myCertBufNoExtendedKeyUsage)));
    }

    void testGetMailFromSmime()
    {
        // Test correct
        std::vector<unsigned char> myCertBufCorrect = ta::readData("CA/smime_cert_possibly_correct.pem");
        TS_ASSERT_EQUALS(ta::CertUtils::getEmailFromSmime(ta::vec2Str(myCertBufCorrect)), "key@talk.co");

        // Test no email address
        std::vector<unsigned char> myCertBufNoEmail = ta::readData("CA/smime_cert_no_email.pem");
        TS_ASSERT_THROWS(ta::CertUtils::getEmailFromSmime(ta::vec2Str(myCertBufNoEmail)), std::invalid_argument);
    }

private:
    bool isCSR(const std::string& aCsrPem)
    {
        return aCsrPem.find("CERTIFICATE REQUEST") != std::string::npos;
    }
#ifndef _WIN32
    bool doesCsrHaveChallengePassword(const std::string& aCsrPem)
    {
        const std::string myCmd = "openssl req -noout -text | grep '^\\s*challengePassword\\s*:'";
        std::string myStdOut, myStdErr;
        return (ta::Process::shellExecSync(myCmd, myStdOut, myStdErr, aCsrPem) == 0);
    }
    void assertCsrChallengePasswordEquals(const std::string& aCsrPem, const std::string& anExpectedPassword)
    {
        const std::string myCmd = str(boost::format("openssl req -noout -text | grep '^\\s*challengePassword\\s*:%s$'") % anExpectedPassword);
        std::string myStdOut, myStdErr;
        if (ta::Process::shellExecSync(myCmd, myStdOut, myStdErr, aCsrPem) != 0)
        {
            TS_FAIL(str(boost::format("No challenge password '%s' found in CSR:\n%s") % anExpectedPassword % aCsrPem).c_str());
        }
    }
    bool doesCsrHaveSan(const std::string& aCsrPem)
    {
        const std::string myCmd = "openssl req -noout -text | grep '^\\s*X509v3 Subject Alternative Name:\\s*:'";
        std::string myStdOut, myStdErr;
        return (ta::Process::shellExecSync(myCmd, myStdOut, myStdErr, aCsrPem) == 0);
    }
    void assertCsrSanEquals(const std::string& aCsrPem,  const ta::StringArray& aSAN)
    {
        const std::string mySANStr = ta::Strings::join(aSAN, ", ");
        const std::string myCmd = str(boost::format("openssl req -noout -text | grep -A1 '^\\s*X509v3 Subject Alternative Name:\\s*' | grep '%s'") % mySANStr);
        std::string myStdOut, myStdErr;
        if (ta::Process::shellExecSync(myCmd, myStdOut, myStdErr, aCsrPem) != 0)
        {
            TS_FAIL(str(boost::format("No CSR '%s' found in CSR:\n%s") % mySANStr % aCsrPem).c_str());
        }
    }
#endif
};

