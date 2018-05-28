#include "url.h"
#include "common.h"
#include "encodingutils.h"
#include "strings.h"
#include "netutils.h"
#include "utils.h"
#include "boost/static_assert.hpp"
#include "boost/regex.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/algorithm/string/regex.hpp"
#include "boost/assign/list_of.hpp"
#include <stdexcept>
#include <cassert>
#include <map>



namespace ta
{
    namespace url
    {
        using std::string;

        // Private stuff
        namespace
        {
            const Scheme SchemeFirst = Http;
            const Scheme SchemeLast  = Other;
            const char* mySchemeStrings[] = {"http", "https", "ftp", "ftps", "file", "other"};
            BOOST_STATIC_ASSERT(sizeof(mySchemeStrings)/sizeof(mySchemeStrings[0]) == SchemeLast - SchemeFirst + 1);

            bool isScheme(int aVal)
            {
                return (aVal >= SchemeFirst && aVal <= SchemeLast);
            }

            string toString(int aScheme)
            {
                if (!isScheme(aScheme))
                    return mySchemeStrings[Other];
                return mySchemeStrings[aScheme-SchemeFirst];
            }

            // "Safe" means that it does not convert already URL-encoded symbols including %2F and %2f
            string safeUrlEncode(const std::string& aStr)
            {
                std::vector<string> myParts;
                boost::algorithm::split_regex(myParts, aStr, boost::regex("%2F|%2f"));
                foreach (string& part, myParts)
                {
                    part = EncodingUtils::urlEncode(EncodingUtils::urlDecode(part));
                }
                return boost::join(myParts, "%2F");
            }



            // Normalization of URL parts
            string normalizeScheme(const Parts& anUrlParts)
            {
                return boost::to_lower_copy(anUrlParts.scheme);
            }
            Authority::Parts normalizeAuthority(const Parts& anUrlParts, bool aRemoveWww)
            {
                Authority::Parts myParts = anUrlParts.authority_parts;
                if (myParts.host.empty())
                    return myParts;
                boost::to_lower(myParts.host);
                if (boost::iends_with(myParts.host, "."))
                    myParts.host = myParts.host.erase(myParts.host.length()-1);
                while (aRemoveWww && boost::istarts_with(myParts.host, "www."))
                    myParts.host = myParts.host.erase(0, strlen("www."));

                if (!myParts.port.empty())
                {
                    if (boost::iequals(anUrlParts.scheme, toString(Http)) && myParts.port == "80")
                        myParts.port="";
                    else if (boost::iequals(anUrlParts.scheme, toString(Https)) && myParts.port == "443")
                        myParts.port="";
                }
                return myParts;
            }

            string normalizePath(const Parts& anUrlParts, const DirectoryIndex* aDirectoryIndex)
            {
                string myPath = anUrlParts.path;

                // remove directory index
                DirectoryIndex myDefDirectoryIndex;
                myDefDirectoryIndex.baseNames = boost::assign::list_of("index")("default");
                myDefDirectoryIndex.exts = boost::assign::list_of("htm")("html")("asp")("php")("chm")("py")("pl");
                if (!aDirectoryIndex)
                    aDirectoryIndex = &myDefDirectoryIndex;
                string::size_type pos = myPath.rfind('/');
                string myFileName = (pos == string::npos) ? myPath : myPath.substr(pos+1);
                foreach (string baseName, myDefDirectoryIndex.baseNames)
                {
                    foreach (string ext, myDefDirectoryIndex.exts)
                    {
                        if (myFileName == baseName+"."+ext)
                        {
                            if (pos != string::npos)
                                myPath = myPath.erase(pos+1);
                            else
                                myPath = "";
                        }
                    }
                }

                // collapse
                try
                {
                    boost::regex mySearchRegEx("([^/\\.]+/\\.\\./?|/\\./|//|/\\.$|^\\.\\./\\.\\./?|^/\\.\\.)");
                    while (true)
                    {
                        const string myCollapsedPath = boost::regex_replace(myPath, mySearchRegEx, "/", boost::format_first_only);
                        if (myCollapsedPath == myPath)
                            break;
                        myPath = myCollapsedPath;
                    }
                }
                catch (boost::regex_error&)
                {
                    assert(!"Regex replace failed");
                }

                // URL-encode and uppercase all already url-encoded characters
                myPath = safeUrlEncode(myPath);

                // prepend '/' if needed
                if (!boost::starts_with(myPath,"/"))
                    myPath = "/" + myPath;

                return myPath;
            }

