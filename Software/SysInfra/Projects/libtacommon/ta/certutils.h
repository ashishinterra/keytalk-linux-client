#pragma once

#include "ta/common.h"
#include "ta/signutils.h"

#include <string>
#include <vector>
#include <stdexcept>
#include "boost/cstdint.hpp"
#include "boost/format.hpp"

struct x509_st;
typedef struct x509_st X509;
struct X509_req_st;
typedef struct X509_req_st X509_REQ;

namespace ta
{
    namespace CertUtils
    {
        typedef std::vector<unsigned char> MemBuffer;

        enum KeyUsage
        {
            // enum values correspond to the keys of key_usage_type_table table defined in v3_bitst.c
            _firstKeyUsage = 0,
            keyusageDigitalSignature = _firstKeyUsage,
            keyusageNonRepudiation,
            keyusageKeyEncipherment,
            keyusageDataEncipherment,
            keyusageKeyAgreement,
            keyusageCertificateSign,
            keyusageCrlSign,
            keyusageEncipherOnly,
            keyusageDecipherOnly,
            _lastKeyUsage = keyusageDecipherOnly
        };
        // e.g. "digitalSignature"
        std::string str(const KeyUsage aKeyUsage);
        KeyUsage parseKeyUsage(const std::string& aKeyUsageStr);
        // e.g. "Digital Signature"
        std::string str_long(const KeyUsage aKeyUsage);
        //
        ta::StringArray strs(const std::vector<KeyUsage> aKeyUsages);
        std::vector<KeyUsage> parseKeyUsages(const ta::StringArray& aKeyUsageStrs);

        enum ExtendedKeyUsage
        {
            _firstExtendedKeyUsage = 0,
            ekuClientAuth = _firstExtendedKeyUsage,
            ekuServerAuth,
            ekuSecureEmail,
            ekuAnyExtendedKeyUsage,
            // enough for now
            _lastExtendedKeyUsage = ekuAnyExtendedKeyUsage
        };

        enum CaFlag
        {
            _firstCaFlag, // sentinel lower bound
            caFalse = _firstCaFlag,
            caTrue,
            _lastCaFlag = caTrue // sentinel upper bound
        };

        static const long PathLenConstraintNone = -1;

        struct BasicConstraints
        {
            BasicConstraints(CaFlag anCaFlag = caFalse, long aPathLen = PathLenConstraintNone)
                : isCA(anCaFlag == caTrue), pathLen(aPathLen) {}

            bool isCA;  // Boolean indicating whether the certificate subject can act as a certification authority (CA) or not.
            long pathLen; // Used only for CA certificate (i.e. isCA is true) and specifies the maximum number of CA certificates that can follow this certificate in a certification path. A value of zero indicated that the CA can only be used to sign end certificates and not further CAs. A value of PathLenConstraintNone indicates that the path length will be determined by the path length of the issuer certificate. If the issuer's path length also does not define constraints, the effective path length will be unlimited. If the issuer's path length is an integer greater than zero, the path length effective path length will be set to a value that's one less than the issuer's path length.

            inline bool operator==(const BasicConstraints& aRhs) const
            {
                if (isCA)
                {
                    return aRhs.isCA && pathLen == aRhs.pathLen;
                }
                else
                {
                    return !aRhs.isCA; // do not care about pathLen for non-CA cert
                }
            }
        };

        enum KeyType
        {
            _FirstKeyType,
            keyRsa = _FirstKeyType,
            keyDsa,
            keyEc,
            _LastKeyType = keyEc
        };
        const std::string KeyTypeStrs[] = {"RSA", "DSA", "EC"};
        BOOST_STATIC_ASSERT(_FirstKeyType < _LastKeyType);
        BOOST_STATIC_ASSERT(sizeof(KeyTypeStrs)/sizeof(KeyTypeStrs[0]) == _LastKeyType-_FirstKeyType+1);
        inline std::string str(const KeyType aKeyType)
        {
            return KeyTypeStrs[aKeyType];
        }
        inline bool isKeyType(const int aVal)
        {
            return aVal >= _FirstKeyType && aVal <= _LastKeyType;
        }

