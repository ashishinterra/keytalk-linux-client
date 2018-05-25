#include "NativeCertStore.h"
#include "Settings.h"
#include "Common.h"
#include "resept/util.h"
#ifdef _WIN32
#include "ta/OsUserInfo.h"
#include "ta/sysinfo.h"
#include "ta/Registry.h"
#endif
#include "ta/logger.h"
#include "ta/encodingutils.h"
#include "ta/assert.h"
#include "ta/hashutils.h"
#include "ta/certutils.h"
#include "ta/strings.h"
#include "ta/process.h"
#include "ta/logger.h"
#include "ta/scopedresource.hpp"
#include "ta/utils.h"
#include "ta/common.h"

#include "boost/algorithm/string.hpp"
#include "boost/filesystem/operations.hpp"
#include <memory>
#include <vector>
#include <algorithm>
#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#include <Certsrv.h>
#else
#include "openssl/pem.h"
#include "openssl/err.h"
#include "openssl/pkcs12.h"
#include <sys/types.h>
#include <sys/stat.h>
#endif
#include <malloc.h>
#include "openssl/asn1.h"


using namespace ta;
using std::string;
using std::wstring;
using std::vector;
using std::swap;
namespace fs = boost::filesystem;

namespace rclient
{
    namespace NativeCertStore
    {
        namespace
        {
            enum CertsRemovalOpt
            {
                certsRemoveAll,
                certsRemoveInvalid
            };

            enum StoreType
            {
                _FirstStoreType,
                storePersonal = _FirstStoreType,
                storeIntermediate,
                storeRoot,
                _LastStoreType = storeRoot
            };
            static const string StoreTypeStrs[] = {"Personal", "Intermediate", "Root" };
            BOOST_STATIC_ASSERT(_FirstStoreType <= _LastStoreType);
            BOOST_STATIC_ASSERT(sizeof(StoreTypeStrs)/sizeof(StoreTypeStrs[0]) == _LastStoreType-_FirstStoreType+1);

            string str(const StoreType aStoreType)
            {
                return StoreTypeStrs[aStoreType-_FirstStoreType];
            }

            enum CertAttribute
            {
                _FirstCertAttribute,
                certAttrIssuerCn = _FirstCertAttribute,
                certAttrSubjCn,
                certAttrSha1Finterprint,
                _LastCertAttribute = certAttrSha1Finterprint
            };
            static const string CertAttributeStrs[] = {"issuer CN", "subject CN", "sha1 fingerprint" };
            BOOST_STATIC_ASSERT(_FirstCertAttribute <= _LastCertAttribute);
            BOOST_STATIC_ASSERT(sizeof(CertAttributeStrs)/sizeof(CertAttributeStrs[0]) == _LastCertAttribute-_FirstCertAttribute+1);
            string str(const CertAttribute aCertAttr)
            {
                return CertAttributeStrs[aCertAttr];
            }

#ifdef _WIN32
            static LPTSTR PersonalStoreName = "MY";
            static LPCWSTR IntermediateStoreName = L"CA\\.Default";
            static LPCWSTR RootStoreName = L"ROOT\\.Default";

            //
            // Abstract  : return whether the certificate is valid
            //
            // Exceptions: throw NativeCertStoreError on error
            //
            bool isCertValid(PCCERT_CONTEXT aCertContextPtr)
            {
                if (::CertVerifyTimeValidity(NULL, aCertContextPtr->pCertInfo) != 0)
                    return false;
                FILETIME myFtNow;
                SYSTEMTIME myStNow;
                ::GetSystemTime(&myStNow);
                ::SystemTimeToFileTime(&myStNow, &myFtNow);

                ULARGE_INTEGER myNotBefore = *(ULARGE_INTEGER*)&aCertContextPtr->pCertInfo->NotBefore;
                ULARGE_INTEGER myNotAfter  = *(ULARGE_INTEGER*)&aCertContextPtr->pCertInfo->NotAfter;
                ULARGE_INTEGER myNow = *(ULARGE_INTEGER*)&myFtNow;

                unsigned int myRemain   = static_cast<unsigned int>((myNotAfter.QuadPart - myNow.QuadPart)/10000000);
                if (!myRemain)
                    return false;
                if (myNotAfter.QuadPart < myNotBefore.QuadPart)
                    TA_THROW_MSG(NativeCertStoreError, "Certificate expires before it gets valid ?!");
                unsigned int myCertDuration = static_cast<unsigned int>((myNotAfter.QuadPart - myNotBefore.QuadPart)/10000000);
                unsigned int myCertValidPercent;
                try {
                    myCertValidPercent = Settings::getCertValidPercentage();
                } catch (SettingsError& e) {
                    TA_THROW_MSG(NativeCertStoreError, e.what());
                }
                bool myIsValid = (myRemain >= myCertDuration * myCertValidPercent / 100);
                DEBUGLOG(boost::format("Session certificate duration is %d sec, remain %d sec, validity percentage  is %d%%, certificate is considered as %svalid") % myCertDuration % myRemain % myCertValidPercent % (myIsValid?"":"in"));
                return myIsValid;
            }

