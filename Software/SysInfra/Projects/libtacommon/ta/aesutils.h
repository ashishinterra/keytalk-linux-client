#pragma once

#include <string>
#include <vector>
#include <stdexcept>

namespace ta
{
    struct AesEncryptError : std::logic_error
    {
        explicit AesEncryptError(const std::string& aMessage = "")	: std::logic_error(aMessage) {}
    };

    struct AesDecryptError : std::logic_error
    {
        explicit AesDecryptError(const std::string& aMessage = "")	: std::logic_error(aMessage) {}
    };

    namespace AesUtils
    {
        static const size_t AllowedUserKeysizesBits[] = {128, 192, 256};

        /**
           Encrypt the input buffer with AES GCM algorithm as
           <gcm-authentication-tag><iv><ciphertext>
           @note There is no OpenSSL CLI counterpart for this kind of encryption
           @param aKeyHex[in] encryption key (HEX) with size one of the AllowedUserKeysizesBits above
           @param anUseRandomIv[in] should always be true in production environment
                                    setting the flag to false effectively uses IV consisting of NULs, which reduces security and shall only be used for testing purposes
           @throws AesEncryptError
        */
        std::string encryptGCM(const std::string& aMsg, size_t aTagSize, size_t anIvSize, const std::string& aKeyHex, bool anUseRandomIv = true);
        std::vector<unsigned char> encryptGCM(const std::vector<unsigned char>& aMsg, size_t aTagSize, size_t anIvSize, const std::string& aKeyHex, bool anUseRandomIv = true);

        /**
           Decrypt the buffer previously encrypted with encryptGCM() function above
           @note There is no OpenSSL CLI counterpart for this kind of encryption
           @param aKeyHex[in] decryption key (HEX) with size one of the AllowedUserKeysizesBits above
           @throws AesDecryptError
        */
        std::string decryptGCM(const std::string& aMsg, size_t aTagSize, size_t anIvSize, const std::string& aKeyHex);
        std::vector<unsigned char> decryptGCM(const std::vector<unsigned char>& aMsg, size_t aTagSize, size_t anIvSize, const std::string& aKeyHex);
    }
}
