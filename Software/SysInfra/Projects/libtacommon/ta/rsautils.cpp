#include "rsautils.h"
#include "strings.h"
#include "scopedresource.hpp"
#include "opensslwrappers.h"
#include "utils.h"
#include "common.h"

#include "openssl/rsa.h"
#include "openssl/pem.h"
#include "openssl/err.h"
#include "openssl/evp.h"
#include "openssl/ssl.h"
#include "openssl/bn.h"

#include <algorithm>
#include <cassert>

namespace ta
{
    namespace RsaUtils
    {
        using std::vector;
        using std::string;

        static const char BlockSizeSep= '#';
        static const size_t OaepPaddingSize = 42;


        //
        // Private stuff
        //
        namespace
        {
            int disable_passphrase_prompt(char *UNUSED(buf),int UNUSED(size),int UNUSED(rwflag), void *UNUSED(u))
            {
                return 0;
            }

            unsigned int getKeySizeBits(const RSA* aKey)
            {
                if (!aKey)
                {
                    TA_THROW_MSG(RsaError, "Key is NULL");
                }
                const int myBits = 8 * RSA_size(aKey);
                if (myBits <= 0)
                {
                    TA_THROW_MSG(RsaError, "Failed to retrieve key size");
                }
                return myBits;
            }

            size_t calcEncryptionBlockSize(const unsigned int aKeyBits)
            {
                const int myBlockSize = (aKeyBits/8) - (int)(Strings::toString(UINT_MAX).size() + 1) /*size prefix*/ - OaepPaddingSize;
                if (myBlockSize <= 0)
                    TA_THROW_MSG(RsaError, boost::format("Invalid block size %d (RSA key too short)") % myBlockSize);
                return  myBlockSize;
            }


            //@return valid pointer to RSA which should be freed with RSA_free
            RSA* makeRsaFromPubKey(const vector<unsigned char>& anRsaPubKey, TransportEncoding aKeyTransportEncoding, PubKeyEncoding aPubKeyEncoding)
            {
                if (anRsaPubKey.empty())
                    TA_THROW_MSG(RsaError, "Empty RSA Public Key");
                if (!isPubKeyEncoding(aPubKeyEncoding))
                    TA_THROW_MSG(RsaError, boost::format("Unsupported public key encoding: '%d'") % aPubKeyEncoding);

                const unsigned char* myKeyPtr = getSafeBuf(anRsaPubKey);
                switch (aKeyTransportEncoding)
                {
                case encPEM:
                {
                    ScopedResource<BIO*> myKeyMemBio(BIO_new(BIO_s_mem()), BIO_free);
                    int myKeySize = (int)anRsaPubKey.size();
                    int myWritten = BIO_write(myKeyMemBio, getSafeBuf(anRsaPubKey), myKeySize);
                    if (myWritten != myKeySize)
                    {
                        TA_THROW_MSG(RsaError, boost::format("BIO_write failed trying to write %1% bytes. Actually written: %2% bytes.") % myKeySize % myWritten);
                    }
                    RSA* myRsa = aPubKeyEncoding == pubkeySubjectPublicKeyInfo ? PEM_read_bio_RSA_PUBKEY(myKeyMemBio, NULL, NULL, NULL)
                                 : PEM_read_bio_RSAPublicKey(myKeyMemBio, NULL, NULL, NULL);
                    if (!myRsa)
                        TA_THROW_MSG(RsaError, boost::format("%s failed. %s") % (aPubKeyEncoding == pubkeySubjectPublicKeyInfo ? "PEM_read_bio_RSA_PUBKEY" : "PEM_read_bio_RSAPublicKey")
                                     % ERR_error_string(ERR_get_error(), NULL));
                    return myRsa;
                }
                case encDER:
                {
                    RSA* myRsa = aPubKeyEncoding == pubkeySubjectPublicKeyInfo ? d2i_RSA_PUBKEY(NULL, &myKeyPtr, (long)anRsaPubKey.size())
                                 : d2i_RSAPublicKey(NULL, &myKeyPtr, (long)anRsaPubKey.size());
                    if (!myRsa)
                        TA_THROW_MSG(RsaError, boost::format("%s failed. %s") % (aPubKeyEncoding == pubkeySubjectPublicKeyInfo ? "d2i_RSA_PUBKEY" : "d2i_RSAPublicKey")
                                     % ERR_error_string(ERR_get_error(), NULL));
                    return myRsa;
                }
                default:
                    TA_THROW_MSG(RsaError, boost::format("Unsupported encoding for RSA Public Key: '%d'") % aKeyTransportEncoding);
                }
            }

