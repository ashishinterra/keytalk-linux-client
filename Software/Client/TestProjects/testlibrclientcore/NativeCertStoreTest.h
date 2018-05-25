#pragma once

#include "rclient/NativeCertStore.h"
#include "rclient/Settings.h"
#include "rclient/Common.h"
#include "ta/utils.h"
#include "ta/certutils.h"
#include "cxxtest/TestSuite.h"
#include "boost/assign/list_of.hpp"

class NativeCertStoreTest : public CxxTest::TestSuite
{
public:
    unsigned int cleanUserCerts(const std::string& anIssuerCnSuffix = "")
    {
        unsigned int myNumDeleted = 0;
        foreach (const std::string& issuer, rclient::Settings::getInstalledUserCaCNs())
        {
            myNumDeleted += rclient::NativeCertStore::deleteUserCertsForIssuerCN(issuer + anIssuerCnSuffix,
                                                                               rclient::NativeCertStore::proceedOnError);
        }
        return myNumDeleted;
    }

    void setUp()
    {
        try
        {
            CxxTest::setAbortTestOnFail(true);

            thePfxWithValidCert.data = ta::readData("CUST_PASSWD_INTERNAL.p12");
            thePfxWithValidCert.password = ta::readData("CUST_PASSWD_INTERNAL.pfx.pass.txt");
            thePfxWithValidCertCertSha1Fingerprint = "5fc049704a74ef926c6eb1edc343ef9a8abb1dae";

            thePfxWithExpiredCert.data = ta::readData("CUST_PASSWD_INTERNAL.Expired.p12");
            thePfxWithExpiredCert.password = ta::readData("CUST_PASSWD_INTERNAL.Expired.pfx.pass.txt");
            thePfxWithExpiredCertCertSha1Fingerprint = "d00fdb152c392913995488d8d42e3e7f0748729b";

            cleanUserCerts();

            CxxTest::setAbortTestOnFail(false);
        }
        catch (std::exception& e)
        {
            TS_TRACE(e.what());
            throw;
        }
        catch (...)
        {
            TS_TRACE("Unknown exception");
            throw;
        }
    }
    void tearDown()
    {
        try
        {
            cleanUserCerts();
        }
        catch (std::exception& e)
        {
            TS_TRACE(e.what());
        }
        catch (...)
        {
            TS_TRACE("Unknown exception");
        }
    }

    void test_import_valid_cert()
    {
        // when
        const std::string myCertSha1Finderprint = rclient::NativeCertStore::importPfx(thePfxWithValidCert);
        // then
        TS_ASSERT_EQUALS(myCertSha1Finderprint, thePfxWithValidCertCertSha1Fingerprint);
        unsigned int myNumValidCerts = rclient::NativeCertStore::validateReseptUserCert();
        TS_ASSERT_EQUALS(myNumValidCerts, 1);

        // when
        unsigned int myNumOfDeleted = rclient::NativeCertStore::deleteInvalidReseptUserCerts();
        // then
        TS_ASSERT_EQUALS(myNumOfDeleted, 0);
        myNumValidCerts = rclient::NativeCertStore::validateReseptUserCert();
        TS_ASSERT_EQUALS(myNumValidCerts, 1);

        // when
        myNumOfDeleted = rclient::NativeCertStore::deleteAllReseptUserCerts();
        // then
        TS_ASSERT_EQUALS(myNumOfDeleted, 1);
        myNumValidCerts = rclient::NativeCertStore::validateReseptUserCert();
        TS_ASSERT_EQUALS(myNumValidCerts, 0);
    }

    void test_import_expired_cert()
    {
        // when
        const std::string myCertSha1Finderprint = rclient::NativeCertStore::importPfx(thePfxWithExpiredCert);
        // then
        TS_ASSERT_EQUALS(myCertSha1Finderprint, thePfxWithExpiredCertCertSha1Fingerprint);
        unsigned int myNumValidCerts = rclient::NativeCertStore::validateReseptUserCert();
        TS_ASSERT_EQUALS(myNumValidCerts, 0);

        // when
        unsigned int myNumOfDeleted = rclient::NativeCertStore::deleteInvalidReseptUserCerts();
        // then
        TS_ASSERT_EQUALS(myNumOfDeleted, 1);
        myNumValidCerts = rclient::NativeCertStore::validateReseptUserCert();
        TS_ASSERT_EQUALS(myNumValidCerts, 0);

        // when
        myNumOfDeleted = rclient::NativeCertStore::deleteAllReseptUserCerts();
        // then
        TS_ASSERT_EQUALS(myNumOfDeleted, 0);
        myNumValidCerts = rclient::NativeCertStore::validateReseptUserCert();
        TS_ASSERT_EQUALS(myNumValidCerts, 0);
    }