            // QueryValue struct is used to distinguish http://domain.com?arg and http://domain.com?arg=
            struct QueryValue
            {
                QueryValue() : exists(false)
                {}
                QueryValue(bool anExists, const string& aVal = string()) : exists(anExists), val(aVal)
                {}
                bool exists;
                string val;
            };

            string normalizeQuery(const Parts& anUrlParts)
            {
                if (!anUrlParts.has_query)
                    return "";

                // Sort and urlencode query, uppercasing all already url-encoded characters
                std::vector<string> myKeyVals = Strings::split(anUrlParts.query, '&');
                std::map<string,QueryValue> mySortedKeyVals;
                foreach (string keyval, myKeyVals)
                {
                    string::size_type pos = keyval.find('=');
                    if (pos != string::npos)
                    {
                        string key =  keyval.substr(0,pos);
                        if (!key.empty())
                        {
                            string val = keyval.substr(pos+1);
                            key = safeUrlEncode(key);
                            val = safeUrlEncode(val);
                            mySortedKeyVals[key] = QueryValue(true, val);
                        }
                    }
                    else
                    {
                        if (!keyval.empty())
                        {
                            string key = safeUrlEncode(keyval);
                            mySortedKeyVals[key] = QueryValue(false);
                        }
                    }
                }

                string myQuery;
                typedef std::pair<string, QueryValue> KeyPairT;
                foreach (KeyPairT keyval, mySortedKeyVals)
                {
                    if (!myQuery.empty())
                    {
                        if (keyval.second.exists)
                            myQuery += "&"+keyval.first+"="+keyval.second.val;
                        else
                            myQuery += "&"+keyval.first;
                    }
                    else
                    {
                        if (keyval.second.exists)
                            myQuery = keyval.first+"="+keyval.second.val;
                        else
                            myQuery = keyval.first;
                    }
                }

                return myQuery;
            }

            //
            // Combine normalized URL parts into URL string
            //
            string combineNormalizedParts2Str(const Parts& aNormaUrlParts)
            {
                string myUserPass;
                if (!aNormaUrlParts.authority_parts.user.empty())
                {
                    if (aNormaUrlParts.authority_parts.has_password)
                        myUserPass = str(boost::format("%s:%s@") % aNormaUrlParts.authority_parts.user % aNormaUrlParts.authority_parts.password);
                    else
                        myUserPass = str(boost::format("%s@") % aNormaUrlParts.authority_parts.user);
                }
                string myHostPort;
                if (!aNormaUrlParts.authority_parts.port.empty())
                    myHostPort = str(boost::format("%s:%s") % aNormaUrlParts.authority_parts.host % aNormaUrlParts.authority_parts.port);
                else
                    myHostPort = aNormaUrlParts.authority_parts.host;

                string myQuery;
                if (aNormaUrlParts.has_query && !aNormaUrlParts.query.empty())
                    myQuery = str(boost::format("?%s") % aNormaUrlParts.query);

                string myUrl = str(boost::format("%s://%s%s%s%s") %
                                   aNormaUrlParts.scheme %
                                   myUserPass %
                                   myHostPort %
                                   aNormaUrlParts.path %
                                   myQuery);
                return myUrl;
            }
        }

        // Public stuff

