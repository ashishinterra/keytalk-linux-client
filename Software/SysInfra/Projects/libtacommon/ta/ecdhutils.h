//----------------------------------------------------------------------------
//
//  Description : Declaration of the utilities providing Elliptic Curve Diffie-Hellman key generation.
//
//----------------------------------------------------------------------------
#pragma once

#include "ta/common.h"
#include <string>
#include <vector>
#include <stdexcept>

namespace ta
{
    struct EcDhError : std::logic_error
    {
        explicit EcDhError(const std::string& aMessage = "")	: std::logic_error(aMessage) {}
    };

    namespace EcDhUtils
    {
        struct EcParams
        {
            EcParams()
                : conversion_form(-1)
            {}
            EcParams(const std::string& aCurveName, int aConversionForm,  const std::string& aGenerator, const std::string& anOrder, const std::string& aCofactor,
                     const std::string& aPparamStr, const std::string& anAparamStr, const std::string& aBparamStr)
                : curve_name(aCurveName), conversion_form(aConversionForm), generator(aGenerator), order(anOrder), cofactor(aCofactor),
                  p(aPparamStr), a(anAparamStr), b(aBparamStr)
            {}
            std::string curve_name;
            int conversion_form;
            std::string generator; // HEX number
            std::string order; // HEX number
            std::string cofactor; // HEX number
            std::string p; // HEX number  (prime)
            std::string a; // HEX number
            std::string b; // HEX number
        };
        inline bool operator==(const EcParams& aLhs, const EcParams& aRhs)
        {
            return (aLhs.curve_name == aRhs.curve_name && aLhs.conversion_form == aRhs.conversion_form &&
                    aLhs.generator == aRhs.generator &&aLhs.order == aRhs.order &&aLhs.cofactor == aRhs.cofactor &&
                    aLhs.p == aRhs.p &&aLhs.a == aRhs.a &&aLhs.b == aRhs.b);
        }


        /**
            Generate ECDH public and private keys for the given curve name

            This is the first step in ECDH key exchange (Alice)

            @param[in] aCurveName curve name such as "prime256v1"
            @param[out] aPubKey ECDH public key (HEX)
            @param[out] aPrivKey ECDH private key (HEX)
            @return EC parameters used to generate the keys. These params are intended to be passed to another communication party for EC key agreement
            @throws EcDhError
            */
        EcParams generateAliceKeys(const std::string& aCurveName, std::string& aPubKey, std::string& aPrivKey);

        /**
            Generate ECDH public and private keys for the given ECDH params

            This is the second step in ECDH key exchange (Bob)

            @param[in] aEcParams ECDH params received from Alice generated new ECDH keypair
            @param[out] aPubKey ECDH public key (HEX)
            @param[out] aPrivKey ECDH private key (HEX)
            @throws EcDhError
            */
        void generateBobKeys(const EcParams& aEcParams, std::string& aPubKey, std::string& aPrivKey);

        /**
            Calculate shared ECDH key from the self private and others public key

            @param [in] aEcParams ECDH params used to create any of the keys (should be he same)
            @param[in] aOtherPubKey public key retrieved from the other's party (HEX)
            @param[in] aSelfPrivKey self private key (HEX)
            @return calculated session key (HEX)
            @throws EcDhError
        */
        std::string calcSharedKey(const EcParams& aEcParams, const std::string& aOtherPubKey, const std::string& aSelfPrivKey);

    }
}

namespace boost
{
    namespace serialization
    {
        template<class Archive>
        void serialize(Archive& ar, ta::EcDhUtils::EcParams& aEcParams, const unsigned int UNUSED(version))
        {
            ar & aEcParams.curve_name;
            ar & aEcParams.conversion_form;
            ar & aEcParams.generator;
            ar & aEcParams.order;
            ar & aEcParams.cofactor;
            ar & aEcParams.p;
            ar & aEcParams.a;
            ar & aEcParams.b;
        }
    }
}
