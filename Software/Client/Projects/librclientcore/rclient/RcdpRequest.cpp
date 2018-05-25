#include "RcdpRequest.h"
#include "ta/encodingutils.h"
#include "ta/sysinfo.h"
#include "ta/utils.h"

#include "boost/assign/list_of.hpp"
#include "boost/property_tree/ptree.hpp"

using namespace resept::rcdpv2;
using ta::EncodingUtils::toJson;
using ta::EncodingUtils::toTree;
using std::string;
using boost::assign::map_list_of;

namespace rclient
{
    namespace rcdpv2request
    {
        ta::StringDict makeAuthenticateRequestParams(const string& aServiceName,
                const resept::Credentials& aCredentials,
                const ta::StringArrayDict& aResolvedURIs,
                const ta::StringDict& aCalculatedDigests)
        {
            ta::StringDict myParams = map_list_of(requestParamNameService, aServiceName)
                                      (requestParamNameCallerHwDescription, ta::SysInfo::getHardwareDescription());

            foreach (const resept::Credential& cred, aCredentials)
            {
                if (cred.type == resept::credResponse)
                {
                    ta::StringDictArray myCRs;
                    foreach (const ta::StringDict::value_type& kv, cred.responseVal)
                    {
                        myCRs.push_back(map_list_of(requestParamNameName, kv.first)
                                        (requestParamNameValue, kv.second));
                    }
                    myParams[requestParamNameResponses] = toJson(myCRs);
                }
                else
                {
                    myParams[str(cred.type)] = cred.val;
                }
            }

            if (!aResolvedURIs.empty())
            {
                boost::property_tree::ptree myUrisIpsTree;
                foreach (const ta::StringArrayDict::value_type& uri2ips, aResolvedURIs)
                {
                    boost::property_tree::ptree myUriIpsTree;
                    myUriIpsTree.put(requestParamNameUri, uri2ips.first);
                    myUriIpsTree.put_child(requestParamNameIps, toTree(uri2ips.second));

                    myUrisIpsTree.push_back(std::make_pair("", myUriIpsTree));
                }
                myParams[requestParamNameResolved] = toJson(myUrisIpsTree);
            }

            if (!aCalculatedDigests.empty())
            {
                ta::StringDictArray myUrisDigests;
                foreach (const ta::StringDict::value_type& uri2digest, aCalculatedDigests)
                {
                    myUrisDigests.push_back(map_list_of(requestParamNameUri, uri2digest.first)
                                            (requestParamNameDigest, uri2digest.second));
                }
                myParams[requestParamNameDigests] = toJson(myUrisDigests);
            }

            return myParams;
        }

        ta::StringDict makeCertRequestParams(const resept::CertFormat aCertFormat,
                                             const bool anIncludeChain,
                                             const ta::KeyPair* aFromKeyPair)
        {
            ta::StringDict myParams = map_list_of(requestParamNameCertFormat, str(aCertFormat))
                                      (requestParamNameCertIncludeChain, resept::rcdp::boolToStr(anIncludeChain));
            if (aFromKeyPair)
            {
                const ta::StringDict myKeyPair = map_list_of(requestParamNamePubKey, ta::vec2Str(aFromKeyPair->pubKey))
                                                 (requestParamNamePrivKey, ta::vec2Str(aFromKeyPair->privKey));
                myParams[requestParamNameKeyPair] = ta::EncodingUtils::toJson(myKeyPair);
            }

            return myParams;
        }

    } // rcdv2prequest
}// rclient
