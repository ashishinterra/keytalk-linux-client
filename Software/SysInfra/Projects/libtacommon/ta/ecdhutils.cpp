#include "ecdhutils.h"
#include "hashutils.h"
#include "strings.h"
#include "common.h"
#include "scopedresource.hpp"
#include "openssl/objects.h"
#include "openssl/bn.h"
#include "openssl/ec.h"
#include "openssl/ecdh.h"
#include "openssl/err.h"
#include <stdexcept>

namespace ta
{
    namespace EcDhUtils
    {
        using std::vector;
        using std::string;

        namespace
        {
            //@return valid group which should be cleaned up with EC_GROUP_free
            EC_GROUP* createGroup(const EcParams& aEcParams)
            {
                int myNid = OBJ_sn2nid(aEcParams.curve_name.c_str());
                if (myNid == 0)
                    TA_THROW_MSG(EcDhError, "Unsupported elliptic curve name: " + aEcParams.curve_name);

                // Create a new group
                ta::ScopedResource<EC_GROUP*> myGroup(EC_GROUP_new_by_curve_name(myNid), EC_GROUP_free);
                if (!myGroup)
                    TA_THROW_MSG(EcDhError, boost::format("Failed to generate EC group by curve name %s (nid %d)") % aEcParams.curve_name % myNid);

                // Set conversion form
                EC_GROUP_set_point_conversion_form(myGroup, static_cast<point_conversion_form_t>(aEcParams.conversion_form));

                // Set generator params
                ScopedResource<EC_POINT*> myGenerator(EC_POINT_hex2point(myGroup, aEcParams.generator.c_str(), NULL, NULL), EC_POINT_free);
                if (!myGenerator)
                    TA_THROW_MSG(EcDhError, boost::format("Failed to convert generator from HEX for curve name %s (nid %d)") % aEcParams.curve_name % myNid);

                BIGNUM* myTmpBn = NULL;
                if (!BN_hex2bn(&myTmpBn, aEcParams.order.c_str()) || !myTmpBn)
                    TA_THROW_MSG(EcDhError, boost::format("Failed to convert order from HEX to BN for curve name %s (nid %d)") % aEcParams.curve_name % myNid);
                ScopedResource<BIGNUM*> myOrder(myTmpBn, BN_free);

                myTmpBn = NULL;
                if (!BN_hex2bn(&myTmpBn, aEcParams.cofactor.c_str()) || !myTmpBn)
                    TA_THROW_MSG(EcDhError, boost::format("Failed to convert cofactor from HEX to BN for curve name %s (nid %d)") % aEcParams.curve_name % myNid);
                ScopedResource<BIGNUM*> myCofactor(myTmpBn, BN_free);

                if (!EC_GROUP_set_generator(myGroup, myGenerator, myOrder, myCofactor))
                    TA_THROW_MSG(EcDhError, boost::format("Failed to set generator on EC group for curve name %s (nid %d)") % aEcParams.curve_name % myNid);

                // Set p, a, b
                myTmpBn = NULL;
                if (!BN_hex2bn(&myTmpBn, aEcParams.p.c_str()) || !myTmpBn)
                    TA_THROW_MSG(EcDhError, boost::format("Failed to convert p param from HEX to BN for curve name %s (nid %d)") % aEcParams.curve_name % myNid);
                ScopedResource<BIGNUM*> myPparam(myTmpBn, BN_free);

                myTmpBn = NULL;
                if (!BN_hex2bn(&myTmpBn, aEcParams.a.c_str()) || !myTmpBn)
                    TA_THROW_MSG(EcDhError, boost::format("Failed to convert a param from HEX to BN for curve name %s (nid %d)") % aEcParams.curve_name % myNid);
                ScopedResource<BIGNUM*> myAparam(myTmpBn, BN_free);

                myTmpBn = NULL;
                if (!BN_hex2bn(&myTmpBn, aEcParams.b.c_str()) || !myTmpBn)
                    TA_THROW_MSG(EcDhError, boost::format("Failed to convert b param from HEX to BN for curve name %s (nid %d)") % aEcParams.curve_name % myNid);
                ScopedResource<BIGNUM*> myBparam(myTmpBn, BN_free);

                const bool myIsPrime = (EC_METHOD_get_field_type(EC_GROUP_method_of(myGroup)) == NID_X9_62_prime_field);
                if (myIsPrime)
                {
                    if (!EC_GROUP_set_curve_GFp(myGroup, myPparam, myAparam, myBparam, NULL))
                        TA_THROW_MSG(EcDhError, boost::format("EC_GROUP_set_curve_GFp failed for curve name %s (nid %d). %s") % aEcParams.curve_name % myNid % ERR_error_string(ERR_get_error(), NULL));
                }
                else
                {
                    if (!EC_GROUP_set_curve_GF2m(myGroup, myPparam, myAparam, myBparam, NULL))
                        TA_THROW_MSG(EcDhError, boost::format("EC_GROUP_set_curve_GF2m failed for curve name %s (nid %d). %s") % aEcParams.curve_name % myNid % ERR_error_string(ERR_get_error(), NULL));
                }

                return myGroup.detach();
            }
        }

