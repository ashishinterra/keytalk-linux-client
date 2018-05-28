#include "signutils.h"
#include "utils.h"
#include "opensslwrappers.h"
#include "scopedresource.hpp"
#include "common.h"

#include "openssl/evp.h"
#include "openssl/err.h"
#include "openssl/pem.h"
#include "openssl/obj_mac.h" // for NIDs
#include <cstdio>

using std::vector;
using std::string;

namespace ta
{
    namespace SignUtils
    {
        //
        // Private API
        //
        namespace
        {

            template <class Exception>
            const EVP_MD* getDigestImpl(Digest aDigestType)
            {
                switch (aDigestType)
                {
                case digestSha1: return EVP_sha1();
                case digestSha256: return EVP_sha256();
                default: TA_THROW_MSG(Exception, boost::format("Unsupported digest type %1%") % str(aDigestType));
                }
            }

            std::vector<unsigned char> signDigestImpl(const std::vector<unsigned char>& aData, Digest aDigestType, OpenSSLPrivateKeyWrapper& aPemPrivKey)
            {
                if (aData.empty())
                {
                    TA_THROW_MSG(SignError, "Cannot sign empty data");
                }

                const EVP_MD* myDigestCtx = getDigestImpl<SignError>(aDigestType);
                const unsigned int myExpectedSignedDataSize = EVP_PKEY_size(aPemPrivKey);
                if (myExpectedSignedDataSize == 0)
                {
                    TA_THROW_MSG(SignError, "Cannot sign data with zero-size key");
                }

                EVP_MD_CTX* md_ctx = EVP_MD_CTX_create(); // as of OpenSSL-1.1.0 EVP_MD_CTX is opaque and hence cannot be instantiated directly on stack
                if (!md_ctx)
                {
                    TA_THROW_MSG(SignError, "Cannot allocate EVP_MD context");
                }
                EVP_SignInit   (md_ctx, myDigestCtx);
                EVP_SignUpdate (md_ctx, getSafeBuf(aData), (unsigned int)aData.size());
                std::vector<unsigned char> mySignedData(myExpectedSignedDataSize);
                unsigned int myActualSignedDataSize;
                const int myErr = EVP_SignFinal (md_ctx, getSafeBuf(mySignedData), &myActualSignedDataSize, aPemPrivKey);
                EVP_MD_CTX_destroy(md_ctx);
                if (myErr != 1)
                {
                    TA_THROW_MSG(SignError, "Failed to sign data");
                }
                if (myActualSignedDataSize > myExpectedSignedDataSize)
                {
                    TA_THROW_MSG(SignError, boost::format("Buffer overflow while signing data, allocated %1% bytes, written %2% bytes") % myExpectedSignedDataSize % myActualSignedDataSize);
                }

                mySignedData.resize(myActualSignedDataSize);
                return mySignedData;
            }
        }

        //
        // Public API
        //


        int digest2Nid(const Digest aDigestType)
        {
            switch(aDigestType)
            {
            case digestSha1: return NID_sha1WithRSAEncryption;
            case digestSha256: return NID_sha256WithRSAEncryption;
            default: TA_THROW_MSG(std::invalid_argument, boost::format("Not supported digest: %s") % str(aDigestType));
            }
        }

        Digest nid2Digest(int aNid)
        {
            switch(aNid)
            {
            case NID_sha1WithRSAEncryption: return digestSha1;
            case  NID_sha256WithRSAEncryption: return digestSha256;
            default: TA_THROW_MSG(std::invalid_argument, boost::format("Not supported signature algorithm NID: %d") % aNid);
            }
        }

        std::vector<unsigned char> signDigest(const std::vector<unsigned char>& aData, Digest aDigestType, const std::string& aPemPrivKeyPath, const std::string& aPrivKeyPasswd)
        {
            try
            {
                OpenSSLPrivateKeyWrapper myPrivKey(aPemPrivKeyPath, aPrivKeyPasswd.c_str());
                return signDigestImpl(aData, aDigestType, myPrivKey);
            }
            catch (SignError&)
            {
                throw;
            }
            catch (std::exception& e)
            {
                TA_THROW_MSG(SignError, e.what());
            }
        }

        std::vector<unsigned char> signDigest(const std::vector<unsigned char>& aData, Digest aDigestType, const std::vector<unsigned char>& aPemPrivKey, const std::string& aPrivKeyPasswd)
        {
            try
            {
                OpenSSLPrivateKeyWrapper myPrivKey(aPemPrivKey, aPrivKeyPasswd.c_str());
                return signDigestImpl(aData, aDigestType, myPrivKey);
            }
            catch (SignError&)
            {
                throw;
            }
            catch (std::exception& e)
            {
                TA_THROW_MSG(SignError, e.what());
            }
        }

