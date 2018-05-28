#pragma once

#include "common.h"
#include <vector>
#include <string>
#include <stdexcept>

namespace ta
{
    struct EcError : std::logic_error
    {
        explicit EcError(const std::string& aMessage = "")	: std::logic_error(aMessage) {}
    };

    namespace EcUtils
    {
        /**
         Generates EC keypair for prime256v1 curve and pre-generated domain parameters

         @param[in]  anEcKeyLen RSA key length (bits).
         Because currently only prime256v1 curve is supported, the only supported key size is 256 bit
         @return pair of PEM-encoded keys, public key is encoded using SubjectPublicKeyInfo format
         @throw EcError
        */
        ta::KeyPair genKeyPair(unsigned int anEcKeyLen);

        ///@note EC asymmetric keys cannot be used to encode/decode
    }
}
