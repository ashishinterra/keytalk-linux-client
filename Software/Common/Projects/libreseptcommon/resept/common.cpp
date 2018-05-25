#include "common.h"
#include "ta/signutils.h"
#include "ta/encodingutils.h"
#include "ta/utils.h"
#include "ta/common.h"

using std::string;
using std::vector;

namespace resept
{
    namespace rcdpv1
    {
        static const char SignatureSeparator = ' ';
        static const ta::SignUtils::Digest DigestType = ta::SignUtils::digestSha1;

        string signMsg(const string& aMsg, const string& aPemPrivKey, DigestSignFunc aDigestSignCbk, const void* aDigestSignCbkContext)
        {
            const vector<unsigned char> myMsg = ta::str2Vec<unsigned char>(aMsg);
            vector<unsigned char> mySignedDigest;

            if (aDigestSignCbk)
            {
                const string myPemPrivKeyId = aPemPrivKey;
                mySignedDigest = aDigestSignCbk(aDigestSignCbkContext, myMsg, DigestType, myPemPrivKeyId);
            }
            else
            {
                // use built-in signer
                const string myPemPrivKeyFilePath = aPemPrivKey;
                mySignedDigest = ta::SignUtils::signDigest(myMsg, DigestType, myPemPrivKeyFilePath);
            }

            const string mySignedDigestB64 = ta::EncodingUtils::toBase64(mySignedDigest, true);
            return mySignedDigestB64 + SignatureSeparator + aMsg;
        }

        string verifySignedMsg(const string& aSignedMsg, const string& aPemPubKeyPath)
        {
            const vector<unsigned char> myPubKey = ta::readData(aPemPubKeyPath);
            return verifySignedMsg(aSignedMsg, myPubKey);
        }

        string verifySignedMsg(const string& aSignedMsg, const vector<unsigned char>& aPemPubKey)
        {
            string::size_type mySepPos = aSignedMsg.find(SignatureSeparator);
            if (mySepPos == string::npos)
            {
                TA_THROW_MSG(std::invalid_argument, "Ill-formed signed message, no signature separator found");
            }
            const string mySignedDigestB64 = aSignedMsg.substr(0, mySepPos);
            const string myMsg = aSignedMsg.substr(mySepPos+1);
            const vector<unsigned char> mySignedDigest = ta::EncodingUtils::fromBase64(mySignedDigestB64, true);
            if (!ta::SignUtils::verifyDigest(ta::str2Vec<unsigned char>(myMsg), mySignedDigest, DigestType, aPemPubKey))
            {
                TA_THROW_MSG(std::logic_error, "Message verification failed");
            }
            return myMsg;
        }
    }

}