    void test_that_cert_without_associated_service_is_ignored()
    {
        // when
        const std::string myCertSha1Finderprint = rclient::NativeCertStore::importPfx(thePfxWithValidCert);
        // then
        TS_ASSERT_EQUALS(myCertSha1Finderprint, thePfxWithValidCertCertSha1Fingerprint);
        unsigned int myNumValidCerts = rclient::NativeCertStore::validateReseptUserCert();
        TS_ASSERT_EQUALS(myNumValidCerts, 1);

        // given (remove the imported cert record from the registry)
        rclient::Settings::removeImportedUserCertFingerprints(boost::assign::list_of(myCertSha1Finderprint));
        // when
        myNumValidCerts = rclient::NativeCertStore::validateReseptUserCert();
        // then (the cert does not exist for KeyTalk)
        TS_ASSERT_EQUALS(myNumValidCerts, 0);
        // when
        unsigned int myNumOfDeleted = rclient::NativeCertStore::deleteAllReseptUserCerts();
        // then
        TS_ASSERT_EQUALS(myNumOfDeleted, 0);

        // given (restore the imported cert back in the registry)
        rclient::Settings::addImportedUserCertFingerprint(myCertSha1Finderprint);
        // when
        myNumValidCerts = rclient::NativeCertStore::validateReseptUserCert();
        // then
        TS_ASSERT_EQUALS(myNumValidCerts, 1);
        // when
        myNumOfDeleted = rclient::NativeCertStore::deleteAllReseptUserCerts();
        // then
        TS_ASSERT_EQUALS(myNumOfDeleted, 1);
    }

    void test_that_cert_with_invalid_password_cannot_be_imported()
    {
        // given
        thePfxWithExpiredCert.password = "_bad_password_";
        TS_ASSERT_EQUALS(rclient::NativeCertStore::validateReseptUserCert(), 0);
        // when-then
        TS_ASSERT_THROWS(rclient::NativeCertStore::importPfx(thePfxWithExpiredCert), rclient::NativeCertStoreError);
    }

    void test_that_multiple_certs_can_be_imported_and_deleted()
    {
        // Given
        // An empty certificate store

        // When
        rclient::NativeCertStore::importPfx(thePfxWithValidCert);
        rclient::NativeCertStore::importPfx(thePfxWithExpiredCert);

        // Then
        TS_ASSERT_EQUALS(rclient::NativeCertStore::validateReseptUserCert(), 1);

        // When
        unsigned int myNumOfDeleted = rclient::NativeCertStore::deleteAllReseptUserCerts();

        // Then
        TS_ASSERT_EQUALS(myNumOfDeleted, 2);
        TS_ASSERT_EQUALS(rclient::NativeCertStore::validateReseptUserCert(), 0);
    }

    void test_certificate_with_can_be_deleted_by_issuer_cn()
    {
        // Given
        // An empty certificate store

        // When
        rclient::NativeCertStore::importPfx(thePfxWithValidCert);
        rclient::NativeCertStore::importPfx(thePfxWithExpiredCert);

        // When
        unsigned int myNumOfDeleted = cleanUserCerts("_invalid");

        // Then
        TS_ASSERT_EQUALS(myNumOfDeleted, 0);

        // When
        myNumOfDeleted = cleanUserCerts();

        // Then
        TS_ASSERT_EQUALS(myNumOfDeleted, 2);

        // When
        myNumOfDeleted = cleanUserCerts();

        // Then (test idempotentness of deletion)
        TS_ASSERT_EQUALS(myNumOfDeleted, 0);
    }