            //
            // Abstract  : return the location to the RSA private key container temporary file associated with the certificate
            //
            // Exceptions: throw NativeCertStoreError on error
            //
            string getRsaPrivKeyTmpFileName(PCCERT_CONTEXT aCertContextPtr)
            {
                DWORD cbData;
                if (!::CertGetCertificateContextProperty(aCertContextPtr, CERT_KEY_PROV_INFO_PROP_ID, NULL, &cbData))
                    TA_THROW_MSG(NativeCertStoreError, "::CertGetCertificateContextProperty (1) failed");
                std::auto_ptr<CRYPT_KEY_PROV_INFO> myCryptKeyProvInfoPtr(static_cast<CRYPT_KEY_PROV_INFO*>(::operator new(cbData)));
                if (!::CertGetCertificateContextProperty(aCertContextPtr, CERT_KEY_PROV_INFO_PROP_ID, myCryptKeyProvInfoPtr.get(), &cbData))
                    TA_THROW_MSG(NativeCertStoreError, "::CertGetCertificateContextProperty (2) failed");
                if (!myCryptKeyProvInfoPtr->pwszContainerName)
                    TA_THROW_MSG(NativeCertStoreError, "Container name is empty");
                wstring myContainerNameW = myCryptKeyProvInfoPtr->pwszContainerName;
                boost::to_lower(myContainerNameW);
                string myContainerName = ta::EncodingUtils::toMbyte(myContainerNameW);
                myContainerName += '\0';
                vector<unsigned char> myMd5ContainerNameBin  = ta::HashUtils::getMd5Bin (myContainerName);
                TA_ASSERT(myMd5ContainerNameBin.size() == 16);
                for (unsigned short i = 0; i <= 12; i += 4)
                {
                    swap(myMd5ContainerNameBin[i], myMd5ContainerNameBin[i+3]);
                    swap(myMd5ContainerNameBin[i+1], myMd5ContainerNameBin[i+2]);
                }
                string myMd5ContainerName = ta::Strings::toHex(ta::getSafeBuf(myMd5ContainerNameBin), myMd5ContainerNameBin.size());

                string myUserSID, myAppDataDir;
                try
                {
                    myUserSID  = ta::OsUserInfo::getCurentUserSID();
                    myAppDataDir = ta::Process::getUserAppDataDir();
                }
                catch (std::runtime_error& e)
                {
                    TA_THROW_MSG(NativeCertStoreError, e.what());
                }
                string myMachineGuid;
                try {
                    ta::Registry::read(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Cryptography", "MachineGuid", myMachineGuid, ta::SysInfo::isWow64());
                } catch (std::exception& e) {
                    TA_THROW_MSG(NativeCertStoreError, e.what());
                }
                string myContainerPath = str(boost::format("%s\\Microsoft\\Crypto\\RSA\\%s\\%s_%s") % myAppDataDir % myUserSID % myMd5ContainerName % myMachineGuid);
                return myContainerPath;
            }

            // return success, call ::GetLast error for extended error info
            boost::optional<string> getCertAttrValue(PCCERT_CONTEXT aCertContextPtr, const CertAttribute aCertAttr)
            {
                if (!aCertContextPtr)
                {
                    ::SetLastError(0);
                    WARNLOG("Certificate context is NULL");
                    return boost::none;
                }

                switch (aCertAttr)
                {
                case certAttrIssuerCn:
                {
                    DWORD myAttrValLen = ::CertGetNameString(aCertContextPtr, CERT_NAME_SIMPLE_DISPLAY_TYPE, CERT_NAME_ISSUER_FLAG, 0, NULL, 0);
                    if (!myAttrValLen)
                    {
                        return boost::none;
                    }
                    std::auto_ptr<TCHAR> myAttrVal(static_cast<LPTSTR>(::operator new (myAttrValLen)));
                    if (!::CertGetNameString(aCertContextPtr,	CERT_NAME_SIMPLE_DISPLAY_TYPE, 	CERT_NAME_ISSUER_FLAG,  0, myAttrVal.get(), myAttrValLen))
                    {
                        return boost::none;
                    }
                    const string myAttrValue = myAttrVal.get();
                    return myAttrValue;
                }
                case certAttrSubjCn:
                {
                    DWORD myAttrValLen = ::CertGetNameString(aCertContextPtr, CERT_NAME_ATTR_TYPE, 0, szOID_COMMON_NAME, NULL, 0);
                    if (!myAttrValLen)
                    {
                        return boost::none;
                    }
                    std::auto_ptr<TCHAR> myAttrVal(static_cast<LPTSTR>(::operator new (myAttrValLen)));
                    if (!::CertGetNameString(aCertContextPtr, CERT_NAME_ATTR_TYPE, 0,  szOID_COMMON_NAME, myAttrVal.get(), myAttrValLen))
                    {
                        return boost::none;
                    }
                    const string myAttrValue = myAttrVal.get();
                    return myAttrValue;
                }
                case certAttrSha1Finterprint:
                {
                    BYTE mySha1HashBin[20];
                    DWORD myHashSize = sizeof(mySha1HashBin);
                    if (!::CertGetCertificateContextProperty(aCertContextPtr, CERT_HASH_PROP_ID, mySha1HashBin, &myHashSize))
                    {
                        return boost::none;
                    }
                    const string myAttrValue = ta::Strings::toHex(mySha1HashBin, myHashSize);
                    return myAttrValue;
                }
                default:
                {
                    ::SetLastError(0);
                    WARNLOG(boost::format("Unsupported certificate attribute type %d") % aCertAttr);
                    return boost::none;
                }
                }
            }

            //@nothrow
            bool isCA(PCCERT_CONTEXT aCertContextPtr)
            {
                TA_ASSERT(aCertContextPtr);
                CERT_EXTENSION* myExtList = aCertContextPtr->pCertInfo->rgExtension;
                if (!myExtList)
                    return false;
                CERT_EXTENSION* myExt = ::CertFindExtension(szOID_BASIC_CONSTRAINTS, aCertContextPtr->pCertInfo->cExtension, myExtList);
                if (myExt)
                {
                    CERT_BASIC_CONSTRAINTS_INFO* info;
                    DWORD size = 0;

                    if (::CryptDecodeObjectEx(X509_ASN_ENCODING, szOID_BASIC_CONSTRAINTS, myExt->Value.pbData, myExt->Value.cbData, CRYPT_DECODE_ALLOC_FLAG, NULL, &info, &size))
                    {
                        if (info->SubjectType.cbData > 0 && info->SubjectType.pbData[0] & CERT_CA_SUBJECT_FLAG)
                            return LocalFree(info), true;
                        LocalFree(info);
                    }
                }
                myExt = CertFindExtension(szOID_BASIC_CONSTRAINTS2, aCertContextPtr->pCertInfo->cExtension, myExtList);
                if (myExt)
                {
                    CERT_BASIC_CONSTRAINTS2_INFO myBasicConstraints;
                    DWORD size = sizeof(CERT_BASIC_CONSTRAINTS2_INFO);

                    if (::CryptDecodeObjectEx(X509_ASN_ENCODING, szOID_BASIC_CONSTRAINTS2, myExt->Value.pbData, myExt->Value.cbData, 0, NULL, &myBasicConstraints, &size))
                    {
                        if (myBasicConstraints.fCA)
                            return true;
                    }
                }
                return false;
            }

            class Store
            {
            public:
                // @throw NativeCertStoreError
                Store(StoreType aStoreType, const bool aReadOnly = false)
                    : theProvider(NULL)
                    , theStore(NULL)
                    , theStoreType(aStoreType)
                    , theReadOnly(aReadOnly)
                {
                    switch (theStoreType)
                    {
                    case storePersonal:
                    {
                        if (!::CryptAcquireContext(&theProvider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT) || !theProvider)
                        {
                            TA_THROW_MSG(NativeCertStoreError, boost::format("Failed to acquire CryptoAPI provider. Error code %d") % ::GetLastError());
                        }
                        theStore = ::CertOpenSystemStore(theProvider, PersonalStoreName);
                        if (!theStore)
                        {
                            const int myError = ::GetLastError();
                            ::CryptReleaseContext(theProvider, 0);
                            TA_THROW_MSG(NativeCertStoreError, boost::format("Failed to open personal store. Error code %d") % myError);
                        }
                        break;
                    }
                    case storeIntermediate:
                    case storeRoot:
                    {
                        LPCWSTR myStoreName = (theStoreType == storeIntermediate) ? IntermediateStoreName : RootStoreName;
                        DWORD myStoreOpenFlags = CERT_SYSTEM_STORE_LOCAL_MACHINE;
                        if (theReadOnly)
                        {
                            myStoreOpenFlags |= CERT_STORE_READONLY_FLAG;
                        }
                        theStore = ::CertOpenStore(CERT_STORE_PROV_PHYSICAL, 0, NULL, myStoreOpenFlags, myStoreName);
                        if (!theStore)
                        {
                            TA_THROW_MSG(NativeCertStoreDeleteError, boost::format("Failed to open '%s' store. Error code %d") % str(theStoreType) % ::GetLastError());
                        }
                        break;
                    }
                    default:
                    {
                        TA_THROW_MSG(NativeCertStoreError, boost::format("Store %s is not supported") % str(theStoreType));
                    }
                    }
                }
                ~Store()
                {
                    TA_ASSERT(theStore);

                    switch (theStoreType)
                    {
                    case storePersonal:
                    {
                        TA_ASSERT(theProvider);
                        ::CertCloseStore(theStore, CERT_CLOSE_STORE_FORCE_FLAG), theStore = NULL;
                        ::CryptReleaseContext(theProvider, 0), theProvider = NULL;
                        break;
                    }
                    case storeIntermediate:
                    case storeRoot:
                    {
                        ::CertCloseStore(theStore, CERT_CLOSE_STORE_FORCE_FLAG), theStore = NULL;
                        break;
                    }
                    default:
                    {
                        WARNLOG(boost::format("Store type %d is not supported") % theStoreType);
                    }
                    }
                }
                operator HCERTSTORE() const
                {
                    TA_ASSERT(theStore);
                    return theStore;
                }


                unsigned int findValidCerts(const ta::StringArray& aCertSha1Fingerprints, const string& aServiceNameHint) const
                {
                    unsigned int myNumOfValidCerts = 0;

                    for (PCCERT_CONTEXT myCertCtx = ::CertEnumCertificatesInStore(theStore, NULL);
                            myCertCtx;
                            myCertCtx = ::CertEnumCertificatesInStore(theStore, myCertCtx))
                    {
                        const boost::optional<string> mySha1Fingerprint = getCertAttrValue(myCertCtx, certAttrSha1Finterprint);
                        if (mySha1Fingerprint && ta::isElemExist(*mySha1Fingerprint, aCertSha1Fingerprints))
                        {
                            if (isCertValid(myCertCtx))
                            {
                                ++myNumOfValidCerts;
                            }
                        }
                    }
                    DEBUGLOG(boost::format("Found %d valid certificate(s) in %s store for service %s") % myNumOfValidCerts % str(theStoreType) % aServiceNameHint);
                    return myNumOfValidCerts;
                }

                boost::optional<string> findFirstCertByAttr(const CertAttribute aCertAttr, const string& anAttrVal) const
                {
                    for (PCCERT_CONTEXT myCertCtx = ::CertEnumCertificatesInStore(theStore, NULL);
                            myCertCtx;
                            myCertCtx = ::CertEnumCertificatesInStore(theStore, myCertCtx))
                    {
                        const boost::optional<string> myAttrVal = getCertAttrValue(myCertCtx, aCertAttr);
                        if (!myAttrVal)
                        {
                            TA_THROW_MSG(std::invalid_argument, "Cannot retrieve certificate attribute " + str(aCertAttr));
                        }
                        if (*myAttrVal == anAttrVal)
                        {
                            const vector<unsigned char> myDerCert(myCertCtx->pbCertEncoded, myCertCtx->pbCertEncoded + myCertCtx->cbCertEncoded);
                            return ta::vec2Str(ta::CertUtils::convDer2Pem(myDerCert));
                        }
                    }
                    return boost::none;
                }

                //@return SHA-1 fingerprints of the certs that exist in the cert store
                ta::StringArray findCertsByFingerprints(const ta::StringArray& aCertSha1Fingerprints) const
                {
                    ta::StringArray myFoundCertFingerprints;

                    for (PCCERT_CONTEXT myCertCtx = ::CertEnumCertificatesInStore(theStore, NULL);
                            myCertCtx;
                            myCertCtx = ::CertEnumCertificatesInStore(theStore, myCertCtx))
                    {
                        const boost::optional<string> mySha1Fingerprint = getCertAttrValue(myCertCtx, certAttrSha1Finterprint);
                        if (mySha1Fingerprint && ta::isElemExist(*mySha1Fingerprint, aCertSha1Fingerprints))
                        {
                            myFoundCertFingerprints.push_back(*mySha1Fingerprint);
                        }
                    }

                    return myFoundCertFingerprints;
                }

                //@return SHA-1 fingerprints of the certs removed
                ta::StringArray removeCertKeys(const ta::StringArray& aCertSha1Fingerprints, CertsRemovalOpt aCertRemovelOpt, const string& aServiceNameHint)
                {
                    DEBUGLOG(boost::format("Deleting certificates from %s store for service %s.") % str(theStoreType) % aServiceNameHint);
                    if (theReadOnly)
                    {
                        TA_THROW_MSG(NativeCertStoreDeleteError, "Cannot remove certificate from the " + str(theStoreType) + " store because the store is opened read-only");
                    }
                    ta::StringArray myRemovedCertsSha1Fingerprints;

                    for (PCCERT_CONTEXT myCertCtx = ::CertEnumCertificatesInStore(theStore, NULL);
                            myCertCtx;
                            myCertCtx = ::CertEnumCertificatesInStore(theStore, myCertCtx))
                    {
                        const boost::optional<string> mySha1Fingerprint = getCertAttrValue(myCertCtx, certAttrSha1Finterprint);
                        if (mySha1Fingerprint  && ta::isElemExist(*mySha1Fingerprint, aCertSha1Fingerprints))
                        {
                            if (aCertRemovelOpt == certsRemoveAll || (aCertRemovelOpt == certsRemoveInvalid && !isCertValid(myCertCtx)))
                            {
                                string myRsaPrivKeyTmpFileName;
                                const bool myNeedCleanupRsaTempKeys = !isCA(myCertCtx);
                                if (myNeedCleanupRsaTempKeys)
                                {
                                    try {
                                        myRsaPrivKeyTmpFileName = getRsaPrivKeyTmpFileName(myCertCtx);
                                    } catch (NativeCertStoreError& e) {
                                        WARNLOG2("RSA temporary keys cleanup error.", e.what());
                                    }
                                }
                                if (!::CertDeleteCertificateFromStore(::CertDuplicateCertificateContext(myCertCtx)))
                                {
                                    TA_THROW_MSG(NativeCertStoreDeleteError, "Failed to delete certificate");
                                }
                                if (myNeedCleanupRsaTempKeys && !myRsaPrivKeyTmpFileName.empty())
                                {
                                    DEBUGLOG("Cleaning up, removing: " + myRsaPrivKeyTmpFileName);
                                    if (!::DeleteFile(myRsaPrivKeyTmpFileName.c_str()))
                                        WARNLOG(boost::format("Failed to delete temp RSA container file (%s)") % myRsaPrivKeyTmpFileName);
                                }
                                myRemovedCertsSha1Fingerprints.push_back(*mySha1Fingerprint);
                            }
                        }
                    }
                    DEBUGLOG(boost::format("Deleted %d certificate(s) from %s store associated with service %s") % myRemovedCertsSha1Fingerprints.size() % str(theStoreType) % aServiceNameHint);
                    return myRemovedCertsSha1Fingerprints;
                }

                unsigned int removeCertKeysByAttr(const CertAttribute aCertAttr, const string& anAttrVal, ErrorPolicy anErrorPolicy)
                {
                    DEBUGLOG(boost::format("Deleting certificate from %s store having %s %s") % str(theStoreType) % str(aCertAttr) % anAttrVal);
                    if (theReadOnly)
                    {
                        TA_THROW_MSG(NativeCertStoreDeleteError, "Cannot remove certificate from the " + str(theStoreType) + " store because the store is opened read-only");
                    }
                    unsigned int myNumOfDeleted = 0;

                    for (PCCERT_CONTEXT myCertCtx = ::CertEnumCertificatesInStore(theStore, NULL);
                            myCertCtx;
                            myCertCtx = ::CertEnumCertificatesInStore(theStore, myCertCtx))
                    {
                        const boost::optional<string> myAttrVal = getCertAttrValue(myCertCtx, aCertAttr);
                        if (myAttrVal && (*myAttrVal == anAttrVal))
                        {
                            string myRsaPrivKeyTmpFileName;
                            if (theStoreType == storePersonal)
                            {
                                try
                                {
                                    myRsaPrivKeyTmpFileName = getRsaPrivKeyTmpFileName(myCertCtx);
                                }
                                catch (NativeCertStoreError& e)
                                {
                                    WARNLOG2("RSA temporary keys cleanup error.", e.what());
                                }
                            }

                            if (::CertDeleteCertificateFromStore(::CertDuplicateCertificateContext(myCertCtx)))
                            {
                                ++myNumOfDeleted;
                            }
                            else
                            {
                                if (anErrorPolicy == proceedOnError)
                                {
                                    WARNLOG2(boost::format("Failed to delete certificate from the personal store having %s %s") % str(aCertAttr) % anAttrVal,
                                             boost::format("LastError is %d") % ::GetLastError());
                                }
                                else
                                {
                                    TA_THROW_MSG(NativeCertStoreDeleteError, boost::format("Failed to delete certificate from the personal store having %s %s. LastError is %d") %
                                                 str(aCertAttr) % anAttrVal % ::GetLastError());
                                }
                            }

                            if (theStoreType == storePersonal && !myRsaPrivKeyTmpFileName.empty())
                            {
                                DEBUGLOG("Cleaning up, removing: " + myRsaPrivKeyTmpFileName);
                                if (!::DeleteFile(myRsaPrivKeyTmpFileName.c_str()))
                                {
                                    WARNLOG(boost::format("Failed to delete temp RSA container file (%s)") % myRsaPrivKeyTmpFileName);
                                }
                            }
                        }
                    }
                    DEBUGLOG(boost::format("Deleted %d certificate(s) from %s store having %s %s") % myNumOfDeleted % str(theStoreType) % str(aCertAttr) % anAttrVal);
                    return myNumOfDeleted;
                }

                void importDerCertFileToTrustedStore(const string& aDerCertPath)
                {
                    DEBUGLOG(boost::format("Importing DER certificate from %s to %s store") % aDerCertPath % str(theStoreType));
                    if (theStoreType == storePersonal)
                    {
                        TA_THROW_MSG(NativeCertStoreImportError, "Trusted store expected but personal store supplied to import the certificate");
                    }
                    if (theReadOnly)
                    {
                        TA_THROW_MSG(NativeCertStoreImportError, "Cannot import certificate to the " + str(theStoreType) + " store because the store is opened read-only");
                    }

                    HANDLE hFile = ::CreateFile(aDerCertPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
                    if (hFile == INVALID_HANDLE_VALUE)
                    {
                        TA_THROW_MSG(NativeCertStoreImportError, boost::format("Cannot open file '%s' for reading") % aDerCertPath);
                    }
                    ta::ScopedResource<HANDLE> myScopedFileHandler(hFile, ::CloseHandle);// for automatic RAII

                    DWORD cchFile = ::GetFileSize(hFile, NULL);
                    BSTR bstrCert = ::SysAllocStringByteLen(NULL, cchFile);
                    DWORD cbRead = 0;
                    if (!::ReadFile(hFile, (char*)bstrCert, cchFile, &cbRead,	 NULL))
                    {
                        TA_THROW_MSG(NativeCertStoreImportError, boost::format("Cannot read file '%s'") % aDerCertPath);
                    }

                    PCCERT_CONTEXT pDesiredCert = ::CertCreateCertificateContext( X509_ASN_ENCODING, (const unsigned char*)bstrCert, cchFile);
                    if (!pDesiredCert)
                    {
                        TA_THROW_MSG(NativeCertStoreImportError, boost::format("Cannot create certificate context for file '%s'") % aDerCertPath);
                    }

                    if (!::CertAddCertificateContextToStore( theStore, pDesiredCert, CERT_STORE_ADD_REPLACE_EXISTING, NULL))
                    {
                        const DWORD myLastError = ::GetLastError();
                        ::CertFreeCertificateContext(pDesiredCert);
                        TA_THROW_MSG(NativeCertStoreImportError, boost::format("CertAddCertificateContextToStore failed for file '%s'. Last error is %d") % aDerCertPath % myLastError);
                    }
                    ::CertFreeCertificateContext(pDesiredCert);
                    DEBUGLOG(boost::format("Successfully imported DER certificate from %s to %s store") % aDerCertPath % str(theStoreType));
                }

                void importPemCertToTrustedStore(const vector<unsigned char>& aPemCert)
                {
                    DEBUGLOG(boost::format("Importing certificate to %s store") % str(theStoreType));
                    if (theStoreType == storePersonal)
                    {
                        TA_THROW_MSG(NativeCertStoreImportError, "Trusted store expected but personal store supplied to import the certificate");
                    }
                    if (theReadOnly)
                    {
                        TA_THROW_MSG(NativeCertStoreImportError, "Cannot import certificate to the " + str(theStoreType) + " store because the store is opened read-only");
                    }

                    const vector<unsigned char> myDerCert = ta::CertUtils::convPem2Der(aPemCert);
                    PCCERT_CONTEXT pDesiredCert = ::CertCreateCertificateContext(X509_ASN_ENCODING, &myDerCert[0], myDerCert.size());
                    if (!pDesiredCert)
                    {
                        TA_THROW_MSG(NativeCertStoreImportError, boost::format("Cannot create certificate context for PEM certificate:\n%s") % ta::vec2Str(aPemCert));
                    }

                    if (!::CertAddCertificateContextToStore(theStore, pDesiredCert, CERT_STORE_ADD_REPLACE_EXISTING, NULL))
                    {
                        const DWORD myLastError = ::GetLastError();
                        ::CertFreeCertificateContext(pDesiredCert);
                        TA_THROW_MSG(NativeCertStoreImportError, boost::format("CertAddCertificateContextToStore failed for certificate\n%s. Last error is %d") % ta::vec2Str(aPemCert) % myLastError);
                    }
                    ::CertFreeCertificateContext(pDesiredCert);
                    DEBUGLOG(boost::format("Successfully imported certificate %s store") % str(theStoreType));
                }

                //
                // add certificate to the desired CS
                //
                // Exceptions: throw NativeCertStoreImportError on error
                //
                void addToStore(PCCERT_CONTEXT aCertContextPtr, DWORD aDisposition)
                {
                    if (theReadOnly)
                    {
                        TA_THROW_MSG(NativeCertStoreImportError, "Cannot add certificate to the " + str(theStoreType) + " store because the store is opened read-only");
                    }
                    if (!::CertAddCertificateContextToStore (theStore, aCertContextPtr, aDisposition, 0))
                    {
                        TA_THROW_MSG(NativeCertStoreImportError, "Failed to add certificate to the " + str(theStoreType) + " store");
                    }
                }

            private:
                HCRYPTPROV theProvider;
                HCERTSTORE theStore;
                const StoreType theStoreType;
                const bool theReadOnly;

                static BOOL WINAPI StoreEnumCallback(
                    const void *pvSystemStore,
                    DWORD UNUSED(dwFlags),
                    PCERT_SYSTEM_STORE_INFO UNUSED(pStoreInfo),
                    void *UNUSED(pvReserved),
                    void *pvArg)
                {
                    if (pvSystemStore == NULL)
                    {
                        ERRORDEVLOG("pvSystemStore is NULL");
                        return FALSE;
                    }
                    else if (pvArg == NULL)
                    {
                        ERRORDEVLOG("pvArg is NULL");
                        return FALSE;
                    }
                    ta::StringArray* pEnumArg = (ta::StringArray*)pvArg;
                    const LPCWSTR pwszSystemStore = (LPCWSTR)pvSystemStore;
                    static int line_counter = 0;

                    pEnumArg->push_back(ta::EncodingUtils::toMbyte(pwszSystemStore));
                    return TRUE;
                }
            public:
                static ta::StringArray getStoreNames()
                {
                    // Based on: https://msdn.microsoft.com/en-us/library/windows/desktop/aa382362%28v=vs.85%29.aspx
                    ta::StringArray myResult;

                    if (!CertEnumSystemStore(
                                CERT_SYSTEM_STORE_LOCAL_MACHINE,
                                NULL,
                                &myResult, // Contains result list to be filled
                                StoreEnumCallback
                            ))
                    {
                        TA_THROW_MSG(NativeCertStoreError, "Cannot enumerate certificate stores.");
                    }
                    return myResult;
                }
            }; // Store

            void closeCertStore(HCERTSTORE hCertStore)
            {
                if (hCertStore)
                {
                    ::CertCloseStore(hCertStore, 0);
                }
            }

            // CA chain
            class CAChain : boost::noncopyable
            {
            public:
                CAChain(HCERTSTORE aStore) : store(aStore)
                {}
                void add(PCCERT_CONTEXT aCert)
                {
                    if (aCert)
                    {
                        chain.push_back(::CertDuplicateCertificateContext(aCert));
                    }
                }
                // ::CertFreeCertificateContext() ought to be called for the returned cert
                PCCERT_CONTEXT terminalCert() const
                {
                    foreach(PCCERT_CONTEXT cert, chain)
                    {
                        if (!hasChildren(cert))
                        {
                            return ::CertDuplicateCertificateContext(cert);
                        }
                    }
                    return NULL;
                }
                ~CAChain()
                {
                    foreach(PCCERT_CONTEXT cert, chain)
                    {
                        ::CertFreeCertificateContext(cert);
                    }
                    chain.clear();
                }
            private:
                bool hasChildren(PCCERT_CONTEXT aCert) const
                {
                    if (aCert)
                    {
                        foreach(PCCERT_CONTEXT cert, chain)
                        {
                            if (cert != aCert) // do not count self-signed
                            {
                                DWORD           dwFlags = 0;
                                ScopedResource<PCCERT_CONTEXT> myIssuerCert(::CertGetIssuerCertificateFromStore(store, cert, NULL, &dwFlags), ::CertFreeCertificateContext);
                                if (myIssuerCert && myIssuerCert == aCert)
                                {
                                    return true; // congratulations, you are a father!
                                }
                            }
                        }
                    }
                    return false;
                }
            private:
                HCERTSTORE store;
                vector<PCCERT_CONTEXT> chain;
            };

            // Locate a first terminal non-CA cert of CA without descendants in the given store
            // @return pointer to the cert context of NULL if not found. When non-NULL it should be freed with ::CertFreeCertificateContext()
            PCCERT_CONTEXT findTerminalCertInStore(HCERTSTORE aStore)
            {
                CAChain myCAChain(aStore);
                for (PCCERT_CONTEXT myCertCtx = ::CertEnumCertificatesInStore(aStore, NULL);
                        myCertCtx;
                        myCertCtx = ::CertEnumCertificatesInStore(aStore, myCertCtx))
                {
                    if (isCA(myCertCtx))
                    {
                        myCAChain.add(myCertCtx);
                    }
                    else
                    {
                        return myCertCtx;
                    }
                }

                // if we end up here, there only have CAs, so select a terminal one
                return myCAChain.terminalCert();
            }


            //@return SHA1 fingerprint (hex) of the imported certificate
            //@throw NativeCertStoreImportError
            string importPfxWin32(const Pfx& aPfx)
            {
                DEBUGLOG("Importing pfx");
                CRYPT_DATA_BLOB myPFX;
                myPFX.cbData = (DWORD)aPfx.data.size();
                myPFX.pbData = (BYTE*)ta::getSafeBuf(aPfx.data);
                std::wstring myPassUni = ta::EncodingUtils::toWide(aPfx.password);

                // Extract PFX to the temporary store
                ta::ScopedResource<HCERTSTORE> myTempStore(::PFXImportCertStore(&myPFX, myPassUni.c_str(), 0), closeCertStore);
                if (!myTempStore)
                {
                    TA_THROW_MSG(NativeCertStoreImportError, boost::format("PFXImportCertStore failed. Last error %d") % ::GetLastError());
                }

                // Locate the cert to be imported in the Personal system store
                ta::ScopedResource<PCCERT_CONTEXT> myCertCtx(findTerminalCertInStore(myTempStore), ::CertFreeCertificateContext);
                if (!myCertCtx)
                {
                    ERRORLOG("No terminal certificate found in the received Pfx");
                    TA_THROW_MSG(NativeCertStoreImportError, "No terminal certificate found in the received Pfx");
                }

                boost::optional<string> myCertSubjCn  = getCertAttrValue(myCertCtx, certAttrSubjCn);
                if (!myCertSubjCn)
                {
                    myCertSubjCn = "unknown-subject";
                }
                const boost::optional<string>  mySha1Fingerprint = getCertAttrValue(myCertCtx, certAttrSha1Finterprint);
                if (!mySha1Fingerprint)
                {
                    TA_THROW_MSG(std::invalid_argument, boost::format("Failed to retrieve SHA1 fingerprint of the cert with CN %s Last error: %d") % (*myCertSubjCn) % ::GetLastError());
                }

                DEBUGLOG(boost::format("Importing certificate with CN %s and sha1 fingerprint %s to the Personal system store") % (*myCertSubjCn) % (*mySha1Fingerprint));
                Store(storePersonal).addToStore(myCertCtx, CERT_STORE_ADD_REPLACE_EXISTING);
                return *mySha1Fingerprint;
            }

#else // non Windows

	

            bool isCertValid(const ta::CertUtils::CertInfo& aCertInfo)
            {
                const time_t myNow = time(NULL);

                const int myCertDuration = aCertInfo.utcNotAfter - aCertInfo.utcNotBefore;
                if (myCertDuration < 0)
                    TA_THROW_MSG(NativeCertStoreError, "Certificate expires before it gets valid ?!");

                if (myNow < aCertInfo.utcNotBefore || myNow  >= aCertInfo.utcNotAfter)
                    return false;

                const int myRemain = aCertInfo.utcNotAfter - myNow;

                unsigned int myCertValidPercent;
                try {
                    myCertValidPercent = Settings::getCertValidPercentage();
                } catch (SettingsError& e) {
                    TA_THROW_MSG(NativeCertStoreError, e.what());
                }
                const int myMinRemain = (int)(myCertDuration * myCertValidPercent / 100);
                const bool myIsValid = (myRemain >= myMinRemain);
                DEBUGLOG(boost::format("Session certificate duration is %d sec, remain %d sec, validity percentage  is %d%%, certificate is considered as %svalid") % myCertDuration % myRemain % myCertValidPercent % (myIsValid?"":"in"));
                return myIsValid;
            }

            class Store
            {
            private:
                const StoreType theStoreType;
                const bool theReadOnly;
            public:
                // @throw NativeCertStoreError
                Store(const StoreType aStoreType, const bool aReadOnly = false)
                    : theStoreType(aStoreType)
                    , theReadOnly(aReadOnly)
                {
                    const string myDir = getStoreDir();
                    if (!ta::isDirExist(myDir))
                    {
                        if (theReadOnly)
                        {
                            TA_THROW_MSG(NativeCertStoreError, boost::format("Failed to initialize read-only access to keystore %s because the store does not exist.") % str(theStoreType));
                        }
                        try
                        {
                            fs::create_directories(myDir);
                        }
                        catch (std::exception& e)
                        {
                            TA_THROW_MSG(NativeCertStoreError, boost::format("Failed to initialize keystore %s. Cannot create directory '%s'. %s") % str(theStoreType) % myDir % e.what());
                        }
                    }
                }
                ~Store()
                {}

                static bool isPemCertFile(fs::path const& path)
                {
                    if (is_regular_file(path))
                    {
                        const string myFilePath = path.string();
                        if (ta::CertUtils::fileHasPemCert(myFilePath))
                        {
                            return true;
                        }
                    }
                    return false;
                }

                unsigned int findValidCerts(const ta::StringArray& aCertSha1Fingerprints, const string& aServiceNameHint) const
                {
                    unsigned int myNumOfValidCerts = 0;

                    fs::directory_iterator it(getStoreDir()), eod;
                    foreach (fs::path const& p, std::make_pair(it, eod))
                    {
                        if (isPemCertFile(p))
                        {
                            const ta::CertUtils::CertInfo myCertInfo = ta::CertUtils::getCertInfoFile(p.string());
                            if (ta::isElemExist(myCertInfo.sha1Fingerprint, aCertSha1Fingerprints))
                            {
                                if (isCertValid(myCertInfo))
                                {
                                    ++myNumOfValidCerts;
                                }
                            }
                        }
                    }
                    DEBUGLOG(boost::format("Found %d valid certificate(s) in %s store for KeyTalk service %s") % myNumOfValidCerts % str(theStoreType) % aServiceNameHint);
                    return myNumOfValidCerts;
                }

                boost::optional<string> findFirstCertByAttr(const CertAttribute aCertAttr, const string& anAttrVal) const
                {
                    fs::directory_iterator it(getStoreDir()), eod;
                    foreach (fs::path const& p, std::make_pair(it, eod))
                    {
                        if (isPemCertFile(p))
                        {
                            const ta::CertUtils::CertInfo myCertInfo = ta::CertUtils::getCertInfoFile(p.string());
                            switch (aCertAttr)
                            {
                            case certAttrSubjCn:
                            {
                                if (myCertInfo.subjCN == anAttrVal) {
                                    return (string)ta::readData(p.string());
                                } else {
                                    continue;
                                }
                            }
                            case certAttrSha1Finterprint:
                            {
                                if (myCertInfo.sha1Fingerprint == anAttrVal) {
                                    return (string)ta::readData(p.string());
                                } else {
                                    continue;
                                }
                            }
                            case certAttrIssuerCn:
                            default:
                                TA_THROW_MSG(std::invalid_argument, "Certificate search attribute " + str(aCertAttr) + " is not supported");
                            }
                        }
                    }
                    return boost::none;
                }

                //@return SHA-1 fingerprints of the certs that exist in the cert store
                ta::StringArray findCertsByFingerprints(const ta::StringArray& aCertSha1Fingerprints) const
                {
                    ta::StringArray myFoundCertFingerprints;

                    fs::directory_iterator it(getStoreDir()), eod;
                    foreach (fs::path const& p, std::make_pair(it, eod))
                    {
                        if (isPemCertFile(p))
                        {
                            const ta::CertUtils::CertInfo myCertInfo = ta::CertUtils::getCertInfoFile(p.string());
                            if (ta::isElemExist(myCertInfo.sha1Fingerprint, aCertSha1Fingerprints))
                            {
                                myFoundCertFingerprints.push_back(myCertInfo.sha1Fingerprint);
                            }
                        }
                    }

                    return myFoundCertFingerprints;
                }

                //@return SHA-1 fingerprints of the certs removed
                ta::StringArray removeCertKeys(const ta::StringArray& aCertSha1Fingerprints, CertsRemovalOpt aCertRemovelOpt, const string& aServiceNameHint)
                {
                    DEBUGLOG(boost::format("Deleting certificates from %s store for service %s") % str(theStoreType) % aServiceNameHint);
                    if (theReadOnly)
                    {
                        TA_THROW_MSG(NativeCertStoreDeleteError, "Cannot remove certificate from the " + str(theStoreType) + " store because the store is opened read-only");
                    }

                    ta::StringArray myRemovedCertsSha1Fingerprints;
                    fs::directory_iterator it(getStoreDir()), eod;
                    foreach (fs::path const& p, std::make_pair(it, eod))
                    {
                        if (isPemCertFile(p))
                        {
                            const ta::CertUtils::CertInfo myCertInfo = ta::CertUtils::getCertInfoFile(p.string());
                            if (ta::isElemExist(myCertInfo.sha1Fingerprint, aCertSha1Fingerprints))
                            {
                                if (aCertRemovelOpt == certsRemoveAll || (aCertRemovelOpt == certsRemoveInvalid && !isCertValid(myCertInfo)))
                                {
                                    fs::remove(p);
                                    effectuateRemovedCertsInStore();
                                    myRemovedCertsSha1Fingerprints.push_back(myCertInfo.sha1Fingerprint);
                                }
                            }
                        }
                    }

                    DEBUGLOG(boost::format("Deleted %d certificate(s) from %s store associated with KeyTalk service %s") % myRemovedCertsSha1Fingerprints.size() % str(theStoreType) % aServiceNameHint);
                    return myRemovedCertsSha1Fingerprints;
                }

                unsigned int removeCertKeysByAttr(const CertAttribute aCertAttr, const string& anAttrVal, const ErrorPolicy anErrorPolicy)
                {
                    DEBUGLOG(boost::format("Deleting certificates from %s store having %s %s") % str(theStoreType) % str(aCertAttr) % anAttrVal);
                    if (theReadOnly)
                    {
                        TA_THROW_MSG(NativeCertStoreDeleteError, "Cannot remove certificate from the " + str(theStoreType) + " store because the store is opened read-only");
                    }

                    unsigned int myNumFilesRemoved = 0;
                    fs::directory_iterator it(getStoreDir()), eod;

                    foreach (fs::path const& p, std::make_pair(it, eod))
                    {
                        if (isPemCertFile(p))
                        {
                            try
                            {
                                const ta::CertUtils::CertInfo myCertInfo = ta::CertUtils::getCertInfoFile(p.string());
                                switch (aCertAttr)
                                {
                                case certAttrIssuerCn:
                                {
                                    if (myCertInfo.issuerCN == anAttrVal)
                                        break;
                                    else
                                        continue;
                                }
                                case certAttrSubjCn:
                                {
                                    if (myCertInfo.subjCN == anAttrVal)
                                        break;
                                    else
                                        continue;
                                }
                                case certAttrSha1Finterprint:
                                {
                                    if (myCertInfo.sha1Fingerprint == anAttrVal)
                                        break;
                                    else
                                        continue;
                                }
                                default:
                                    continue;
                                }

                                DEBUGLOG(boost::format("Removing cert having %s %s") % str(aCertAttr) % anAttrVal);
                                fs::remove_all(p);
                                effectuateRemovedCertsInStore();
                                ++myNumFilesRemoved;
                            }
                            catch (std::exception& e)
                            {
                                if (anErrorPolicy == proceedOnError)
                                {
                                    WARNLOG2(boost::format("Failed to remove cert having %s %s") % str(aCertAttr) % anAttrVal, e.what());
                                }
                                else
                                {
                                    throw;
                                }
                            }
                        }
                    }
                    DEBUGLOG(boost::format("Deleted %d certificate(s) from %s store having %s %s") % myNumFilesRemoved % str(theStoreType) % str(aCertAttr) % anAttrVal);
                    return myNumFilesRemoved;
                }

                //@return SHA1 fingerprint (uppercase hex) of the imported certificate
                //@throw NativeCertStoreImportError
                string importPfx(const Pfx& aPfx)
                {
                    DEBUGLOG("Importing Pfx");
                    if (theStoreType != storePersonal)
                    {
                        TA_THROW_MSG(NativeCertStoreImportError, "Personal store expected but " + str(theStoreType) + " store supplied to import Pfx certificate");
                    }
                    if (theReadOnly)
                    {
                        TA_THROW_MSG(NativeCertStoreImportError, "Cannot import certificate into the " + str(theStoreType) + " store because the store is opened read-only");
                    }

                    ta::ScopedResource<BIO*> myPfxMemBio(BIO_new(BIO_s_mem()), BIO_free);
                    const int myPfxSize = (int)aPfx.data.size();
                    const int myWritten = BIO_write(myPfxMemBio, ta::getSafeBuf(aPfx.data), myPfxSize);
                    if (myWritten != myPfxSize)
                    {
                        TA_THROW_MSG(NativeCertStoreImportError, boost::format("BIO_write failed trying to write %1% bytes of Pfx. Actuall written: %2% bytes.") % myPfxSize % myWritten);
                    }

                    ta::ScopedResource<PKCS12*> p12(d2i_PKCS12_bio(myPfxMemBio, NULL), PKCS12_free);
                    if (!p12)
                    {
                        TA_THROW_MSG(NativeCertStoreImportError, boost::format("Error reading PKCS#12 package. %s") % ERR_error_string(ERR_get_error(), NULL));
                    }

                    EVP_PKEY* myExtractedPrivKey = NULL;
                    X509* myExtractedCert = NULL;
                    STACK_OF(X509) *myExtractedCAs = NULL;
                    if (!PKCS12_parse(p12, aPfx.password.c_str(), &myExtractedPrivKey, &myExtractedCert, &myExtractedCAs))
                    {
                        TA_THROW_MSG(NativeCertStoreImportError, boost::format("Error parsing PKCS#12 package. %s") % ERR_error_string(ERR_get_error(), NULL));
                    }

                    ta::ScopedResource<EVP_PKEY*> scopedPkey(myExtractedPrivKey, EVP_PKEY_free); // just for RAII
                    ta::ScopedResource<X509*> scopedCert(myExtractedCert, X509_free); // just for RAII
                    ta::ScopedResource<STACK_OF(X509)*> scopedCa(myExtractedCAs, freeStackOf509);  // just for RAII

                    importCertKeyToPersonalStore(myExtractedCert, myExtractedPrivKey, myExtractedCAs);

                    const ta::CertUtils::CertInfo myCertInfo = ta::CertUtils::getCertInfo(myExtractedCert);
                    DEBUGLOG(boost::format("Successfully imported Pfx. Certificate CN %s, SHA1 fingerprint: %s") % myCertInfo.subjCN % myCertInfo.sha1Fingerprint);

                    return myCertInfo.sha1Fingerprint;
                }

                void importDerCertFileToTrustedStore(const string& aDerCertPath)
                {
                    DEBUGLOG(boost::format("Importing DER certificate from %s to %s store") % aDerCertPath % str(theStoreType));
                    const vector<unsigned char> myPemCert = ta::CertUtils::convDer2Pem(ta::readData(aDerCertPath));
                    importPemCertToTrustedStore(myPemCert);
                    DEBUGLOG(boost::format("Successfully imported DER certificate from %s to %s store") % aDerCertPath % str(theStoreType));
                }

                void importPemCertToTrustedStore(const std::vector<unsigned char>& aPemCert)
                {
                    DEBUGLOG(boost::format("Importing PEM certificate to %s store") % str(theStoreType));
                    if (theStoreType == storePersonal)
                    {
                        TA_THROW_MSG(NativeCertStoreImportError, "Trusted store expected but personal store supplied to import the certificate");
                    }
                    if (theReadOnly)
                    {
                        TA_THROW_MSG(NativeCertStoreImportError, "Cannot remove certificate from the " + str(theStoreType) + " store because the store is opened read-only");
                    }
                    const string myCertPath = makeNewCertKeyPath(theStoreType);
                    ta::writeData(myCertPath, aPemCert);
                    effectuateNewCertsInStore();
                    DEBUGLOG(boost::format("Successfully imported certificate to %s store") % str(theStoreType));
                }

            private:
                string getStoreDir() const
                {
                    return getStoreDir(theStoreType);
                }
                static string getStoreDir(const StoreType aStoreType)
                {
                    switch (aStoreType)
                    {
                    case storePersonal:
                        return str(boost::format("%s/keystore") % Settings::getUserConfigDir());
                    case storeIntermediate:
                    case storeRoot:
                        return "/etc/pki/ca-trust/source/anchors/";
                    default:
                        TA_THROW_MSG(NativeCertStoreError, "Unknown store type " + str(aStoreType));
                    }
                }

                static string genCertKeyPath(const StoreType aStoreType)
                {
                    static const string myCertFilePrefix = "keytalk_"; //to be easier distinguished from non-KeyTalk certs
                    switch (aStoreType)
                    {
                    case storePersonal:
                        return str(boost::format("%s/%s%s.pem") % getStoreDir(aStoreType) % myCertFilePrefix % ta::genUuid());
                    case storeIntermediate:
                    case storeRoot:
                        // .crt extension is required to be understood by 'update-ca-trust' utility
                        return str(boost::format("%s/%s%s.crt") % getStoreDir(aStoreType) % myCertFilePrefix % ta::genUuid());
                    default:
                        TA_THROW_MSG(NativeCertStoreError, "Unknown store type " + str(aStoreType));
                    }
                }

                static string makeNewCertKeyPath(const StoreType aStoreType)
                {
                    while (true)
                    {
                        const string myNewFilePath = genCertKeyPath(aStoreType);
                        if (!ta::isFileExist(myNewFilePath))
                        {
                            return myNewFilePath;
                        }
                        continue; // collision, try again
                    }
                }
                static void freeStackOf509(STACK_OF(X509) * ca)
                {
                    if (ca)
                    {
                        sk_X509_pop_free(ca, X509_free);
                    }
                }

                static void importCertKeyToPersonalStore(X509* aCert, EVP_PKEY* aPrivateKey, STACK_OF(X509) *anCa)
                {
                    const string myCertKeyPath = makeNewCertKeyPath(storePersonal);

                    {
                        ta::ScopedResource<FILE*> myCertKeyFile(fopen(myCertKeyPath.c_str(), "w"), fclose);

                        if (!myCertKeyFile)
                        {
                            TA_THROW_MSG(NativeCertStoreError, "Cannot open certkey file " + myCertKeyPath + " for writing PKCS#12");
                        }

                        PEM_write_X509(myCertKeyFile, aCert);
                        PEM_write_PrivateKey(myCertKeyFile, aPrivateKey, NULL, NULL, 0, NULL, NULL);

                        // append CAs if any
                        if (anCa && sk_X509_num(anCa))
                        {
                            for (int i = 0; i < sk_X509_num(anCa); ++i)
                            {
                                PEM_write_X509(myCertKeyFile, sk_X509_value(anCa, i));
                            }
                        }
                    }

                    // make user cert&key only readable by owner
                    if (chmod(myCertKeyPath.c_str(), 0400) != 0)
                    {
                        const string myChmodErrorStr = strerror(errno);
                        try {
                            fs::remove(myCertKeyPath.c_str());
                        } catch (std::exception& e) {
                            TA_THROW_MSG(NativeCertStoreError, boost::format("Failed to set permissions on the imported personal certificate with private key at %s. %s. Furthermore the file cannot be removed as well. %s") % myCertKeyPath % myChmodErrorStr % e.what());
                        }
                        TA_THROW_MSG(NativeCertStoreError, boost::format("Failed to set permissions on the imported personal certificate with private key at %s. %s") % myCertKeyPath % myChmodErrorStr);
                    }

                    effectuateNewCertsInStore(storePersonal);
                }

		
                static void effectuateNewCertsInStore(const StoreType aStoreType)
                {
                    switch (aStoreType)
                    {
                    case storePersonal:
                        return; // nothing to do
                    case storeIntermediate:
                    case storeRoot:
                        return ta::Process::checkedShellExecSync("update-ca-trust");
                    default:
                        TA_THROW_MSG(NativeCertStoreError, "Unknown store type " + str(aStoreType));
                    }
                }
                void effectuateNewCertsInStore()
                {
                    effectuateNewCertsInStore(theStoreType);
                }
                void effectuateRemovedCertsInStore()
                {
                    switch (theStoreType)
                    {
                    case storePersonal:
                        return; // nothing to do
                    case storeIntermediate:
                    case storeRoot:
                        return ta::Process::checkedShellExecSync("update-ca-trust extract");
                    default:
                        TA_THROW_MSG(NativeCertStoreError, "Unknown store type " + str(theStoreType));
                    }
                }
            }; //Store
#endif // _WIN32

            // Cleanup settings for current service/provider by removing cert fingerprints that do not exist any more in the personal store
            void cleanupImportedUserCertFingerpintsInSettings()
            {
                const ta::StringArray myOrigCertFingerprints = Settings::getImportedUserCertFingerprints();

                const Store myStore(storePersonal);
                const ta::StringArray myFoundCertFingerprints = myStore.findCertsByFingerprints(myOrigCertFingerprints);

                ta::StringArray myCertFingerprintsToRemove;
                foreach (const string& fingerprint, myOrigCertFingerprints)
                {
                    if (!isElemExist(fingerprint, myFoundCertFingerprints))
                    {
                        myCertFingerprintsToRemove.push_back(fingerprint);
                    }
                }

                Settings::removeImportedUserCertFingerprints(myCertFingerprintsToRemove);
            }

        } // private API


        //
        // Public API
        //

        unsigned int validateReseptUserCert()
        {
            try
            {
                cleanupImportedUserCertFingerpintsInSettings();

                const string myServiceName = Settings::getLatestService();
                const ta::StringArray myCertFingerprints = Settings::getImportedUserCertFingerprints();

                Store myStore(storePersonal);
                return myStore.findValidCerts(myCertFingerprints, myServiceName);
            }
            catch (NativeCertStoreValidateError&)
            {
                throw;
            }
            catch (std::exception& e)
            {
                TA_THROW_MSG(NativeCertStoreValidateError, e.what());
            }
        }

        unsigned int deleteAllReseptUserCerts()
        {
            try
            {
                cleanupImportedUserCertFingerpintsInSettings();

                const string myServiceName = Settings::getLatestService();
                const ta::StringArray myCertFingerprints = Settings::getImportedUserCertFingerprints();

                Store myStore(storePersonal);
                const ta::StringArray myRemovedCertsSha1Fingerprints = myStore.removeCertKeys(myCertFingerprints, certsRemoveAll, myServiceName);
                Settings::removeImportedUserCertFingerprints(myRemovedCertsSha1Fingerprints);
                return myRemovedCertsSha1Fingerprints.size();

            }
            catch (NativeCertStoreDeleteError&)
            {
                throw;
            }
            catch (std::exception& e)
            {
                TA_THROW_MSG(NativeCertStoreDeleteError, e.what());
            }
        }

        unsigned int deleteInvalidReseptUserCerts()
        {
            try
            {
                cleanupImportedUserCertFingerpintsInSettings();

                const string myServiceName = Settings::getLatestService();
                const ta::StringArray myCertFingerprints = Settings::getImportedUserCertFingerprints();

                Store myStore(storePersonal);
                const ta::StringArray myRemovedCertsSha1Fingerprints = myStore.removeCertKeys(myCertFingerprints, certsRemoveInvalid, myServiceName);
                Settings::removeImportedUserCertFingerprints(myRemovedCertsSha1Fingerprints);
                return myRemovedCertsSha1Fingerprints.size();

            }
            catch (NativeCertStoreDeleteError&)
            {
                throw;
            }
            catch (std::exception& e)
            {
                TA_THROW_MSG(NativeCertStoreDeleteError, e.what());
            }
        }

        unsigned int deleteUserCertsForIssuerCN(const string& anIssuerCn, const ErrorPolicy anErrorPolicy)
        {
            try
            {
                Store myStore(storePersonal);
                return myStore.removeCertKeysByAttr(certAttrIssuerCn, anIssuerCn, anErrorPolicy);
            }
            catch (NativeCertStoreDeleteError&)
            {
                throw;
            }
            catch (std::exception& e)
            {
                TA_THROW_MSG(NativeCertStoreDeleteError, e.what());
            }
        }

        string importPfx(const Pfx& aPfx)
        {
            try
            {
                cleanupImportedUserCertFingerpintsInSettings();
#ifdef _WIN32
                const string myCertFingerprint = importPfxWin32(aPfx);
#else
                const string myCertFingerprint = Store(storePersonal).importPfx(aPfx);
#endif
                Settings::addImportedUserCertFingerprint(myCertFingerprint);
                return myCertFingerprint;
            }
            catch (NativeCertStoreError&)
            {
                throw;
            }
            catch (std::exception& e)
            {
                TA_THROW_MSG(NativeCertStoreDeleteError, e.what());
            }
        }

        void installCAs(const string& aUcaDerPath,
                        const string& anScaDerPath,
                        const string& aPcaDerPath,
                        const string& anRcaDerPath,
                        const ta::StringArray& anExtraSigningCAsPemPaths)
        {
            using ta::CertUtils::getCertInfo;
            using ta::CertUtils::getCertInfoFile;

            try
            {
                //@note We remove existing RESEPT certs first because of peculiarity of Windows cert store API:
                // ::CertAddCertificateContextToStore() with CERT_STORE_ADD_REPLACE_EXISTING flag sometimes gives access denied when 2 copies of the same cert present in the same logical but different physical stores
                const string myUcaCn = getCertInfoFile(aUcaDerPath, ta::CertUtils::DER).subjCN;
                const string myScaCn = getCertInfoFile(anScaDerPath, ta::CertUtils::DER).subjCN;
                const string myPcaCn = getCertInfoFile(aPcaDerPath, ta::CertUtils::DER).subjCN;
                const bool myIsRcaExist = !anRcaDerPath.empty();
                const string myRcaCn = myIsRcaExist ? getCertInfoFile(anRcaDerPath, ta::CertUtils::DER).subjCN : "";

                // remove KeyTalk certs starting from child towards parent
                deleteFromIntermediateStoreByCN(myUcaCn, failOnError);
                deleteFromIntermediateStoreByCN(myScaCn, failOnError);
                if (myIsRcaExist)
                {
                    deleteFromIntermediateStoreByCN(myPcaCn, failOnError);
                    deleteFromRootStoreByCN(myRcaCn, failOnError);
                }
                else
                {
                    deleteFromRootStoreByCN(myPcaCn, failOnError);
                }

                // import KeyTalk certs starting with parent towards children
                if (myIsRcaExist)
                {
                    Store(storeRoot).importDerCertFileToTrustedStore(anRcaDerPath);
                    rclient::Settings::addInstalledRootCA(myRcaCn);
                    Store(storeIntermediate).importDerCertFileToTrustedStore(aPcaDerPath);
                    rclient::Settings::addInstalledPrimaryCA(myPcaCn);
                }
                else
                {
                    Store(storeRoot).importDerCertFileToTrustedStore(aPcaDerPath);
                    rclient::Settings::addInstalledPrimaryCA(myPcaCn);
                }
                Store(storeIntermediate).importDerCertFileToTrustedStore(anScaDerPath);
                rclient::Settings::addInstalledServerCA(myScaCn);
                Store(storeIntermediate).importDerCertFileToTrustedStore(aUcaDerPath);
                rclient::Settings::addInstalledUserCA(myUcaCn);

                // import extra signing CAs
                foreach (const string& path, anExtraSigningCAsPemPaths)
                {
                    vector<vector<unsigned char> > myCAs = ta::CertUtils::extractPemCertsFromFile(path);
                    // CAs we receive are ordered from child to parent, but import should happen in reverse direction
                    std::reverse(myCAs.begin(), myCAs.end());
                    foreach (const vector<unsigned char>& ca, myCAs)
                    {
                        const string mySha1Fingerprint = getCertInfo(ca).sha1Fingerprint;
                        if (ta::CertUtils::isSelfSignedCert(ca))
                        {
                            // root CA should go to the root store
                            DEBUGLOG("Importing extra signing CA with sha1 fingerprint " + mySha1Fingerprint + " to the root store");
                            Store(storeRoot).importPemCertToTrustedStore(ca);
                            rclient::Settings::addInstalledExtraSigningRootCA(mySha1Fingerprint);
                        }
                        else
                        {
                            // intermediate CA should go to the intermediate store
                            DEBUGLOG("Importing extra signing CA with sha1 fingerprint " + mySha1Fingerprint + " to the intermediate store");
                            Store(storeIntermediate).importPemCertToTrustedStore(ca);
                            rclient::Settings::addInstalledExtraSigningIntCA(mySha1Fingerprint);
                        }
                    }
                }
            }
            catch (NativeCertStoreError&)
            {
                throw;
            }
            catch (std::exception& e)
            {
                TA_THROW_MSG(NativeCertStoreError, e.what());
            }
        }

        void getInstalledCAs(ta::StringArray& aUCAs,
                             ta::StringArray& anSCAs,
                             ta::StringArray& aPCAs,
                             ta::StringArray& anRCAs,
                             ta::StringArray& anExtraSigningCAs)
        {
            aUCAs.clear();
            anSCAs.clear();
            aPCAs.clear();
            anRCAs.clear();
            anExtraSigningCAs.clear();

            // open stores with minimal sufficient privileges
            const bool myReadOnly = true;
            const Store myRootStore(storeRoot, myReadOnly);
            const Store myIntStore(storeIntermediate, myReadOnly);

            foreach (const string& cn, Settings::getInstalledUserCaCNs())
            {
                if (const boost::optional<string> pem = myIntStore.findFirstCertByAttr(certAttrSubjCn, cn))
                {
                    aUCAs.push_back(*pem);
                }
            }
            foreach (const string& cn, Settings::getInstalledServerCaCNs())
            {
                if (const boost::optional<string> pem = myIntStore.findFirstCertByAttr(certAttrSubjCn, cn))
                {
                    anSCAs.push_back(*pem);
                }
            }
            foreach (const string& cn, Settings::getInstalledPrimaryCaCNs())
            {
                if (const boost::optional<string> pem = myRootStore.findFirstCertByAttr(certAttrSubjCn, cn))
                {
                    aPCAs.push_back(*pem);
                }
            }
            foreach (const string& cn, Settings::getInstalledRootCaCNs())
            {
                if (const boost::optional<string> pem = myRootStore.findFirstCertByAttr(certAttrSubjCn, cn))
                {
                    anRCAs.push_back(*pem);
                }
            }
            foreach (const string& fp, Settings::getInstalledExtraSigningIntCaSha1Fingerprints())
            {
                if (const boost::optional<string> pem = myIntStore.findFirstCertByAttr(certAttrSha1Finterprint, fp))
                {
                    anExtraSigningCAs.push_back(*pem);
                }
            }
            foreach (const string& fp, Settings::getInstalledExtraSigningRootCaSha1Fingerprints())
            {
                if (const boost::optional<string> pem = myRootStore.findFirstCertByAttr(certAttrSha1Finterprint, fp))
                {
                    anExtraSigningCAs.push_back(*pem);
                }
            }
        }

        unsigned int deleteFromRootStoreByCN(const string& aSubjCN, const ErrorPolicy anErrorPolicy)
        {
            Store myStore(storeRoot);
            return myStore.removeCertKeysByAttr(certAttrSubjCn, aSubjCN, anErrorPolicy);
        }
        unsigned int deleteFromIntermediateStoreByCN(const string& aSubjCN, ErrorPolicy anErrorPolicy)
        {
            Store myStore(storeIntermediate);
            return myStore.removeCertKeysByAttr(certAttrSubjCn, aSubjCN, anErrorPolicy);
        }

        unsigned int deleteFromRootStoreByFingerprint(const string& aSha1Fingerprint, ErrorPolicy anErrorPolicy)
        {
            Store myStore(storeRoot);
            return myStore.removeCertKeysByAttr(certAttrSha1Finterprint, aSha1Fingerprint, anErrorPolicy);
        }

        unsigned int deleteFromIntermediateStoreByFingerprint(const string& aSha1Fingerprint, ErrorPolicy anErrorPolicy)
        {
            Store myStore(storeIntermediate);
            return myStore.removeCertKeysByAttr(certAttrSha1Finterprint, aSha1Fingerprint, anErrorPolicy);
        }

#ifdef _WIN32
        ta::StringArray getStoreNames()
        {
            return Store::getStoreNames();
        }

        bool isStoreExists(const std::string& aStoreName)
        {
            return ta::isElemExist(aStoreName, getStoreNames());
        }
#endif
    }// NativeCertStore
}// rclient