            RSA* makeRsaFromPrivKey(const vector<unsigned char>& anRsaPrivKey, TransportEncoding aKeyTransportEncoding, const char* aPemKeyPasswd)
            {
                if (anRsaPrivKey.empty())
                    TA_THROW_MSG(RsaError, "Empty RSA Private Key");
                const unsigned char* myKeyPtr = getSafeBuf(anRsaPrivKey);

                switch (aKeyTransportEncoding)
                {
                case encPEM:
                {
                    ScopedResource<BIO*> myKeyMemBio(BIO_new(BIO_s_mem()), BIO_free);
                    int myKeySize = (int)anRsaPrivKey.size();
                    int myWritten = BIO_write(myKeyMemBio, getSafeBuf(anRsaPrivKey), myKeySize);
                    if (myWritten != myKeySize)
                    {
                        TA_THROW_MSG(RsaError, boost::format("BIO_write failed trying to write %1% bytes. Actuall written: %2% bytes.") % myKeySize % myWritten);
                    }

                    char* myPasswd = aPemKeyPasswd ? new char[strlen(aPemKeyPasswd)+1] : NULL;
                    if (myPasswd)
                        strlcpy(myPasswd, aPemKeyPasswd, strlen(aPemKeyPasswd)+1);
                    RSA* myRsa = PEM_read_bio_RSAPrivateKey(myKeyMemBio, NULL, myPasswd ? NULL : disable_passphrase_prompt, myPasswd);
                    delete []myPasswd;
                    if (!myRsa)
                        TA_THROW_MSG(RsaError, boost::format("PEM_read_bio_RSAPrivateKey failed. %s") % ERR_error_string(ERR_get_error(), NULL));
                    return myRsa;
                }
                case encDER:
                {
                    RSA* myRsa = d2i_RSAPrivateKey(0, &myKeyPtr, (long)anRsaPrivKey.size());
                    if (!myRsa)
                        TA_THROW_MSG(RsaError, boost::format("d2i_RSAPrivateKey failed. %s") % ERR_error_string(ERR_get_error(), NULL));
                    return myRsa;
                }
                default:
                    TA_THROW_MSG(RsaError, boost::format("Unsupported encoding for RSA Private Key: '%d'") % aKeyTransportEncoding);
                }
            }

            ta::KeyPair rsa2KeyPair(RSA* anRsa, TransportEncoding aKeyTransportEncoding, PubKeyEncoding aPubKeyEncoding)
            {
                if (!anRsa)
                    TA_THROW_MSG(RsaError, "RSA is NULL pointer");
                if (!isTransportEncoding(aKeyTransportEncoding))
                    TA_THROW_MSG(RsaError, boost::format("Unsupported key transport encoding: '%d'") % aKeyTransportEncoding);
                if (!isPubKeyEncoding(aPubKeyEncoding))
                    TA_THROW_MSG(RsaError, boost::format("Unsupported public key encoding: '%d'") % aPubKeyEncoding);

                ta::KeyPair myRetVal;

                switch (aKeyTransportEncoding)
                {
                case encPEM:
                {
                    ScopedResource<BIO*> pubKeyBio(BIO_new(BIO_s_mem()), BIO_free);
                    switch (aPubKeyEncoding)
                    {
                    case pubkeySubjectPublicKeyInfo:
                    {
                        if (!PEM_write_bio_RSA_PUBKEY(pubKeyBio, anRsa))
                        {
                            TA_THROW_MSG(RsaError, boost::format("PEM_write_bio_RSAPublicKey failed. %s") % ERR_error_string(ERR_get_error(), NULL));
                        }
                        break;
                    }
                    case pubkeyPKCS1:
                    {
                        if (!PEM_write_bio_RSAPublicKey(pubKeyBio, anRsa))
                        {
                            TA_THROW_MSG(RsaError, boost::format("PEM_write_bio_RSAPublicKey failed. %s") % ERR_error_string(ERR_get_error(), NULL));
                        }
                        break;
                    }
                    default:
                        TA_THROW_MSG(RsaError, boost::format("Unsupported public key encoding: '%d'") % aPubKeyEncoding);

                    }// switch

                    // write public key
                    BUF_MEM *buf = NULL;
                    BIO_get_mem_ptr(pubKeyBio, &buf);
                    (void)BIO_set_close(pubKeyBio, BIO_NOCLOSE);
                    myRetVal.pubKey.assign(buf->data, buf->data + buf->length);
                    BUF_MEM_free(buf);

                    // write private key
                    ScopedResource<BIO*> privKeyBio(BIO_new(BIO_s_mem()), BIO_free);
                    if (!PEM_write_bio_RSAPrivateKey(privKeyBio, anRsa, NULL, NULL, 0, NULL, NULL))
                    {
                        TA_THROW_MSG(RsaError, "PEM_write_bio_RSAPrivateKey failed");
                    }
                    BIO_get_mem_ptr(privKeyBio, &buf);
                    (void)BIO_set_close(privKeyBio, BIO_NOCLOSE);
                    myRetVal.privKey.assign(buf->data, buf->data + buf->length);
                    BUF_MEM_free(buf);
                    return myRetVal;
                }
                case encDER:
                {
                    switch (aPubKeyEncoding)
                    {
                    case pubkeySubjectPublicKeyInfo:
                    {
                        int myPubKeyLen = i2d_RSA_PUBKEY(anRsa, NULL);
                        if (myPubKeyLen <= 0)
                            TA_THROW_MSG(RsaError, boost::format("i2d_RSA_PUBKEY failed. %s") % ERR_error_string(ERR_get_error(), NULL));
                        myRetVal.pubKey.resize(myPubKeyLen);
                        unsigned char* myTmpBuf = getSafeBuf(myRetVal.pubKey);
                        i2d_RSA_PUBKEY(anRsa, &myTmpBuf);
                        break;
                    }
                    case pubkeyPKCS1:
                    {
                        int myPubKeyLen = i2d_RSAPublicKey(anRsa, NULL);
                        if (myPubKeyLen <= 0)
                            TA_THROW_MSG(RsaError, boost::format("i2d_RSAPublicKey failed. %s") % ERR_error_string(ERR_get_error(), NULL));
                        myRetVal.pubKey.resize(myPubKeyLen);
                        unsigned char* myTmpBuf = getSafeBuf(myRetVal.pubKey);
                        i2d_RSAPublicKey(anRsa, &myTmpBuf);
                        break;
                    }
                    default:
                        TA_THROW_MSG(RsaError, boost::format("Unsupported public key encoding: '%d'") % aPubKeyEncoding);
                    }

                    int myPrivKeyLen  = i2d_RSAPrivateKey(anRsa, NULL);
                    if (myPrivKeyLen <= 0)
                        TA_THROW_MSG(RsaError, boost::format("i2d_RSAPrivateKey failed. %s") % ERR_error_string(ERR_get_error(), NULL));
                    myRetVal.privKey.resize(myPrivKeyLen);
                    unsigned char* myTmpBuf = getSafeBuf(myRetVal.privKey);
                    i2d_RSAPrivateKey(anRsa, &myTmpBuf);
                    return myRetVal;
                }
                default:
                    TA_THROW_MSG(RsaError, boost::format("Unsupported key transport encoding: '%d'") % aKeyTransportEncoding);
                } // switch
            }

