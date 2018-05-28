#pragma once

#include "ta/common.h"

#include <vector>
#include <string>
#include <stdexcept>
#include "boost/serialization/access.hpp"

namespace ta
{
    struct RsaError : std::logic_error
    {
        explicit RsaError(const std::string& aMessage = "")    : std::logic_error(aMessage) {}
    };

    namespace RsaUtils
    {
        enum TransportEncoding
        {
            _firstTransportEncoding,
            encPEM = _firstTransportEncoding,
            encDER,
            _lastTransportEncoding = encDER
        };
        inline bool isTransportEncoding(int aVal)
        {
            return aVal >= _firstTransportEncoding && aVal <= _lastTransportEncoding;
        }

        enum PubKeyEncoding
        {
            _firstPubKeyEncoding,
            pubkeyPKCS1 = _firstPubKeyEncoding, // "-----BEGIN RSA PUBLIC KEY-----"
            pubkeySubjectPublicKeyInfo,         // "-----BEGIN PUBLIC KEY-----", see PKCS#8 for more info
            _lastPubKeyEncoding = pubkeySubjectPublicKeyInfo
        };
        inline bool isPubKeyEncoding(int aVal)
        {
            return aVal >= _firstPubKeyEncoding && aVal <= _lastPubKeyEncoding;
        }

        struct PrivateKey
        {
            PrivateKey() {}
            PrivateKey(const std::vector<unsigned char>& aN, const std::vector<unsigned char>& aE, const std::vector<unsigned char>& aD,
                       const std::vector<unsigned char>& aP, const std::vector<unsigned char>& aQ,
                       const std::vector<unsigned char>& aDmp1, const std::vector<unsigned char>& aDmq1, const std::vector<unsigned char>& aIqmp )
                : n(aN), e(aE), d(aD), p(aP), q(aQ), dmp1(aDmp1), dmq1(aDmq1), iqmp(aIqmp)
            {}

            friend class boost::serialization::access;
            template<class Archive>  void serialize(Archive& ar, const unsigned int UNUSED(version))
            {
                ar & n;
                ar & e;
                ar & d;
                ar & p;
                ar & q;
                ar & dmp1;
                ar & dmq1;
                ar & iqmp;
            }

            inline bool operator==(const PrivateKey& rhs) const
            {
                return (n==rhs.n &&
                        e==rhs.e &&
                        d==rhs.d &&
                        p==rhs.p &&
                        q==rhs.q &&
                        dmp1==rhs.dmp1 &&
                        dmq1==rhs.dmq1 &&
                        iqmp==rhs.iqmp);
            }

            std::vector<unsigned char> n;
            std::vector<unsigned char> e;
            std::vector<unsigned char> d;
            std::vector<unsigned char> p;
            std::vector<unsigned char> q;
            std::vector<unsigned char> dmp1;
            std::vector<unsigned char> dmq1;
            std::vector<unsigned char> iqmp;
        };

        struct PublicKey
        {
            PublicKey() {}
            PublicKey(const std::vector<unsigned char>& aN, const std::vector<unsigned char>& aE)
                : n(aN), e(aE)
            {}

            friend class boost::serialization::access;
            template<class Archive>  void serialize(Archive& ar, const unsigned int UNUSED(version))
            {
                ar & n;
                ar & e;
            }

            inline bool operator==(const PublicKey& rhs) const
            {
                return (n==rhs.n &&
                        e==rhs.e);
            }

            std::vector<unsigned char> n;
            std::vector<unsigned char> e;
        };

        /**
         Generates RSA keypair with private key in PKCS#5 format (“-----BEGIN RSA PRIVATE KEY----“)

         @param[in]  anRsaKeyBit RSA key length (bits)
         @param[in]  aKeyTransportEncoding desired transport key encoding
         @param[in]  aPubKeyEncoding desired public key encoding
         @throw RsaError
        */
        ta::KeyPair genKeyPair(const unsigned int anRsaKeyBit, const TransportEncoding aKeyTransportEncoding, const PubKeyEncoding aPubKeyEncoding);

        /**
            @return whether public and private key in the keypair belong together
        */
        bool isKeyPair(const ta::KeyPair& aKeyPair, TransportEncoding aKeyTransportEncoding, PubKeyEncoding aPubKeyEncoding, const char* aPemKeyPasswd = NULL);

        /*
            @return key size in bits
        */
        unsigned int getKeySizeBits(const std::vector<unsigned char>& aModulus, const std::vector<unsigned char>& aPubExponent);
        unsigned int getPublicKeySizeBits(const PublicKey& aPublicKey);
        unsigned int getPrivateKeySizeBits(const PrivateKey& aPrivateKey);
        unsigned int getPrivateKeySizeBitsFile(const std::string& aPemKeyPath, const char* aPemKeyPasswd = NULL);
        unsigned int getPrivateKeySizeBits(const std::vector<unsigned char>& aKey, const char* aPemKeyPasswd = NULL);

        // Decode PEM-encoded private key
        PrivateKey decodePrivateKey(const std::vector<unsigned char>& aKey, const char* aKeyPasswd = NULL);
        PrivateKey decodePrivateKeyFile(const std::string& aKeyPath, const char* aKeyPasswd = NULL);

        // PEM-encode private key.
        ta::KeyPair encodePrivateKey(const PrivateKey& aKey, PubKeyEncoding aPubKeyEncoding);

