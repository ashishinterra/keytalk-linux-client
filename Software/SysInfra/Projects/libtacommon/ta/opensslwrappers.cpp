#include "opensslwrappers.h"
#include "certutils.h"
#include "scopedresource.hpp"
#include "common.h"

#include "openssl/bio.h"
#include "openssl/pem.h"
#include "openssl/err.h"

#include <stdexcept>

namespace ta
{
    // OpenSSLCertificateWrapper
    //
    OpenSSLCertificateWrapper::OpenSSLCertificateWrapper()
        : theCertificate(NULL)
    {}

    OpenSSLCertificateWrapper::OpenSSLCertificateWrapper(const std::string& aPemCertFilePath)
        : theCertificate(NULL)
    {
        loadFromFile(aPemCertFilePath);
    }

    OpenSSLCertificateWrapper::OpenSSLCertificateWrapper(const std::vector<unsigned char>& aPemCertBuf)
        : theCertificate(NULL)
    {
        loadFromBuf(aPemCertBuf);
    }

    OpenSSLCertificateWrapper::OpenSSLCertificateWrapper(X509* aCertificate)
        : theCertificate(aCertificate)
    {
        if (!theCertificate)
            TA_THROW_MSG(std::runtime_error, "Failed to create OpenSSLCertificateWrapper from NULL");
    }
    OpenSSLCertificateWrapper::~OpenSSLCertificateWrapper()
    {
        reset(NULL);
    }


    void OpenSSLCertificateWrapper::loadFromFile(const std::string& aPemCertFilePath)
    {
        if (!ta::CertUtils::fileHasPemCert(aPemCertFilePath))
        {
            TA_THROW_MSG(std::runtime_error, boost::format("No PEM certificate found in %s") % aPemCertFilePath);
        }
        ta::ScopedResource<FILE*> myCertFile(fopen (aPemCertFilePath.c_str(), "r"), fclose);
        if (!myCertFile)
        {
            TA_THROW_MSG(std::runtime_error, boost::format("Failed to open certificate file %s") % aPemCertFilePath);
        }

        X509* myCertificate = PEM_read_X509 (myCertFile, NULL, NULL, NULL);
        if (!myCertificate)
        {
            TA_THROW_MSG(std::runtime_error, boost::format("Failed to load PEM certificate from %s. PEM_read_X509 returned error: %s") % aPemCertFilePath % ERR_error_string(ERR_get_error(), NULL));
        }
        reset(myCertificate);
    }

    void OpenSSLCertificateWrapper::loadFromBuf(const std::vector<unsigned char>& aPemCertBuf)
    {
        std::string myError;
        if (!ta::CertUtils::hasPemCertEx(aPemCertBuf, myError))
        {
            TA_THROW_MSG(std::runtime_error, myError);
        }

        X509* myCertificate = ta::CertUtils::getCertX509(aPemCertBuf);
        if (!myCertificate)
        {
            TA_THROW_MSG(std::runtime_error, "Failed to read X509 PEM cert from the memory buffer");
        }
        reset(myCertificate);
    }

    void OpenSSLCertificateWrapper::reset(X509* aCertificate)
    {
        if (theCertificate)
        {
            X509_free(theCertificate);
        }
        theCertificate = aCertificate;
    }

    OpenSSLCertificateWrapper::operator X509*() const
    {
        return theCertificate;
    }
    X509* OpenSSLCertificateWrapper::operator ->() const
    {
        return theCertificate;
    }


    //
    // OpenSSLPrivateKeyWrapper
    //
    OpenSSLPrivateKeyWrapper::OpenSSLPrivateKeyWrapper()
        : theKey(NULL)
    {}

    OpenSSLPrivateKeyWrapper::OpenSSLPrivateKeyWrapper(const std::string& aPemKeyFilePath, const char* aKeyPassword)
        : theKey(NULL)
    {
        loadFromFile(aPemKeyFilePath, aKeyPassword);
    }