            size_t calcEncryptedSize(size_t anSrcSize, const RSA* anEncKey)
            {
                const unsigned int myKeyBits = getKeySizeBits(anEncKey);
                const size_t myBlockSize = calcEncryptionBlockSize(myKeyBits);

                const size_t myBlocks = (anSrcSize / myBlockSize) + ((anSrcSize % myBlockSize) ? 1 : 0);
                return myBlocks * (myKeyBits/8);
            }

            vector<unsigned char> encryptBlockPublicWithOpenSSL(const vector<unsigned char>& aBlock, RSA* anEncKey)
            {
                if (!anEncKey)
                {
                    TA_THROW_MSG(RsaError, "Encryption key is NULL");
                }
                const unsigned int myKeySize = getKeySizeBits(anEncKey)/8;
                vector<unsigned char> myCipherText(myKeySize);
                vector<unsigned char> myBlock(aBlock); // to get rid of constness

                if (RSA_public_encrypt((int)myBlock.size(), getSafeBuf(myBlock), getSafeBuf(myCipherText), anEncKey, RSA_PKCS1_OAEP_PADDING) != (int)myKeySize)
                {
                    TA_THROW_MSG(RsaError, boost::format("RSA_public_encrypt failed. %s") % ERR_error_string(ERR_get_error(), NULL));
                }
                return myCipherText;
            }

            vector<unsigned char> decryptBlockPrivateWithOpenSSL(const unsigned char* aBlock, size_t aBlockSize, RSA* aDecKey)
            {
                if (!aBlock)
                {
                    TA_THROW_MSG(RsaError, "Decryption block is NULL");
                }
                if (!aDecKey)
                {
                    TA_THROW_MSG(RsaError, "Decryption key is NULL");
                }
                vector<unsigned char> myDecrypted(aBlockSize);
                vector<unsigned char> myBlock(aBlock, aBlock + aBlockSize);
                const int myBytesDecrypted = RSA_private_decrypt((int)aBlockSize, getSafeBuf(myBlock), getSafeBuf(myDecrypted), aDecKey, RSA_PKCS1_OAEP_PADDING);
                if (myBytesDecrypted == -1)
                {
                    TA_THROW_MSG(RsaError, boost::format("RSA_private_decrypt failed. %s") % ERR_error_string(ERR_get_error(), NULL));
                }
                if ((size_t)myBytesDecrypted > aBlockSize)
                {
                    TA_THROW_MSG(RsaError, boost::format("RSA_private_decrypt failed. Bytes decrypted: %d is bigger than block size: %d") % myBytesDecrypted % aBlockSize);
                }
                myDecrypted.resize(myBytesDecrypted);

                return myDecrypted;
            }

            vector<unsigned char> encryptBlockPublic(const char* aBlock, size_t aBlockSize, void* anEncKey, void* aCookie, EncryptBlockFunc anEncryptBlockCbk, GetEncKeyBitsFunc aGetEncKeyBitsCbk)
            {
                if (!aBlock)
                {
                    TA_THROW_MSG(RsaError, "Encryption block is NULL");
                }
                if (!anEncKey)
                {
                    TA_THROW_MSG(RsaError, "Encryption key is NULL");
                }

                const unsigned int myKeySize = (aGetEncKeyBitsCbk != NULL) ? aGetEncKeyBitsCbk(aCookie, *(const string*)anEncKey)/8
                                               : getKeySizeBits((const RSA*)anEncKey)/8;
                // Message is padded so we have to store the message size to be able to decrypt it back
                vector<unsigned char> myBlock(aBlock, aBlock + aBlockSize);
                const string myPrefix = str(boost::format("%lu%c") % aBlockSize % BlockSizeSep);
                myBlock.insert(myBlock.begin(), myPrefix.begin(), myPrefix.end());
                if (myBlock.size() + OaepPaddingSize > myKeySize)
                {
                    TA_THROW_MSG(RsaError, boost::format("Too large block size %d") % myBlock.size());
                }
                return (anEncryptBlockCbk != NULL) ? anEncryptBlockCbk(aCookie, myBlock, *(const string*)anEncKey)
                       : encryptBlockPublicWithOpenSSL(myBlock, (RSA*)anEncKey);
            }