        // Decode PEM-encoded public key
        PublicKey decodePublicKey(const std::vector<unsigned char>& aKey, PubKeyEncoding aPubKeyEncoding);
        PublicKey decodePublicKeyFile(const std::string& aKeyPath, PubKeyEncoding aPubKeyEncoding);

        // PEM-encode public key
        std::vector<unsigned char> encodePublicKey(const PublicKey& aKey, PubKeyEncoding aPubKeyEncoding);

        // Removes the password from the given password-protected private PEM key
        std::vector<unsigned char> unwrapPrivateKey(const std::vector<unsigned char>& aKey, const std::string& aKeyPasswd);
        std::vector<unsigned char> unwrapPrivateKeyFile(const std::string& aKeyPath, const std::string& aKeyPasswd);

        // Converts PEM-encoded private key to DER-encoded PKCS#8 PrivateKeyInfo format
        std::vector<unsigned char> convPrivateKey2Pkcs8Der(const std::vector<unsigned char>& aKey);

        // Convert public key from PKCS#1 ("BEGIN RSA PUBLIC KEY") to PKCS#8 SubjectPublicKeyInfo ("BEGIN PUBLIC KEY")
        std::vector<unsigned char> pubKeyPkcs1ToPkcs8(const std::vector<unsigned char>& aPubKey);

        /**
            Encrypt memory buffer or string with EME-OAEP padding with SHA-1, MGF1 and an empty encoding parameter.
            Function prototype with callbacks is used to when encryption routines go from elsewhere

            @param[in] anSrc source buffer/string for encryption
            @param[in] anRsaPubKey RSA public key
            @param[in] aKeyTransportEncoding encoding of the RSA public key
            @param[in] aPubKeyEncoding internal public key encoding
            @param[in] anEncryptBlockCbk callback to do the actual encryption of a block.
            @param[in] aGetEncKeyBitsCbk callback to calculate encryption key size.
            @return resulted ciphertext
            @throw RsaError
        */
        typedef std::vector<unsigned char> (*EncryptBlockFunc)(void* aCookie, const std::vector<unsigned char>& aBlock, const std::string& anEncKeyId);
        typedef unsigned int (GetEncKeyBitsFunc)(void* aCookie, const std::string& anEncKeyId);
        std::vector<unsigned char> encrypt(const std::vector<char>& anSrc,
                                           const std::vector<unsigned char>& anRsaPubKey,
                                           TransportEncoding aKeyTransportEncoding,
                                           PubKeyEncoding aPubKeyEncoding);
        std::vector<unsigned char> encrypt(const std::string& anSrc,
                                           const std::vector<unsigned char>& anRsaPubKey,
                                           TransportEncoding aKeyTransportEncoding,
                                           PubKeyEncoding aPubKeyEncoding);
        std::vector<unsigned char> encrypt(const std::vector<unsigned char>& anSrc,
                                           const std::string& anRsaPubKeyId,
                                           void* aCookie,
                                           EncryptBlockFunc anEncryptBlockCbk,
                                           GetEncKeyBitsFunc aGetEncKeyBitsCbk);

        /**
            Calculate the size of the buffer encrypted with EME-OAEP padding with SHA-1, MGF1 and an empty encoding parameter.
            This function may be useful to estimate the resulted encrypted data without actually encrypting it

            @param[in] anSrcSize size of the source data for encryption
            @param[in] anRsaPubKey PEM-encoded RSA public key
            @param[in] aKeyTransportEncoding encoding of the RSA public key
            @param[in] aPubKeyEncoding internal public key encoding
            @return size of the resulted encrypted buffer
            @throw RsaError
        */
        size_t calcEncryptedSize(size_t anSrcSize, const std::vector<unsigned char>& anRsaPubKey, TransportEncoding aKeyTransportEncoding, PubKeyEncoding aPubKeyEncoding);

        /**
         Decrypt ciphertext previously created with one of 'encrypt' function above (with size embedded) with EME-OAEP padding with SHA-1, MGF1 and an empty encoding parameter
         Function prototype with callbacks is used to when decryption routines go from elsewhere

         @param[in] aCipherText ciphertext to be decrypted
         @param[in] anRsaPrivKey PEM-encoded RSA private key
         @param[in] aKeyTransportEncoding key encoding
         @param[in] aPemKeyPasswd If not NULL, specifies a password for the password-protected PEM-encoded private key
         @param[in] aDecryptBlockCbk callback to do the actual decryption of a block. When NULL, built-in decryption engine is used
         @param[in] aGetDecKeyBitsFunc callback to calculate decryption key size. When NULL, built-in encryption engine is used
         @return decrypted string
         @throw RsaError
        */
        typedef std::vector<unsigned char> (*DecryptBlockFunc)(void* aCookie, const unsigned char* aBlock, size_t aBlockSize, const std::string& anDecKeyId);
        typedef unsigned int (GetDecKeyBitsFunc)(void* aCookie, const std::string& aDecKeyId);
        std::string decrypt(const std::vector<unsigned char>& aCipherText,
                            const std::vector<unsigned char>& anRsaPrivKey,
                            TransportEncoding aKeyTransportEncoding,
                            const char* aPemKeyPasswd = NULL);
        std::vector<unsigned char> decrypt(const std::vector<unsigned char>& aCipherText,
                                           const std::string& anRsaPrivKeyId,
                                           void* aCookie,
                                           DecryptBlockFunc aDecryptBlockCbk,
                                           GetDecKeyBitsFunc aGetDecKeyBitsFunc);
    }
}