    void test_that_CAs_can_be_installed_and_removed()
    {
        using namespace rclient::NativeCertStore;
        using ta::CertUtils::getCertInfo;
        using ta::CertUtils::getCertInfoFile;
        using boost::assign::list_of;

#ifndef _WIN32
        // given (prepare CA certs; cleanup cert store)
        const string myUcaPath = "signingcacert.der";
        const string myScaPath = "commcacert.der";
        const string myPcaPath = "pcacert.der";
        const vector<string> myExtraSigningCAsPemPaths = list_of("../Common/CA/globalsign_orgca.pem")("../Common/CA/globalsign_evca.pem");

        const string myUcaCn = getCertInfoFile(myUcaPath, ta::CertUtils::DER).subjCN;
        const string myScaCn = getCertInfoFile(myScaPath, ta::CertUtils::DER).subjCN;
        const string myPcaCn = getCertInfoFile(myPcaPath, ta::CertUtils::DER).subjCN;
        vector<string> myExtraSigningIntCAsFingerprints, myExtraSigningRootCAsFingerprints;
        foreach (const string& path, myExtraSigningCAsPemPaths)
        {
            foreach (const vector<unsigned char>& ca, ta::CertUtils::extractPemCertsFromFile(path))
            {
                const string myFingerprint = ta::CertUtils::getCertInfo(ca).sha1Fingerprint;
                if (ta::CertUtils::isSelfSignedCert(ca)) {
                    myExtraSigningRootCAsFingerprints.push_back(myFingerprint);
                } else {
                   myExtraSigningIntCAsFingerprints.push_back(myFingerprint);
                }
            }
        }
        // cleanup the store
        deleteFromIntermediateStoreByCN(myScaCn, proceedOnError);
        deleteFromIntermediateStoreByCN(myUcaCn, proceedOnError);
        deleteFromRootStoreByCN(myPcaCn, proceedOnError);
        foreach (const string& fingerprint, myExtraSigningRootCAsFingerprints)
        {
            deleteFromRootStoreByFingerprint(fingerprint, proceedOnError);
        }
        foreach (const string& fingerprint, myExtraSigningIntCAsFingerprints)
        {
            deleteFromIntermediateStoreByFingerprint(fingerprint, proceedOnError);
        }
        ta::StringArray myUCAs, mySCAs, myPCAs, myRCAs, myExtraSigningCAs;
        getInstalledCAs(myUCAs, mySCAs, myPCAs, myRCAs, myExtraSigningCAs);
        TS_ASSERT_EQUALS(myUCAs.size(), 0);
        TS_ASSERT_EQUALS(mySCAs.size(), 0);
        TS_ASSERT_EQUALS(myPCAs.size(), 0);
        TS_ASSERT_EQUALS(myRCAs.size(), 0);
        TS_ASSERT_EQUALS(myExtraSigningCAs.size(), 0);

        // when
        installCAs(myUcaPath, myScaPath, myPcaPath, "" /*no RCA*/, myExtraSigningCAsPemPaths);

        // then
        getInstalledCAs(myUCAs, mySCAs, myPCAs, myRCAs, myExtraSigningCAs);
        TS_ASSERT_EQUALS(myUCAs.size(), 1);
        TS_ASSERT_EQUALS(getCertInfo(myUCAs.at(0)).sha1Fingerprint, getCertInfoFile(myUcaPath, ta::CertUtils::DER).sha1Fingerprint);
        TS_ASSERT_EQUALS(mySCAs.size(), 1);
        TS_ASSERT_EQUALS(getCertInfo(mySCAs.at(0)).sha1Fingerprint, getCertInfoFile(myScaPath, ta::CertUtils::DER).sha1Fingerprint);
        TS_ASSERT_EQUALS(myPCAs.size(), 1);
        TS_ASSERT_EQUALS(getCertInfo(myPCAs.at(0)).sha1Fingerprint, getCertInfoFile(myPcaPath, ta::CertUtils::DER).sha1Fingerprint);
        TS_ASSERT_EQUALS(myRCAs.size(), 0);
        TS_ASSERT_EQUALS(myExtraSigningCAs.size(), myExtraSigningRootCAsFingerprints.size() + myExtraSigningIntCAsFingerprints.size());

        // when, then
        TS_ASSERT_EQUALS(deleteFromIntermediateStoreByCN(myScaCn, failOnError), 1);
        TS_ASSERT_EQUALS(deleteFromIntermediateStoreByCN(myUcaCn, failOnError), 1);
        TS_ASSERT_EQUALS(deleteFromRootStoreByCN(myPcaCn, failOnError), 1);
        foreach (const string& fingerprint, myExtraSigningRootCAsFingerprints)
        {
            TS_ASSERT_EQUALS(deleteFromRootStoreByFingerprint(fingerprint, failOnError), 1);
        }
        foreach (const string& fingerprint, myExtraSigningIntCAsFingerprints)
        {
            TS_ASSERT_EQUALS(deleteFromIntermediateStoreByFingerprint(fingerprint, failOnError), 1);
        }

        // when, then
        TS_ASSERT_EQUALS(deleteFromIntermediateStoreByCN(myScaCn, failOnError), 0);
        TS_ASSERT_EQUALS(deleteFromIntermediateStoreByCN(myUcaCn, failOnError), 0);
        TS_ASSERT_EQUALS(deleteFromRootStoreByCN(myPcaCn, failOnError), 0);
        foreach (const string& fingerprint, myExtraSigningRootCAsFingerprints)
        {
            TS_ASSERT_EQUALS(deleteFromRootStoreByFingerprint(fingerprint, failOnError), 0);
        }
        foreach (const string& fingerprint, myExtraSigningIntCAsFingerprints)
        {
            TS_ASSERT_EQUALS(deleteFromIntermediateStoreByFingerprint(fingerprint, failOnError), 0);
        }
        // then
        getInstalledCAs(myUCAs, mySCAs, myPCAs, myRCAs, myExtraSigningCAs);
        TS_ASSERT_EQUALS(myUCAs.size(), 0);
        TS_ASSERT_EQUALS(mySCAs.size(), 0);
        TS_ASSERT_EQUALS(myPCAs.size(), 0);
        TS_ASSERT_EQUALS(myRCAs.size(), 0);
        TS_ASSERT_EQUALS(myExtraSigningCAs.size(), 0);
#else
        ta::StringArray myUCAs, mySCAs, myPCAs, myRCAs, myExtraSigningCAs;
        getInstalledCAs(myUCAs, mySCAs, myPCAs, myRCAs, myExtraSigningCAs);
        TS_ASSERT_EQUALS(myUCAs.size(), 0);
        TS_ASSERT_EQUALS(mySCAs.size(), 0);
        TS_ASSERT_EQUALS(myPCAs.size(), 0);
        TS_ASSERT_EQUALS(myRCAs.size(), 0);
        TS_ASSERT_EQUALS(myExtraSigningCAs.size(), 0);

        TS_SKIP("Skipping CA installation test under Windows because it requires privileges elevation");
#endif
    }