            vector<unsigned char> encryptPublic(const vector<char>& anSrc, void* anEncKey,
                                                void* aCookie = NULL, EncryptBlockFunc anEncryptBlockCbk = NULL, GetEncKeyBitsFunc aGetEncKeyBitsCbk = NULL)
            {
                const unsigned int myKeyBits = (aGetEncKeyBitsCbk != NULL) ? aGetEncKeyBitsCbk(aCookie, *(const string*)anEncKey)
                                               : getKeySizeBits((const RSA*)anEncKey);
                const size_t myBlockSize = calcEncryptionBlockSize(myKeyBits);
                const int myBlocks = (int)((anSrc.size() / myBlockSize) + ((anSrc.size() % myBlockSize) ? 1 : 0));

                vector<unsigned char> myCipherText;
                for(int iBlock=0; iBlock < myBlocks; ++iBlock)
                {
                    if (iBlock != myBlocks-1)
                    {
                        const size_t myOffset = myBlockSize*iBlock;
                        myCipherText += encryptBlockPublic(getSafeBuf(anSrc, myOffset), myBlockSize, anEncKey, aCookie, anEncryptBlockCbk, aGetEncKeyBitsCbk);
                    }
                    else
                    {
                        const int myRemaining = (int)(anSrc.size() - myBlockSize*(myBlocks-1));
                        const size_t myOffset = anSrc.size()-myRemaining;
                        myCipherText += encryptBlockPublic(getSafeBuf(anSrc, myOffset), myRemaining, anEncKey, aCookie, anEncryptBlockCbk, aGetEncKeyBitsCbk);
                    }
                }

                return myCipherText;
            }

            vector<unsigned char> decryptBlockPrivate(const unsigned char* aBlock, size_t aBlockSize, void *aDecKey, void* aCookie, DecryptBlockFunc aDecryptBlockCbk)
            {
                vector<unsigned char> myDecrypted = (aDecryptBlockCbk != NULL) ? aDecryptBlockCbk(aCookie, aBlock, aBlockSize, *(const string*)aDecKey)
                                                    : decryptBlockPrivateWithOpenSSL(aBlock, aBlockSize, (RSA*)aDecKey);

                // Parse the message size and extract the message
                vector<unsigned char>::iterator mySepIt = std::find(myDecrypted.begin(), myDecrypted.end(), BlockSizeSep);
                if (mySepIt == myDecrypted.end())
                {
                    TA_THROW_MSG(RsaError, "No size separator found in the decrypted message");
                }
                string myMsgSizeStr(myDecrypted.begin(), mySepIt);
                size_t myMsgSize = 0;
                try { myMsgSize = Strings::parse<size_t>(myMsgSizeStr); }
                catch (std::exception&) { TA_THROW_MSG(RsaError, boost::format("Failed to parse message size from '%s'") % myMsgSizeStr);}
                ++mySepIt;
                myDecrypted.erase(myDecrypted.begin(), mySepIt);
                myDecrypted.resize(myMsgSize);

                return myDecrypted;
            }

            vector<unsigned char> decryptPrivate(const vector<unsigned char>& aCipherText, void *aDecKey,
                                                 void* aCookie = NULL, DecryptBlockFunc aDecryptBlockCbk = NULL, GetDecKeyBitsFunc aGetDecKeyBitsFunc = NULL)
            {
                if (!aDecKey)
                {
                    TA_THROW_MSG(RsaError, "Decryption key is NULL");
                }
                const unsigned int myBlockSize = (aGetDecKeyBitsFunc != NULL) ? aGetDecKeyBitsFunc(aCookie, *(const string*)aDecKey)/8
                                                 : getKeySizeBits((const RSA*)aDecKey)/8;
                if (aCipherText.size() % myBlockSize)
                {
                    TA_THROW_MSG(RsaError, boost::format("Ciphertext size %d is not multiple to the decryption block size %d") % aCipherText.size() % myBlockSize);
                }
                const size_t myBlocks = aCipherText.size() / myBlockSize;
                vector<unsigned char> myDecrypted;
                for(size_t iBlock=0; iBlock < myBlocks; ++iBlock)
                {
                    const size_t myOffset = myBlockSize*iBlock;
                    myDecrypted += decryptBlockPrivate(getSafeBuf(aCipherText, myOffset), myBlockSize, aDecKey, aCookie, aDecryptBlockCbk);
                }

                if (myDecrypted.size() > aCipherText.size())
                {
                    TA_THROW_MSG(RsaError, boost::format("Decryption failed. Total bytes decrypted: %d is bigger than ciphertext size: %d") % myDecrypted.size() % aCipherText.size());
                }
                return myDecrypted;
            }
        }

        //
        // Public API
        //

        ta::KeyPair genKeyPair(const unsigned int anRsaKeyBit, const TransportEncoding aKeyTransportEncoding, const PubKeyEncoding aPubKeyEncoding)
        {
            if (!isTransportEncoding(aKeyTransportEncoding))
            {
                TA_THROW_MSG(RsaError, boost::format("Unsupported key transport encoding supplied for generating RSA keypair: '%d'") % aKeyTransportEncoding);
            }
            if (!isPubKeyEncoding(aPubKeyEncoding))
            {
                TA_THROW_MSG(RsaError, boost::format("Unsupported public key encoding supplied for generating RSA keypair: '%d'") % aPubKeyEncoding);
            }

            ScopedResource<BIGNUM*> bne(BN_new(), BN_free);
            if (BN_set_word(bne, RSA_F4) != 1)
            {
                TA_THROW_MSG(RsaError, "Failed to initialize exponent for key generation");
            }

            ScopedResource<RSA*> myRsa(RSA_new(), RSA_free);
            if (RSA_generate_key_ex(myRsa, anRsaKeyBit, bne, NULL) != 1)
            {
                TA_THROW_MSG(RsaError, boost::format("Failed to generate %d-bit RSA keypair. %s") % anRsaKeyBit % ERR_error_string(ERR_get_error(), NULL));
            }

            return rsa2KeyPair(myRsa, aKeyTransportEncoding, aPubKeyEncoding);
        }

