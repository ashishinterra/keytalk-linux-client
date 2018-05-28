#include "aesutils.h"
#include "utils.h"
#include "strings.h"
#include "scopedresource.hpp"
#include "common.h"

#include "openssl/aes.h"
#include "openssl/evp.h"
#include <algorithm>

using std::vector;
using std::string;

namespace ta
{
    namespace AesUtils
    {
        namespace
        {
            static const size_t MinGcmTagSize = 16;

            template <class Exception>
            void validateKeySize(size_t aBits)
            {
                const vector<size_t> myAllowedUserKeySizes(AllowedUserKeysizesBits,
                        AllowedUserKeysizesBits + sizeof(AllowedUserKeysizesBits)/sizeof(AllowedUserKeysizesBits[0]));
                if (!isElemExist(aBits, myAllowedUserKeySizes))
                {
                    TA_THROW_MSG(Exception, boost::format("Invalid key size %u bits") % aBits);
                }
            }

            template <class Exception>
            void validateGcmTagSize(size_t aSize)
            {
                if (aSize < MinGcmTagSize)
                {
                    TA_THROW_MSG(Exception, boost::format("Invalid AES GCM TAG size. Actual: %u bytes. Expected: at least %u bytes") % aSize % MinGcmTagSize);
                }
            }

            template <class Exception>
            void validateGcmIvSize(size_t aSize)
            {
                if (aSize != EVP_MAX_IV_LENGTH)
                {
                    TA_THROW_MSG(Exception, boost::format("Invalid AES GCM IV size. Actual: %u bytes. Expected: %u bytes") % aSize % EVP_MAX_IV_LENGTH);
                }
            }

            //@return key in binary
            template <class Exception>
            vector<unsigned char> prepareKey(const string& aKeyHex)
            {
                validateKeySize<Exception>(4 * aKeyHex.size());
                vector<unsigned char> myKeyBin;
                try {
                    myKeyBin  = Strings::fromHex(aKeyHex);
                } catch (std::exception& e) {
                    TA_THROW_MSG(Exception, e.what());
                }
                if (myKeyBin.empty())
                    TA_THROW_MSG(Exception, "Empty key");

                return myKeyBin;
            }

            struct GcmMsg
            {
                vector<unsigned char> tag;
                vector<unsigned char> iv;
                vector<unsigned char> payload;
            };

            GcmMsg parseEncryptedGcmMsg(const vector<unsigned char>& aMsg, size_t aTagSize, size_t anIvSize)
            {
                const size_t myMinSize = aTagSize + anIvSize;
                if (aMsg.size() < myMinSize)
                {
                    TA_THROW_MSG(AesDecryptError, boost::format("Cannot decrypt message with AES GCM because it is too short. Actual size: %d, minimal required size: %d") % aMsg.size() % myMinSize);
                }

                GcmMsg myMsg;
                myMsg.tag.assign(aMsg.begin(), aMsg.begin() + aTagSize);
                myMsg.iv.assign(aMsg.begin() + aTagSize, aMsg.begin() + aTagSize + anIvSize);
                myMsg.payload.assign(aMsg.begin() + aTagSize + anIvSize, aMsg.end());

                return myMsg;
            }

            template <class Exception>
            const EVP_CIPHER* getGsmCipher(size_t aKeySizeBits)
            {
                switch (aKeySizeBits)
                {
                case 128:
                    return EVP_aes_128_gcm();
                case 192:
                    return EVP_aes_192_gcm();
                case 256:
                    return EVP_aes_256_gcm();
                default:
                    TA_THROW_MSG(Exception, boost::format("Unexpected key size for AES GCM encryption %d") % aKeySizeBits);
                }
            }

        } // unnamed ns

        ///
        /// Public API
        ///

        string encryptGCM(const string& aMsg, size_t aTagSize, size_t anIvSize, const string& aKeyHex, bool anUseRandomIv)
        {
            return vec2Str(encryptGCM(str2Vec<unsigned char>(aMsg), aTagSize, anIvSize, aKeyHex, anUseRandomIv));
        }