    OpenSSLPrivateKeyWrapper::OpenSSLPrivateKeyWrapper(const std::vector<unsigned char>& aPemKeyBuf, const char* aKeyPassword)
        : theKey(NULL)
    {
        loadFromBuf(aPemKeyBuf, aKeyPassword);
    }

    OpenSSLPrivateKeyWrapper::~OpenSSLPrivateKeyWrapper()
    {
        reset(NULL);
    }

    void OpenSSLPrivateKeyWrapper::loadFromFile(const std::string& aPemKeyFilePath, const char* aKeyPassword)
    {
        if (!ta::CertUtils::fileHasPemPrivKey(aPemKeyFilePath))
        {
            TA_THROW_MSG(std::runtime_error, boost::format("No PEM private key found in %s") % aPemKeyFilePath);
        }
        ta::ScopedResource<FILE*> myKeyFile(fopen(aPemKeyFilePath.c_str(), "r"), fclose);
        if (!myKeyFile)
        {
            TA_THROW_MSG(std::runtime_error, boost::format("Failed to open key file %s") % aPemKeyFilePath);
        }

        EVP_PKEY* myKey = NULL;
        if (aKeyPassword)
        {
            std::vector<char> myKeyPassword = str2Vec<char>(aKeyPassword); // because PEM_read_PrivateKey needs non-const password
            myKeyPassword.push_back('\0');
            myKey = PEM_read_PrivateKey(myKeyFile, NULL, NULL, getSafeBuf(myKeyPassword));
            if (!myKey)
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Failed to load password-protected private key from %s. PEM_read_PrivateKey returned error: %s") % aPemKeyFilePath % ERR_error_string(ERR_get_error(), NULL));
            }

        }
        else
        {
            char myPkPassword[] = "";/// pass empty password i.o. NULL to prevent password prompts when it turns out that the password is required
            myKey = PEM_read_PrivateKey(myKeyFile, NULL, NULL, myPkPassword);
            if (!myKey)
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Failed to load not password-protected private key from %s. PEM_read_PrivateKey returned error: %s") % aPemKeyFilePath % ERR_error_string(ERR_get_error(), NULL));
            }
        }
        reset(myKey);
    }

    void OpenSSLPrivateKeyWrapper::loadFromBuf(const std::vector<unsigned char>& aPemKeyBuf, const char* aKeyPassword)
    {
        if (!ta::CertUtils::hasPemPrivKey(aPemKeyBuf))
        {
            TA_THROW_MSG(std::runtime_error, "Invalid private key buffer");
        }

        ta::ScopedResource<BIO*> myMemBio(BIO_new(BIO_s_mem()), BIO_free);
        if (!myMemBio)
        {
            TA_THROW_MSG(std::runtime_error, "Could not create memory BIO for the key");
        }

        const int mySize = (int)aPemKeyBuf.size();
        const int myWritten = BIO_write(myMemBio, getSafeBuf(aPemKeyBuf), mySize);
        if (myWritten != mySize)
        {
            TA_THROW_MSG(std::runtime_error, boost::format("BIO_write failed trying to write %1% bytes of private key. Actuall written: %2% bytes.") % mySize % myWritten);
        }

        if (aKeyPassword)
        {
            std::vector<char> myKeyPassword = str2Vec<char>(aKeyPassword); // because PEM_read_PrivateKey needs non-const password
            myKeyPassword.push_back('\0');
            EVP_PKEY* myKey = PEM_read_bio_PrivateKey(myMemBio, NULL, NULL, getSafeBuf(myKeyPassword));
            if (!myKey)
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Could not load password-protected private key. %1%") % ERR_error_string(ERR_get_error(), NULL));
            }
            reset(myKey);
        }
        else
        {
            char myDummyPassword[] = ""; // pass empty password i.o. NULL to prevent password prompts when it turns out that the password is required
            EVP_PKEY* myKey = PEM_read_bio_PrivateKey(myMemBio, NULL, NULL, myDummyPassword);
            if (!myKey)
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Could not load not password-protected private key. %1%") % ERR_error_string(ERR_get_error(), NULL));
            }
            reset(myKey);
        }
    }

    void OpenSSLPrivateKeyWrapper::reset(EVP_PKEY* aKey)
    {
        if (theKey)
        {
            EVP_PKEY_free(theKey);
        }
        theKey = aKey;
    }

    OpenSSLPrivateKeyWrapper::operator EVP_PKEY*() const
    {
        return theKey;
    }
    EVP_PKEY* OpenSSLPrivateKeyWrapper::operator ->() const
    {
        return theKey;
    }

    //
    // OpenSSLPublicKeyWrapper
    //
    OpenSSLPublicKeyWrapper::OpenSSLPublicKeyWrapper()
        : theKey(NULL)
    {}

    OpenSSLPublicKeyWrapper::OpenSSLPublicKeyWrapper(const std::vector<unsigned char>& aPemKeyBuf, const ta::RsaUtils::PubKeyEncoding anEncoding)
        : theKey(NULL)
    {
        loadFromBuf(aPemKeyBuf, anEncoding);
    }

    OpenSSLPublicKeyWrapper::~OpenSSLPublicKeyWrapper()
    {
        reset(NULL);
    }

    void OpenSSLPublicKeyWrapper::loadFromBuf(const std::vector<unsigned char>& aPemKeyBuf, const ta::RsaUtils::PubKeyEncoding anEncoding)
    {
        if (!ta::CertUtils::hasPemPubKey(aPemKeyBuf))
        {
            TA_THROW_MSG(std::runtime_error, "Invalid public key buffer");
        }

        ta::ScopedResource<BIO*> myMemBio(BIO_new(BIO_s_mem()), BIO_free);
        if (!myMemBio)
        {
            TA_THROW_MSG(std::runtime_error, "Could not create memory BIO for the key");
        }

        const int mySize = (int)aPemKeyBuf.size();
        const int myWritten = BIO_write(myMemBio, getSafeBuf(aPemKeyBuf), mySize);
        if (myWritten != mySize)
        {
            TA_THROW_MSG(std::runtime_error, boost::format("BIO_write failed trying to write %1% bytes of private key. Actuall written: %2% bytes.") % mySize % myWritten);
        }

        ScopedResource<EVP_PKEY*> myPubPemKeyEvp;
        switch (anEncoding)
        {
        case RsaUtils::pubkeySubjectPublicKeyInfo:
        {
            myPubPemKeyEvp.assign(PEM_read_bio_PUBKEY(myMemBio, NULL, NULL, NULL), EVP_PKEY_free);
            break;
        }
        case RsaUtils::pubkeyPKCS1:
        {
            RSA* myPubKeyRsa = PEM_read_bio_RSAPublicKey(myMemBio, NULL, NULL, NULL);
            if (!myPubKeyRsa)
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Failed to read public key. %s") % ERR_error_string(ERR_get_error(), NULL));
            }
            myPubPemKeyEvp.assign(EVP_PKEY_new(), EVP_PKEY_free);
            if (!EVP_PKEY_assign_RSA(myPubPemKeyEvp, myPubKeyRsa))
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Failed to convert public key from RSA to EVP_PKEY. %s") % ERR_error_string(ERR_get_error(), NULL));
            }
            //@note myPubKeyRsa is freed by EVP_PKEY_assign_RSA()
            break;
        }
        default:
            TA_THROW_MSG(std::runtime_error, boost::format("Unsupported public key encoding %d") % anEncoding);
        }
        if (!myPubPemKeyEvp)
        {
            TA_THROW_MSG(std::runtime_error, boost::format("Failed to read public key from PEM memory buffer. %s") % ERR_error_string(ERR_get_error(), NULL));
        }
        reset(myPubPemKeyEvp.detach());
    }

    void OpenSSLPublicKeyWrapper::reset(EVP_PKEY* aKey)
    {
        if (theKey)
        {
            EVP_PKEY_free(theKey);
        }
        theKey = aKey;
    }

    OpenSSLPublicKeyWrapper::operator EVP_PKEY*() const
    {
        return theKey;
    }
    EVP_PKEY* OpenSSLPublicKeyWrapper::operator ->() const
    {
        return theKey;
    }


} // namespace ta

