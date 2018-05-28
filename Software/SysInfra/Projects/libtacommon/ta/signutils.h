#pragma once

#include "ta/rsautils.h"

#include <vector>
#include <string>
#include <stdexcept>
#include "boost/static_assert.hpp"

namespace ta
{
    namespace SignUtils
    {
        struct SignError : std::runtime_error
        {
            explicit SignError(const std::string& aMessage = "")    : std::runtime_error(aMessage.c_str()) {}
        };

        struct VerifyError : std::runtime_error
        {
            explicit VerifyError(const std::string& aMessage = "")    : std::runtime_error(aMessage.c_str()) {}
        };

        class SignatureVerifyError : public VerifyError
        {
        private:
            const std::string theCertificateFile;

        public:
            explicit SignatureVerifyError(const std::string& aCertificateFile, const std::string& aMessage = "")
                : VerifyError(aMessage),
                  theCertificateFile(aCertificateFile)  {}
            virtual ~SignatureVerifyError() throw() {}

            const std::string getCertificateFile() const { return theCertificateFile; }
        };


        enum Digest
        {
            _firstDigest,
            digestSha1 = _firstDigest,
            digestSha256,
            _lastDigest = digestSha256
        };
        const std::string DigestStrings[] = {"sha1", "sha256"};
        BOOST_STATIC_ASSERT(_firstDigest < _lastDigest);
        BOOST_STATIC_ASSERT(sizeof(DigestStrings)/sizeof(DigestStrings[0]) == _lastDigest-_firstDigest+1);

        inline std::string str(const Digest aDigest)
        {
            return DigestStrings[aDigest-_firstDigest];
        }
        inline bool isDigest(const int aDigest)
        {
            return aDigest >= _firstDigest && aDigest <= _lastDigest;
        }
        inline bool parseDigest(const std::string& aDigestStr, Digest& aDigest)
        {
            for (int i = _firstDigest; i <= _lastDigest; ++i)
            {
                Digest myDigest = static_cast<Digest>(i);
                if (str(myDigest) == aDigestStr)
                {
                    aDigest = myDigest;
                    return true;
                }
            }
            return false;
        }

        struct SignatureAlgorithm
        {
            int nid;          ///< Signature algorithm NID. Definition of NIDs can be found e.g. in obj_mac.h of OpenSSL (#include "openssl/obj_mac.h")
            std::string name; ///< Signature algorithm friendly name

            inline bool operator==(const SignatureAlgorithm& aRhs) const
            {
                return nid == aRhs.nid;
            }
        };

        // Convert NID <-> digest type
        // @note We limit ourselves to RSA encryption algorithms
        int digest2Nid(const Digest aDigestType);
        Digest nid2Digest(int aNid);

        /**
          Sign the given memory buffer using the provided digest type

          @param[in] aData Data to be signed
          @param[in] aDigestType Digest type
          @param[in] aPemPrivKeyPath/aPemPrivKey Path/buffer holding PEM private key
          @param [in] aPrivKeyPasswd password for the private key if required
          @return signed digest
          @throw SignError
         */
        std::vector<unsigned char> signDigest(const std::vector<unsigned char>& aData,
                                              Digest aDigestType,
                                              const std::string& aPemPrivKeyPath,
                                              const std::string& aPrivKeyPasswd = "");
        std::vector<unsigned char> signDigest(const std::vector<unsigned char>& aData,
                                              Digest aDigestType,
                                              const std::vector<unsigned char>& aPemPrivKey,
                                              const std::string& aPrivKeyPasswd = "");

        /**
          Create a signed PKCS#7 file from file/buffer.

          @param[in] anInputFileName/anInputBuf input file/buffer to sign
          @param[in] anOutputFileName resulted signed file. This should differ from anInputFileName.
          @param[in] aSigningCertPassword signing certificate password
          @param[in] aSigningCertWithPrivKey PEM certificate with private key with which file needs to be signed
          @param[in] aPrependSmimeSignatures if true, add SMIME header
          @throw SignError
         */
        void signPKCS7(const std::string& anInputFileName,
                       const std::string& anOutputFileName,
                       const std::string& aSigningCertPassword,
                       const std::string& aSigningCertWithPrivKey,
                       bool aPrependSmimeSignatures = true);
        std::vector<unsigned char> signPKCS7(const std::vector<unsigned char>& anInputBuf,
                                             const std::string& aSigningCertPassword,
                                             const std::string& aSigningCertWithPrivKey,
                                             bool aPrependSmimeSignatures = true);

        /**
          Verifies the signed digest with the provided public key

          @param[in] aData Data to be verified
          @param[in] aSignedDigest Digested data
          @param[in] aDigestType Digest type
          @param[in] aPemPubKeyPath/aPemPubKey path/buffer holding PEM public key
          @param[in] aPubKeyEncoding encoding of the public key
          @return true if verification is ok, false otherwise
          @throw VerifyError
         */
        bool verifyDigest(const std::vector<unsigned char>& aData,
                          const std::vector<unsigned char>& aSignedDigest,
                          Digest aDigestType,
                          const std::string& aPemPubKeyPath,
                          ta::RsaUtils::PubKeyEncoding aPubKeyEncoding = ta::RsaUtils::pubkeySubjectPublicKeyInfo);
        bool verifyDigest(const std::vector<unsigned char>& aData,
                          const std::vector<unsigned char>& aSignedDigest,
                          Digest aDigestType,
                          const std::vector<unsigned char>& aPemPubKey,
                          ta::RsaUtils::PubKeyEncoding aPubKeyEncoding = ta::RsaUtils::pubkeySubjectPublicKeyInfo);

        /**
          Verifies the signed PKCS7 file/buffer with the provided CA cert file.

          @param anInputFileName/anInputBuf input filename/buffer  (signed) to be verified
          @param aCertificateFile filename of the CA certificate file used for verification
          @param aWithSmimeHeaders if true, verify with SMIME header
          @throw VerifyError
          @return the contents of the original file before it was signed
         */
        std::string verifyPKCS7(const std::string& anInputFileName,
                                const std::string& aCertificateFile,
                                bool aWithSmimeHeaders = true);
        std::string verifyPKCS7(const std::vector<unsigned char>& anInputBuf,
                                const std::string& aCertificateFile,
                                bool aWithSmimeHeaders = true);

        /**
          Load the content of the given PKCS7 with SMIME headers file without verifying its signature

          @param anInputFileName input PKCS7 filename
          @param aWithSmimeHeaders if true, verify with SMIME header
          @throw std::exception
          @return the contents of the original file before it was signed
         */
        std::string loadNotVerifyPKCS7WithSMIME(const std::string& anInputFileName);
    }
}