        vector<unsigned char> encryptGCM(const vector<unsigned char>& aMsg, size_t aTagSize, size_t anIvSize, const string& aKeyHex, bool anUseRandomIv)
        {
            validateGcmTagSize<AesEncryptError>(anIvSize);
            validateGcmIvSize<AesEncryptError>(anIvSize);
            vector<unsigned char> myKeyBin = prepareKey<AesEncryptError>(aKeyHex);
            vector<unsigned char> myEncryptedMsg(aMsg.size() + /*just in case*/EVP_MAX_BLOCK_LENGTH);

            // Initialize encryption
            EVP_CIPHER_CTX ctx;
            EVP_CIPHER_CTX_init(&ctx);

            // Init IV
            const vector<unsigned char> iv = anUseRandomIv ? ta::genRandBuf(anIvSize) : vector<unsigned char>(anIvSize, 0);

            // Set IV size since we do not use default
            ScopedResource<EVP_CIPHER_CTX*> myScopedCtx(&ctx, EVP_CIPHER_CTX_cleanup); // just wrap ctx to automatically clean it up
            EVP_CIPHER_CTX_ctrl(&ctx, EVP_CTRL_GCM_SET_IVLEN, (int)bytes2Bits(anIvSize), NULL);

            const size_t myKeyBits = bytes2Bits(myKeyBin.size());
            const EVP_CIPHER* myCipher = getGsmCipher<AesEncryptError>(myKeyBits);
            vector<unsigned char> iv_copy = iv; // make a copy because EVP_EncryptInit may change iv
            if (EVP_EncryptInit(&ctx, myCipher, getSafeBuf(myKeyBin), getSafeBuf(iv_copy)) != 1)
            {
                TA_THROW_MSG(AesEncryptError, boost::format("Failed to initialize AES GCM encryption (key size %d)") % myKeyBits);
            }
            // encrypt
            int myEncryptedSize;
            if (EVP_EncryptUpdate(&ctx, getSafeBuf(myEncryptedMsg), &myEncryptedSize, getSafeBuf(aMsg), static_cast<int>(aMsg.size())) != 1)
            {
                TA_THROW_MSG(AesEncryptError, boost::format("Failed to process AES GCM encryption (key size %d)") % myKeyBits);
            }

            // encrypt the last chunk if any
            if (!aMsg.empty())
            {
                int myLastChunkSize;
                if (EVP_EncryptFinal_ex(&ctx, getSafeBuf(myEncryptedMsg, myEncryptedSize), &myLastChunkSize) != 1)
                {
                    TA_THROW_MSG(AesEncryptError, boost::format("Failed to finalize AES GCM encryption (key size %d, message size %d)") % myKeyBits % aMsg.size());
                }
                myEncryptedSize += myLastChunkSize;
            }

            // Sanity check
            if ((size_t)myEncryptedSize != aMsg.size())
            {
                TA_THROW_MSG(AesEncryptError, boost::format("The size of cryptotext obtained with AES GCM encryption does not equal the size of the original text: %d != %d. key size %d")
                             % myEncryptedSize % aMsg.size() % myKeyBits);
            }
            myEncryptedMsg.resize(myEncryptedSize);

            // Get authentication tag
            vector<unsigned char> tag(aTagSize);
            if (EVP_CIPHER_CTX_ctrl(&ctx, EVP_CTRL_GCM_GET_TAG, static_cast<int>(tag.size()), getSafeBuf(tag)) != 1)
            {
                TA_THROW_MSG(AesEncryptError, boost::format("Failed to get authentication tag to AES GCM encrypted data of size %d. Tag size: %d, key size %d") % myEncryptedMsg.size() % tag.size() % myKeyBits);
            }

            // Prepend IV and authentication tag
            myEncryptedMsg = tag + iv + myEncryptedMsg;

            return myEncryptedMsg;
        }


