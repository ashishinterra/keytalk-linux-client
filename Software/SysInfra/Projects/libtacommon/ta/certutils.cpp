#include "certutils.h"
#include "rsautils.h"
#include "strings.h"
#include "process.h"
#include "netutils.h"
#include "utils.h"
#include "url.h"
#include "scopedresource.hpp"
#include "opensslwrappers.h"
#include "common.h"
#include "ta/logger.h"

#ifdef _WIN32
#include <Ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

#include "openssl/x509.h"
#include "openssl/x509v3.h"
#include "openssl/err.h"
#include "openssl/pem.h"
#include "openssl/evp.h"
#include "openssl/objects.h"
#include "openssl/obj_mac.h"// for NIDs
#include "openssl/bio.h"
#include "openssl/pkcs12.h"
#include "openssl/crypto.h"

#include "boost/regex.hpp"
#include "boost/algorithm/string.hpp"
#include <cstdio>
#include <cassert>
#include <memory>
#include <iostream>

using std::string;
using std::vector;
using boost::assign::list_of;
using ta::Process::ScopedDir;


namespace ta
{
    namespace CertUtils
    {
        // Internal API
        namespace
        {
            const char X509KeyUsageSep = ',';
            const char X509CertPoliciesSep = ',';
            const string X509_CRL_URI_Prefix = "URI:";
            const string X509_OCSP_URI_Prefix = "OCSP;URI:";

            //
            // Convert ASN1_TIME to UCT time_t. Adapted from http://www.mail-archive.com/openssl-users@openssl.org/msg33365.html
            //
            time_t _getUtcTimeFromASN1(const ASN1_TIME* aTime)
            {
                if (!aTime)
                {
                    TA_THROW_MSG(std::invalid_argument, "ASN1_TIME is NULL");
                }

                char   buffer[24] = {0};
                char*   pBuffer   = buffer;
                char*   pString   = (char*)aTime->data;
                const size_t timeLength = aTime->length;

                if (aTime->type == V_ASN1_UTCTIME)
                {
                    if ((timeLength < 11) || (timeLength > 17))
                        TA_THROW_MSG(std::runtime_error, boost::format("Failed to convert time from V_ASN1_UTCTIME. Incorrect length: %1%") % timeLength);

                    memcpy(pBuffer, pString, 10);
                    pBuffer += 10;
                    pString += 10;
                }
                else
                {
                    if (timeLength < 13)
                        TA_THROW_MSG(std::runtime_error, boost::format("Failed to convert time from ASN1. Incorrect length: %1%") % timeLength);

                    memcpy(pBuffer, pString, 12);
                    pBuffer += 12;
                    pString += 12;
                }

                if ((*pString == 'Z') || (*pString == '-') || (*pString == '+'))
                {
                    *(pBuffer++) = '0';
                    *(pBuffer++) = '0';
                }
                else
                {
                    *(pBuffer++) = *(pString++);
                    *(pBuffer++) = *(pString++);
                    // Skip any fractional seconds...
                    if (*pString == '.')
                    {
                        pString++;
                        while ((*pString >= '0') && (*pString <= '9'))
                        {
                            pString++;
                        }
                    }
                }

                *(pBuffer++) = 'Z';
                *(pBuffer++) = '\0';

                time_t secondsFromUCT = 0;

                if (*pString == 'Z')
                {
                    secondsFromUCT = 0;
                }
                else
                {
                    if ((*pString != '+') && (pString[5] != '-'))
                        TA_THROW_MSG(std::runtime_error,"Failed to convert time from ASN1. Incorrect format");

                    secondsFromUCT = ((pString[1]-'0') * 10 + (pString[2]-'0')) * 60;
                    secondsFromUCT += (pString[3]-'0') * 10 + (pString[4]-'0');
                    if (*pString == '-')
                    {
                        secondsFromUCT = -secondsFromUCT;
                    }
                }

                tm myTime = {0};
                myTime.tm_sec  = ((buffer[10] - '0') * 10) + (buffer[11] - '0');
                time_t now = ::time (NULL);
                myTime.tm_sec += (int)(mktime (localtime (&now)) - mktime (gmtime (&now)));
                myTime.tm_min  = ((buffer[8] - '0') * 10) + (buffer[9] - '0');
                myTime.tm_hour = ((buffer[6] - '0') * 10) + (buffer[7] - '0');
                myTime.tm_mday = ((buffer[4] - '0') * 10) + (buffer[5] - '0');
                myTime.tm_mon  = (((buffer[2] - '0') * 10) + (buffer[3] - '0')) - 1;
                myTime.tm_year = ((buffer[0] - '0') * 10) + (buffer[1] - '0');

                if (myTime.tm_year < 50)
                {
                    myTime.tm_year += 100; // RFC 2459
                }
                myTime.tm_wday = 0;
                myTime.tm_yday = 0;
                myTime.tm_isdst = 0;

                time_t result = mktime(&myTime);
                if (result == (time_t)-1)
                {
                    TA_THROW_MSG(std::runtime_error,"Failed to convert time from ASN1. mktime failed");
                }

                result += secondsFromUCT;
                return result;
            }

            // format certificate serial by separating bytes with ':' and lowercasing it
            string fmtCertSerial(const string& aSerialHex)
            {
                string mySerial;
                const size_t mySerialHexLen = aSerialHex.size();
                for (size_t i=0; i < mySerialHexLen; ++i)
                {
                    mySerial += aSerialHex[i];
                    if (i % 2 && i != mySerialHexLen-1)
                        mySerial += ":";
                }
                return boost::to_lower_copy(mySerial);
            }

            bool isKeyUsageExists(X509& aCert, KeyUsage aKeyUsage)
            {
                ScopedResource<ASN1_BIT_STRING*> b_asn ((ASN1_BIT_STRING*)X509_get_ext_d2i(&aCert, NID_key_usage, NULL, NULL), ASN1_BIT_STRING_free);
                if (!b_asn)
                {
                    return false; // no key usage extension
                }
                const bool myRetVal = !!(ASN1_BIT_STRING_get_bit(b_asn, (unsigned int)aKeyUsage));
                return myRetVal;
            }

            vector<ExtendedKeyUsage> getExtendedKeyUsage(X509& aCert)
            {
                vector<ExtendedKeyUsage> myEKUs;

                int loc = X509_get_ext_by_NID(&aCert, NID_ext_key_usage, -1);
                if (loc < 0) // No EKU exists
                {
                    return myEKUs;
                }
                X509_EXTENSION* ext = X509_get_ext(&aCert, loc);
                if (!ext)
                {
                    return myEKUs; // just tolerate
                }
                // get the extendedKeyUsage
                EXTENDED_KEY_USAGE *eku = static_cast<EXTENDED_KEY_USAGE*>(X509V3_EXT_d2i(ext));
                if (!eku)
                {
                    return myEKUs; // just tolerate
                }
                for (int n = 0; n < sk_ASN1_OBJECT_num(eku); ++n)
                {
                    ASN1_OBJECT *obj = sk_ASN1_OBJECT_value(eku, n);
                    int nid = OBJ_obj2nid(obj);
                    if(nid == NID_undef)
                    {
                        continue;
                    }

                    switch(nid)
                    {
                    case NID_client_auth:
                        myEKUs.push_back(ekuClientAuth);
                        break;
                    case NID_server_auth:
                        myEKUs.push_back(ekuServerAuth);
                        break;
                    case NID_email_protect:
                        myEKUs.push_back(ekuSecureEmail);
                        break;
                    case NID_anyExtendedKeyUsage:
                        myEKUs.push_back(ekuAnyExtendedKeyUsage);
                    default:
                        break;
                    }
                }

                EXTENDED_KEY_USAGE_free(eku);
                return myEKUs;
            }

            // do a best guess whether the given buffer contains PEM-encoded CRL
            bool isPemCrl(const vector<unsigned char>& aBuf)
            {
                const string myBufAsStr = vec2Str(aBuf);
                return (myBufAsStr.find("-----BEGIN X509 CRL-----") != string::npos)
                       && (myBufAsStr.find("-----END X509 CRL-----") != string::npos);
            }

            // do a best guess whether the given buffer contains PEM-encoded X.509 certificate
            bool isPemCert(const vector<unsigned char>& aBuf)
            {
                const string myBufAsStr = vec2Str(aBuf);
                return (myBufAsStr.find("-----BEGIN CERTIFICATE-----") != string::npos)
                       && (myBufAsStr.find("-----END CERTIFICATE-----") != string::npos);
            }
            bool isPemCertFile(const string& aPath)
            {
                return isPemCert(ta::readData(aPath));
            }

            ta::StringArray parsePemCerts(const string& aPem)
            {
                ta::StringArray myCerts;

                //@notice we cannot deal with certs marked with BEGIN TRUSTED CERTIFICATE and BEGIN X509 CERTIFICATE so we do not parse them
                static const string BeginBeacon = "-----BEGIN CERTIFICATE-----";
                static const string EndBeacon = "-----END CERTIFICATE-----";
                const string myRegexStr = str(boost::format("%s[A-Za-z0-9\\+\\/\\=\\r\\n]+?%s") % ta::regexEscapeStr(BeginBeacon) % ta::regexEscapeStr(EndBeacon));
                boost::regex myRegex(myRegexStr);

                for (boost::sregex_token_iterator it(aPem.begin(), aPem.end(), myRegex), end; it != end; ++it)
                {
                    myCerts.push_back(*it);
                }

                return myCerts;
            }

            ta::StringArray parsePemPrivKeys(const string& aPem)
            {
                ta::StringArray myKeys;

                static const string BeginBeacon = "-----BEGIN ([A-Z ]*?)PRIVATE KEY-----";
                static const string EndBeacon = "-----END \\1PRIVATE KEY-----";
                const string myRegexStr = str(boost::format("%s.+?%s") % BeginBeacon % EndBeacon);
                boost::regex myRegex(myRegexStr);

                for (boost::sregex_token_iterator it(aPem.begin(), aPem.end(), myRegex), end; it != end; ++it)
                {
                    myKeys.push_back(*it);
                }

                return myKeys;
            }

            ta::StringArray parsePemEncryptedPrivKeys(const string& aPem)
            {
                ta::StringArray myKeys;

                static const boost::regex myKeyRegex("-----BEGIN ([A-Z ]*?)PRIVATE KEY-----.+?-----END \\1PRIVATE KEY-----");
                static const boost::regex myEncryptedKeyRegex("^Proc-Type\\:\\s+4,ENCRYPTED$");

                for (boost::sregex_token_iterator it(aPem.begin(), aPem.end(), myKeyRegex), end; it != end; ++it)
                {
                    const string myKey = *it;
                    if (regex_search(myKey, myEncryptedKeyRegex))
                    {
                        myKeys.push_back(myKey);
                    }
                }
                return myKeys;
            }

            ta::StringArray parsePemPubKeys(const string& aPem)
            {
                ta::StringArray myKeys;

                const boost::regex myRegex("-----BEGIN (RSA |)PUBLIC KEY-----[A-Za-z0-9\\+\\/\\=\\r\\n]+?-----END \\1PUBLIC KEY-----");

                for (boost::sregex_token_iterator it(aPem.begin(), aPem.end(), myRegex), end; it != end; ++it)
                {
                    myKeys.push_back(*it);
                }

                return myKeys;
            }

            struct ScopedP12Info
            {
                ScopedP12Info(EVP_PKEY* aPkey, X509* aCert, STACK_OF(X509) *aCa)
                    : pkey(aPkey), cert(aCert), ca(aCa)
                {}
                ~ScopedP12Info()
                {
                    EVP_PKEY_free(pkey);
                    X509_free(cert);
                    if (ca)
                    {
                        sk_X509_pop_free(ca, X509_free);
                    }
                }

                EVP_PKEY* pkey;
                X509* cert;
                STACK_OF(X509) *ca;
            };