        Scheme getScheme(const string& anUrl)
        {
            try
            {
                Parts myParts = parse(anUrl);
                for (int iScheme = SchemeFirst; iScheme <= SchemeLast; ++iScheme)
                {
                    string myScheme = toString(iScheme);
                    if (boost::iequals(myParts.scheme, myScheme) && isScheme(iScheme))
                        return static_cast<Scheme>(iScheme);
                }
            }
            catch (UrlParseError&)
            {}
            return Other;
        }

        string normalize(const string& anUrl, bool aRemoveWww, const DirectoryIndex* aDirectoryIndex)
        {
            string myUrl = boost::algorithm::trim_copy(anUrl);

            if (boost::istarts_with(myUrl,"\\\\"))
                myUrl = toString(File)+":"+myUrl;

            myUrl = boost::algorithm::replace_all_copy(myUrl, "\\", "/");
#ifdef _WIN32
            if (boost::istarts_with(myUrl, "file://"))
            {
                boost::to_lower(myUrl);
                return myUrl;
            }
#endif

            if (!boost::istarts_with(myUrl, "http://") && !boost::istarts_with(myUrl, "https://"))
                return myUrl;

            // Now we have http:// or https:// url
            Parts myUrlParts = parse(myUrl);
            if (!boost::iequals(myUrlParts.scheme, toString(Http)) &&
                    !boost::iequals(myUrlParts.scheme, toString(Https)))
                return myUrl;

            myUrlParts.scheme = normalizeScheme(myUrlParts);
            myUrlParts.authority_parts = normalizeAuthority(myUrlParts, aRemoveWww);
            myUrlParts.path = normalizePath(myUrlParts, aDirectoryIndex);
            myUrlParts.query = normalizeQuery(myUrlParts);
            myUrlParts.has_fragment = false;

            myUrl = combineNormalizedParts2Str(myUrlParts);

            return myUrl;
        }

        string makeNativePath(const string& anUrl)
        {
            string myNativePath = anUrl;
            string myPrefix = toString(File) + "://";
            if (myNativePath.substr(0,myPrefix.length()) == myPrefix)
                myNativePath = myNativePath.erase(0,myPrefix.length());
#ifdef _WIN32
            myNativePath = boost::algorithm::replace_all_copy(myNativePath, "/", "\\");
#endif
            return myNativePath;
        }

        Parts parse(const string& anUrl)
        {
            string myUrl = boost::algorithm::trim_copy(anUrl);
            if (myUrl.empty())
                TA_THROW_MSG(UrlParseError, "Failed to parse empty URL (at least scheme and host parts should be non-empty)");
            Parts myParts;
            try
            {
                static const string myRegExStr = "^((?<scheme>[^:/?#]+):)?(//(?<authority>[^/?#]*))?(?<path>[^?#]*)(\\?(?<query>[^#]*))?(#(?<fragment>.*))?";
                boost::regex myRegEx(myRegExStr);
                boost::match_results<string::const_iterator> myMatch;
                if (!regex_search(myUrl, myMatch, myRegEx))
                    TA_THROW_MSG(UrlParseError, boost::format("Failed to parse URL: no matches found for URL %s, regexp %s") % myUrl % myRegExStr);

                string myAuthority = myMatch["authority"];
                myParts.authority_parts = Authority::parse(myAuthority);
                myParts.scheme   = myMatch["scheme"];
                if (myParts.scheme.empty())
                    TA_THROW_MSG(UrlParseError, boost::format("Failed to parse URL (%s). Scheme part cannot be empty") % anUrl);
                myParts.path  = myMatch["path"];
                if (myMatch["query"].matched)
                {
                    myParts.has_query = true;
                    myParts.query = myMatch["query"];
                }
                if (myMatch["fragment"].matched)
                {
                    myParts.has_fragment = true;
                    myParts.fragment = myMatch["fragment"];
                }
                return myParts;
            }
            catch (boost::regex_error& e)
            {
                TA_THROW_MSG(UrlParseError, boost::format("Failed to parse URL (%s). Error: %s") % anUrl % e.what());
            }
        }