        void signPKCS7(const string& anInputFileName, const string& anOutputFileName, const string& aSigningCertPassword, const string& aSigningCertWithPrivKey, bool aPrependSmimeSignatures)
        {
            ScopedResource<FILE*> certFile(fopen(aSigningCertWithPrivKey.c_str(), "r"), fclose);
            if (!certFile)
                TA_THROW_MSG(SignError, boost::format("Failed to read signing cert '%1%'") % aSigningCertWithPrivKey);
            ScopedResource<X509*> signer(PEM_read_X509(certFile, NULL, NULL, NULL), X509_free);
            if (!signer)
                TA_THROW_MSG(SignError, boost::format("Failed to read signing PEM cert from '%1%'") % aSigningCertWithPrivKey);
            rewind(certFile);

            std::vector<char> mySigningKeyPassword = str2Vec<char>(aSigningCertPassword);
            mySigningKeyPassword.push_back('\0');
            ScopedResource<EVP_PKEY*> pkey(PEM_read_PrivateKey(certFile, NULL, NULL, (void*)getSafeBuf(mySigningKeyPassword)), EVP_PKEY_free);
            if (!pkey)
                TA_THROW_MSG(SignError, boost::format("Failed to read privkey from '%1%'") % aSigningCertWithPrivKey);

            ScopedResource<BIO*> inFile(BIO_new_file(anInputFileName.c_str(), "r"), BIO_free);
            if (!inFile)
                TA_THROW_MSG(SignError, boost::format("Failed to read input file '%1%'") % anInputFileName);

            int flags = aPrependSmimeSignatures ? PKCS7_DETACHED : 0;
            ScopedResource<PKCS7*> myP7(PKCS7_sign(signer, pkey, NULL, inFile, flags), PKCS7_free);
            if  (!myP7)
                TA_THROW_MSG(SignError, boost::format("PKCS7_sign failed. %1%") % ERR_error_string(ERR_get_error(), NULL));
            (void)BIO_reset(inFile);

            if (aPrependSmimeSignatures)
            {
                ScopedResource<BIO*> outFile(BIO_new_file(anOutputFileName.c_str(), "w"), BIO_free);
                if (!outFile)
                    TA_THROW_MSG(SignError, boost::format("Failed to open output file '%1%' for writing") % anOutputFileName);
                if (SMIME_write_PKCS7(outFile, myP7, inFile, flags) != 1)
                    TA_THROW_MSG(SignError, boost::format("SMIME_write_PKCS7 failed. %1%") % ERR_error_string(ERR_get_error(), NULL));
            }
            else
            {
                ScopedResource<FILE*> outFile(fopen(anOutputFileName.c_str(), "w"), fclose);
                if (!outFile)
                    TA_THROW_MSG(SignError, boost::format("Failed to open output file '%1%' for writing") % anOutputFileName);
                if (PEM_write_PKCS7(outFile,myP7) != 1)
                    TA_THROW_MSG(SignError, boost::format("PEM_write_PKCS7 failed. %1%") % ERR_error_string(ERR_get_error(), NULL));
            }
        }