            string convPrivKey2Pem(EVP_PKEY* aKey)
            {
                if (!aKey)
                {
                    TA_THROW_MSG(std::invalid_argument, boost::format("Failed to extract private key from PKCS#12 package. PEM_write_bio_PrivateKey failed. %s") % ERR_error_string(ERR_get_error(), NULL));
                }
                ta::ScopedResource<BIO*> myPrivKeyPemMemBio( BIO_new(BIO_s_mem()), BIO_free);
                static char pass[] = ""; // no password
                if (!PEM_write_bio_PrivateKey(myPrivKeyPemMemBio, aKey, NULL, NULL, 0, 0, pass))
                {
                    TA_THROW_MSG(std::runtime_error, boost::format("Failed to convert private key. PEM_write_bio_PrivateKey failed. %s") % ERR_error_string(ERR_get_error(), NULL));
                }
                BUF_MEM* myPrivKeyBuf = NULL;
                if (BIO_get_mem_ptr(myPrivKeyPemMemBio, &myPrivKeyBuf) < 0 || myPrivKeyBuf->length <= 0)
                {
                    TA_THROW_MSG(std::runtime_error, "BIO_get_mem_ptr failed");
                }
                return string(myPrivKeyBuf->data, myPrivKeyBuf->length);
            }

            void addSubjectEntry(X509_NAME* aSubject, int aNid, const string& aValue)
            {
                if (!aValue.empty())
                {
                    vector<unsigned char> myValue = str2Vec<unsigned char>(aValue); // to get rid of constness
                    if (1 != X509_NAME_add_entry_by_NID(aSubject, aNid, MBSTRING_UTF8, ta::getSafeBuf(myValue), (int)myValue.size(), -1, 0))
                    {
                        TA_THROW_MSG(std::runtime_error, boost::format("Could not add subject entry with NID %d and value '%s'") % aNid % aValue);
                    }
                }
            }

            void addExtToCsr(X509_REQ* aReq, const int aNid, const string& aValue)
            {
                vector<char> myValue = ta::str2Vec<char>(aValue);// conversion to please openssl demanding non-const char*
                X509_EXTENSION *ext = X509V3_EXT_conf_nid(NULL, NULL, aNid, ta::getSafeBuf(myValue));
                if (!ext)
                {
                    TA_THROW_MSG(std::invalid_argument, boost::format("Could not create extension with NID %d and value %s") % aNid % aValue);
                }

                STACK_OF(X509_EXTENSION) *exts = sk_X509_EXTENSION_new_null();
                sk_X509_EXTENSION_push(exts, ext);
                X509_REQ_add_extensions(aReq, exts);
                sk_X509_EXTENSION_pop_free(exts, X509_EXTENSION_free);
            }

            void addSAN(X509_REQ* aReq, const ta::StringArray& aSAN)
            {
                if (!aSAN.empty())
                {
                    addExtToCsr(aReq, NID_subject_alt_name, serializeSAN(aSAN));
                }
            }

            void addChallengePassword(X509_REQ* aReq, const string& aPassword)
            {
                if (!aPassword.empty())
                {
                    // challenge password should be in ASN printable string format
                    ScopedResource<ASN1_PRINTABLESTRING *> myAsn1Pass(ASN1_PRINTABLESTRING_new(), ASN1_PRINTABLESTRING_free);
                    const size_t myPassLen = aPassword.size();
                    ASN1_STRING_set(myAsn1Pass, (const unsigned char *)aPassword.c_str(), myPassLen);

                    const int ret = X509_REQ_add1_attr_by_NID(aReq, NID_pkcs9_challengePassword, myAsn1Pass->type, myAsn1Pass->data, myPassLen);
                    if (ret != 1)
                    {
                        TA_THROW_MSG(std::runtime_error, boost::format("Could not add challenge password to the CRL (error code: %d)") % ret);
                    }
                }
            }

            string asn1ToStr(ASN1_STRING *anAsn1Str)
            {
                unsigned char* mySzStr = NULL;
                const int myStrLen = ASN1_STRING_to_UTF8(&mySzStr, anAsn1Str);
                const string myStr((const char*)mySzStr, myStrLen);
                OPENSSL_free(mySzStr);
                return myStr;
            }

            string getSubjectName(X509& aX509Cert)
            {
                X509_NAME* myX509SubjName = X509_get_subject_name(&aX509Cert);
                if (!myX509SubjName)
                {
                    TA_THROW_MSG(std::runtime_error, "Failed to read X509 subject name from X509 cert");
                }
                char* mySubjName = X509_NAME_oneline(myX509SubjName, NULL, 0);
                if (!mySubjName)
                {
                    TA_THROW_MSG(std::runtime_error, "Failed to read subject name from X509 cert");
                }
                const string mySubjNameStr = mySubjName;
                OPENSSL_free(mySubjName);
                return mySubjNameStr;
            }

            string getSubjectCN(X509& aX509Cert)
            {
                X509_NAME* myX509SubjName = X509_get_subject_name(&aX509Cert);
                if (!myX509SubjName)
                {
                    TA_THROW_MSG(std::runtime_error, "Failed to read X509 subject name from X509 cert");
                }
                const int myCnIndex = X509_NAME_get_index_by_NID(myX509SubjName, NID_commonName, -1);
                if (myCnIndex < 0)
                {
                    TA_THROW_MSG(std::runtime_error, "Failed to read subject CN index from X509 cert");
                }
                X509_NAME_ENTRY* myCnEntry = X509_NAME_get_entry(myX509SubjName, myCnIndex);
                if (!myCnEntry)
                {
                    TA_THROW_MSG(std::runtime_error, "Failed to read subject CN from X509 cert");
                }
                return asn1ToStr(X509_NAME_ENTRY_get_data(myCnEntry));
            }

            string getSubjectO(X509& aX509Cert)
            {
                X509_NAME* myX509SubjName = X509_get_subject_name(&aX509Cert);
                if (!myX509SubjName)
                {
                    TA_THROW_MSG(std::runtime_error, "Failed to read X509 subject O from X509 cert");
                }
                const int myCnIndex = X509_NAME_get_index_by_NID(myX509SubjName, NID_organizationName, -1);
                if (myCnIndex < 0)
                {
                    return ""; // O is not mandatory
                }
                X509_NAME_ENTRY* myCnEntry = X509_NAME_get_entry(myX509SubjName, myCnIndex);
                if (!myCnEntry)
                {
                    TA_THROW_MSG(std::runtime_error, "Failed to read subject O from X509 cert");
                }
                return asn1ToStr(X509_NAME_ENTRY_get_data(myCnEntry));
            }

            string getSubjectOU(X509& aX509Cert)
            {
                X509_NAME* myX509SubjName = X509_get_subject_name(&aX509Cert);
                if (!myX509SubjName)
                {
                    TA_THROW_MSG(std::runtime_error, "Failed to read X509 subject OU from X509 cert");
                }
                const int myCnIndex = X509_NAME_get_index_by_NID(myX509SubjName, NID_organizationalUnitName, -1);
                if (myCnIndex < 0)
                {
                    return ""; // OU is not mandatory
                }
                X509_NAME_ENTRY* myCnEntry = X509_NAME_get_entry(myX509SubjName, myCnIndex);
                if (!myCnEntry)
                {
                    TA_THROW_MSG(std::runtime_error, "Failed to read subject OU from X509 cert");
                }
                return asn1ToStr(X509_NAME_ENTRY_get_data(myCnEntry));
            }

            string getIssuerName(X509& aX509Cert)
            {
                X509_NAME* myX509IssuerName = X509_get_issuer_name(&aX509Cert);
                if (!myX509IssuerName)
                {
                    TA_THROW_MSG(std::runtime_error, "Failed to read X509 issuer name from X509 cert");
                }
                char* myIssuerName = X509_NAME_oneline(myX509IssuerName, NULL, 0);
                if (!myIssuerName)
                {
                    TA_THROW_MSG(std::runtime_error, "Failed to read issuer name from X509 cert");
                }
                const string myIssuerNameStr = myIssuerName;
                OPENSSL_free(myIssuerName);
                return myIssuerNameStr;
            }

            string getIssuerCN(X509& aX509Cert)
            {
                X509_NAME* myX509IssuerName = X509_get_issuer_name(&aX509Cert);
                if (!myX509IssuerName)
                {
                    TA_THROW_MSG(std::runtime_error, "Failed to read X509 issuer name from X509 cert");
                }
                const int myCnIndex = X509_NAME_get_index_by_NID(myX509IssuerName, NID_commonName, -1);
                if (myCnIndex < 0)
                {
                    TA_THROW_MSG(std::runtime_error, "Failed to read issuer CN index from X509 cert");
                }
                X509_NAME_ENTRY* myCnEntry = X509_NAME_get_entry(myX509IssuerName, myCnIndex);
                if (!myCnEntry)
                {
                    TA_THROW_MSG(std::runtime_error, "Failed to read issuer CN from X509 cert");
                }
                return asn1ToStr(X509_NAME_ENTRY_get_data(myCnEntry));
            }

            const EVP_MD* getDigest(const SignUtils::Digest aDigestType)
            {
                switch (aDigestType)
                {
                case SignUtils::digestSha1: return EVP_sha1();
                case SignUtils::digestSha256: return EVP_sha256();
                default: TA_THROW_MSG(std::runtime_error, boost::format("Unsupported digest type %1%") % str(aDigestType));
                }
            }

        } // unnamed ns


        //
        // Public API
        //