        namespace Authority
        {
            Parts parse(const std::string& anAurhority)
            {
                Parts myParts;
                string myAuthority = boost::algorithm::trim_copy(anAurhority);
                if (myAuthority.empty())
                    return myParts;
                try
                {
                    static const string myRegExStr = "^((?<user>[^:/?#@]*)([:](?<password>[^:/?#@]*))?[@])?((?<ipv6_host>\\[[\\:\\.\\da-fA-F]+\\])|(?<non_ipv6_host>[^:/?#@]+))(\\:(?<port>[\\d]+))?";
                    boost::regex myRegEx(myRegExStr);
                    boost::match_results<string::const_iterator> myMatch;
                    if (!regex_search(myAuthority, myMatch, myRegEx))
                        return myParts;

                    myParts.user = myMatch["user"];
                    if (myMatch["password"].matched)
                    {
                        myParts.has_password = true;
                        myParts.password = myMatch["password"];
                    }
                    if (myMatch["ipv6_host"].matched)
                    {
                        string myIpv6Host = myMatch["ipv6_host"];
                        assert(myIpv6Host.size() > 2);
                        assert(myIpv6Host[0] == '[');
                        assert(myIpv6Host[myIpv6Host.size()-1] == ']');
                        myIpv6Host = myIpv6Host.substr(1, myIpv6Host.size()-2);
                        myParts.host = myIpv6Host;
                    }
                    else if (myMatch["non_ipv6_host"].matched)
                    {
                        myParts.host = myMatch["non_ipv6_host"];
                    }
                    else
                    {
                        TA_THROW_MSG(UrlParseError, boost::format("Error parsing Authority (%s).") % myAuthority);
                    }
                    myParts.port = myMatch["port"];
                    return myParts;
                }
                catch (boost::regex_error& e)
                {
                    TA_THROW_MSG(UrlParseError, boost::format("Failed to parse Authority (%s). Error: %s") % myAuthority % e.what());
                }
            }
        }

        bool wildcardHostMatch(const string& aHostName, const string& aHostTempl)
        {
            const string myHostName = boost::to_lower_copy(boost::trim_copy(aHostName));
            string myHostTempl = boost::to_lower_copy(boost::trim_copy(aHostTempl));
            if (myHostName.empty() || myHostTempl.empty())
                TA_THROW_MSG(UrlParseError, boost::format("Both host name and template cannot be empty"));

            if (myHostName == myHostTempl)
                return true;

            if (NetUtils::isValidIpv4(aHostName) || NetUtils::isValidIpv6(aHostName))
                return false; // no wildacrd for IPs

            // correct wildcard template should represent at least 3rd-level domain
            boost::regex myTemplRegEx("\\*(\\.[^.*:/?#@]+){2,}");
            if (!regex_match(aHostTempl, myTemplRegEx))
                return false;

            boost::regex myHostRegex("[^.*:/?#@]+(\\.[^.*:/?#@]+)*" + regexEscapeStr(myHostTempl.erase(0, 1)));
            if (!regex_match(myHostName, myHostRegex))
                return false;

            return true;
        }

        bool hasScheme(const std::string& anUrl)
        {
            const string myUrl = boost::algorithm::trim_copy(anUrl);
            if (myUrl.empty())
                return false;

            boost::regex myRegEx("^(([^:/?#]+)://).+");
            boost::match_results<string::const_iterator> myMatch;
            return regex_search(myUrl, myMatch, myRegEx);
        }

        std::string join(const std::string& anUrlPart1, const std::string& anUrlPart2)
        {
            // normalize URL parts
            string myUrlPart1 = boost::trim_copy(anUrlPart1);
            string myUrlPart2 = boost::trim_copy(anUrlPart2);
            boost::trim_left_if(myUrlPart2, boost::is_any_of("/"));

            if (!anUrlPart2.empty())
            {
                if (!boost::ends_with(myUrlPart1, "/"))
                {
                    myUrlPart1 += "/";
                }
                return myUrlPart1 + myUrlPart2;
            }
            else
            {
                return myUrlPart1;
            }
        }
    }
}
