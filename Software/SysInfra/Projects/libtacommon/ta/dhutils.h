#pragma once

#include <string>
#include <vector>
#include <stdexcept>

namespace ta
{
    struct DhError : std::logic_error
    {
        explicit DhError(const std::string& aMessage = "")	: std::logic_error(aMessage) {}
    };

    namespace DhUtils
    {
        /**
            Generate DH public and private keys

            @param[in] aDhp DH p parameter (modulus) (HEX)
            @param[in] aDhg DH g parameter (generator). Normally "2" or "5".
            @param[out] aPubKey DH public key (HEX)
            @param[out] aPrivKey DH private key (HEX)
            @throws DhError
        */
        void generateKeys(const std::string& aDhp, const std::string& aDhg, std::string& aPubKey, std::string& aPrivKey);


        /**
            Determine number of bits in generated DH public key or DH private key

            @param[in] aDhGeneratedKey
            @return calculated length in bits
        */
        size_t determineGeneratedKeySizeInBits(const std::string& aDhGeneratedKey);


        /**
            Dh++Key	    = SHA2-256(SaltyDhKey)
            SaltyDhKey	= Salt DhKey
            DhKey	    = B^a mod p = b^A mod p

            A and B are Alice and Bob public keys; a and b are  Alice and Bob private keys.
            p is DH p parameter. SHA2-256 is a SHA2-256 hash function.


            Calculate shared key, according to the DH++ spec. above

            @param[in] aDhp DH p parameter (modulus) (HEX)
            @param[in] aDhs Salt parameter
            @param[in] aPubKey Other's party DH public key (HEX)
            @param[in] aPrivKey Self DH private key (HEX)
            @return calculated shared key (HEX)
            @throws DhError
        */
        std::string calcSharedKey(const std::string& aDhp, const std::string& aDhs, const std::string& aPubKey, const std::string& aPrivKey);


        /**
            Determine number of bits in DH Shared Key

            @param[in] aDhSharedKey
            @return calculated length in bits
        */
        size_t determineSharedKeySizeInBits(const std::string& aDhSharedKey);


        /**
          Checks whether the DH parameters (given as HEX string) coming from Alice on the first step of DH agreement are weak
          For example setting g to 1 will result the Bob's pubkey to be 1 so that the secret key will become weak as well
        */
        bool isAliceParamWeak(const std::string& aDhp, const std::string& aDhg, const std::string& aPubKey);

        /**
          Checks whether the DH pubkey (given as HEX string) coming from Bob ob the second step of DH agreement is weak.
          Weak Bob's DH key which can be e.g. because Bob received weak Alice on the first DH agreement step params and did not check them
        */
        bool isBobPubkeyWeak(const std::string& aPubKey);
    }
}