        /**
          Certificate information fields
         */
        struct CertInfo
        {
            std::string issuerName;         ///< Issuer name as one line e.g. "/CN=Demo/C=NL/..."
            std::string issuerCN;           ///< Issuer common name e.g. "Demo"  (parsed from issuer name)
            std::string subjName;           ///< Subject name as one line e.g. "/CN=Demo/C=NL/..."
            std::string subjCN;             ///< Subject common name e.g. "Demo"  (parsed from subject name)
            std::string subjO;             /// < Subject Organization name e.g. "KeyTalk B.V." (parsed from subject)
            std::string subjOU;             ///< Subject Organization Unit name e.g. "Sales" (parsed from subject)
            std::string serial;             /// serial number in HEX with bytes separated by semicolons e.g. 11:21:7b:82:ef:53:23:c8:a2:1e:09:6b:6f:d7:4f:91:29:30
            std::string sha1Fingerprint;    ///< SHA1 fingerprint (lowercase hex; derived field)
            time_t utcNotBefore;            ///< not before UTC
            time_t utcNotAfter;             ///< not after UTC
            std::vector<KeyUsage> keyUsage; ///< Key usage
            std::vector<ExtendedKeyUsage> extKeyUsage; ///< Extended key usage
            ta::SignUtils::SignatureAlgorithm signatureAlgorithm; ///< Signature algorithm e.g. sha1WithRSAEncryption
            KeyType pubKeyType;          ///< Type of the public key
            boost::uint32_t pubKeyBits;     ///< Size of public key in bits
            BasicConstraints basicConstraints; /// < X509 Basic Constraints
            ta::StringArray crlDistributinPoints; ///< CRL distribution points
            ta::StringArray ocspUrls;            ///< OCSP URLs
            StringDict optionalExtensions; ///< Optional extensions. Currently supported extensions:
            ///   - "DNS:" and "IP:" parts of Subject Alternative Name
            ///    - nsBaseUrl

            inline bool operator==(const CertInfo& aRhs) const
            {
                return (issuerName == aRhs.issuerName &&
                        issuerCN == aRhs.issuerCN &&
                        subjName == aRhs.subjName &&
                        subjO == aRhs.subjO &&
                        subjOU == aRhs.subjOU &&
                        serial == aRhs.serial &&
                        subjCN == aRhs.subjCN &&
                        sha1Fingerprint == aRhs.sha1Fingerprint &&
                        utcNotBefore == aRhs.utcNotBefore &&
                        utcNotAfter == aRhs.utcNotAfter &&
                        keyUsage == aRhs.keyUsage &&
                        extKeyUsage == aRhs.extKeyUsage &&
                        signatureAlgorithm == aRhs.signatureAlgorithm &&
                        pubKeyType == aRhs.pubKeyType &&
                        pubKeyBits == aRhs.pubKeyBits &&
                        basicConstraints == aRhs.basicConstraints &&
                        optionalExtensions == aRhs.optionalExtensions
                       );
            }
        };

        /**
          Certificate encoding mechanism
         */
        enum CertEncoding
        {
            PEM, DER
        };

        /**
          Retrieve certificate info from the certificate file

          @param[in] aFilePath certificate file path.
          @param[in] aCertEnc Certificate encoding mechanism
          @notes  For PEM certificates containing several certificates the info about the first certificate is returned.
          To return info about all certificates in the PEM file use getPemCertsInfoFile()
         */
        CertInfo getCertInfoFile(const std::string& aFilePath, CertEncoding aCertEnc = PEM);

        /**
          Retrieve certificate info from the certificate memory buffer
         */
        CertInfo getCertInfo(const std::vector<unsigned char>& aCert, CertEncoding aCertEnc = PEM);
        CertInfo getCertInfo(const std::vector<char>& aCert, CertEncoding aCertEnc = PEM);
        CertInfo getCertInfo(const std::string& aCert, CertEncoding aCertEnc = PEM);
        CertInfo getCertInfo(X509* aCertX509);

