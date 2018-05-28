#include "hashutils.h"
#include "strings.h"
#include "scopedresource.hpp"
#include "common.h"

#include "openssl/evp.h"
#include <stdexcept>
#ifdef _WIN32_
# include <string.h>
#else
# include <errno.h>
#endif

namespace ta
{
    namespace HashUtils
    {
        using std::string;
        using std::vector;

        //
        // Private API
        //
        namespace
        {
            enum Digest
            {
                dgstMd5, dgstSha1, dgstSha256
            };

            vector<unsigned char> getBufDigestBin(const vector<unsigned char>& aVal, Digest aDigestType)
            {
                const EVP_MD* myDigestCtx = NULL;
                switch (aDigestType)
                {
                case dgstMd5: myDigestCtx = EVP_md5(); break;
                case dgstSha1: myDigestCtx = EVP_sha1(); break;
                case dgstSha256: myDigestCtx = EVP_sha256(); break;
                default:  TA_THROW_MSG(std::invalid_argument, boost::format("Unsupported digest type %1%") % aDigestType);
                }

                EVP_MD_CTX* md_ctx = EVP_MD_CTX_create(); // as of OpenSSL-1.1.0 EVP_MD_CTX is opaque and hence cannot be instantiated directly on stack
                if (!md_ctx)
                {
                    TA_THROW_MSG(std::runtime_error, "Cannot allocate EVP_MD context");
                }
                EVP_DigestInit(md_ctx, myDigestCtx);
                EVP_DigestUpdate(md_ctx, getSafeBuf(aVal), aVal.size());

                vector<unsigned char> myHash(EVP_MAX_MD_SIZE);
                unsigned int myHashLen = 0;
                EVP_DigestFinal(md_ctx, getSafeBuf(myHash), &myHashLen);
                EVP_MD_CTX_destroy(md_ctx);
                myHash.resize(myHashLen);
                return myHash;
            }

            vector<unsigned char> getFileDigestBin(const string& aFilePath, Digest aDigestType)
            {
                ScopedResource<FILE*> myFile(fopen(aFilePath.c_str(),"rb"), fclose);
                if (!myFile)
                {
                    TA_THROW_MSG(std::runtime_error, boost::format("File %s cannot be opened for reading. %s") % aFilePath % strerror(errno));
                }

                const EVP_MD* myDigestCtx = NULL;
                switch (aDigestType)
                {
                case dgstMd5: myDigestCtx = EVP_md5(); break;
                case dgstSha1: myDigestCtx = EVP_sha1(); break;
                case dgstSha256: myDigestCtx = EVP_sha256(); break;
                default:  TA_THROW_MSG(std::invalid_argument, boost::format("Unsupported digest type %1%") % aDigestType);
                }

                EVP_MD_CTX* md_ctx = EVP_MD_CTX_create(); // as of OpenSSL-1.1.0 EVP_MD_CTX is opaque and hence cannot be instantiated directly on stack
                if (!md_ctx)
                {
                    TA_THROW_MSG(std::runtime_error, "Cannot allocate EVP_MD context");
                }
                EVP_DigestInit(md_ctx, myDigestCtx);

                char myBuf[128];
                while (!feof(myFile))
                {
                    size_t myRead = fread(myBuf, 1, sizeof(myBuf), myFile);
                    if (myRead)
                        EVP_DigestUpdate(md_ctx, myBuf, myRead);
                    else
                        break;
                }

                vector<unsigned char> myHash(EVP_MAX_MD_SIZE);
                unsigned int myHashLen = 0;
                EVP_DigestFinal(md_ctx, getSafeBuf(myHash), &myHashLen);
                EVP_MD_CTX_destroy(md_ctx);
                myHash.resize(myHashLen);
                return myHash;
            }
        }

        //
        // md5
        //

        string getMd5Hex (const std::string& aVal)
        {
            return getMd5Hex(ta::str2Vec<unsigned char>(aVal));
        }

        string getMd5Hex(const std::vector<unsigned char>& aVal)
        {
            return Strings::toHex(getBufDigestBin(aVal, dgstMd5));
        }

        vector<unsigned char> getMd5Bin(const std::string& aVal)
        {
            return getMd5Bin(ta::str2Vec<unsigned char>(aVal));
        }

        vector<unsigned char> getMd5Bin(const vector<unsigned char>& aVal)
        {
            return getBufDigestBin(aVal, dgstMd5);
        }

        string getMd5HexFile(const string& aFilePath)
        {
            return Strings::toHex(getFileDigestBin(aFilePath, dgstMd5));
        }

        vector<unsigned char> getMd5BinFile(const string& aFilePath)
        {
            return getFileDigestBin(aFilePath, dgstMd5);
        }


        //
        // sha-1
        //

        string getSha1Hex (const std::string& aVal)
        {
            return getSha1Hex(ta::str2Vec<unsigned char>(aVal));
        }

        string getSha1Hex (const std::vector<unsigned char>& aVal)
        {
            return Strings::toHex(getBufDigestBin(aVal, dgstSha1));
        }

        vector<unsigned char> getSha1Bin(const std::string& aVal)
        {
            return getSha1Bin(ta::str2Vec<unsigned char>(aVal));
        }

        vector<unsigned char> getSha1Bin (const vector<unsigned char>& aVal)
        {
            return getBufDigestBin(aVal, dgstSha1);
        }

        string getSha1HexFile(const string& aFilePath)
        {
            return Strings::toHex(getFileDigestBin(aFilePath, dgstSha1));
        }

        vector<unsigned char> getSha1BinFile(const string& aFilePath)
        {
            return getFileDigestBin(aFilePath, dgstSha1);
        }


        //
        // sha-256
        //

        string getSha256Hex (const std::string& aVal)
        {
            return getSha256Hex(ta::str2Vec<unsigned char>(aVal));
        }

        string getSha256Hex (const std::vector<unsigned char>& aVal)
        {
            return Strings::toHex(getBufDigestBin(aVal, dgstSha256));
        }

        vector<unsigned char> getSha256Bin(const std::string& aVal)
        {
            return getSha256Bin(ta::str2Vec<unsigned char>(aVal));
        }

        vector<unsigned char> getSha256Bin(const vector<unsigned char>& aVal)
        {
            return getBufDigestBin(aVal, dgstSha256);
        }

        string getSha256HexFile(const string& aFilePath)
        {
            return Strings::toHex(getFileDigestBin(aFilePath, dgstSha256));
        }

        vector<unsigned char> getSha256BinFile(const string& aFilePath)
        {
            return getFileDigestBin(aFilePath, dgstSha256);
        }

    }
}