        bool isKeyPair(const ta::KeyPair& aKeyPair, TransportEncoding aKeyTransportEncoding, PubKeyEncoding aPubKeyEncoding, const char* aPemKeyPasswd)
        {
            ScopedResource<RSA*> myPrivRsa(makeRsaFromPrivKey(aKeyPair.privKey, aKeyTransportEncoding, aPemKeyPasswd), RSA_free);
            ScopedResource<RSA*> myPubRsa (makeRsaFromPubKey(aKeyPair.pubKey, aKeyTransportEncoding, aPubKeyEncoding), RSA_free);

#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
            const BIGNUM *priv_n, *pub_n;
            RSA_get0_key(myPrivRsa, &priv_n, NULL, NULL);
            RSA_get0_key(myPubRsa, &pub_n, NULL, NULL);

            return BN_cmp(priv_n, pub_n) == 0;
#else
            return BN_cmp(myPrivRsa->n, myPubRsa->n) == 0;
#endif
        }

        unsigned int getKeySizeBits(const std::vector<unsigned char>& aModulus, const std::vector<unsigned char>& aPubExponent)
        {
            ScopedResource<RSA*> myRsa(RSA_new(), RSA_free);

#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
            BIGNUM *n = BN_bin2bn(getSafeBuf(aModulus), aModulus.size(), NULL);
            BIGNUM *e = BN_bin2bn(getSafeBuf(aPubExponent), aPubExponent.size(), NULL);

            // RSA_set0_key will transfer memory management of n and e to myRsa
            if (RSA_set0_key(myRsa, n, e, NULL) != 1)
            {
                TA_THROW_MSG(RsaError, "Failed to retrieve RSA key size");
            }
#else
            myRsa->n = BN_bin2bn(getSafeBuf(aModulus), aModulus.size(), NULL);
            myRsa->e = BN_bin2bn(getSafeBuf(aPubExponent), aPubExponent.size(), NULL);
#endif

            return getKeySizeBits(myRsa);
        }
        unsigned int getPublicKeySizeBits(const PublicKey& aPublicKey)
        {
            return getKeySizeBits(aPublicKey.n, aPublicKey.e);
        }
        unsigned int getPrivateKeySizeBits(const PrivateKey& aPrivateKey)
        {
            return getKeySizeBits(aPrivateKey.n, aPrivateKey.e);
        }
        unsigned int getPrivateKeySizeBitsFile(const string& aPemKeyPath, const char* aPemKeyPasswd)
        {
            const PrivateKey myPrivateKey = decodePrivateKeyFile(aPemKeyPath, aPemKeyPasswd);
            return getPrivateKeySizeBits(myPrivateKey);
        }
        unsigned int getPrivateKeySizeBits(const vector<unsigned char>& aKey, const char* aPemKeyPasswd)
        {
            const PrivateKey myPrivateKey = decodePrivateKey(aKey, aPemKeyPasswd);
            return getPrivateKeySizeBits(myPrivateKey);
        }

        PrivateKey decodePrivateKey(const vector<unsigned char>& aKey, const char* aKeyPasswd)
        {
            ScopedResource<RSA*> myPrivRsa(makeRsaFromPrivKey(aKey, encPEM, aKeyPasswd), RSA_free);

#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
            // get modulus, public and private exponent
            const BIGNUM *n_bn, *e_bn, *d_bn;
            RSA_get0_key(myPrivRsa, &n_bn, &e_bn, &d_bn);
            vector<unsigned char> n(BN_num_bytes(n_bn));

            BN_bn2bin(n_bn, getSafeBuf(n));
            vector<unsigned char> e(BN_num_bytes(e_bn));
            BN_bn2bin(e_bn, getSafeBuf(e));
            vector<unsigned char> d(BN_num_bytes(d_bn));
            BN_bn2bin(d_bn, getSafeBuf(d));

            // get factors
            const BIGNUM *p_bn, *q_bn;
            RSA_get0_factors(myPrivRsa, &p_bn, &q_bn);

            vector<unsigned char> p(BN_num_bytes(p_bn));
            BN_bn2bin(p_bn, getSafeBuf(p));
            vector<unsigned char> q(BN_num_bytes(q_bn));
            BN_bn2bin(q_bn, getSafeBuf(q));

            // get params
            const BIGNUM *dmp1_bn, *dmq1_bn, *iqmp_bn;
            RSA_get0_crt_params(myPrivRsa, &dmp1_bn, &dmq1_bn, &iqmp_bn);

            vector<unsigned char> dmp1(BN_num_bytes(dmp1_bn));
            BN_bn2bin(dmp1_bn, getSafeBuf(dmp1));
            vector<unsigned char> dmq1(BN_num_bytes(dmq1_bn));
            BN_bn2bin(dmq1_bn, getSafeBuf(dmq1));
            vector<unsigned char> iqmp(BN_num_bytes(iqmp_bn));
            BN_bn2bin(iqmp_bn, getSafeBuf(iqmp));
#else
            // get modulus, public and private exponent
            vector<unsigned char> n(BN_num_bytes(myPrivRsa->n));
            BN_bn2bin(myPrivRsa->n, getSafeBuf(n));
            vector<unsigned char> e(BN_num_bytes(myPrivRsa->e));
            BN_bn2bin(myPrivRsa->e, getSafeBuf(e));
            vector<unsigned char> d(BN_num_bytes(myPrivRsa->d));
            BN_bn2bin(myPrivRsa->d, getSafeBuf(d));

            // get factors
            vector<unsigned char> p(BN_num_bytes(myPrivRsa->p));
            BN_bn2bin(myPrivRsa->p, getSafeBuf(p));
            vector<unsigned char> q(BN_num_bytes(myPrivRsa->q));
            BN_bn2bin(myPrivRsa->q, getSafeBuf(q));

            // get params
            vector<unsigned char> dmp1(BN_num_bytes(myPrivRsa->dmp1));
            BN_bn2bin(myPrivRsa->dmp1, getSafeBuf(dmp1));
            vector<unsigned char> dmq1(BN_num_bytes(myPrivRsa->dmq1));
            BN_bn2bin(myPrivRsa->dmq1, getSafeBuf(dmq1));
            vector<unsigned char> iqmp(BN_num_bytes(myPrivRsa->iqmp));
            BN_bn2bin(myPrivRsa->iqmp, getSafeBuf(iqmp));
#endif

            return PrivateKey(n, e, d, p, q, dmp1, dmq1, iqmp);
        }