        /**
          Retrieve certificates info from the PEM file or buffer in the order they appear
         */
        std::vector<CertInfo> getPemCertsInfoFile(const std::string& aFilePath);
        std::vector<CertInfo> getPemCertsInfo(const std::string& aPemBuf);
        std::vector<CertInfo> getPemCertsInfo(const std::vector<unsigned char>& aPemBuf);

        /**
          Retrieve certificates info in X509 format from file or memory buffer in the order they appear
          IMPORTANT: Caller is responsible for calling X509_free() for each retrieved certificate or calling freeX509() provided for convenience
         */
        X509* getCertX509(const std::vector<unsigned char>& aCert, CertEncoding aCertEnc = PEM);
        X509* getCertX509(const std::string& aCert, CertEncoding aCertEnc = PEM);
        std::vector<X509*> getPemCertsX509File(const std::string& aFilePath);
        std::vector<X509*> getPemCertsX509(const std::string& aPemBuf);
        void freeX509Certs(const std::vector<X509*>& aCerts);

        /**
        	Conversion routines
        */
        std::vector<unsigned char> convPem2Der(const std::vector<unsigned char>& aCert);
        std::vector<unsigned char> convDer2Pem(const std::vector<unsigned char>& aCert);
        std::vector<unsigned char> convX509_2Pem(X509* aCertX509);
        // convert PEM to PFX; private key in PEM should be not password-protected
        std::vector<unsigned char> convPem2Pfx(const std::string& aPemCertKey, const std::string& aPfxPassword, const std::string& aPfxCertKeyFriendlyName = "");
        // Convert PFX into a single PEM file; private key is stored in PEM not password protected
        // POST: in the resulted PEM the end-certificate goes first, followed by a key and then by CAs from child towards parent
        std::string convPfx2Pem(const std::vector<unsigned char>& aPfx, const std::string& aPfxPassword);

        /**
           Checks whether the given PEM certificate is issued by another PEM certificate.
           Only a single level in the certificate chain is checked!
        */
        bool isCertFileIssuedBy(const std::string& aCertFilePath, const std::string& aCaCertFilePath);
        bool isCertIssuedBy(const std::vector<unsigned char>& aCert, const std::vector<unsigned char>& aCaCert);
        bool isCertIssuedBy(const std::string& aCert, const std::string& aCaCert);

        // Insert the given PEM-encoded cert into the chain respecting the chain order from child to parent.
        // If no parent/child cert found in the chain, append the cert to the end
        void insertCertInChain(const std::string& aCert, ta::StringArray& aChain);
        // order CAs of the given PEM-encoded cert from child towards parent
        ta::StringArray orderCAs(const std::string& aCert, const ta::StringArray& aCAs);

        // Convenient shortcuts
        inline bool isSelfSignedCertFile(const std::string& aCertFilePath) { return isCertFileIssuedBy(aCertFilePath, aCertFilePath); }
        inline bool isSelfSignedCert(const std::vector<unsigned char>& aCert) { return isCertIssuedBy(aCert, aCert); }
        inline bool isSelfSignedCert(const std::string& aCert) { return isCertIssuedBy(aCert, aCert); }