        CertInfo getCertInfo(X509* aX509Cert)
        {
            CertInfo myCertInfo;

            // Issuer
            myCertInfo.issuerName = getIssuerName(*aX509Cert);
            myCertInfo.issuerCN = getIssuerCN(*aX509Cert);

            // Subject
            myCertInfo.subjName = getSubjectName(*aX509Cert);
            myCertInfo.subjCN = getSubjectCN(*aX509Cert);
            myCertInfo.subjO = getSubjectO(*aX509Cert);
            myCertInfo.subjOU = getSubjectOU(*aX509Cert);


            // Serial number
            ASN1_INTEGER *myAsnSerial = X509_get_serialNumber(aX509Cert);
            if (!myAsnSerial)
            {
                TA_THROW_MSG(std::runtime_error, "Failed to read serial number from X509 cert");
            }
            BIGNUM *myBnSerial = ASN1_INTEGER_to_BN(myAsnSerial, NULL);
            char* mySerialHex = BN_bn2hex(myBnSerial);
            myCertInfo.serial = fmtCertSerial(mySerialHex);
            OPENSSL_free(mySerialHex);
            BN_free(myBnSerial);

            // Validity
            ASN1_INTEGER* myAsn1Time = X509_get_notBefore(aX509Cert);
            if (!myAsn1Time)
            {
                TA_THROW_MSG(std::runtime_error, "Failed to read 'time not before' from X509 cert");
            }
            myCertInfo.utcNotBefore = _getUtcTimeFromASN1(myAsn1Time);

            myAsn1Time = X509_get_notAfter(aX509Cert);
            if (!myAsn1Time)
            {
                TA_THROW_MSG(std::runtime_error, "Failed to read 'time not after' from X509 cert");
            }
            myCertInfo.utcNotAfter = _getUtcTimeFromASN1(myAsn1Time);

            // Key usage
            for (int ku = _firstKeyUsage; ku <= _lastKeyUsage; ++ku)
            {
                const KeyUsage myKu = static_cast<KeyUsage>(ku);
                if (isKeyUsageExists(*aX509Cert, myKu))
                {
                    myCertInfo.keyUsage.push_back(myKu);
                }
            }

            // Extended key usage
            myCertInfo.extKeyUsage = getExtendedKeyUsage(*aX509Cert);

            // Digest
            unsigned int md_size;
            unsigned char md[EVP_MAX_MD_SIZE]= {};
            if (X509_digest(aX509Cert, EVP_sha1(), md, &md_size) != 1)
                TA_THROW_MSG(std::runtime_error, "Failed to calculate sha1 digest for X509 cert");
            myCertInfo.sha1Fingerprint = Strings::toHex(md, md_size);

            // Signature algorithm
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
            const X509_ALGOR* sigalg = X509_get0_tbs_sigalg(aX509Cert);
#else
            const X509_ALGOR* sigalg = aX509Cert->sig_alg;
#endif
            myCertInfo.signatureAlgorithm.nid = OBJ_obj2nid(sigalg->algorithm);
            char mySignAlgoBuf[128] = {};
            if (i2t_ASN1_OBJECT(mySignAlgoBuf, sizeof(mySignAlgoBuf)-1, sigalg->algorithm) <= 0)
            {
                TA_THROW_MSG(std::runtime_error, "Failed to retrieve signature algorithm for X509 cert");
            }
            myCertInfo.signatureAlgorithm.name = mySignAlgoBuf;

            // Pubkey type
            ta::ScopedResource<EVP_PKEY*> myPubKey(X509_get_pubkey(aX509Cert), EVP_PKEY_free);
            if (!myPubKey)
            {
                TA_THROW_MSG(std::runtime_error, "X509_get_pubkey failed");
            }
            const int myPubKeyType = EVP_PKEY_base_id(myPubKey);
            switch (myPubKeyType)
            {
            case EVP_PKEY_RSA:
            {
                myCertInfo.pubKeyType = keyRsa;
                ScopedResource<RSA*> myPubKeyRsa(EVP_PKEY_get1_RSA(myPubKey), RSA_free);
                myCertInfo.pubKeyBits = 8*RSA_size(myPubKeyRsa);
                break;
            }
            case EVP_PKEY_DSA:
            {
                myCertInfo.pubKeyType = keyDsa;
                ScopedResource<DSA*> myPubKeyDsa(EVP_PKEY_get1_DSA(myPubKey), DSA_free);
                myCertInfo.pubKeyBits = 8*DSA_size(myPubKeyDsa);
                break;
            }
            case EVP_PKEY_EC:
            {
#ifdef OPENSSL_NO_EC
                TA_THROW_MSG(std::runtime_error, "Elliptic Key cryptography is not supported by OpenSSL");
#else
                myCertInfo.pubKeyType = keyEc;
                ta::ScopedResource<EC_KEY*> ec_params(EVP_PKEY_get1_EC_KEY(myPubKey), EC_KEY_free);
                myCertInfo.pubKeyBits = EC_GROUP_get_degree(EC_KEY_get0_group(ec_params));
                break;
#endif
            }
            default:  TA_THROW_MSG(std::runtime_error, boost::format("Unexpected certificate pubkey type %d") % myPubKeyType);
            }

            // Check whether the cert is CA by reading basic constraints extension
            ScopedResource<BASIC_CONSTRAINTS*> bc ((BASIC_CONSTRAINTS*)X509_get_ext_d2i(aX509Cert, NID_basic_constraints, NULL, NULL), BASIC_CONSTRAINTS_free);
            if (bc && bc->ca)
            {
                myCertInfo.basicConstraints.isCA = true;

                if (bc->pathlen)
                {
                    if (bc->pathlen->type == V_ASN1_NEG_INTEGER)
                    {
                        myCertInfo.basicConstraints.pathLen = 0;
                    }
                    else
                    {
                        myCertInfo.basicConstraints.pathLen = ASN1_INTEGER_get(bc->pathlen);
                    }
                }
                else
                {
                    myCertInfo.basicConstraints.pathLen = PathLenConstraintNone;
                }
            }

            // CRL
            ScopedResource<STACK_OF(DIST_POINT)*> dist_points((STACK_OF(DIST_POINT)*)X509_get_ext_d2i(aX509Cert, NID_crl_distribution_points, NULL, NULL),
                    CRL_DIST_POINTS_free);
            for (int i = 0; i < sk_DIST_POINT_num(dist_points); ++i)
            {
                DIST_POINT *dp = sk_DIST_POINT_value(dist_points, i);
                if (dp)
                {
                    DIST_POINT_NAME    *distpoint = dp->distpoint;
                    if (distpoint)
                    {
                        if (distpoint->type==0) // fullname GENERALIZEDNAME
                        {
                            for (int k = 0; k < sk_GENERAL_NAME_num(distpoint->name.fullname); k++)
                            {
                                GENERAL_NAME *gen = sk_GENERAL_NAME_value(distpoint->name.fullname, k);
                                if (gen->type == GEN_URI)
                                {
                                    const string myCrlUrl = asn1ToStr(gen->d.uniformResourceIdentifier);
                                    myCertInfo.crlDistributinPoints.push_back(myCrlUrl);
                                }
                            }
                        }
                        //@todo?
                        // else if (distpoint->type==1)//relativename X509NAME
                        // {
                        //     STACK_OF(X509_NAME_ENTRY) *sk_relname = distpoint->name.relativename;
                        //     for (int k = 0; k < sk_X509_NAME_ENTRY_num(sk_relname); k++)
                        //     {
                        //         X509_NAME_ENTRY *e = sk_X509_NAME_ENTRY_value(sk_relname, k);
                        //         ASN1_STRING *d = X509_NAME_ENTRY_get_data(e);
                        //         list.push_back( string( (char*)ASN1_STRING_data(d), ASN1_STRING_length(d) ) );
                        //     }
                        // }
                    }
                }
            }

            // OCSP
            ScopedResource<STACK_OF(OPENSSL_STRING)*> ocsp_list((STACK_OF(OPENSSL_STRING)*)X509_get1_ocsp(aX509Cert), X509_email_free);
            for (int i = 0; i < sk_OPENSSL_STRING_num(ocsp_list); i++)
            {
                myCertInfo.ocspUrls.push_back( string( sk_OPENSSL_STRING_value(ocsp_list, i) ) );
            }

            // Netscape Base URL extension
            int loc = X509_get_ext_by_NID(aX509Cert, NID_netscape_base_url, -1);
            if (loc > 0)
            {
                X509_EXTENSION* ext = X509_get_ext(aX509Cert, loc);
                ASN1_OCTET_STRING* octet_str = X509_EXTENSION_get_data(ext);
                if (octet_str == NULL)
                    TA_THROW_MSG(std::runtime_error, "Could not retrieve Netscape base url.");
                const unsigned char* octet_str_data = octet_str->data;
                if (octet_str_data == NULL)
                    TA_THROW_MSG(std::runtime_error, "Could not retrieve Netscape base url.");
                long xlen;
                int tag, xclass;
                int ret = ASN1_get_object(&octet_str_data, &xlen, &tag, &xclass, octet_str->length);

                if (ret & 0x80)
                    TA_THROW_MSG(std::runtime_error, "Could not retrieve Netscape base url.");
                if (tag != V_ASN1_IA5STRING)
                    TA_THROW_MSG(std::runtime_error, boost::format("Netscape base url is of type %d, expected %d.") % tag % V_ASN1_OCTET_STRING);

                myCertInfo.optionalExtensions[SN_netscape_base_url] = string((const char*)octet_str_data, xlen);
            }

            // Subject Alternative Names extensions
            ScopedResource<GENERAL_NAMES*> mySANs ((GENERAL_NAMES*)X509_get_ext_d2i(aX509Cert, NID_subject_alt_name, 0, 0 ), GENERAL_NAMES_free);
            if (mySANs)
            {
                ta::StringArray myParsedSANs;
                const int myNumOfSANs = sk_GENERAL_NAME_num(mySANs);
                for (int i = 0; i < myNumOfSANs; ++i)
                {
                    GENERAL_NAME* entry = sk_GENERAL_NAME_value(mySANs, i);
                    if (!entry)
                    {
                        continue;
                    }

                    if (entry->type == GEN_DNS || entry->type == GEN_EMAIL)
                    {
                        unsigned char* myDnsSANUtf8 = NULL;
                        const int len = ASN1_STRING_to_UTF8(&myDnsSANUtf8, entry->d.dNSName);
                        if (myDnsSANUtf8)
                        {
                            if (len == (int)strlen((const char*)myDnsSANUtf8)) // silently ignore mailformed SANs with embedded nulls
                            {
                                switch (entry->type)
                                {
                                case GEN_DNS:
                                    myParsedSANs.push_back(str(boost::format("DNS:%s") % (const char*)myDnsSANUtf8));
                                    break;
                                case GEN_EMAIL:
                                    myParsedSANs.push_back(str(boost::format("email:%s") % (const char*)myDnsSANUtf8));
                                    break;
                                default:
                                    break;
                                }

                            }
                            OPENSSL_free(myDnsSANUtf8);
                        }
                    }
                    else if (entry->type == GEN_IPADD)
                    {
                        if (entry->d.ip->length == 4)
                        {
                            char myIpSzBuf[INET_ADDRSTRLEN] = {};
                            inet_ntop(AF_INET, entry->d.ip->data, myIpSzBuf, sizeof(myIpSzBuf));
                            myParsedSANs.push_back(str(boost::format("IP:%s") % myIpSzBuf));
                        }
                        else if (entry->d.ip->length == 16)
                        {
                            char myIpSzBuf[INET6_ADDRSTRLEN] = {};
                            inet_ntop(AF_INET6, entry->d.ip->data, myIpSzBuf, sizeof(myIpSzBuf));
                            myParsedSANs.push_back(str(boost::format("IP:%s") % myIpSzBuf));
                        }
                        else
                        {
                            continue;
                        }
                    }
                }// for

                if (!myParsedSANs.empty())
                {
                    myCertInfo.optionalExtensions[SN_subject_alt_name] = ta::Strings::join(myParsedSANs, ",");
                }
            }

            return myCertInfo;
        }

        KeyUsage parseKeyUsage(const string& aKeyUsageStr)
        {
            for (int ku = _firstKeyUsage; ku <= _lastKeyUsage; ++ku)
            {
                const KeyUsage myKU = static_cast<KeyUsage>(ku);
                if (str(myKU) == boost::trim_copy(aKeyUsageStr))
                {
                    return myKU;
                }
            }
            TA_THROW_MSG(std::invalid_argument, boost::format("Cannot parse key usage from %s") % aKeyUsageStr);
        }

        string str(const KeyUsage aKeyUsage)
        {
            const X509V3_EXT_METHOD* method = X509V3_EXT_get_nid(NID_key_usage);
            if (!method || !method->usr_data)
            {
                TA_THROW_MSG(std::runtime_error, "Failed to query KeyUsage certificate extension info");
            }
            for (const BIT_STRING_BITNAME* ku = (const BIT_STRING_BITNAME*)method->usr_data; ku->sname; ++ku)
            {
                if (ku->bitnum == aKeyUsage)
                {
                    return ku->sname;
                }
            }
            TA_THROW_MSG(std::runtime_error, boost::format("Failed to query short name info for KeyUsage %d") % aKeyUsage);
        }
        string str_long(const KeyUsage aKeyUsage)
        {
            const X509V3_EXT_METHOD* method = X509V3_EXT_get_nid(NID_key_usage);
            if (!method || !method->usr_data)
            {
                TA_THROW_MSG(std::runtime_error, "Failed to query KeyUsage certificate extension info");
            }
            for (const BIT_STRING_BITNAME* ku = (const BIT_STRING_BITNAME*)method->usr_data; ku->lname; ++ku)
            {
                if (ku->bitnum == aKeyUsage)
                {
                    return ku->lname;
                }
            }
            TA_THROW_MSG(std::runtime_error, boost::format("Failed to query long name for KeyUsage %d") % aKeyUsage);
        }

        ta::StringArray strs(const vector<KeyUsage> aKeyUsages)
        {
            ta::StringArray myKeyUsageStrs;
            foreach (KeyUsage ku, aKeyUsages)
            {
                myKeyUsageStrs.push_back(str(ku));
            }
            return myKeyUsageStrs;
        }

        vector<KeyUsage> parseKeyUsages(const ta::StringArray& aKeyUsageStrs)
        {
            vector<KeyUsage> myKeyUsages;
            foreach (const string& ku, aKeyUsageStrs)
            {
                myKeyUsages.push_back(parseKeyUsage(ku));
            }
            return myKeyUsages;
        }

        CertInfo getCertInfoFile(const string& aCertFileName, CertEncoding aCertEnc)
        {
            ScopedResource<FILE*> myCertFp(fopen(aCertFileName.c_str(), "r"), fclose);
            if (!myCertFp)
                TA_THROW_MSG(std::runtime_error, boost::format("Failed to open '%1%'. Errno %2%") % aCertFileName % errno);
            X509* myX509Ptr = NULL;
            switch (aCertEnc)
            {
            case PEM:
                myX509Ptr = PEM_read_X509(myCertFp, NULL, NULL, NULL);
                if (!myX509Ptr)
                    TA_THROW_MSG(std::runtime_error, boost::format("Failed to read X509 PEM cert from '%1%'") % aCertFileName);
                break;
            case DER:
                myX509Ptr = d2i_X509_fp(myCertFp, NULL);
                if (!myX509Ptr)
                    TA_THROW_MSG(std::runtime_error, boost::format("Failed to read X509 DER cert from '%1%'") % aCertFileName);
                break;
            default:
                TA_THROW_MSG(std::runtime_error, boost::format("Unsupported certificate encoding '%1%'") % aCertEnc);
            }
            ScopedResource<X509*> myX509ScopedPtr(myX509Ptr, X509_free);
            CertInfo myRetVal = getCertInfo(myX509Ptr);
            return myRetVal;
        }