        string decryptGCM(const string& aMsg, size_t aTagSize, size_t anIvSize, const string& aKeyHex)
        {
            return vec2Str(decryptGCM(str2Vec<unsigned char>(aMsg), aTagSize, anIvSize, aKeyHex));
        }

        vector<unsigned char> decryptGCM(const vector<unsigned char>& aMsg, size_t aTagSize, size_t anIvSize, const string& aKeyHex)
        {
            validateGcmTagSize<AesDecryptError>(anIvSize);
            validateGcmIvSize<AesDecryptError>(anIvSize);
            vector<unsigned char> myKeyBin = prepareKey<AesDecryptError>(aKeyHex);
            GcmMsg myEncryptedMsg = parseEncryptedGcmMsg(aMsg, aTagSize, anIvSize);

            // Initialize decryption
            EVP_CIPHER_CTX ctx;
            EVP_CIPHER_CTX_init(&ctx);

            // Set IV size since we do not use default
            ScopedResource<EVP_CIPHER_CTX*> myScopedCtx(&ctx, EVP_CIPHER_CTX_cleanup); // just wrap ctx to automatically clean it up
            EVP_CIPHER_CTX_ctrl(&ctx, EVP_CTRL_GCM_SET_IVLEN, (int)bytes2Bits(anIvSize), NULL);
            const size_t myKeyBits = bytes2Bits(myKeyBin.size());
            const EVP_CIPHER* myCipher = getGsmCipher<AesDecryptError>(myKeyBits);
            if (EVP_DecryptInit(&ctx, myCipher, getSafeBuf(myKeyBin), getSafeBuf(myEncryptedMsg.iv)) != 1)
            {
                TA_THROW_MSG(AesDecryptError, boost::format("Failed to initialize AES GCM decryption (key size %d)") % myKeyBits);
            }

            // set expected tag
            if (EVP_CIPHER_CTX_ctrl(&ctx, EVP_CTRL_GCM_SET_TAG, static_cast<int>(myEncryptedMsg.tag.size()), getSafeBuf(myEncryptedMsg.tag)) != 1)
            {
                TA_THROW_MSG(AesDecryptError, "Failed to initialize AES GCM authentication tag");
            }

            // decrypt
            vector<unsigned char> myDecryptedMsg(myEncryptedMsg.payload.size() + /*just in case*/EVP_MAX_BLOCK_LENGTH);
            int myDecryptedSize;
            if (EVP_DecryptUpdate(&ctx, getSafeBuf(myDecryptedMsg), &myDecryptedSize, getSafeBuf(myEncryptedMsg.payload), static_cast<int>(myEncryptedMsg.payload.size())) != 1)
            {
                TA_THROW_MSG(AesDecryptError, boost::format("Failed to process AES GCM decryption. Encrypted payload size: %d bytes. Key size: %d bit") % myEncryptedMsg.payload.size() % myKeyBits);
            }

            // decrypt the last chunk if any
            if (!myEncryptedMsg.payload.empty())
            {
                int myLastChunkSize;
                if (EVP_DecryptFinal(&ctx, getSafeBuf(myDecryptedMsg, myDecryptedSize), &myLastChunkSize) != 1)
                {
                    TA_THROW_MSG(AesDecryptError, boost::format("Failed to finalize AES GCM decryption. Encrypted payload size: %d bytes. Decrypted so far: %d bytes. Key size: %d bit") % myEncryptedMsg.payload.size() % myDecryptedSize % myKeyBits);
                }
                myDecryptedSize += myLastChunkSize;
            }

            if ((size_t)myDecryptedSize != myEncryptedMsg.payload.size())
            {
                TA_THROW_MSG(AesDecryptError, boost::format("The size of the text decrypted with AES GCM does not equal cryptotext size: %d != %d. key size %d") % myDecryptedSize % myEncryptedMsg.payload.size() % myKeyBits);
            }
            myDecryptedMsg.resize(myDecryptedSize);

            return myDecryptedMsg;
        }

    }
}