        vector<unsigned char> signPKCS7(const vector<unsigned char>& anInputBuf, const string& aSigningCertPassword, const string& aSigningCertWithPrivKey, bool aPrependSmimeSignatures)
        {
            if (anInputBuf.empty())
                TA_THROW_MSG(SignError, "Signing buffer cannot be empty");

            ScopedResource<FILE*> certFile(fopen(aSigningCertWithPrivKey.c_str(), "r"), fclose);
            if (!certFile)
                TA_THROW_MSG(SignError, boost::format("Failed to read signing cert '%1%'") % aSigningCertWithPrivKey);
            ScopedResource<X509*> signer(PEM_read_X509(certFile, NULL, NULL, NULL), X509_free);
            if (!signer)
                TA_THROW_MSG(SignError, boost::format("Failed to read signing PEM cert from '%1%'") % aSigningCertWithPrivKey);
            rewind(certFile);

            std::vector<char> mySigningKeyPassword = str2Vec<char>(aSigningCertPassword);
            mySigningKeyPassword.push_back('\0');
            ScopedResource<EVP_PKEY*> pkey(PEM_read_PrivateKey(certFile, NULL, NULL, (void*)getSafeBuf(mySigningKeyPassword)), EVP_PKEY_free);
            if (!pkey)
                TA_THROW_MSG(SignError, boost::format("Failed to read privkey from '%1%'") % aSigningCertWithPrivKey);

            ScopedResource<BIO*> myInBio( BIO_new(BIO_s_mem()), BIO_free);
            int mySize = (int)anInputBuf.size();
            int myWritten = BIO_write(myInBio, getSafeBuf(anInputBuf), mySize);
            if (myWritten != mySize)
                TA_THROW_MSG(SignError, boost::format("BIO_write failed trying to write %d bytes. Actually written: %d bytes.") % mySize % myWritten);

            int flags = aPrependSmimeSignatures ? PKCS7_DETACHED : 0;
            ScopedResource<PKCS7*> myP7(PKCS7_sign(signer, pkey, NULL, myInBio, flags), PKCS7_free);
            if  (!myP7)
                TA_THROW_MSG(SignError, boost::format("PKCS7_sign failed. %1%") % ERR_error_string(ERR_get_error(), NULL));

            ScopedResource<BIO*> myOutBio(BIO_new(BIO_s_mem()), BIO_free);
            if (!myOutBio)
                TA_THROW_MSG(SignError,"Failed to open output BIO");
            if (aPrependSmimeSignatures)
            {
                //@note we make another BIO for SMIME_write_PKCS7(), BIO_reset(myInBio) since not to be sufficient
                ScopedResource<BIO*> myInBio2( BIO_new(BIO_s_mem()), BIO_free);
                myWritten = BIO_write(myInBio2, getSafeBuf(anInputBuf), mySize);
                if (myWritten != mySize)
                    TA_THROW_MSG(SignError, boost::format("BIO_write failed trying to write %d bytes. Actually written: %d bytes.") % mySize % myWritten);

                if (SMIME_write_PKCS7(myOutBio, myP7, myInBio2, flags) != 1)
                    TA_THROW_MSG(SignError, boost::format("SMIME_write_PKCS7 failed. %1%") % ERR_error_string(ERR_get_error(), NULL));
            }
            else
            {
                if (PEM_write_bio_PKCS7(myOutBio, myP7) != 1)
                    TA_THROW_MSG(SignError, boost::format("PEM_write_bio_PKCS7 failed. %1%") % ERR_error_string(ERR_get_error(), NULL));
            }
            BUF_MEM* myBioPtr = NULL;
            BIO_get_mem_ptr(myOutBio, &myBioPtr);
            const std::vector<unsigned char> mySignedContents(myBioPtr->data, myBioPtr->data + myBioPtr->length);
            return mySignedContents;
        }

        bool verifyDigest(const vector<unsigned char>& aData, const vector<unsigned char>& aSignedDigest, Digest aDigestType,
                          const string& aPemPubKeyPath, ta::RsaUtils::PubKeyEncoding aPubKeyEncoding)
        {
            std::vector<unsigned char> myPemPubKey;
            try
            {
                myPemPubKey = readData(aPemPubKeyPath);
            }
            catch (std::exception& e)
            {
                TA_THROW_MSG(VerifyError, boost::format("Failed to read '%1%'. %2%") % aPemPubKeyPath % e.what());
            }
            return verifyDigest(aData, aSignedDigest,  aDigestType, myPemPubKey, aPubKeyEncoding);
        }

        bool verifyDigest(const vector<unsigned char>& aData, const vector<unsigned char>& aSignedDigest, Digest aDigestType,
                          const std::vector<unsigned char>& aPemPubKey, ta::RsaUtils::PubKeyEncoding aPubKeyEncoding)
        {
            if (aData.empty())
            {
                TA_THROW_MSG(VerifyError, "Invalid verification data (empty)");
            }
            if (aSignedDigest.empty())
            {
                TA_THROW_MSG(VerifyError, "Invalid signed digest (empty)");
            }
            if (aPemPubKey.empty())
            {
                TA_THROW_MSG(VerifyError, "Invalid verification public key (empty)");
            }

            OpenSSLPublicKeyWrapper myPubKey(aPemPubKey, aPubKeyEncoding);

            const EVP_MD* myDigestCtx = getDigestImpl<VerifyError>(aDigestType);
            EVP_MD_CTX* md_ctx = EVP_MD_CTX_create();// as of OpenSSL-1.1.0 EVP_MD_CTX is opaque and hence cannot be instantiated directly on stack
            if (!md_ctx)
            {
                TA_THROW_MSG(SignError, "Cannot allocate EVP_MD context");
            }
            EVP_VerifyInit(md_ctx, myDigestCtx);
            EVP_VerifyUpdate (md_ctx, getSafeBuf(aData), (int)aData.size());
            vector<unsigned char>::size_type mySignedDigestSize = aSignedDigest.size();
            unsigned char* mySignedDigestBuf = new unsigned char[mySignedDigestSize];
            memcpy(mySignedDigestBuf, getSafeBuf(aSignedDigest), mySignedDigestSize);
            int myErr = EVP_VerifyFinal(md_ctx, mySignedDigestBuf, (unsigned int)mySignedDigestSize, myPubKey);
            delete []mySignedDigestBuf;
            EVP_MD_CTX_destroy(md_ctx);

            if (myErr < 0 || myErr > 1)
            {
                TA_THROW_MSG(VerifyError, "Failed to verify data");
            }
            const bool myIsSignatureValid = (myErr == 1);

            return myIsSignatureValid;
        }