        CertInfo getCertInfo(const vector<unsigned char>& aCert, CertEncoding aCertEnc)
        {
            ta::OpenSSLCertificateWrapper myCert(getCertX509(aCert, aCertEnc));
            return getCertInfo(myCert);
        }

        CertInfo getCertInfo(const vector<char>& aCert, CertEncoding aCertEnc)
        {
            return getCertInfo(vector<unsigned char>(aCert.begin(), aCert.end()), aCertEnc);
        }

        CertInfo getCertInfo(const string& aCert, CertEncoding aCertEnc)
        {
            return getCertInfo(ta::str2Vec<unsigned char>(aCert), aCertEnc);
        }

        vector<MemBuffer> extractPemCertsFromFile(const string& aFilePath)
        {
            if (!isFileExist(aFilePath))
            {
                return vector<MemBuffer>();
            }

            const string pemData = readData(aFilePath);
            return extractPemCerts(pemData);
        }

        vector<MemBuffer> extractPemCerts(const string& aPemBuf)
        {
            vector<MemBuffer> certs;
            foreach (const string& certText, parsePemCerts(aPemBuf))
            {
                certs.push_back(str2Vec<unsigned char>(certText));
            }
            return certs;
        }

        vector<MemBuffer> extractPemCerts(const vector<unsigned char>& aPemBuf)
        {
            return extractPemCerts(vec2Str(aPemBuf));
        }

        vector<MemBuffer> extractPemPrivKeysFromFile(const string& aFilePath, KeyFilter aKeyFilter)
        {
            if (!isFileExist(aFilePath))
            {
                return vector<MemBuffer>();
            }

            const string pemData = readData(aFilePath);
            return extractPemPrivKeys(pemData, aKeyFilter);
        }

        vector<MemBuffer> extractPemPrivKeys(const string& aPemBuf, KeyFilter aKeyFilter)
        {
            vector<MemBuffer> keys;
            switch (aKeyFilter)
            {
            case keyFilterEncryptedOnly:
            {
                foreach (const string& keyText, parsePemEncryptedPrivKeys(aPemBuf))
                {
                    keys.push_back(str2Vec<unsigned char>(keyText));
                }
                return keys;
            }
            case keyFilterNotEncryptedOnly:
            {
                const ta::StringArray myEncrypted = parsePemEncryptedPrivKeys(aPemBuf);
                foreach (const string& keyText, parsePemPrivKeys(aPemBuf))
                {
                    if (!isElemExist(keyText, myEncrypted))
                    {
                        keys.push_back(str2Vec<unsigned char>(keyText));
                    }
                }
                return keys;
            }
            case keyFilterNone:
            {
                foreach (const string& keyText, parsePemPrivKeys(aPemBuf))
                {
                    keys.push_back(str2Vec<unsigned char>(keyText));
                }
                return keys;
            }
            default:
                TA_THROW_MSG(std::invalid_argument, boost::format("Unsupported key filter %d") % aKeyFilter);
            }
        }

        vector<CertInfo> getPemCertsInfoFile(const string& aFilePath)
        {
            vector<CertInfo> certInfos;

            vector<MemBuffer> certs = extractPemCertsFromFile(aFilePath);
            foreach (const MemBuffer& cert, certs)
            {
                certInfos.push_back(getCertInfo(cert, PEM));
            }

            return certInfos;
        }

        vector<CertInfo> getPemCertsInfo(const string& aPemBuf)
        {
            vector<CertInfo> certInfos;

            foreach (const string& certText, parsePemCerts(aPemBuf))
            {
                certInfos.push_back(getCertInfo(str2Vec<unsigned char>(certText), PEM));
            }

            return certInfos;
        }
        vector<CertInfo> getPemCertsInfo(const vector<unsigned char>& aPemBuf)
        {
            return getPemCertsInfo(vec2Str(aPemBuf));
        }


        X509* getCertX509(const vector<unsigned char>& aCert, CertEncoding aCertEnc)
        {
            if (aCert.empty())
            {
                TA_THROW_MSG(std::runtime_error, "Invalid certificate (empty)");
            }
            ScopedResource<BIO*> myMemBio( BIO_new(BIO_s_mem()), BIO_free);
            const int mySize = (int)aCert.size();
            const int myWritten = BIO_write(myMemBio, ta::getSafeBuf(aCert), mySize);
            if (myWritten != mySize)
                TA_THROW_MSG(std::runtime_error, boost::format("BIO_write failed trying to write %1% bytes of X509. Actually written: %2% bytes.") % mySize % myWritten);

            X509* myX509Ptr = NULL;
            switch (aCertEnc)
            {
            case PEM:
                myX509Ptr = PEM_read_bio_X509(myMemBio, NULL, NULL, NULL);
                if (!myX509Ptr)
                {
                    TA_THROW_MSG(std::runtime_error, "Failed to read X509 PEM cert from the memory buffer");
                }
                break;
            case DER:
                myX509Ptr = d2i_X509_bio(myMemBio, NULL);
                if (!myX509Ptr)
                {
                    TA_THROW_MSG(std::runtime_error, "Failed to read X509 DER cert from the memory buffer");
                }
                break;
            default:
                TA_THROW_MSG(std::runtime_error, boost::format("Unsupported certificate encoding '%1%'") % aCertEnc);
            }
            return myX509Ptr;
        }

        X509* getCertX509(const string& aCert, CertEncoding aCertEnc)
        {
            return getCertX509(ta::str2Vec<unsigned char>(aCert), aCertEnc);
        }

        vector<X509*> getPemCertsX509(const string& aPemBuf)
        {
            vector<X509*> myParsedCerts;
            foreach (const string& cert, parsePemCerts(aPemBuf))
            {
                X509* myX509Ptr = getCertX509(cert);
                if (myX509Ptr)
                {
                    myParsedCerts.push_back(myX509Ptr);
                }
            }
            return myParsedCerts;
        }

        vector<X509*> getPemCertsX509File(const string& aFilePath)
        {
            if (!isFileExist(aFilePath))
            {
                TA_THROW_MSG(std::invalid_argument, boost::format("Cannot check/extract certificate from %s. File does not exist") % aFilePath);
            }
            const string myFileData = ta::readData(aFilePath);
            return getPemCertsX509(myFileData);

        }

        void freeX509Certs(const vector<X509*>& aCerts)
        {
            std::for_each(aCerts.begin(), aCerts.end(), X509_free);
        }

        vector<unsigned char> convPem2Der(const vector<unsigned char>& aCert)
        {
            if (aCert.empty())
            {
                TA_THROW_MSG(std::invalid_argument, "Invalid PEM certificate (empty)");
            }

            ta::OpenSSLCertificateWrapper myCert(getCertX509(aCert));
            const int myDerLen = i2d_X509((X509*)myCert, NULL);
            if (myDerLen <= 0)
            {
                TA_THROW_MSG(std::runtime_error, "Error converting certificate to DER format");
            }

            vector<unsigned char> myRetVal(myDerLen);
            unsigned char* p = ta::getSafeBuf(myRetVal);
            i2d_X509((X509*)myCert, &p);

            return myRetVal;
        }

        vector<unsigned char> convDer2Pem(const vector<unsigned char>& aCert)
        {
            if (aCert.empty())
                TA_THROW_MSG(std::invalid_argument, "Invalid DER certificate (empty)");

            ScopedResource<BIO*> myMemBio( BIO_new(BIO_s_mem()), BIO_free);
            const int mySize = (int)aCert.size();
            const int myWritten = BIO_write(myMemBio, ta::getSafeBuf(aCert), mySize);
            if (myWritten != mySize)
                TA_THROW_MSG(std::runtime_error, boost::format("BIO_write failed trying to write %1% bytes or DER certificate. Actually written: %2% bytes.") % mySize % myWritten);

            ScopedResource<X509*> myX509(d2i_X509_bio(myMemBio, NULL), X509_free);
            if (!myX509)
                TA_THROW_MSG(std::runtime_error, "Failed to read X509 DER cert from the memory buffer");

            return convX509_2Pem(myX509);
        }

        vector<unsigned char> convX509_2Pem(X509* aCertX509)
        {
            if (!aCertX509)
                TA_THROW_MSG(std::invalid_argument, "Invalid X509 certificate (NULL)");

            ScopedResource<BIO*> myPemMemBio( BIO_new(BIO_s_mem()), BIO_free);
            if (!PEM_write_bio_X509(myPemMemBio, aCertX509))
                TA_THROW_MSG(std::runtime_error, "PEM_write_bio_X509 failed for certificate");
            BUF_MEM* myPemBuf = NULL;
            if (BIO_get_mem_ptr(myPemMemBio, &myPemBuf) < 0 || myPemBuf->length <= 0)
                TA_THROW_MSG(std::runtime_error, "BIO_get_mem_ptr failed");

            return vector<unsigned char>(myPemBuf->data, myPemBuf->data + myPemBuf->length);
        }

        vector<unsigned char> convPem2Pfx(const string& aPemCertKey, const string& aPfxPassword, const string& aPfxCertKeyFriendlyName)
        {
            const vector<MemBuffer> myPrivKeys = extractPemPrivKeys(aPemCertKey);
            if (myPrivKeys.size() != 1)
            {
                TA_THROW_MSG(std::invalid_argument, boost::format("%d keys found in the input PEM file to create PFX from") % myPrivKeys.size());
            }
            const vector<MemBuffer> myCertChain = extractPemCerts(aPemCertKey);
            if (myCertChain.empty())
            {
                TA_THROW_MSG(std::invalid_argument, "No certificate found in the input PEM file to create PFX from");
            }

            ta::OpenSSLCertificateWrapper myCertificate(myCertChain[0]);
            ta::OpenSSLPrivateKeyWrapper myPrivateKey(myPrivKeys[0]);

            vector<char> myExportPasword(ta::str2Vec<char>(aPfxPassword));
            myExportPasword.push_back('\0');
            vector<char> myCertFriendlyName(ta::str2Vec<char>(aPfxCertKeyFriendlyName));
            myCertFriendlyName.push_back('\0');
            PKCS12* myP12Struct;

            if (myCertChain.size() > 1)
            {
                // we got chain
                STACK_OF(X509) *cas = sk_X509_new_null();

                // Add CAs
                for (size_t i = 1 /*skip the topmost*/; i < myCertChain.size(); ++i)
                {
                    X509* ca = getCertX509(myCertChain[i]);
                    sk_X509_push(cas, ca); // ca will be freed by sk_X509_pop_free()
                }

                myP12Struct = PKCS12_create(ta::getSafeBuf(myExportPasword),
                                            ta::getSafeBuf(myCertFriendlyName),
                                            myPrivateKey,
                                            myCertificate,
                                            cas, 0,0,0,0,0);
                sk_X509_pop_free(cas, X509_free);
            }
            else
            {
                // no chain, just cert&key
                myP12Struct = PKCS12_create(ta::getSafeBuf(myExportPasword),
                                            ta::getSafeBuf(myCertFriendlyName),
                                            myPrivateKey,
                                            myCertificate,
                                            NULL, 0,0,0,0,0);
            }

            ta::ScopedResource<PKCS12*> myP12Package(myP12Struct, PKCS12_free);
            if (!myP12Package)
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Error creating PKCS#12 structure. %s") % ERR_error_string(ERR_get_error(), NULL));
            }

            const int myP12Length = i2d_PKCS12(myP12Package, NULL);
            if (myP12Length <= 0)
            {
                TA_THROW_MSG(std::runtime_error, "Error serializing PFX");
            }