        PrivateKey decodePrivateKeyFile(const std::string& aKeyPath, const char* aKeyPasswd)
        {
            if (!ta::isFileExist(aKeyPath))
            {
                TA_THROW_MSG(std::invalid_argument, boost::format("Cannot decode private key from %s. The file does not exist.") % aKeyPath);
            }
            const vector<unsigned char> myKey = ta::readData(aKeyPath);
            return decodePrivateKey(myKey, aKeyPasswd);
        }

        ta::KeyPair encodePrivateKey(const PrivateKey& aKey, PubKeyEncoding aPubKeyEncoding)
        {
            if (!isPubKeyEncoding(aPubKeyEncoding))
            {
                TA_THROW_MSG(RsaError, boost::format("Unsupported public key encoding supplied for encoding private key: '%d'") % aPubKeyEncoding);
            }

            ScopedResource<RSA*> myRsa(RSA_new(), RSA_free);

#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
            // set modulus, public and private exponent
            BIGNUM *n = BN_bin2bn(getSafeBuf(aKey.n), aKey.n.size(), NULL);
            BIGNUM *e = BN_bin2bn(getSafeBuf(aKey.e), aKey.e.size(), NULL);
            BIGNUM *d = BN_bin2bn(getSafeBuf(aKey.d), aKey.d.size(), NULL);
            // RSA_set0_key will transfer memory management of n, e and d to myRsa
            if (RSA_set0_key(myRsa, n, e, d) != 1)
            {
                TA_THROW_MSG(RsaError, "Failed to set RSA modulus, public and private exponent");
            }

            // set factors
            BIGNUM * p = BN_bin2bn(getSafeBuf(aKey.p), aKey.p.size(), NULL);
            BIGNUM * q = BN_bin2bn(getSafeBuf(aKey.q), aKey.q.size(), NULL);
            // RSA_set0_factors will transfer memory management of p and q to myRsa
            if (RSA_set0_factors(myRsa, p, q) != 1)
            {
                TA_THROW_MSG(RsaError, "Failed to set RSA factors");
            }

            // set params
            BIGNUM * dmp1 = BN_bin2bn(getSafeBuf(aKey.dmp1), aKey.dmp1.size(), NULL);
            BIGNUM * dmq1 = BN_bin2bn(getSafeBuf(aKey.dmq1), aKey.dmq1.size(), NULL);
            BIGNUM * iqmp = BN_bin2bn(getSafeBuf(aKey.iqmp), aKey.iqmp.size(), NULL);
            // RSA_set0_crt_params will transfer memory management of dmp1, dmq1 and iqmp to myRsa
            if (RSA_set0_crt_params(myRsa, dmp1, dmq1, iqmp) != 1)
            {
                TA_THROW_MSG(RsaError, "Failed to set RSA parameters");
            }
#else
            // set modulus, public and private exponent
            myRsa->n = BN_bin2bn(getSafeBuf(aKey.n), aKey.n.size(), NULL);
            myRsa->e = BN_bin2bn(getSafeBuf(aKey.e), aKey.e.size(), NULL);
            myRsa->d = BN_bin2bn(getSafeBuf(aKey.d), aKey.d.size(), NULL);

            // set factors
            myRsa->p = BN_bin2bn(getSafeBuf(aKey.p), aKey.p.size(), NULL);
            myRsa->q = BN_bin2bn(getSafeBuf(aKey.q), aKey.q.size(), NULL);

            // set params
            myRsa->dmp1 = BN_bin2bn(getSafeBuf(aKey.dmp1), aKey.dmp1.size(), NULL);
            myRsa->dmq1 = BN_bin2bn(getSafeBuf(aKey.dmq1), aKey.dmq1.size(), NULL);
            myRsa->iqmp = BN_bin2bn(getSafeBuf(aKey.iqmp), aKey.iqmp.size(), NULL);

#endif

            // check
            if (RSA_check_key(myRsa) != 1)
            {
                TA_THROW_MSG(RsaError, boost::format("The supplied private key is not valid. %s") % ERR_error_string(ERR_get_error(), NULL));
            }

            return rsa2KeyPair(myRsa, encPEM, aPubKeyEncoding);
        }