        /**
          Checks whether the given source contains valid PEM-encoded X509 certificates and retrieves them
          @param aParsedCertsBuf [in,out] if not NULL the buffer is filled parsed with certs filtered from aPemBuf
         */
        bool fileHasPemCert(const std::string& aFilePath, std::string* aParsedCertsBuf = NULL);
        bool hasPemCert(const std::string& aPemBuf, std::string* aParsedCertsBuf = NULL);
        bool hasPemCert(const std::vector<char>& aPemBuf, std::string* aParsedCertsBuf = NULL);
        bool hasPemCert(const std::vector<unsigned char>& aPemBuf, std::string* aParsedCertsBuf = NULL);
        // extended versions of the above counterparts with added error reporting
        bool hasPemCertEx(const std::string& aPemBuf, std::string& anErrorMsg, std::string* aParsedCertsBuf = NULL);
        bool hasPemCertEx(const std::vector<char>& aPemBuf, std::string& anErrorMsg, std::string* aParsedCertsBuf = NULL);
        bool hasPemCertEx(const std::vector<unsigned char>& aPemBuf, std::string& anErrorMsg, std::string* aParsedCertsBuf = NULL);

        bool hasDerCert(const std::vector<unsigned char>& aBuf);
        bool hasDerCertEx(const std::vector<unsigned char>& aBuf, std::string& anErrorMsg);

        /**
          Checks whether the given source contains at least one valid PEM-encoded private key and retrieves them
          @param aParsedKeysBuf [in,out] if not NULL the buffer is filled with parsed private keys filtered from aPemBuf
         */
        bool fileHasPemPrivKey(const std::string& aFilePath, std::string* aParsedKeysBuf = NULL);
        bool fileHasPemEncryptedPrivKey(const std::string& aFilePath, std::string* aParsedKeysBuf = NULL);
        bool hasPemPrivKey(const std::string& aPemBuf, std::string* aParsedKeysBuf = NULL);
        bool hasPemPrivKey(const std::vector<char>& aPemBuf, std::string* aParsedKeysBuf = NULL);
        bool hasPemPrivKey(const std::vector<unsigned char>& aPemBuf, std::string* aParsedKeysBuf = NULL);

        /**
          Checks whether the given memory buffer contains at least one valid PEM-encoded public key and retrieves them
          @param aParsedKeysBuf [in,out] if not NULL the buffer is filled with parsed public keys filtered from aPemBuf
         */
        bool fileHasPemPubKey(const std::string& aFilePath, std::string* aParsedKeysBuf = NULL);
        bool hasPemPubKey(const std::string& aPemBuf, std::string* aParsedKeysBuf = NULL);
        bool hasPemPubKey(const std::vector<char>& aPemBuf, std::string* aParsedKeysBuf = NULL);
        bool hasPemPubKey(const std::vector<unsigned char>& aPemBuf, std::string* aParsedKeysBuf = NULL);

        /**
          Extract certificates from the PEM file or buffer in the order they appear
         */
        std::vector<MemBuffer> extractPemCertsFromFile(const std::string& aFilePath);
        std::vector<MemBuffer> extractPemCerts(const std::string& aPemBuf);
        std::vector<MemBuffer> extractPemCerts(const std::vector<unsigned char>& aPemBuf);

        /**
          Extract PEM-encoded private keys from file or buffer in the order they appear
         */
        enum KeyFilter
        {
            keyFilterEncryptedOnly, keyFilterNotEncryptedOnly, keyFilterNone
        };
        std::vector<MemBuffer> extractPemPrivKeysFromFile(const std::string& aFilePath, KeyFilter aKeyFilter = keyFilterNone);
        std::vector<MemBuffer> extractPemPrivKeys(const std::string& aPemBuf, KeyFilter aKeyFilter = keyFilterNone);

        /**
          Extract public key in PKCS#8 SubjectPublicKeyInfo format from the DER-encoded certificate
         */
        std::vector<unsigned char> extractRSAPubKeyFile(const std::string& aDerCertPath);
        std::vector<unsigned char> extractRSAPubKey(const std::vector<unsigned char>& aDerCert);

        /**
          Extract public key in PKCS#8 SubjectPublicKeyInfo format from the PEM-encoded certificate
          openssl x509 -noout -pubkey -in cert.pem
         */
        std::vector<unsigned char> extractPemPubKeyFile(const std::string& aPemCertPath);
        std::vector<unsigned char> extractPemPubKey(const std::vector<unsigned char>& aPemCert);
        std::vector<unsigned char> extractPemPubKey(const std::string& aPemCert);

