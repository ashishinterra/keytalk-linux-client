#include "dhutils.h"
#include "hashutils.h"
#include "strings.h"
#include "common.h"
#include "scopedresource.hpp"
#include "openssl/dh.h"
#include "openssl/bn.h"
#include <stdexcept>
#include "boost/algorithm/string.hpp"

namespace ta
{
    namespace DhUtils
    {
        using std::vector;
        using std::string;

        namespace
        {
            bool isHexBignumWeak(const string& aBigNum)
            {
                // consider 0 and 1 weak
                const string myBigNum = boost::trim_copy(aBigNum);
                const char* p = myBigNum.c_str();
                // trim leading zeroes
                while (*p=='0')
                    ++p;
                if (!strlen(p) || strcmp(p,"1")==0)
                    return true;
                return false;
            }
        }

        void generateKeys(const string& aDhp, const string& aDhg, string& aPubKey, string& aPrivKey)
        {
            DH* myDhParams = DH_new();
            ScopedResource<DH*> myDhParamsRaii(myDhParams, DH_free);// just for automatic cleanup

            if (!BN_hex2bn(&myDhParams->p, aDhp.c_str()))
            {
                TA_THROW_MSG(DhError, boost::format("Failed to read DH p parameter '%s'") % aDhp);
            }
            if (!BN_hex2bn(&myDhParams->g, aDhg.c_str()))
            {
                TA_THROW_MSG(DhError, boost::format("Failed to read DH g parameter '%s'") % aDhg);
            }
            if (DH_generate_key(myDhParams) != 1)
            {
                TA_THROW_MSG(DhError, "DH_generate_key failed");
            }

            int myDhCheckResult;
            if (!DH_check(myDhParams, &myDhCheckResult))
            {
                TA_THROW_MSG(DhError, boost::format("DH_generate_key produced weak params. Check result: %d") % myDhCheckResult);
            }

            char* pubStr = BN_bn2hex(myDhParams->pub_key);
            char* privStr = BN_bn2hex(myDhParams->priv_key);
            aPubKey = pubStr;
            aPrivKey = privStr;
            OPENSSL_free(pubStr);
            OPENSSL_free(privStr);
        }


        size_t determineGeneratedKeySizeInBits(const std::string& aDhGeneratedKey)
        {
            return aDhGeneratedKey.size() * 4;
        }


        string calcSharedKey(const string& aDhp, const string& aDhs, const string& aPubKeyStr, const string& aPrivKeyStr)
        {
            BIGNUM* pub_key = BN_new();
            ScopedResource<BIGNUM*> pub_key_raii(pub_key, BN_free);// just for automatic cleanup
            if (!BN_hex2bn (&pub_key, aPubKeyStr.c_str()))
            {
                TA_THROW_MSG(DhError, "Failed to read BOB public key");
            }

            DH* myDhParams = DH_new();
            ScopedResource<DH*> myDhParamsRaii(myDhParams, DH_free);// just for automatic cleanup
            if (!BN_hex2bn(&myDhParams->p, aDhp.c_str()))
            {
                TA_THROW_MSG(DhError, "Failed to read DH p parameter");
            }
            if (!BN_hex2bn(&myDhParams->priv_key, aPrivKeyStr.c_str()))
            {
                TA_THROW_MSG(DhError, "Failed to read Alice provate key");
            }
            if (DH_size(myDhParams) <= 0)
            {
                TA_THROW_MSG(DhError, "Invalid DH params");
            }
            vector<unsigned char> mySharedKey(DH_size(myDhParams));
            const int len = DH_compute_key(ta::getSafeBuf(mySharedKey), pub_key, myDhParams);
            if (len <= 0 || len > DH_size(myDhParams))
            {
                TA_THROW_MSG(DhError, "DH_compute_key failed");
            }

            mySharedKey.resize(len);
            string mySharedKeyStr = Strings::toHex(mySharedKey);
            boost::to_upper(mySharedKeyStr);
            mySharedKeyStr = aDhs+mySharedKeyStr;
            mySharedKey.assign(mySharedKeyStr.begin(), mySharedKeyStr.end());
            mySharedKey.push_back('\0');

            return HashUtils::getSha256Hex(mySharedKey);
        }


        size_t determineSharedKeySizeInBits(const std::string& aDhSharedKey)
        {
            return aDhSharedKey.size() * 4;
        }


        bool isAliceParamWeak(const std::string& aDhp, const std::string& aDhg, const std::string& aPubKey)
        {
            // An additional (paranoidal?) check to DH_check() called by generateKeys() above
            return isHexBignumWeak(aPubKey) || isHexBignumWeak(aDhp) || isHexBignumWeak(aDhg);
        }

        bool isBobPubkeyWeak(const std::string& aPubKey)
        {
            // An additional (paranoidal?) check to DH_check_pub_key() called by DH_compute_key() called by calcSharedKey() above
            return isHexBignumWeak(aPubKey);
        }
    }
}