        PublicKey decodePublicKeyFile(const std::string& aKeyPath, PubKeyEncoding aPubKeyEncoding)
        {
            if (!ta::isFileExist(aKeyPath))
            {
                TA_THROW_MSG(std::invalid_argument, boost::format("Cannot decode public key from %s. The file does not exist.") % aKeyPath);
            }
            const vector<unsigned char> myKey = ta::readData(aKeyPath);
            return decodePublicKey(myKey, aPubKeyEncoding);
        }

        PublicKey decodePublicKey(const std::vector<unsigned char>& aKey, PubKeyEncoding aPubKeyEncoding)
        {
            ScopedResource<RSA*> myPubRsa (makeRsaFromPubKey(aKey, encPEM, aPubKeyEncoding), RSA_free);

#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
            const BIGNUM *n_bn, *e_bn;
            RSA_get0_key(myPubRsa, &n_bn, &e_bn, NULL);

            vector<unsigned char> n(BN_num_bytes(n_bn));
            BN_bn2bin(n_bn, getSafeBuf(n));
            vector<unsigned char> e(BN_num_bytes(e_bn));
            BN_bn2bin(e_bn, getSafeBuf(e));
#else
            vector<unsigned char> n(BN_num_bytes(myPubRsa->n));
            BN_bn2bin(myPubRsa->n, getSafeBuf(n));
            vector<unsigned char> e(BN_num_bytes(myPubRsa->e));
            BN_bn2bin(myPubRsa->e, getSafeBuf(e));
#endif

            return PublicKey(n, e);
        }

        std::vector<unsigned char> encodePublicKey(const PublicKey& aKey, PubKeyEncoding aPubKeyEncoding)
        {
            ScopedResource<RSA*> myRsa(RSA_new(), RSA_free);

#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
            // set modulus and public exponent
            BIGNUM *n = BN_bin2bn(getSafeBuf(aKey.n), aKey.n.size(), NULL);
            BIGNUM *e = BN_bin2bn(getSafeBuf(aKey.e), aKey.e.size(), NULL);
            // RSA_set0_key will transfer memory management of n and e to myRsa
            if (RSA_set0_key(myRsa, n, e, NULL) != 1)
            {
                TA_THROW_MSG(RsaError, "Failed to set RSA modulus and public exponent");
            }
#else
            myRsa->n = BN_bin2bn(getSafeBuf(aKey.n), aKey.n.size(), NULL);
            myRsa->e = BN_bin2bn(getSafeBuf(aKey.e), aKey.e.size(), NULL);
#endif

            ScopedResource<BIO*> pubKeyBio(BIO_new(BIO_s_mem()), BIO_free);
            switch (aPubKeyEncoding)
            {
            case pubkeySubjectPublicKeyInfo:
            {
                if (!PEM_write_bio_RSA_PUBKEY(pubKeyBio, myRsa))
                {
                    TA_THROW_MSG(RsaError, boost::format("PEM_write_bio_RSAPublicKey failed. %s") % ERR_error_string(ERR_get_error(), NULL));
                }
                break;
            }
            case pubkeyPKCS1:
            {
                if (!PEM_write_bio_RSAPublicKey(pubKeyBio, myRsa))
                {
                    TA_THROW_MSG(RsaError, boost::format("PEM_write_bio_RSAPublicKey failed. %s") % ERR_error_string(ERR_get_error(), NULL));
                }
                break;
            }
            default:
                TA_THROW_MSG(RsaError, boost::format("Unsupported public key encoding: '%d'") % aPubKeyEncoding);

            }// switch

            BUF_MEM *buf = NULL;
            BIO_get_mem_ptr(pubKeyBio, &buf);
            (void)BIO_set_close(pubKeyBio, BIO_NOCLOSE);
            const std::vector<unsigned char> myRetVal(buf->data, buf->data + buf->length);
            BUF_MEM_free(buf);

            if (myRetVal.empty())
            {
                TA_THROW_MSG(RsaError, "The resulted public key is empty");
            }

            return myRetVal;
        }

        std::vector<unsigned char> unwrapPrivateKey(const std::vector<unsigned char>& aKey, const std::string& aKeyPasswd)
        {
            const PrivateKey myRsaKey = decodePrivateKey(aKey, aKeyPasswd.c_str());
            return encodePrivateKey(myRsaKey, pubkeyPKCS1).privKey;
        }

        std::vector<unsigned char> unwrapPrivateKeyFile(const std::string& aKeyPath, const std::string& aKeyPasswd)
        {
            if (!ta::isFileExist(aKeyPath))
            {
                TA_THROW_MSG(std::invalid_argument, boost::format("Cannot unwrap private key from %s. The file does not exist.") % aKeyPath);
            }
            const vector<unsigned char> myKey = ta::readData(aKeyPath);
            return unwrapPrivateKey(myKey, aKeyPasswd);
        }