        /**
            Check whether the public RSA key in the given certificate and private RSA key belong together
        */
        bool isKeyPairFile(const std::string& aPemCertPath, const std::string& aPemKeyPath, const char* aPemKeyPasswd = NULL);
        bool isKeyPair(const std::vector<unsigned char>& aPemCert, const std::vector<unsigned char>& aPemKey, const char* aPemKeyPasswd = NULL);
        bool isKeyPair(const std::string& aPemCert, const std::string& aPemKey, const char* aPemKeyPasswd = NULL);

        //@return the number of certificates and CAs parsed from the PFX package
        // the extracted private key is encoded in PKCS#8 format ("-----BEGIN PRIVATE KEY----")
        // anExtractedPemCAs are ordered from child towards parent
        size_t parsePfx(const std::vector<unsigned char>& aPfx, const std::string& aPassword, std::string& anExtractedPemKey, std::string& anExtractedPemCert, ta::StringArray& anExtractedPemCAs);
        size_t parsePfx(const std::vector<unsigned char>& aPfx, const std::string& aPassword, std::string& anExtractedPemKey, std::string& anExtractedPemCert);
        size_t parsePfx(const std::vector<unsigned char>& aPfx, const std::string& aPassword);


        // X.509 name constraints
        struct NameConstraints
        {
            NameConstraints() {}
            NameConstraints(const ta::StringArray& aPermits, const ta::StringArray& anExcludes): permits(aPermits), excludes(anExcludes) {}

            ta::StringArray permits;
            ta::StringArray excludes;

            inline bool operator==(const NameConstraints& aRhs) const
            {
                return (permits == aRhs.permits && excludes == aRhs.excludes);
            }
        };

        // X.509 policy constraints
        static const int PolicyConstraintNone = -1;
        struct PolicyConstraints
        {
            PolicyConstraints(int aRequireExplicitPolicy = PolicyConstraintNone, int anInhibitPolicyMapping = PolicyConstraintNone)
                : requireExplicitPolicy(aRequireExplicitPolicy), inhibitPolicyMapping(anInhibitPolicyMapping)
            {}

            int requireExplicitPolicy;
            int inhibitPolicyMapping;

            inline bool operator==(const PolicyConstraints& aRhs) const
            {
                return (requireExplicitPolicy == aRhs.requireExplicitPolicy && inhibitPolicyMapping == aRhs.inhibitPolicyMapping);
            }
        };

        // serialize (extended) key usage to X509 format
        std::string x509SerializeKeyUsage(const std::vector<KeyUsage>& aKeyUsages);
        std::string x509SerializeKeyUsage(const ta::StringArray& aKeyUsages);
        // serialize basic constraints to X509 format
        std::string x509SerializeBasicConstraints(const BasicConstraints& aBasicConstraints);
        // serialize name constraints to X509 format
        std::string x509SerializeNameConstraints(const NameConstraints& aNameConstraints);
        // serialize policy constraints to X509 format
        std::string x509SerializePolicyConstraints(const PolicyConstraints& aPolicyConstraints);
        // serialize CRL (Certificate Revocation List)
        std::string x509SerializeCrlUri(const std::string& anUri);
        // serialize OCSP URI (Online Certificate Status Protocol)
        std::string x509SerializeOcspUri(const std::string& anUri);
        // serialize certificate policies to X509 format
        std::string x509SerializeCertPolicies(const ta::StringArray& aCertPolicies);