        EcParams generateAliceKeys(const std::string& aCurveName, std::string& aPubKey, std::string& aPrivKey)
        {
            int myNid = OBJ_sn2nid(aCurveName.c_str());
            if (myNid == 0)
                TA_THROW_MSG(EcDhError, "Unsupported elliptic curve name: " + aCurveName);

            // Generate new keypair
            ta::ScopedResource<EC_KEY*> myKeyPair(EC_KEY_new_by_curve_name(myNid), EC_KEY_free);
            if (!myKeyPair)
                TA_THROW_MSG(EcDhError, boost::format("Failed to allocate new EC keypair by curve name %s (nid %d)") % aCurveName % myNid);
            if (!EC_KEY_generate_key(myKeyPair))
                TA_THROW_MSG(EcDhError, boost::format("Failed to generate EC keypair by curve name %s (nid %d)") % aCurveName % myNid);

            const EC_GROUP* myGroup = EC_KEY_get0_group(myKeyPair);

            // Retrieve order from the generated keypair
            ScopedResource<BIGNUM*> myOrder(BN_new(), BN_free);
            if (!myOrder)
                TA_THROW_MSG(EcDhError, boost::format("Failed to allocate memory for order EC parameter for curve name %s (nid %d)") % aCurveName % myNid);
            if (!EC_GROUP_get_order(myGroup, myOrder, NULL))
                TA_THROW_MSG(EcDhError, boost::format("Failed to retrieve order EC parameter for curve name %s (nid %d)") % aCurveName % myNid);

            // Retrieve cofactor from the generated keypair
            ScopedResource<BIGNUM*> myCofactor(BN_new(), BN_free);
            if (!myCofactor)
                TA_THROW_MSG(EcDhError, boost::format("Failed to allocate memory for cofactor EC parameter for curve name %s (nid %d)") % aCurveName % myNid);
            if (!EC_GROUP_get_cofactor(myGroup, myCofactor, NULL))
                TA_THROW_MSG(EcDhError, boost::format("Failed to retrieve cofactor EC parameter for curve name %s (nid %d)") % aCurveName % myNid);


            // Retrieve p, a, b domain params
            ScopedResource<BIGNUM*> myAparam(BN_new(), BN_free);
            ScopedResource<BIGNUM*> myBparam(BN_new(), BN_free);
            ScopedResource<BIGNUM*> myPparam(BN_new(), BN_free);
            if (!myPparam || !myAparam || !myBparam)
                TA_THROW_MSG(EcDhError, boost::format("Failed to allocate memory for domain params for curve name %s (nid %d)") % aCurveName % myNid);
            const bool  myIsPrime = (EC_METHOD_get_field_type(EC_GROUP_method_of(myGroup)) == NID_X9_62_prime_field);
            if (myIsPrime)
            {
                if (!EC_GROUP_get_curve_GFp(myGroup, myPparam, myAparam, myBparam, NULL))
                    TA_THROW_MSG(EcDhError, boost::format("Failed to retrieve domain params (prime) for curve name %s (nid %d)") % aCurveName % myNid);
            }
            else
            {
                if (!EC_GROUP_get_curve_GF2m(myGroup, myPparam, myAparam, myBparam, NULL))
                    TA_THROW_MSG(EcDhError, boost::format("Failed to retrieve domain params (binary) for curve name %s (nid %d)") % aCurveName % myNid);
            }

            const point_conversion_form_t myConversionForm = EC_GROUP_get_point_conversion_form(myGroup);

            char* myPubKeyStr = EC_POINT_point2hex(myGroup, EC_KEY_get0_public_key(myKeyPair), myConversionForm, NULL);
            char* myPrivKeyStr = BN_bn2hex(EC_KEY_get0_private_key(myKeyPair));
            char* myOrderStr = BN_bn2hex(myOrder);
            char* myCofactorStr = BN_bn2hex(myCofactor);
            char* myGeneratorStr = EC_POINT_point2hex (myGroup, EC_GROUP_get0_generator(myGroup), myConversionForm, NULL);
            char* myAparamStr = BN_bn2hex(myAparam);
            char* myBparamStr = BN_bn2hex(myBparam);
            char* myPparamStr = BN_bn2hex(myPparam);

            aPubKey = myPubKeyStr;
            aPrivKey = myPrivKeyStr;
            const EcParams myRetVal(aCurveName, myConversionForm, myGeneratorStr, myOrderStr, myCofactorStr, myPparamStr, myAparamStr, myBparamStr);

            OPENSSL_free(myPparamStr);
            OPENSSL_free(myBparamStr);
            OPENSSL_free(myAparamStr);
            OPENSSL_free(myGeneratorStr);
            OPENSSL_free(myCofactorStr);
            OPENSSL_free(myOrderStr);
            OPENSSL_free(myPrivKeyStr);
            OPENSSL_free(myPubKeyStr);

            return myRetVal;
        }

