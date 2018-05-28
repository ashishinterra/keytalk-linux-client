#include "ecutils.h"
#include "strings.h"
#include "scopedresource.hpp"
#include "common.h"
#include "openssl/x509.h"
#include "openssl/evp.h"
#include "openssl/bio.h"
#include "openssl/pem.h"
#include <algorithm>
#include <cassert>

namespace ta
{
    namespace EcUtils
    {
        using std::vector;
        using std::string;

        const size_t EcKeyLen = 256;

        // pre-generated with 'openssl ecparam -name prime256v1'
        const char EcParamsPem[] = "-----BEGIN EC PARAMETERS-----\n"
                                   "BggqhkjOPQMBBw==\n"
                                   "-----END EC PARAMETERS-----\n";

        //
        // Public API
        //

        ta::KeyPair genKeyPair(unsigned int anEcKeyLen)
        {
            if (anEcKeyLen != EcKeyLen)
                TA_THROW_MSG(EcError, boost::format("Only %d-bit EC keys are supported, %d-bit requested") % EcKeyLen % anEcKeyLen);

            ta::ScopedResource<EC_KEY*> ec_params(EC_KEY_new(), EC_KEY_free);
            if (!ec_params)
                TA_THROW_MSG(EcError, "Could not allocate EC params");

            // Load EC params from the hardcoded values
            ta::ScopedResource<BIO*> myEcParamsMemBio(BIO_new(BIO_s_mem()), BIO_free);
            if (!myEcParamsMemBio)
                TA_THROW_MSG(EcError, "Could not create memory BIO");
            ta::ScopedResource<BUF_MEM*> myEcParamsBuf(BUF_MEM_new(), BUF_MEM_free);
            if (!BUF_MEM_grow(myEcParamsBuf,sizeof(EcParamsPem)+1))
                TA_THROW_MSG(EcError, "BUF_MEM_grow failed");
            memcpy(myEcParamsBuf->data,EcParamsPem,sizeof(EcParamsPem));
            BIO_set_mem_buf(myEcParamsMemBio,(BUF_MEM*)myEcParamsBuf,BIO_NOCLOSE);

            EC_GROUP* group = PEM_read_bio_ECPKParameters(myEcParamsMemBio, NULL, NULL, NULL);
            if (!group)
                TA_THROW_MSG(EcError, "Could not load EC group");

            if (EC_KEY_set_group(ec_params, group) == 0)
            {
                EC_GROUP_free(group);
                TA_THROW_MSG(EcError, "EC_KEY_set_group failed");
            }

            // Check keysize
            EC_GROUP_free(group);
            long myKeySize = EC_GROUP_get_degree(EC_KEY_get0_group(ec_params));
            if ((size_t)myKeySize != EcKeyLen)
                TA_THROW_MSG(EcError, boost::format("Invalid EC keysize. Actual: %ld-bit, expected: %ld-bit") % myKeySize % EcKeyLen);

            // Generate EC keypair
            ScopedResource<EVP_PKEY*> pkey(EVP_PKEY_new(), EVP_PKEY_free);
            if (!pkey)
                TA_THROW_MSG(EcError, "Could not allocate EVP_PKEY");
            if (!EC_KEY_generate_key(ec_params))
                TA_THROW_MSG(EcError, "EC_KEY_generate_key failed");
            if (!EVP_PKEY_assign_EC_KEY(pkey, (EC_KEY*)ec_params))
                TA_THROW_MSG(EcError, "EVP_PKEY_assign_EC_KEY failed");
            ec_params.detach();
            if (!pkey)
                TA_THROW_MSG(EcError, "Could not create EC keypair");

            // Store privkey
            ScopedResource<BIO*> myPrivKeyMem(BIO_new(BIO_s_mem()), BIO_free);
            if (!myPrivKeyMem)
                TA_THROW_MSG(EcError, "Could not create Memory BIO");

            static char pass[] = "";
            if (!PEM_write_bio_PrivateKey(myPrivKeyMem,pkey,NULL,NULL,0,NULL,pass))
                TA_THROW_MSG(EcError, "PEM_write_bio_PrivateKey failed for EC key");

            BUF_MEM* myPemBuf = NULL;
            if (BIO_get_mem_ptr(myPrivKeyMem, &myPemBuf) < 0 || myPemBuf->length <= 0)
                TA_THROW_MSG(EcError, "BIO_get_mem_ptr failed for privkey");
            vector<unsigned char> myPrivKey(myPemBuf->data, myPemBuf->data + myPemBuf->length);

            // Store pubkey
            ScopedResource<BIO*> myPubKeyMem(BIO_new(BIO_s_mem()), BIO_free);
            if (!myPubKeyMem)
                TA_THROW_MSG(EcError, "Could not create Memory BIO");
            if (!PEM_write_bio_PUBKEY(myPubKeyMem,pkey))
                TA_THROW_MSG(EcError, "PEM_write_bio_PUBKEY failed for EC key");
            myPemBuf = NULL;
            if (BIO_get_mem_ptr(myPubKeyMem, &myPemBuf) < 0 || myPemBuf->length <= 0)
                TA_THROW_MSG(EcError, "BIO_get_mem_ptr failed for pubkey");
            vector<unsigned char> myPubKey(myPemBuf->data, myPemBuf->data + myPemBuf->length);

            return ta::KeyPair(myPrivKey, myPubKey);
        }
    }
}
