#pragma once

#include <string>
#include <vector>
#include <stdexcept>

namespace ta
{
    struct UrlParseError : std::logic_error
    {
        explicit UrlParseError(const std::string& aMessage = "")	: std::logic_error(aMessage) {}
    };

    namespace url
    {
        struct DirectoryIndex
        {
            std::vector<std::string> baseNames; // file basename without extension e.g. "index"
            std::vector<std::string> exts;      // file extension e.g. "htm"
        };
        //
        // Normalizes the given URL.
        //
        // The following transformations are performed:
        //
        // remove all leading and trailing whitespace
        // IF url starts with double backslash:
        //     prepend url with 'file:'
        // FI
        // replace backslahes with direct slashes
        // IF url starts with 'file://' and OS is Windows:
        //     lowercase the url
        // ELIF url starts with http:// or https://:
        //  parse the URL with URL::parse() and applies the following transormations to its parts:
        //  - collapse the path: foo/bar/../ => foo/ and more
        //  - lowercase the scheme and hostname: HTTP://www.Example.com/ => http://www.example.com/
        //  - add trailing '/' to the authority: http://www.example.com => http://www.example.com/
        //  - remove default port: http://foo.com:80 => http://foo.com
        //  - remove fragment: http://foo.com/bar#fragment => http://foo.com/bar
        //  - remove trailing '.' in the hostname: http://foo.com. => http://foo.com
        //  - remove the '?' when the querystring is empty: http://www.foo.com/bar? => http://www.foo.com/bar
        //  - URL-encode path and query: http://www.foo.com/~bar?arg=v~al=> http://www.foo.com/%7Ebar?arg=v%7Eal
        //  - uppercase URL-encoded characters in path and query: http://www.foo.com/%7ebar?arg=v%7eal => http://www.foo.com/%7Ebar?arg=v%7Eal
        //  - sort arguments in query string: http://www.foo.com/?arg2=val2&arg0&arg1=val1=> http://www.foo.com/?arg0&arg1=val1&arg2=val2
        //  - if aRemoveWww is set, removes all 'www.' at the beginning of the url's hostname: http://www.foo.com/ => http://foo.com/
        //  - depending on the directory_index argument, remove directory index: http://foo.com/dir/index.htm => http://foo.com/dir/
        // ELSE:
        //     nothing is done
        // ENDIF
        //
        // anUrl: URL to be normalized
        // aRemoveWww: if set removes 'www' as the beginning of the url's hostname
        // aDirectoryIndex: pointer to DirectoryIndex. Default to NULL which means that default directory index is used with
        //                  "index", "default" as basename and "htm", "html", "asp", "php", "chm", "py", "pl" extension
        // Return: normalized URL
        // Exceptions: throw UrlParseError if URL provided is ill-formed URL
        //
        std::string normalize(const std::string& anUrl, bool aRemoveWww = true, const DirectoryIndex* aDirectoryIndex = NULL);

        enum Scheme
        {
            Http, Https, Ftp, Ftps, File, Other // Other means scheme is not from the listed above or no scheme at all (ill-formed URL)
        };
        Scheme getScheme(const std::string& anUrl);

        // Removes 'file://' prefix. On Windows also replaces direct slashes with backslashes
        // E.g. file://C:/prog.exe => C:\prog.exe
        std::string makeNativePath(const std::string& anUrl);


        // Authority parts according to RFC3986
        namespace Authority
        {
            struct Parts
            {
                Parts() : has_password(false) {}
                std::string user;
                bool has_password;
                std::string password;
                std::string host; // could be IPv4, IPv6 or FQDN
                std::string port;
            };

            //
            // Abstract: parse Authority according to RFC3986
            //
            //           E.g. parsing of "user:password@www.domain.com:8080"
            //           will give: user="user", password="password", host="www.domain.com",
            //           port="8080".
            //
            // Exceptions: throw UrlParseError on error
            Parts parse(const std::string& anAurhority);
        }
        // URL parts according to RFC3986
        struct Parts
        {
            Parts():  has_query(false), has_fragment(false) {}
            std::string scheme;
            Authority::Parts authority_parts;
            std::string path;
            bool has_query;
            std::string query;
            bool has_fragment;
            std::string fragment;
        };

        //
        // Abstract: parses URL according to RFC3986.
        //           The function is sensitive to not RFC3986-compliant URLs, thus the proper usage is first normalize URL
        //           with URL::normalize() and then call URL::parse() with the normalized URL.
        //
        //           E.g. parsing of "http://user:password@www.domain.com:8080/folder/page.pl?name=val#section"
        //           will give: scheme="http", user="user", password="password", host="www.domain.com",
        //           port="8080", path="/folder/page.pl", query="name=val", fragment="section".
        //           Scheme and either host or path parts are obligatory, other parts are optional
        //
        // Exceptions: throw UrlParseError on error
        Parts parse(const std::string& anUrl);

        //
        // Match aHostName against aDomainTempl respecting '*.' DNS wildcard in aHostTempl
        // Some examples (see unit tests for more):
        // login.example.com will match *.example.com
        // online.login.example.com will match *.example.com
        // example.com will not match *.example.com
        // example.com will not match *.com
        bool wildcardHostMatch(const std::string& aHostName, const std::string& aHostTempl);

        // Checks whether the given input starts with scheme i.e. http://www.nu.nl or www.nu.nl
        bool hasScheme(const std::string& anUrl);

        // join URL parts correctly handling trailing/leading slashes, e.g. join("http://site.com/", "/path") -> "http://site.com/path"
        std::string join(const std::string& anUrlPart1, const std::string& anUrlPart2);
    }
}
