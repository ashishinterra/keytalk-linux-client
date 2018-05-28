#pragma once

#include "ta/rsautils.h"

#include "boost/utility.hpp"
#include <memory>
#include <vector>
#include <string>

#include "openssl/x509.h"

//
// RAII wrappers for OpenSSL structures
//

namespace ta
{
    /**
      RAII wrapper for X509*.
      When loaded from file or buffer containing multiple certificates, the first certificate is used
     */
    class OpenSSLCertificateWrapper: boost::noncopyable
    {
    public:
        OpenSSLCertificateWrapper();
        OpenSSLCertificateWrapper(const std::string& aPemCertFilePath);
        OpenSSLCertificateWrapper(const std::vector<unsigned char>& aPemCertBuf);
        OpenSSLCertificateWrapper(X509* aCertificate);
        ~OpenSSLCertificateWrapper();

        void loadFromFile(const std::string& aPemCertFilePath);
        void loadFromBuf(const std::vector<unsigned char>& aPemCertBuf);

        operator X509*() const;
        X509* operator->() const;

    private:
        void reset(X509* aCertificate);

    private:
        X509* theCertificate;
    };


    /**
      RAII wrapper class for an private key as EVP_PKEY*
      When loaded from file or buffer containing multiple keys, the first key is used
     */
    class OpenSSLPrivateKeyWrapper: private boost::noncopyable
    {
    public:
        OpenSSLPrivateKeyWrapper();
        OpenSSLPrivateKeyWrapper(const std::string& aPemKeyFilePath, const char* aKeyPassword = NULL);
        OpenSSLPrivateKeyWrapper(const std::vector<unsigned char>& aPemKeyBuf, const char* aKeyPassword = NULL);
        ~OpenSSLPrivateKeyWrapper();

        void loadFromFile(const std::string& aPemKeyFilePath, const char* aKeyPassword = NULL);
        void loadFromBuf(const std::vector<unsigned char>& aPemKeyBuf, const char* aKeyPassword = NULL);

        operator EVP_PKEY*() const;
        EVP_PKEY* operator ->() const;

    private:
        void reset(EVP_PKEY* aKey);

    private:
        EVP_PKEY* theKey;
    };

    /**
      RAII wrapper class for an public key as EVP_PKEY*
      When loaded from buffer containing multiple keys, the first key is used
     */
    class OpenSSLPublicKeyWrapper: private boost::noncopyable
    {
    public:
        OpenSSLPublicKeyWrapper();
        OpenSSLPublicKeyWrapper(const std::vector<unsigned char>& aPemKeyBuf, const ta::RsaUtils::PubKeyEncoding anEncoding);
        ~OpenSSLPublicKeyWrapper();

        void loadFromBuf(const std::vector<unsigned char>& aPemKeyBuf, const ta::RsaUtils::PubKeyEncoding anEncoding);

        operator EVP_PKEY*() const;
        EVP_PKEY* operator ->() const;

    private:
        void reset(EVP_PKEY* aKey);

    private:
        EVP_PKEY* theKey;
    };

} // namespace ta