            vector<unsigned char> myPFXBuff(myP12Length);
            unsigned char* p = ta::getSafeBuf(myPFXBuff);
            i2d_PKCS12(myP12Package, &p);

            return myPFXBuff;
        }

        string convPfx2Pem(const vector<unsigned char>& aPfx, const string& aPfxPassword)
        {
            string myParsedKey, myParsedCert;
            ta::StringArray myParsedCAs;

            parsePfx(aPfx, aPfxPassword, myParsedKey, myParsedCert, myParsedCAs);
            if (myParsedCert.empty())
            {
                TA_THROW_MSG(std::invalid_argument, "No end-certificate found in PKCS#12 package");
            }
            if (myParsedKey.empty())
            {
                TA_THROW_MSG(std::invalid_argument, "No private key found in PKCS#12 package");
            }

            ta::StringArray myPemChain = list_of(boost::trim_copy(myParsedCert))
                                         (boost::trim_copy(myParsedKey));
            myPemChain += myParsedCAs;

            return ta::Strings::join(myPemChain, '\n');
        }

        static X509* loadPEMCertificate(const string& aCertFilePath)
        {
            ScopedResource<FILE*> myCertFp(fopen(aCertFilePath.c_str(), "r"), fclose);
            if (!myCertFp)
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Failed to open '%1%'. Errno %2%") % aCertFilePath % errno);
            }