    //@todo test with root store

    void test_that_invalid_CAs_cannot_be_installed()
    {
        TS_ASSERT_THROWS(rclient::NativeCertStore::installCAs("non-existing-uca", "non-existing-sca", "non-existing-pca", "", ta::StringArray()),
                         rclient::NativeCertStoreError);
    }

    void test_get_store_names_yields_some_common_store_names()
    {
#ifdef _WIN32
        const ta::StringArray myStoreNames = rclient::NativeCertStore::getStoreNames();
        TS_ASSERT(ta::isElemExist("My", myStoreNames));
        TS_ASSERT(ta::isElemExist("Root", myStoreNames));
        TS_ASSERT(ta::isElemExist("Trust", myStoreNames));
        TS_ASSERT(ta::isElemExist("CA", myStoreNames));

        TS_ASSERT(!ta::isElemExist("my", myStoreNames));
        TS_ASSERT(!ta::isElemExist("Nonexisting", myStoreNames));
#else
        TS_SKIP("This test is for Windows only");
#endif
    }

    void test_store_name_existence_for_some_common_store_names()
    {
#ifdef _WIN32
        TS_ASSERT(rclient::NativeCertStore::isStoreExists("My"));
        TS_ASSERT(rclient::NativeCertStore::isStoreExists("Root"));
        TS_ASSERT(rclient::NativeCertStore::isStoreExists("Trust"));
        TS_ASSERT(rclient::NativeCertStore::isStoreExists("CA"));

        // On Windows, stores may be referred to in a case insensitive way
        // Here we are more strict. Changing to a less strict policy is possible at a later point.
        TS_ASSERT(!rclient::NativeCertStore::isStoreExists("my"));
        TS_ASSERT(!rclient::NativeCertStore::isStoreExists("Nonexisting"));
#else
        TS_SKIP("This test is for Windows only");
#endif
    }

private:
    rclient::Pfx thePfxWithValidCert;
    std::string thePfxWithValidCertCertSha1Fingerprint;
    rclient::Pfx thePfxWithExpiredCert;
    std::string thePfxWithExpiredCertCertSha1Fingerprint;
};