        string verifyPKCS7(const string& anInputFileName, const string& aCertificateFile, bool aWithSmimeHeaders)
        {
            ScopedResource<X509_STORE*> myStore(X509_STORE_new(), X509_STORE_free);
            X509_LOOKUP* myLookup = X509_STORE_add_lookup(myStore,X509_LOOKUP_file());
            if (!X509_LOOKUP_load_file(myLookup,aCertificateFile.c_str(),X509_FILETYPE_PEM))
                TA_THROW_MSG(VerifyError, boost::format("Failed to open certificate file '%1%'") % aCertificateFile);

            PKCS7* myP7 = NULL;
            BIO* myContent = NULL;
            if (aWithSmimeHeaders)
            {
                ScopedResource<BIO*> mySignedFile(BIO_new_file(anInputFileName.c_str(), "r"), BIO_free);
                if (!mySignedFile)
                    TA_THROW_MSG(VerifyError, boost::format("Failed to open input file '%1%'") % anInputFileName);
                myP7 = SMIME_read_PKCS7(mySignedFile, &myContent);
                if (!myP7)
                    TA_THROW_MSG(VerifyError, boost::format("SMIME_read_PKCS7 failed for '%1%'. %2%") % anInputFileName % ERR_error_string(ERR_get_error(), NULL));
                if (!myContent)
                {
                    PKCS7_free(myP7);
                    TA_THROW_MSG(VerifyError, boost::format("SMIME_read_PKCS7 succeeded for '%1%' but returned BIO content is NULL ?!") % anInputFileName);
                }
            }
            else
            {
                ScopedResource<FILE*> mySignedFile(fopen(anInputFileName.c_str(), "r"), fclose);
                if (!mySignedFile)
                    TA_THROW_MSG(VerifyError, boost::format("Failed to open input file '%1%'") % anInputFileName);
                myP7 = PEM_read_PKCS7(mySignedFile, NULL, NULL, NULL);
                if (!myP7)
                    TA_THROW_MSG(VerifyError, boost::format("PEM_read_PKCS7 failed for '%1%'. %2%") % anInputFileName % ERR_error_string(ERR_get_error(), NULL));
            }

            // Verify the signature
            ScopedResource<BIO*> myOutBio(BIO_new(BIO_s_mem()), BIO_free);
            if (!PKCS7_verify(myP7, NULL, myStore, myContent, myOutBio, 0))
            {
                const string myErrStr = ERR_error_string(ERR_get_error(), NULL);
                PKCS7_free(myP7);
                if (myContent)
                    BIO_free(myContent);

                TA_THROW_ARG_MSG(SignatureVerifyError, aCertificateFile, boost::format("Failed to PKCS7 verify signed buffer using certificate '%1%'. PKCS7_verify error: %2%") % aCertificateFile % myErrStr);
            }

            BUF_MEM* myBioPtr = NULL;
            BIO_get_mem_ptr(myOutBio, &myBioPtr);
            const string myOrigContents(myBioPtr->data, myBioPtr->length);
            PKCS7_free(myP7);
            if (myContent)
                BIO_free(myContent);
            return myOrigContents;
        }