        void generateBobKeys(const EcParams& aEcParams, std::string& aPubKey, std::string& aPrivKey)
        {
            ta::ScopedResource<EC_GROUP*> myGroup(createGroup(aEcParams), EC_GROUP_free);

            // Generate new keypair and associate it with the group
            ta::ScopedResource<EC_KEY*> myKeyPair(EC_KEY_new(), EC_KEY_free);
            if (!myKeyPair)
                TA_THROW_MSG(EcDhError, boost::format("Failed to allocate new EC keypair by curve name %s") % aEcParams.curve_name);
            if (!EC_KEY_set_group(myKeyPair, myGroup))
                TA_THROW_MSG(EcDhError, boost::format("Failed to associate a new EC keypair with the group for curve name %s") % aEcParams.curve_name);
            if (!EC_KEY_generate_key(myKeyPair))
                TA_THROW_MSG(EcDhError, boost::format("Failed to generate EC keypair by curve name %s. %s") % aEcParams.curve_name %  ERR_error_string(ERR_get_error(), NULL));

            char* myPubKeyStr = EC_POINT_point2hex(EC_KEY_get0_group(myKeyPair), EC_KEY_get0_public_key(myKeyPair),
                                                   EC_GROUP_get_point_conversion_form(EC_KEY_get0_group(myKeyPair)), NULL);
            char* myPrivKeyStr = BN_bn2hex(EC_KEY_get0_private_key(myKeyPair));

            aPubKey = myPubKeyStr;
            aPrivKey = myPrivKeyStr;

            OPENSSL_free(myPrivKeyStr);
            OPENSSL_free(myPubKeyStr);
        }


        string calcSharedKey(const EcParams& aEcParams, const string& aOtherPubKey, const string& aSelfPrivKey)
        {
            ScopedResource<EC_GROUP*> myGroup(createGroup(aEcParams), EC_GROUP_free);

            // Prepare pubkey
            ScopedResource<EC_POINT*> myOtherPubkey(EC_POINT_hex2point(myGroup, aOtherPubKey.c_str(), NULL, NULL), EC_POINT_free);
            if (!myOtherPubkey)
                TA_THROW_MSG(EcDhError, boost::format("Failed to other's pubkey from HEX. %s") % ERR_error_string(ERR_get_error(), NULL));
            ScopedResource<EC_POINT*> myPubKey(EC_POINT_new(myGroup), EC_POINT_free);
            if (!myPubKey)
                TA_THROW_MSG(EcDhError, boost::format("Failed to allocate EC public key by group. %s") % ERR_error_string(ERR_get_error(), NULL));
            if (!EC_POINT_copy(myPubKey, myOtherPubkey))
                TA_THROW_MSG(EcDhError, boost::format("Failed to copy other's EC public key. %s") % ERR_error_string(ERR_get_error(), NULL));

            // Prepare private key
            ta::ScopedResource<EC_KEY*> myKeyPair(EC_KEY_new(), EC_KEY_free);
            if (!myKeyPair)
                TA_THROW_MSG(EcDhError, "Failed to allocate new EC keypair");
            if (!EC_KEY_set_group(myKeyPair, myGroup))
                TA_THROW_MSG(EcDhError, boost::format("Failed to assign the group tho the new EC keypair. %s") % ERR_error_string(ERR_get_error(), NULL));
            BIGNUM* myTmpBn = NULL;
            if (!BN_hex2bn(&myTmpBn, aSelfPrivKey.c_str()) || !myTmpBn)
                TA_THROW_MSG(EcDhError, "Failed to convert private key from HEX to BN.");
            ScopedResource<BIGNUM*> myPrivKey(myTmpBn, BN_free);
            if (!EC_KEY_set_private_key(myKeyPair, myPrivKey))
                TA_THROW_MSG(EcDhError, boost::format("Failed to set the given privkey for the new EC keypair. %s") % ERR_error_string(ERR_get_error(), NULL));

            // Compute the shared key
            int myFieldSize = EC_GROUP_get_degree(myGroup);
            if (myFieldSize <= 0)
                TA_THROW_MSG(EcDhError, "Failed to retrieve EC group field size");
            size_t  myKeySize = (myFieldSize+7)/8;
            unsigned char* myKeyStr = (unsigned char*)OPENSSL_malloc(myKeySize);
            int myActualKeySize = ECDH_compute_key(myKeyStr, myKeySize, myPubKey, myKeyPair, NULL);
            if (myActualKeySize <= 0)
            {
                OPENSSL_free(myKeyStr);
                TA_THROW_MSG(EcDhError, boost::format("Failed to generate EC shared key. %s") %  ERR_error_string(ERR_get_error(), NULL));
            }

            string mySharedKey = Strings::toHex(myKeyStr, myActualKeySize);
            OPENSSL_free(myKeyStr);
            boost::to_upper(mySharedKey);
            return mySharedKey;
        }
    }
}