            return PEM_read_X509(myCertFp, NULL, NULL, NULL);
        }

        static bool verifyPEMCertIsIssuedByStore(const string& aCertFilePath, X509_STORE* aCtx)
        {
            ScopedResource<X509*> cert(loadPEMCertificate(aCertFilePath), X509_free);
            if (!cert)
            {
                TA_THROW_MSG(std::runtime_error, "loadPEMCertificate failed");
            }

            ScopedResource<X509_STORE_CTX*> csc(X509_STORE_CTX_new(), X509_STORE_CTX_free);
            if (!csc)
            {
                TA_THROW_MSG(std::runtime_error, "X509_STORE_CTX_new failed");
            }

            X509_STORE_set_flags(aCtx, 0);
            if (!X509_STORE_CTX_init(csc, aCtx, cert, 0))
            {
                TA_THROW_MSG(std::runtime_error, "X509_STORE_CTX_init failed");
            }

            X509_verify_cert(csc);
            int verificationError = X509_STORE_CTX_get_error(csc);

            bool verificationResult = false;

            switch (verificationError)
            {
            case X509_V_OK:
                verificationResult = true;
                break;

            case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
            {
                // The issuer certificate of an untrusted certificate cannot be found
                X509* currentErrorCert = X509_STORE_CTX_get_current_cert(csc);
                if ( currentErrorCert != cert )
                {
                    // The issuer of the lookup certificate could not be retrieved: the certificate chain is incomplete.
                    // This is a correct situation because only a certificate chain of one level is verified.
                    verificationResult = true;
                }
                break;
            }

            default:
                break;
            }

            return verificationResult;
        }

        // Verify that the certificate specified in the file aCertFilePath is issued by the certificate specified in the file aCACertFilePath.
        // Only one level in the certificate chain is verified.
        // Both certificate files have to be in PEM format.
        static bool verifyPEMCertIsIssuedBy(const string& aCertFilePath, const string& aCACertFilePath)
        {
            ScopedResource<X509_STORE*> cert_ctx(X509_STORE_new(), X509_STORE_free);
            if (!cert_ctx)
            {
                TA_THROW_MSG(std::runtime_error, "X509_STORE_new failed");
            }

            // The necessary call to openssl_add_all_algorithms() is already done

            X509_LOOKUP* lookupfile = X509_STORE_add_lookup(cert_ctx, X509_LOOKUP_file());
            if (lookupfile == NULL)
            {
                TA_THROW_MSG(std::runtime_error, "X509_STORE_add_lookup failed");
            }

            if (!X509_LOOKUP_load_file(lookupfile, aCACertFilePath.c_str(), X509_FILETYPE_PEM))
            {
                TA_THROW_MSG(std::runtime_error, "X509_LOOKUP_load_file failed");
            }

            X509_LOOKUP* lookupdir = X509_STORE_add_lookup(cert_ctx, X509_LOOKUP_hash_dir());
            if (lookupdir == NULL)
            {
                TA_THROW_MSG(std::runtime_error, "X509_STORE_add_lookup failed");
            }

            X509_LOOKUP_add_dir(lookupdir, NULL, X509_FILETYPE_DEFAULT);

            return verifyPEMCertIsIssuedByStore(aCertFilePath, cert_ctx);
        }

        bool isCertFileIssuedBy(const string& aCertFilePath, const string& aCaCertFilePath)
        {
            return verifyPEMCertIsIssuedBy(aCertFilePath, aCaCertFilePath);
        }

        bool isCertIssuedBy(const vector<unsigned char>& aCert, const vector<unsigned char>& aCaCert)
        {
            ScopedDir tempDir(ta::Process::genTempPath("certs"));

            const string issuedCertFilePath = tempDir.path + ta::getDirSep() + "cert1";
            ta::writeData(issuedCertFilePath, aCert);

            const string caCertFilePath = tempDir.path + ta::getDirSep() + "cert2";
            ta::writeData(caCertFilePath, aCaCert);

            return isCertFileIssuedBy(issuedCertFilePath, caCertFilePath);
        }

        bool isCertIssuedBy(const string& aCert, const string& aCaCert)
        {
            return isCertIssuedBy(ta::str2Vec<unsigned char>(aCert),
                                  ta::str2Vec<unsigned char>(aCaCert));
        }

        void insertCertInChain(const string& aCert, ta::StringArray& aChain)
        {
            for (ta::StringArray::iterator it = aChain.begin(), end = aChain.end(); it != end; ++it)
            {
                if (isCertIssuedBy(aCert, *it))
                {
                    aChain.insert(it, aCert);
                    return;
                }
            }
            aChain.push_back(aCert);
        }

        ta::StringArray orderCAs(const string& aCert, const ta::StringArray& aCAs)
        {
            ta::StringArray myOrderedChain = list_of(aCert);
            foreach (const string& ca, aCAs)
            {
                insertCertInChain(ca, myOrderedChain);
            }
            myOrderedChain.erase(myOrderedChain.begin());// remove the certificate itself
            return myOrderedChain;
        }

        bool fileHasPemCert(const string& aFilePath, string* aParsedCertsBuf)
        {
            if (!isFileExist(aFilePath))
            {
                TA_THROW_MSG(std::invalid_argument, boost::format("Cannot check/extract certificate from %s. File does not exist") % aFilePath);
            }
            const string myFileData = ta::readData(aFilePath);
            return hasPemCert(myFileData, aParsedCertsBuf);
        }

        bool hasPemCert(const string& aPemBuf, string* aParsedCertsBuf)
        {
            string myCerts;
            foreach (const string& cert, parsePemCerts(aPemBuf))
            {
                try { getCertInfo(str2Vec<char>(cert), PEM); }// validate
                catch (...) { continue; }
                myCerts += cert + "\n";
            }
            if (aParsedCertsBuf)
                *aParsedCertsBuf = myCerts;
            return !myCerts.empty();
        }
        bool hasPemCert(const vector<char>& aPemBuf, string* aParsedCertsBuf)
        {
            return hasPemCert(ta::vec2Str(aPemBuf), aParsedCertsBuf);
        }
        bool hasPemCert(const vector<unsigned char>& aPemBuf, string* aParsedCertsBuf)
        {
            return hasPemCert(ta::vec2Str(aPemBuf), aParsedCertsBuf);
        }

        bool hasPemCertEx(const string& aPemBuf, string& anErrorMsg, string* aParsedCertsBuf)
        {
            try
            {
                string myCerts;
                foreach (const string& cert, parsePemCerts(aPemBuf))
                {
                    getCertInfo(str2Vec<char>(cert), PEM); // validate
                    myCerts += cert + "\n";
                }
                if (aParsedCertsBuf)
                    *aParsedCertsBuf = myCerts;

                if (!myCerts.empty())
                    return true;

                anErrorMsg = "No PEM certificates found";
                return false;
            }
            catch (std::exception& e)
            {
                anErrorMsg = e.what();
                return false;
            }
        }
        bool hasPemCertEx(const vector<char>& aPemBuf, string& anErrorMsg, string* aParsedCertsBuf)
        {
            return hasPemCertEx(ta::vec2Str(aPemBuf), anErrorMsg, aParsedCertsBuf);
        }
        bool hasPemCertEx(const vector<unsigned char>& aPemBuf, string& anErrorMsg, string* aParsedCertsBuf)
        {
            return hasPemCertEx(ta::vec2Str(aPemBuf), anErrorMsg, aParsedCertsBuf);
        }

        bool hasDerCert(const vector<unsigned char>& aBuf)
        {
            string myErrorMsg;
            return hasDerCertEx(aBuf, myErrorMsg);
        }

        bool hasDerCertEx(const vector<unsigned char>& aBuf, string& anErrorMsg)
        {
            try
            {
                getCertInfo(aBuf, DER);
                return true;
            }
            catch(std::exception& e)
            {
                anErrorMsg = e.what();
                return false;
            }
        }

        bool fileHasPemPrivKey(const string& aFilePath, string* aParsedKeysBuf)
        {
            if (!isFileExist(aFilePath))
            {
                TA_THROW_MSG(std::invalid_argument, boost::format("Cannot check/extract private key from %s. File does not exist") % aFilePath);
            }
            const string myFileData = ta::readData(aFilePath);
            return hasPemPrivKey(myFileData, aParsedKeysBuf);
        }

        bool fileHasPemEncryptedPrivKey(const string& aFilePath, string* aParsedKeysBuf)
        {
            if (!isFileExist(aFilePath))
            {
                TA_THROW_MSG(std::invalid_argument, boost::format("Cannot check/extract enctypted private key from %s. File does not exist") % aFilePath);
            }

            const string myPemBuf = ta::readData(aFilePath);

            string myKeys;
            foreach (const string& key, parsePemEncryptedPrivKeys(myPemBuf))
            {
                myKeys += key + "\n";
            }

            if (aParsedKeysBuf)
            {
                *aParsedKeysBuf = myKeys;
            }
            return !myKeys.empty();
        }

        bool hasPemPrivKey(const string& aPemBuf, string* aParsedKeysBuf)
        {
            string myKeys;
            foreach (const string& key, parsePemPrivKeys(aPemBuf))
            {
                myKeys += key + "\n";
            }
            if (aParsedKeysBuf)
                *aParsedKeysBuf = myKeys;
            return !myKeys.empty();
        }
        bool hasPemPrivKey(const vector<char>& aPemBuf, string* aParsedCertsBuf)
        {
            return hasPemPrivKey(ta::vec2Str(aPemBuf), aParsedCertsBuf);
        }
        bool hasPemPrivKey(const vector<unsigned char>& aPemBuf, string* aParsedCertsBuf)
        {
            return hasPemPrivKey(ta::vec2Str(aPemBuf), aParsedCertsBuf);
        }

        bool fileHasPemPubKey(const string& aFilePath, string* aParsedKeysBuf)
        {
            if (!isFileExist(aFilePath))
            {
                TA_THROW_MSG(std::invalid_argument, boost::format("Cannot check/extract public key from %s. File does not exist") % aFilePath);
            }
            const string myFileData = ta::readData(aFilePath);
            return hasPemPubKey(myFileData, aParsedKeysBuf);
        }
        bool hasPemPubKey(const string& aPemBuf, string* aParsedKeysBuf)
        {
            string myKeys;
            foreach (const string& key, parsePemPubKeys(aPemBuf))
            {
                myKeys += key + "\n";
            }
            if (aParsedKeysBuf)
                *aParsedKeysBuf = myKeys;
            return !myKeys.empty();
        }
        bool hasPemPubKey(const vector<char>& aPemBuf, string* aParsedCertsBuf)
        {
            return hasPemPubKey(ta::vec2Str(aPemBuf), aParsedCertsBuf);
        }
        bool hasPemPubKey(const vector<unsigned char>& aPemBuf, string* aParsedCertsBuf)
        {
            return hasPemPubKey(ta::vec2Str(aPemBuf), aParsedCertsBuf);
        }

        vector<unsigned char> extractRSAPubKeyFile(const string& aDerCertPath)
        {
            vector<unsigned char> myDerCert = readData(aDerCertPath);
            return extractRSAPubKey(myDerCert);
        }

        vector<unsigned char> extractRSAPubKey(const vector<unsigned char>& aDerCert)
        {
            if (aDerCert.empty())
                TA_THROW_MSG(std::invalid_argument, "The certificate is invalid (empty)");
            vector<unsigned char>::size_type myCertBufSize = aDerCert.size();
            const unsigned char* myCertBuf = ta::getSafeBuf(aDerCert);
            ScopedResource<X509*> x509Cert(d2i_X509(NULL, &myCertBuf, (long)myCertBufSize), X509_free);
            if (!x509Cert)
                TA_THROW_MSG(std::invalid_argument, "The certificate is invalid");
            ScopedResource<EVP_PKEY*> pPublicKey(X509_get_pubkey(x509Cert), EVP_PKEY_free);
            if (!pPublicKey)
                TA_THROW_MSG(std::runtime_error, "Failed to extract pubkey from the certificate");
            ScopedResource<RSA*> myPubKeyRsa(EVP_PKEY_get1_RSA(pPublicKey), RSA_free);
            const int myKeySize = i2d_RSA_PUBKEY(myPubKeyRsa, NULL);
            vector<unsigned char> myPk(myKeySize);
            unsigned char* pKey = (unsigned char*)ta::getSafeBuf(myPk);
            i2d_RSA_PUBKEY(myPubKeyRsa, &pKey);
            return myPk;
        }

        vector<unsigned char> extractPemPubKeyFile(const string& aPemCertPath)
        {
            vector<unsigned char> myPemCert = readData(aPemCertPath);
            return extractPemPubKey(myPemCert);
        }

        vector<unsigned char> extractPemPubKey(const vector<unsigned char>& aPemCert)
        {
            if (aPemCert.empty())
                TA_THROW_MSG(std::invalid_argument, "The certificate is invalid (empty)");
            ScopedResource<BIO*> myCertMemBio( BIO_new(BIO_s_mem()), BIO_free);
            const int mySize = (int)aPemCert.size();
            const int myWritten = BIO_write(myCertMemBio, ta::getSafeBuf(aPemCert), mySize);
            if (myWritten != mySize)
                TA_THROW_MSG(std::runtime_error, boost::format("BIO_write failed trying to write %1% bytes of PEM certificate. Actually written: %2% bytes.") % mySize % myWritten);

            ScopedResource<X509*> myX509ScopedPtr(PEM_read_bio_X509(myCertMemBio, NULL, NULL, NULL), X509_free);
            if (!myX509ScopedPtr)
                TA_THROW_MSG(std::runtime_error, "Failed to read X509 PEM cert from the memory buffer");
            ta::ScopedResource<EVP_PKEY*> myPubKey(X509_get_pubkey(myX509ScopedPtr), EVP_PKEY_free);
            if (!myPubKey)
                TA_THROW_MSG(std::runtime_error, "X509_get_pubkey failed");
            ScopedResource<BIO*> myPubKeyMemBio( BIO_new(BIO_s_mem()), BIO_free);
            if (!PEM_write_bio_PUBKEY(myPubKeyMemBio, myPubKey))
                TA_THROW_MSG(std::runtime_error, "PEM_write_bio_PUBKEY failed for pubkey");

            BUF_MEM* myPubKeyBuf = NULL;
            if (BIO_get_mem_ptr(myPubKeyMemBio, &myPubKeyBuf) < 0 || myPubKeyBuf->length <= 0)
                TA_THROW_MSG(std::runtime_error, "BIO_get_mem_ptr failed");
            return vector<unsigned char>(myPubKeyBuf->data, myPubKeyBuf->data + myPubKeyBuf->length);
        }

        vector<unsigned char> extractPemPubKey(const string& aPemCert)
        {
            return extractPemPubKey(ta::str2Vec<unsigned char>(aPemCert));
        }

        bool isKeyPairFile(const string& aPemCertPath, const string& aPemKeyPath, const char* aPemKeyPasswd)
        {
            const vector<unsigned char> myCertPemPubKey = extractPemPubKeyFile(aPemCertPath);
            const vector<unsigned char> myPemPrivKey = readData(aPemKeyPath);
            return RsaUtils::isKeyPair(KeyPair(myPemPrivKey, myCertPemPubKey), RsaUtils::encPEM, RsaUtils::pubkeySubjectPublicKeyInfo, aPemKeyPasswd);
        }

        bool isKeyPair(const vector<unsigned char>& aPemCert, const vector<unsigned char>& aPemKey, const char* aPemKeyPasswd)
        {
            const vector<unsigned char> myCertPemPubKey = extractPemPubKey(aPemCert);
            return RsaUtils::isKeyPair(KeyPair(aPemKey, myCertPemPubKey), RsaUtils::encPEM, RsaUtils::pubkeySubjectPublicKeyInfo, aPemKeyPasswd);
        }

        bool isKeyPair(const string& aPemCert, const string& aPemKey, const char* aPemKeyPasswd)
        {
            return isKeyPair(ta::str2Vec<unsigned char>(aPemCert), ta::str2Vec<unsigned char>(aPemKey), aPemKeyPasswd);
        }

        size_t parsePfx(const vector<unsigned char>& aPfx, const string& aPassword)
        {
            string myDummyKey, myDummyCert;
            return parsePfx(aPfx, aPassword, myDummyKey, myDummyCert);
        }

        size_t parsePfx(const vector<unsigned char>& aPfx, const string& aPassword, string& anExtractedPemKey, string& anExtractedPemCert)
        {
            ta::StringArray myDummyCAs;
            return parsePfx(aPfx, aPassword, anExtractedPemKey, anExtractedPemCert, myDummyCAs);
        }


        size_t parsePfx(const vector<unsigned char>& aPfx, const string& aPassword, string& anExtractedPemKey, string& anExtractedPemCert, ta::StringArray& anExtractedPemCAs)
        {
            if (aPfx.empty())
            {
                TA_THROW_MSG(std::invalid_argument, "Cannot parse empty Pfx package");
            }
            ta::ScopedResource<BIO*> myPfxMemBio( BIO_new(BIO_s_mem()), BIO_free);
            BIO_write(myPfxMemBio, ta::getSafeBuf(aPfx), (int)aPfx.size());
            ta::ScopedResource<PKCS12*> p12(d2i_PKCS12_bio(myPfxMemBio, NULL), PKCS12_free);
            if (!p12)
            {
                TA_THROW_MSG(std::invalid_argument, boost::format("Ill-formed Pfx package. %s") % ERR_error_string(ERR_get_error(), NULL));
            }
            if (!PKCS12_verify_mac(p12, aPassword.c_str(), (int)aPassword.length()))
            {
                TA_THROW_MSG(std::invalid_argument, "Invalid Pfx password");
            }

            EVP_PKEY *pkey = NULL;
            X509 *cert = NULL;
            STACK_OF(X509) *cas = NULL;
            if (!PKCS12_parse(p12, aPassword.c_str(), &pkey, &cert, &cas))
            {
                TA_THROW_MSG(std::invalid_argument, boost::format("Failed to parse PFX (while password verified successfully). %s") % ERR_error_string(ERR_get_error(), NULL));
            }
            ScopedP12Info myScopedP12Info(pkey, cert, cas); // just for RAII

            size_t myNumParsedCerts = 0;
            if (cert)
            {
                ++myNumParsedCerts;
                anExtractedPemCert = ta::vec2Str(convX509_2Pem(cert));
            }
            anExtractedPemKey = convPrivKey2Pem(pkey);
            if (cas)
            {
                const int myNumCAs = sk_X509_num(cas);
                if (myNumCAs)
                {
                    for (int i = 0; i < myNumCAs; ++i)
                    {
                        X509 *ca = sk_X509_value(cas, i);
                        anExtractedPemCAs.push_back(ta::vec2Str(convX509_2Pem(ca)));
                    }
                    myNumParsedCerts += anExtractedPemCAs.size();
                }
            }
            anExtractedPemCAs = orderCAs(anExtractedPemCert, anExtractedPemCAs);

            return myNumParsedCerts;
        }

        string x509SerializeKeyUsage(const std::vector<KeyUsage>& aKeyUsages)
        {
            ta::StringArray myKeyUsages;
            foreach (const KeyUsage ku, aKeyUsages)
            {
                myKeyUsages.push_back(str(ku));
            }
            return x509SerializeKeyUsage(myKeyUsages);
        }
        string x509SerializeKeyUsage(const ta::StringArray& aKeyUsages)
        {
            return Strings::join(aKeyUsages, X509KeyUsageSep, Strings::emptyStringsSkip);
        }

        string x509SerializeBasicConstraints(const BasicConstraints& aBasicConstraints)
        {
            if (aBasicConstraints.isCA)
            {
                if (aBasicConstraints.pathLen == PathLenConstraintNone)
                {
                    return "CA:TRUE";
                }
                if (aBasicConstraints.pathLen >= 0)
                {
                    return str(boost::format("CA:TRUE,pathlen:%d") % aBasicConstraints.pathLen);
                }
                TA_THROW_MSG(std::invalid_argument, boost::format("Invalid path length contstraint %d") % aBasicConstraints.pathLen);
            }
            else
            {
                return "CA:FALSE";
            }
        }

        string x509SerializeNameConstraints(const NameConstraints& aNameConstraints)
        {
            string myRetVal;

            foreach (const string& permit, aNameConstraints.permits)
            {
                const string myPermit = boost::trim_copy(permit);
                if (!myPermit.empty())
                {
                    // lean&mean validation
                    if (myPermit.find(',') != string::npos)
                    {
                        TA_THROW_MSG(std::invalid_argument, "Invalid permitted name constraint: " + myPermit);
                    }

                    if (!myRetVal.empty())
                    {
                        myRetVal += ",";
                    }
                    myRetVal += "permitted;" + myPermit;
                }
            }
            foreach (const string& exclude, aNameConstraints.excludes)
            {
                const string myExclude = boost::trim_copy(exclude);
                if (!myExclude.empty())
                {
                    // lean&mean validation
                    if (myExclude.find(',') != string::npos)
                    {
                        TA_THROW_MSG(std::invalid_argument, "Invalid excluded name constraint: " + myExclude);
                    }

                    if (!myRetVal.empty())
                    {
                        myRetVal += ",";
                    }
                    myRetVal += "excluded;" + myExclude;
                }
            }
            return myRetVal;
        }

        string x509SerializePolicyConstraints(const PolicyConstraints& aPolicyConstraints)
        {
            string myRetVal;

            if (aPolicyConstraints.requireExplicitPolicy >= 0)
            {
                myRetVal = str(boost::format("requireExplicitPolicy:%d") % aPolicyConstraints.requireExplicitPolicy);
            }
            else if (aPolicyConstraints.requireExplicitPolicy != PolicyConstraintNone)
            {
                TA_THROW_MSG(std::invalid_argument, boost::format("Invalid 'require explicit policy' constraint %d") % aPolicyConstraints.requireExplicitPolicy);
            }

            if (aPolicyConstraints.inhibitPolicyMapping >= 0)
            {
                if (!myRetVal.empty())
                {
                    myRetVal += ",";
                }
                myRetVal += str(boost::format("inhibitPolicyMapping:%d") % aPolicyConstraints.inhibitPolicyMapping);
            }
            else if (aPolicyConstraints.inhibitPolicyMapping != PolicyConstraintNone)
            {
                TA_THROW_MSG(std::invalid_argument, boost::format("Invalid 'inhibit policy mapping' constraint %d") % aPolicyConstraints.inhibitPolicyMapping);
            }

            return myRetVal;
        }

        string x509SerializeCrlUri(const string& anUri)
        {
            const string myUri = boost::trim_copy(anUri);
            if (!myUri.empty())
            {
                try {
                    url::parse(ta::url::normalize(myUri));
                } catch (...) {
                    TA_THROW_MSG(std::invalid_argument, boost::format("'%s' is an invalid URI.") % anUri);
                }
                return X509_CRL_URI_Prefix + myUri;
            }
            else
            {
                return "";
            }
        }

        string x509SerializeOcspUri(const string& anUri)
        {
            const string myUri = boost::trim_copy(anUri);
            if (!myUri.empty())
            {
                try {
                    url::parse(ta::url::normalize(myUri));
                } catch (...) {
                    TA_THROW_MSG(std::invalid_argument, boost::format("'%s' is an invalid URI.") % anUri);
                }
                return X509_OCSP_URI_Prefix + myUri;
            }
            else
            {
                return "";
            }
        }

        string x509SerializeCertPolicies(const ta::StringArray& aCertPolicies)
        {
            return Strings::join(aCertPolicies, X509CertPoliciesSep, Strings::emptyStringsSkip);
        }

        ta::StringArray x509DeserializeKeyUsage(const string& aKeyUsages)
        {
            ta::StringArray myRetVal;
            foreach (string elem, Strings::split(aKeyUsages, X509KeyUsageSep))
            {
                string myElem = boost::algorithm::trim_copy(elem);
                if (myElem.empty())
                {
                    continue;
                }
                myRetVal.push_back(myElem);
            }
            return myRetVal;
        }

        BasicConstraints x509DeserializeBasicConstraints(const string& aBasicConstraints)
        {
            static boost::regex myRegEx("\\s*((?<isCA>CA\\:TRUE(\\s*,\\s*pathlen\\:(?<pathlen>[-]?\\d+))?)|(?<isNotCA>CA\\:FALSE))\\s*");
            boost::cmatch myMatch;
            if (!regex_match(aBasicConstraints.c_str(), myMatch, myRegEx))
            {
                TA_THROW_MSG(std::invalid_argument, boost::format("Cannot parse basic constraints from string '%1%'") % aBasicConstraints);
            }

            if (myMatch["isCA"].matched)
            {
                if (myMatch["pathlen"].matched)
                {
                    long myPathLen = Strings::parse<long>(myMatch["pathlen"]);
                    if (myPathLen < 0)
                    {
                        myPathLen = 0;
                    }
                    return BasicConstraints(caTrue, myPathLen);
                }
                return BasicConstraints(caTrue);
            }

            if (myMatch["isNotCA"].matched)
            {
                return BasicConstraints(caFalse);
            }
            TA_THROW_MSG(std::logic_error, boost::format("Error parsing basic constraints from string '%1%'") % aBasicConstraints);
        }

        NameConstraints x509DeserializeNameConstraints(const string& aNameConstraints)
        {
            static boost::regex myRegEx("(?<permitted>permitted;\\s*(?<value>[^,]+))|(?<excluded>excluded;\\s*(?<value>[^,]+))");
            NameConstraints myRetVal;

            foreach (const string& constraint, Strings::split(aNameConstraints, ','))
            {
                const string myNameConstraint = boost::trim_copy(constraint);
                if (!myNameConstraint.empty())
                {
                    boost::cmatch myMatch;
                    if (!regex_match(myNameConstraint.c_str(), myMatch, myRegEx))
                    {
                        TA_THROW_MSG(std::invalid_argument, boost::format("Cannot parse name constraints (1) from string '%1%'") % aNameConstraints);
                    }

                    if (myMatch["permitted"].matched && myMatch["value"].matched)
                    {
                        myRetVal.permits.push_back(myMatch["value"]);
                    }
                    else if (myMatch["excluded"].matched && myMatch["value"].matched)
                    {
                        myRetVal.excludes.push_back(myMatch["value"]);
                    }
                    else
                    {
                        TA_THROW_MSG(std::invalid_argument, boost::format("Cannot parse name constraints (2) from string '%1%'") % aNameConstraints);
                    }
                }
            }
            return myRetVal;
        }

        PolicyConstraints x509DeserializePolicyConstraints(const string& aPolicyConstraints)
        {
            PolicyConstraints myRetVal;
            const string myPolicyConstraint = boost::trim_copy(aPolicyConstraints);
            if (!myPolicyConstraint.empty())
            {
                static boost::regex myRegEx("(?<name>requireExplicitPolicy|inhibitPolicyMapping):\\s*(?<value>\\d+)");
                boost::match_results<string::const_iterator> myMatch;
                string::const_iterator myBeg = myPolicyConstraint.begin(), myEnd = myPolicyConstraint.end();

                bool myMatched = false;
                while (regex_search(myBeg, myEnd, myMatch, myRegEx))
                {
                    myMatched = true;

                    if (myMatch["name"] == "requireExplicitPolicy")
                    {
                        int value = Strings::parse<int>(myMatch["value"]);
                        if (value >= 0)
                        {
                            myRetVal.requireExplicitPolicy = value;
                        }
                        else
                        {
                            TA_THROW_MSG(std::invalid_argument, "The value of 'require explicit policy' constraint cannot ne negative");
                        }
                    }
                    else if (myMatch["name"] == "inhibitPolicyMapping")
                    {
                        int value = Strings::parse<int>(myMatch["value"]);
                        if (value >= 0)
                        {
                            myRetVal.inhibitPolicyMapping = value;
                        }
                        else
                        {
                            TA_THROW_MSG(std::invalid_argument, "The value of 'inhibit policy mapping' constraint cannot be negative");
                        }
                    }
                    else
                    {
                        TA_THROW_MSG(std::logic_error, boost::format("Internal error parsing parse policy constraints from '%s'") % aPolicyConstraints);
                    }
                    myBeg = myMatch[0].second;
                }

                if (!myMatched)
                {
                    TA_THROW_MSG(std::invalid_argument, boost::format("Cannot parse policy constraints from '%s'") % aPolicyConstraints);
                }
            }

            return myRetVal;
        }

        string x509DeserializeCrlUri(const string& aCrlUri)
        {
            string myCrlUri = boost::trim_copy(aCrlUri);
            if (!myCrlUri.empty())
            {
                if (!boost::starts_with(myCrlUri, X509_CRL_URI_Prefix))
                {
                    TA_THROW_MSG(std::invalid_argument, boost::format("'%s' is an invalid CRL distribution point specification. Distribution point URI should start with '%s'") % aCrlUri % X509_CRL_URI_Prefix);
                }
                const string myUri = boost::trim_copy(myCrlUri.substr(X509_CRL_URI_Prefix.size()));
                try {
                    url::parse(ta::url::normalize(myUri));
                } catch (...) {
                    TA_THROW_MSG(std::invalid_argument, boost::format("'%s' is an invalid CRL distribution list URI.") % aCrlUri);
                }
                return myUri;
            }
            else
            {
                return "";
            }
        }

        string x509DeserializeOcspUri(const string& anOcspUri)
        {
            string myOcspUri = boost::trim_copy(anOcspUri);
            if (!myOcspUri.empty())
            {
                if (!boost::starts_with(myOcspUri, X509_OCSP_URI_Prefix))
                {
                    TA_THROW_MSG(std::invalid_argument, boost::format("'%s' is an invalid OCSP host specification. OCSP host URI should start with '%s'") % anOcspUri % X509_OCSP_URI_Prefix);
                }
                const string myUri = boost::trim_copy(myOcspUri.substr(X509_OCSP_URI_Prefix.size()));
                try {
                    url::parse(ta::url::normalize(myUri));
                } catch (...) {
                    TA_THROW_MSG(std::invalid_argument, boost::format("'%s' is an invalid OCSP host URI.") % anOcspUri);
                }
                return myUri;
            }
            else
            {
                return "";
            }
        }

        ta::StringArray x509DeserializeCertPolicies(const string& aCertPolicies)
        {
            ta::StringArray myRetVal;
            foreach (const string& elem, Strings::split(aCertPolicies, X509CertPoliciesSep))
            {
                string myElem = boost::algorithm::trim_copy(elem);
                if (myElem.empty())
                {
                    continue;
                }
                myRetVal.push_back(myElem);
            }
            return myRetVal;
        }

        string normalizeExtendedKeyUsage(const string& anExtendedKeyUsage)
        {
            // trim whitespace
            string myExtKeyUsage = boost::trim_copy(anExtendedKeyUsage);

            // replace OID with symbolic representation
            if (myExtKeyUsage == "1.3.6.1.5.5.7.3.1")
            {
                myExtKeyUsage = SN_server_auth;
            }
            else if (myExtKeyUsage == "1.3.6.1.5.5.7.3.2")
            {
                myExtKeyUsage = SN_client_auth;
            }
            else if (myExtKeyUsage == "1.3.6.1.5.5.7.3.3")
            {
                myExtKeyUsage = SN_code_sign;
            }
            else if (myExtKeyUsage == "1.3.6.1.5.5.7.3.4")
            {
                myExtKeyUsage = SN_email_protect;
            }
            else if (myExtKeyUsage == "1.3.6.1.5.5.7.3.5")
            {
                myExtKeyUsage = SN_ipsecEndSystem;
            }
            else if (myExtKeyUsage == "1.3.6.1.5.5.7.3.6")
            {
                myExtKeyUsage = SN_ipsecTunnel;
            }
            else if (myExtKeyUsage == "1.3.6.1.5.5.7.3.7")
            {
                myExtKeyUsage = SN_ipsecUser;
            }
            else if (myExtKeyUsage == "1.3.6.1.5.5.7.3.8")
            {
                myExtKeyUsage = SN_time_stamp;
            }
            else if (myExtKeyUsage == "1.3.6.1.5.5.7.3.9")
            {
                myExtKeyUsage = SN_OCSP_sign;
            }
            return myExtKeyUsage;
        }

        ta::StringArray normalizeExtendedKeyUsages(const ta::StringArray& anExtendedKeyUsages)
        {
            // normalize individual key usages
            ta::StringArray myNormalizedExtendedKeyUsages = anExtendedKeyUsages;
            foreach (string& elem, myNormalizedExtendedKeyUsages)
            {
                elem = normalizeExtendedKeyUsage(elem);
            }

            // remove duplicates
            boost::sort(myNormalizedExtendedKeyUsages);
            ta::StringArray::iterator new_end = std::unique(myNormalizedExtendedKeyUsages.begin(), myNormalizedExtendedKeyUsages.end());
            myNormalizedExtendedKeyUsages.erase(new_end, myNormalizedExtendedKeyUsages.end());

            return myNormalizedExtendedKeyUsages;
        }


        string normalizeSerializedCertPolicies(const string& aCertPolicies)
        {
            ta::StringArray myNormalizedCertPolicies = x509DeserializeCertPolicies(aCertPolicies);

            // remove duplicates
            boost::sort(myNormalizedCertPolicies);
            ta::StringArray::iterator new_end = std::unique(myNormalizedCertPolicies.begin(), myNormalizedCertPolicies.end());
            myNormalizedCertPolicies.erase(new_end, myNormalizedCertPolicies.end());

            return x509SerializeCertPolicies(myNormalizedCertPolicies);
        }


        X509_REQ* createCSR(const ta::KeyPair& aKeyPair,
                            const Subject& aSubject,
                            const SignUtils::Digest* aSignatureAlgorithm,
                            const ta::StringArray& aSAN,
                            const string& aChallengePassword)
        {
            // create CSR
            ScopedResource<X509_REQ*> myRequest(X509_REQ_new(), X509_REQ_free);
            if (!myRequest)
            {
                TA_THROW_MSG(std::runtime_error, "Could not create new certificate request");
            }

            const int myVersion = 1;
            if (X509_REQ_set_version(myRequest, myVersion) != 1)
            {
                TA_THROW_MSG(std::runtime_error, "Could not set version on CSR");
            }

            // set public key
            OpenSSLPublicKeyWrapper myPubKey(aKeyPair.pubKey, RsaUtils::pubkeyPKCS1);
            if (X509_REQ_set_pubkey(myRequest, myPubKey) != 1)
            {
                TA_THROW_MSG(std::runtime_error, "Could not set public key on CSR");
            }

            // Add subject
            ScopedResource<X509_NAME*> mySubject(X509_NAME_new(), X509_NAME_free);
            addSubjectEntry(mySubject, NID_commonName, aSubject.cn);
            addSubjectEntry(mySubject, NID_countryName, aSubject.c);
            addSubjectEntry(mySubject, NID_stateOrProvinceName, aSubject.st);
            addSubjectEntry(mySubject, NID_localityName, aSubject.l);
            addSubjectEntry(mySubject, NID_organizationName, aSubject.o);
            addSubjectEntry(mySubject, NID_organizationalUnitName, aSubject.ou);
            addSubjectEntry(mySubject, NID_pkcs9_emailAddress, aSubject.e);
            if (X509_REQ_set_subject_name(myRequest, mySubject) != 1)
            {
                TA_THROW_MSG(std::runtime_error, "Could not add the subject fields to the request");
            }

            addSAN(myRequest, aSAN);
            addChallengePassword(myRequest, aChallengePassword);

            if (aSignatureAlgorithm)
            {
                OpenSSLPrivateKeyWrapper myPrivKey(aKeyPair.privKey);
                if (X509_REQ_sign(myRequest, myPrivKey, getDigest(*aSignatureAlgorithm)) < 0)
                {
                    TA_THROW_MSG(std::runtime_error, "Could not sign CSR using " + str(*aSignatureAlgorithm));
                }
            }

            return myRequest.detach();
        }

        string convX509_REQ_2Pem(X509_REQ* aReq)
        {
            if (!aReq)
            {
                TA_THROW_MSG(std::invalid_argument, "Invalid X509_REQ request (NULL)");
            }

            ScopedResource<BIO*> myPemMemBio( BIO_new(BIO_s_mem()), BIO_free);
            if (!PEM_write_bio_X509_REQ(myPemMemBio, aReq))
            {
                TA_THROW_MSG(std::runtime_error, "PEM_write_bio_X509_REQ failed for CSR");
            }
            BUF_MEM* myPemBuf = NULL;
            if (BIO_get_mem_ptr(myPemMemBio, &myPemBuf) < 0 || myPemBuf->length <= 0)
            {
                TA_THROW_MSG(std::runtime_error, "BIO_get_mem_ptr failed");
            }
            return string((const char*)myPemBuf->data, myPemBuf->length);
        }

        string parseCnFromSignedCSR(const string& aCsrPem)
        {
            // Convert CSR from PEM to X509_REQ*
            ScopedResource<BIO*> myMemBio( BIO_new(BIO_s_mem()), BIO_free);
            const int mySize = (int)aCsrPem.size();
            const int myWritten = BIO_write(myMemBio, aCsrPem.c_str(), mySize);
            if (myWritten != mySize)
            {
                TA_THROW_MSG(std::runtime_error, boost::format("BIO_write failed trying to write %1% bytes of CSR. Actually written: %2% bytes.") % mySize % myWritten);
            }
            ScopedResource<X509_REQ*> myX509ReqPtr(PEM_read_bio_X509_REQ(myMemBio, NULL, NULL, NULL), X509_REQ_free);
            if (!myX509ReqPtr)
            {
                TA_THROW_MSG(std::runtime_error, "Failed to read X509 CSR from PEM memory buffer");
            }

            // Parse subject CN
            X509_NAME* myX509SubjName = X509_REQ_get_subject_name(myX509ReqPtr);
            if (!myX509SubjName)
            {
                TA_THROW_MSG(std::runtime_error, "Failed to read X509 subject name from CSR");
            }
            const int myCnIndex = X509_NAME_get_index_by_NID(myX509SubjName, NID_commonName, -1);
            if (myCnIndex < 0)
            {
                TA_THROW_MSG(std::runtime_error, "Failed to read subject CN index from CSR subject name");
            }
            X509_NAME_ENTRY* myCnEntry = X509_NAME_get_entry(myX509SubjName, myCnIndex);
            if (!myCnEntry)
            {
                TA_THROW_MSG(std::runtime_error, "Failed to read subject CN from CSR subject name");
            }
            return asn1ToStr(X509_NAME_ENTRY_get_data(myCnEntry));
        }

        // "DNS:example.com" => ("DNS", "example.com")
        boost::tuple<string, string> parseSingleSAN(const string& aSAN)
        {
            const string mySAN = boost::trim_copy(aSAN);
            if (boost::starts_with(mySAN, "IP:"))
            {
                const string mySanVal = boost::trim_copy(mySAN.substr(strlen("IP:")));
                if (!ta::NetUtils::isValidIpv4(mySanVal) && !ta::NetUtils::isValidIpv6(mySanVal))
                {
                    TA_THROW_MSG(std::invalid_argument, boost::format("Invalid Subject Alternative Name IP address value '%s'.") % mySanVal);
                }
                return boost::make_tuple("IP", mySanVal);
            }
            else if (boost::starts_with(mySAN, "DNS:"))
            {
                const string mySanVal = boost::trim_copy(mySAN.substr(strlen("DNS:")));
                if (mySanVal.empty())
                {
                    TA_THROW_MSG(std::invalid_argument, boost::format("Invalid Subject Alternative Name DNS value '%s'.") % mySanVal);
                }
                return boost::make_tuple("DNS", mySanVal);
            }
            else if (boost::starts_with(mySAN, "email:"))
            {
                const string mySanVal = boost::trim_copy(mySAN.substr(strlen("email:")));
                if (mySanVal.empty())
                {
                    TA_THROW_MSG(std::invalid_argument, boost::format("Invalid Subject Alternative Name email value '%s'.") % mySanVal);
                }
                return boost::make_tuple("email", mySanVal);
            }
            TA_THROW_MSG(std::invalid_argument, boost::format("Invalid Subject Alternative Name prefix '%s'. Only DNS:, email: and IP: are supported at this moment.") % aSAN);
        }

        void validateSingleSAN(const string& aSAN)
        {
            parseSingleSAN(aSAN);
        }

        void validateSAN(const std::string& aSAN)
        {
            deserializeSAN(aSAN);
        }

        string serializeSAN(const ta::StringArray& aSANs)
        {
            ta::StringArray mySANs;
            foreach (const string& san, aSANs)
            {
                const string mySAN = boost::trim_copy(san);
                if (!mySAN.empty())
                {
                    validateSingleSAN(mySAN);
                    mySANs.push_back(mySAN);
                }
            }
            return ta::Strings::join(mySANs,
                                     ',',
                                     ta::Strings::emptyStringsSkip);
        }

        bool doesSANContainKey(const string& aSANs, const string& aKey)
        {
            foreach (const string& san, deserializeSAN(aSANs))
            {
                if (parseSingleSAN(san).get<0>() == aKey)
                {
                    return true;
                }
            }
            return false;
        }

        ta::StringArray deserializeSAN(const string& aSANs)
        {
            ta::StringArray myRetVal;
            foreach (const string& san, ta::Strings::split(boost::trim_copy(aSANs),
                     ',',
                     ta::Strings::sepsMergeOn,
                     ta::Strings::emptyTokensDrop))
            {
                const string mySAN = boost::trim_copy(san);
                if (!mySAN.empty())
                {
                    validateSingleSAN(mySAN);
                    myRetVal.push_back(mySAN);
                }
            }
            return myRetVal;
        }

        ta::StringArray extractSAN_Values(const ta::StringArray& aSAN)
        {
            ta::StringArray myRetVal;
            foreach (const string& san, aSAN)
            {
                const string mySAN = boost::trim_copy(san);
                if (!mySAN.empty())
                {
                    const string mySanValue = boost::get<1>(parseSingleSAN(mySAN));
                    myRetVal.push_back(mySanValue);
                }
            }
            return myRetVal;
        }

        bool isCertFileRevoked(const string& aCertPath, string* aWarnings)
        {
            vector <vector<unsigned char> > myCRLs;
            foreach (const string& crlUrl, getCertInfoFile(aCertPath, isPemCertFile(aCertPath) ? PEM : DER).crlDistributinPoints)
            {
                try {
                    const vector<unsigned char> myCRL = ta::fetchHttpUrl(crlUrl);
                    myCRLs.push_back(myCRL);
                } catch (std::exception& e) {
                    if (aWarnings) {
                        *aWarnings += str(boost::format(" Failed to retrieve CRL for URL %s. %s.") % crlUrl % e.what());
                    }
                }
            }
            return isCertFileRevokedForCrl(aCertPath, myCRLs);
        }

        bool isCertFileRevokedForCrl(const string& aCertPath, const vector <vector<unsigned char> >& aCRLs)
        {
            OpenSSLCertificateWrapper myCert;
            if (isPemCertFile(aCertPath)) {
                myCert.loadFromFile(aCertPath);
            } else {
                myCert.loadFromBuf(convDer2Pem(ta::readData(aCertPath)));
            }

            foreach (const vector<unsigned char>& crl, aCRLs)
            {
                if (crl.empty())
                {
                    TA_THROW_MSG(std::invalid_argument, "Invalid CRL (empty)");
                }
                ScopedResource<BIO*> myMemBio( BIO_new(BIO_s_mem()), BIO_free);
                const int mySize = (int)crl.size();
                const int myWritten = BIO_write(myMemBio, ta::getSafeBuf(crl), mySize);
                if (myWritten != mySize)
                {
                    TA_THROW_MSG(std::runtime_error, boost::format("BIO_write failed trying to write %1% bytes of CRL. Actually written: %2% bytes.") % mySize % myWritten);
                }

                ta::ScopedResource<X509_CRL*> myCRL(isPemCrl(crl) ? PEM_read_bio_X509_CRL(myMemBio, NULL, NULL, NULL) : d2i_X509_CRL_bio(myMemBio, NULL),
                                                    X509_CRL_free);

                if (!myCRL)
                {
                    TA_THROW_MSG(std::invalid_argument, "Invalid CRL supplied");
                }

                X509_REVOKED *revoked = NULL;
                if (X509_CRL_get0_by_cert(myCRL, &revoked, myCert))
                {
                    // this also includes 'removeFromCRL (8)' status which means the cert is temporary suspended, though we treat it as the cert is revoked
                    return true;
                }
            }
            return false;
        }

        bool isSmimeCert(const string& aCertificate)
        {
            if (!hasPemCert(aCertificate))
            {
                TA_THROW_MSG(std::invalid_argument, "Not a S/MIME Certificate: Input is not a certificate.");
            }

            const CertInfo myCertInfo = getCertInfo(aCertificate);
            string mySAN;
            if (!ta::findValueByKey(SN_subject_alt_name, myCertInfo.optionalExtensions, mySAN) || !doesSANContainKey(mySAN, "email"))
            {
                DEBUGLOG("Not a S/MIME Certificate: Email missing, email should be present in SAN. With SAN: " + mySAN);
                return false;
            }
            if (!myCertInfo.keyUsage.empty())
            {
                if (!ta::isElemExist(keyusageNonRepudiation, myCertInfo.keyUsage) && !ta::isElemExist(keyusageDigitalSignature, myCertInfo.keyUsage))
                {
                    DEBUGLOG("Not a S/MIME Certificate: Certificate contains keyUsage, but misses required bits.");
                    return false;
                }
            }
            if (!myCertInfo.extKeyUsage.empty())
            {
                if (!ta::isElemExist(ekuAnyExtendedKeyUsage, myCertInfo.extKeyUsage) && !ta::isElemExist(ekuSecureEmail, myCertInfo.extKeyUsage))
                {
                    DEBUGLOG("Not a S/MIME Certificate: Certificate contains extendedKeyUsage, but misses required bits.");
                    return false;
                }
            }

            return true;
        }

        string getEmailFromSmime(const string& aCertificate)
        {
            if (!isSmimeCert(aCertificate))
            {
                TA_THROW_MSG(std::invalid_argument, "Certificate is not S/MIME");
            }

            const string myNotFoundError = "Certificate is S/MIME, but email could not be found. This could indicate an error in the 'is S/MIME certificate' check.";

            const CertInfo myCertInfo = getCertInfo(aCertificate);
            string mySAN;
            if (!ta::findValueByKey(SN_subject_alt_name, myCertInfo.optionalExtensions, mySAN))
            {
                TA_THROW_MSG(std::invalid_argument, myNotFoundError);
            }

            foreach (const string& san, deserializeSAN(mySAN))
            {
                if (parseSingleSAN(san).get<0>() == "email")
                {
                    return parseSingleSAN(san).get<1>();
                }
            }
            TA_THROW_MSG(std::invalid_argument, myNotFoundError);
        }

    }// namespace CertUtils
}// namespace ta