        string verifyPKCS7(const std::vector<unsigned char>& anInputBuf, const std::string& aCertificateFile, bool aWithSmimeHeaders)
        {
            if (anInputBuf.empty())
                TA_THROW_MSG(VerifyError, "Signed buffer cannot be empty");

            ScopedResource<X509_STORE*> myStore(X509_STORE_new(), X509_STORE_free);
            X509_LOOKUP* myLookup = X509_STORE_add_lookup(myStore,X509_LOOKUP_file());
            if (!X509_LOOKUP_load_file(myLookup,aCertificateFile.c_str(),X509_FILETYPE_PEM))
                TA_THROW_MSG(VerifyError, boost::format("Failed to open certificate file '%1%'") % aCertificateFile);

            PKCS7* myP7 = NULL;
            BIO* myContent = NULL;

            ScopedResource<BIO*> myInBio( BIO_new(BIO_s_mem()), BIO_free);
            int mySize = (int)anInputBuf.size();
            int myWritten = BIO_write(myInBio, getSafeBuf(anInputBuf), mySize);
            if (myWritten != mySize)
                TA_THROW_MSG(VerifyError, boost::format("BIO_write failed trying to write %d bytes. Actual written: %d bytes.") % mySize % myWritten);

            if (aWithSmimeHeaders)
            {
                myP7 = SMIME_read_PKCS7(myInBio, &myContent);
                if (!myP7)
                    TA_THROW_MSG(VerifyError, boost::format("SMIME_read_PKCS7 failed. %1%") % ERR_error_string(ERR_get_error(), NULL));

                if (!myContent)
                {
                    PKCS7_free(myP7);
                    TA_THROW_MSG(VerifyError, "SMIME_read_PKCS7 succeeded but returned BIO content is NULL ?!");
                }
            }
            else
            {
                myP7 = PEM_read_bio_PKCS7(myInBio, NULL, NULL, NULL);
                if (!myP7)
                    TA_THROW_MSG(VerifyError, boost::format("PEM_read_PKCS7 failed. %1%") % ERR_error_string(ERR_get_error(), NULL));
            }

            // Verify the signature
            ScopedResource<BIO*> myOutBio(BIO_new(BIO_s_mem()), BIO_free);
            if (!PKCS7_verify(myP7, NULL, myStore, myContent, myOutBio, 0))
            {
                const string myErrStr = ERR_error_string(ERR_get_error(), NULL);
                PKCS7_free(myP7);
                if (myContent)
                    BIO_free(myContent);

                TA_THROW_ARG_MSG(SignatureVerifyError, aCertificateFile, boost::format("Failed to PKCS7 verify signed buffer using certificate '%1%'. PKCS7_verify error: %2%") % aCertificateFile % myErrStr);
            }

            BUF_MEM* myBioPtr = NULL;
            BIO_get_mem_ptr(myOutBio, &myBioPtr);
            const string myOrigContents(myBioPtr->data, myBioPtr->length);
            PKCS7_free(myP7);
            if (myContent)
                BIO_free(myContent);
            return myOrigContents;
        }

        string loadNotVerifyPKCS7WithSMIME(const string& anInputFileName)
        {
            ScopedResource<BIO*> mySignedFile(BIO_new_file(anInputFileName.c_str(), "r"), BIO_free);
            if (!mySignedFile)
            {
                TA_THROW_MSG(std::invalid_argument, boost::format("Failed to open input file '%1%'") % anInputFileName);
            }

            BIO* myContent = NULL;
            PKCS7* myP7 = SMIME_read_PKCS7(mySignedFile, &myContent);
            if (!myP7)
            {
                TA_THROW_MSG(std::runtime_error, boost::format("SMIME_read_PKCS7 failed for '%1%'. %2%") % anInputFileName % ERR_error_string(ERR_get_error(), NULL));
            }
            if (!myContent)
            {
                PKCS7_free(myP7);
                TA_THROW_MSG(std::runtime_error, boost::format("SMIME_read_PKCS7 succeeded for '%1%' but returned BIO content is NULL ?!") % anInputFileName);
            }

            // Extract the content
            ScopedResource<BIO*> myOutBio(BIO_new(BIO_s_mem()), BIO_free);
            if (!PKCS7_verify(myP7, NULL, NULL, myContent, myOutBio, PKCS7_NOVERIFY|PKCS7_NOSIGS))
            {
                const string myErrStr = ERR_error_string(ERR_get_error(), NULL);
                PKCS7_free(myP7);
                if (myContent)
                {
                    BIO_free(myContent);
                }
                TA_THROW_MSG(std::runtime_error, boost::format("Failed to extract content from PKCS7 signed file %s. PKCS7_verify error: %s") % anInputFileName % myErrStr);
            }

            BUF_MEM* myBioPtr = NULL;
            BIO_get_mem_ptr(myOutBio, &myBioPtr);
            const string myOrigContents(myBioPtr->data, myBioPtr->length);
            PKCS7_free(myP7);
            if (myContent)
            {
                BIO_free(myContent);
            }
            return myOrigContents;
        }

    }// namespace SignUtils
}// namespace ta