        std::vector<unsigned char> convPrivateKey2Pkcs8Der(const std::vector<unsigned char>& aKey)
        {
            OpenSSLPrivateKeyWrapper myPrivateKey(aKey);

            static char pass[] = "";
            ta::ScopedResource<BIO*> myPemMemBio( BIO_new(BIO_s_mem()), BIO_free);
            if (!i2d_PKCS8PrivateKey_bio(myPemMemBio, myPrivateKey, NULL, NULL, 0, 0, pass))
            {
                TA_THROW_MSG(RsaError, boost::format("i2d_PKCS8PrivateKey_bio failed for privkey and no password. %s") % ERR_error_string(ERR_get_error(), NULL));
            }
            BUF_MEM* myPrivKeyBuf = NULL;
            if (BIO_get_mem_ptr(myPemMemBio, &myPrivKeyBuf) < 0 || myPrivKeyBuf->length <= 0)
            {
                TA_THROW_MSG(RsaError, "BIO_get_mem_ptr failed");
            }
            return vector<unsigned char>(myPrivKeyBuf->data, myPrivKeyBuf->data + myPrivKeyBuf->length);
        }

        std::vector<unsigned char> pubKeyPkcs1ToPkcs8(const std::vector<unsigned char>& aPubKey)
        {
            ScopedResource<RSA*> myPubRsa(makeRsaFromPubKey(aPubKey, encPEM, pubkeyPKCS1), RSA_free);

            ScopedResource<BIO*> pubKeyBio(BIO_new(BIO_s_mem()), BIO_free);
            if (!PEM_write_bio_RSA_PUBKEY(pubKeyBio, myPubRsa))
            {
                TA_THROW_MSG(RsaError, boost::format("PEM_write_bio_RSAPublicKey failed. %s") % ERR_error_string(ERR_get_error(), NULL));
            }
            BUF_MEM *buf = NULL;
            BIO_get_mem_ptr(pubKeyBio, &buf);
            (void)BIO_set_close(pubKeyBio, BIO_NOCLOSE);
            const vector<unsigned char> myPkcs8PubKey(buf->data, buf->data + buf->length);
            BUF_MEM_free(buf);

            return myPkcs8PubKey;
        }

        vector<unsigned char> encrypt(const vector<char>& anSrc,
                                      const vector<unsigned char>& anRsaPubKey,
                                      TransportEncoding aKeyTransportEncoding,
                                      PubKeyEncoding aPubKeyEncoding)
        {
            ScopedResource<RSA*> myRsa(makeRsaFromPubKey(anRsaPubKey, aKeyTransportEncoding, aPubKeyEncoding), RSA_free);
            return encryptPublic(anSrc, myRsa);
        }

        vector<unsigned char> encrypt(const string& anSrc,
                                      const vector<unsigned char>& anRsaPubKey,
                                      TransportEncoding aKeyTransportEncoding,
                                      PubKeyEncoding aPubKeyEncoding)
        {
            return encrypt(str2Vec<char>(anSrc), anRsaPubKey, aKeyTransportEncoding, aPubKeyEncoding);
        }

        vector<unsigned char> encrypt(const vector<unsigned char>& anSrc,
                                      const std::string& anRsaPubKeyId,
                                      void* aCookie,
                                      EncryptBlockFunc anEncryptBlockCbk,
                                      GetEncKeyBitsFunc aGetEncKeyBitsCbk)
        {
            try
            {
                string myPubKeyId = anRsaPubKeyId;  // to get rid of constness
                return encryptPublic(vec2Vec<char>(anSrc), &myPubKeyId, aCookie, anEncryptBlockCbk, aGetEncKeyBitsCbk);
            }
            catch (RsaError&)
            {
                throw;
            }
            catch (std::exception& e)
            {
                TA_THROW_MSG(RsaError, e.what());
            }
        }

        size_t calcEncryptedSize(size_t anSrcSize, const std::vector<unsigned char>& anRsaPubKey, TransportEncoding aKeyTransportEncoding, PubKeyEncoding aPubKeyEncoding)
        {
            ScopedResource<RSA*> myRsa(makeRsaFromPubKey(anRsaPubKey, aKeyTransportEncoding, aPubKeyEncoding), RSA_free);
            return calcEncryptedSize(anSrcSize, myRsa);
        }

        string decrypt(const vector<unsigned char>& aCipherText,
                       const vector<unsigned char>& anRsaPrivKey,
                       TransportEncoding aKeyTransportEncoding,
                       const char* aPemKeyPasswd)
        {
            ScopedResource<RSA*> myRsa(makeRsaFromPrivKey(anRsaPrivKey, aKeyTransportEncoding, aPemKeyPasswd), RSA_free);
            return ta::vec2Str(decryptPrivate(aCipherText, myRsa));
        }
        std::vector<unsigned char> decrypt(const vector<unsigned char>& aCipherText,
                                           const std::string& anRsaPrivKeyId,
                                           void* aCookie,
                                           DecryptBlockFunc aDecryptBlockCbk,
                                           GetDecKeyBitsFunc aGetDecKeyBitsFunc)
        {
            try
            {
                string myPrivKeyId = anRsaPrivKeyId;  // to get rid of constness
                return decryptPrivate(aCipherText, &myPrivKeyId, aCookie, aDecryptBlockCbk, aGetDecKeyBitsFunc);
            }
            catch (RsaError&)
            {
                throw;
            }
            catch (std::exception& e)
            {
                TA_THROW_MSG(RsaError, e.what());
            }
        }

    }
}