        // de-serialize (extended) key usage from X509 format
        ta::StringArray x509DeserializeKeyUsage(const std::string& aKeyUsages);
        // de-serialize basic constraints from X509 format
        BasicConstraints x509DeserializeBasicConstraints(const std::string& aBasicConstraints);
        // de-serialize name constraints from X509 format
        NameConstraints x509DeserializeNameConstraints(const std::string& aNameConstraints);
        // de-serialize policy constraints from X509 format
        PolicyConstraints x509DeserializePolicyConstraints(const std::string& aPolicyConstraints);
        // de-serialize CRL (Certificate Revocation List)
        std::string x509DeserializeCrlUri(const std::string& aCrlUri);
        // de-serialize OCSP URI (Online Certificate Status Protocol)
        std::string x509DeserializeOcspUri(const std::string& aOcspUri);
        // de-serialize certificate policies from X509 format
        ta::StringArray x509DeserializeCertPolicies(const std::string& aCertPolicies);

        // Trim whitespace and replace known OIDs with their symbolic representation
        std::string normalizeExtendedKeyUsage(const std::string& anExtendedKeyUsage);
        // Trim whitespace, replace some known OIDs with their symbolic representation, sort and remove duplicates
        ta::StringArray normalizeExtendedKeyUsages(const ta::StringArray& anExtendedKeyUsages);
        // Trim whitespace, sort and remove duplicates
        std::string normalizeSerializedCertPolicies(const std::string& aCertPolicies);


        struct Subject
        {
            Subject(const std::string& aCN,
                    const std::string& aC = "",
                    const std::string& aSt = "",
                    const std::string& aL = "",
                    const std::string& aO = "",
                    const std::string& aOU = "",
                    const std::string& aE = ""
                   )
                : cn(aCN)
                , c(aC)
                , st(aSt)
                , l(aL)
                , o(aO)
                , ou(aOU)
                , e(aE)
            {}
            std::string cn;
            std::string c;
            std::string st;
            std::string l;
            std::string o;
            std::string ou;
            std::string e;
        };
        /**
            Generate CSR for the given public key and certificate fields
            @param [in] aKeyPair RSA keypair with PEM-encoded PKCS1 public key
            @param [in] aSignatureAlgorithm signature algorithm or NULL is CSR does not need to be signed
            @return Valid pointer to X509_REQ which ought to be freed with X509_REQ_free
        */
        X509_REQ* createCSR(const ta::KeyPair& aKeyPair,
                            const Subject& aSubject,
                            const ta::SignUtils::Digest* aSignatureAlgorithm = NULL,
                            const ta::StringArray& aSAN = ta::StringArray(),
                            const std::string& aChallengePassword = "");
        std::string convX509_REQ_2Pem(X509_REQ* aReq);
        std::string parseCnFromSignedCSR(const std::string& aCsrPem);

        void validateSAN(const std::string& aSAN);

        std::string serializeSAN(const ta::StringArray& aSAN);
        // "DNS:example.com, IP: 192.168.33.1" => ["DNS:example.com", "IP:192.168.33.1"]
        ta::StringArray deserializeSAN(const std::string& aSAN);
        //  ["DNS:example.com", "IP: 192.168.33.1"] => ["example.com", "192.168.33.1"]
        ta::StringArray extractSAN_Values(const ta::StringArray& aSAN);

        // Check whether the given certificate is revoked by checking CRL included in the certificate if any
        // @param [in] aCertPath path to the PEM- or DER-encoded certificate
        bool isCertFileRevoked(const std::string& aCertPath, std::string* aWarnings = NULL);

        // Check whether the given certificate is listed int the given CRL
        // @param [in] aCertPath path to the PEM- or DER-encoded certificate
        // @param [in] aCRLs list of DER (ASN1) or PEM-encoded CRLs
        bool isCertFileRevokedForCrl(const std::string& aCertPath, const std::vector<std::vector<unsigned char> >& aCRLs);

        // Check whether the given Certificate is S/MIME compatible as per RFC 3850:
        // The certificate contains email in Subject Alternative Name
        // KU, when non-empty, should include digitalSignature and/or nonRepudiation
        // EKU, when non-empty, should include emailProtection and/or anyExtendedKeyUsage
        bool isSmimeCert(const std::string& aCertificate);
        // Get email address from S/MIME Certificate
        std::string getEmailFromSmime(const std::string& aCertificate);
    }
}
