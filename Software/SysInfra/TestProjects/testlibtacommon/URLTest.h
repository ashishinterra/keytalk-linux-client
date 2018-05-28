#pragma once

#include "ta/url.h"
#include "ta/common.h"
#include "cxxtest/TestSuite.h"

#include "boost/algorithm/string/regex.hpp"
#include "boost/algorithm/string.hpp"
#include <string>
#include <stdexcept>



class URLTest : public CxxTest::TestSuite
{
public:
    void testNormalizeUrl()
    {
		using namespace ta;

        // file://, Win32 paths
        TS_ASSERT_EQUALS(url::normalize("\\\\host\\share "), "file://host/share");
        TS_ASSERT_EQUALS(url::normalize(" \\host\\sharE"), "/host/sharE");
        TS_ASSERT_EQUALS(url::normalize(" host\\share   "), "host/share");
#ifdef _WIN32
        TS_ASSERT_EQUALS(url::normalize("file://C:\\Program Files\\CoolSoft\\CoolProg.exe "), "file://c:/program files/coolsoft/coolprog.exe");
        TS_ASSERT_EQUALS(url::normalize(" file://C:/Program Files/CoolSoft/CoolProg.exe"), "file://c:/program files/coolsoft/coolprog.exe");
        TS_ASSERT_EQUALS(url::normalize("file:///etc/hOsts"), "file:///etc/hosts");
#else
        TS_ASSERT_EQUALS(url::normalize("file://C:\\Program Files\\CoolSoft\\CoolProg.exe "), "file://C:/Program Files/CoolSoft/CoolProg.exe");
        TS_ASSERT_EQUALS(url::normalize(" file://C:/Program Files/CoolSoft/CoolProg.exe"), "file://C:/Program Files/CoolSoft/CoolProg.exe");
        TS_ASSERT_EQUALS(url::normalize("file:///etc/hOsts"), "file:///etc/hOsts");
#endif
        // lowercase the scheme and hostname
        TS_ASSERT_EQUALS(url::normalize("HttP://USER:pass@Example.COM/fOo"), "http://USER:pass@example.com/fOo");

        // remove 'www' as the first domain label:
        TS_ASSERT_EQUALS(url::normalize("http://www.foo.com/"),  "http://foo.com/");
        TS_ASSERT_EQUALS(url::normalize("http://www.foo.com/", true),  "http://foo.com/");
        TS_ASSERT_EQUALS(url::normalize("http://www2.foo.com/"), "http://www2.foo.com/");
        TS_ASSERT_EQUALS(url::normalize("http://www2.foo.com/", true), "http://www2.foo.com/");
        TS_ASSERT_EQUALS(url::normalize("http://www.foo.com/", false),  "http://www.foo.com/");
        TS_ASSERT_EQUALS(url::normalize("http://www.www.foo.com/", true),  "http://foo.com/");
        TS_ASSERT_EQUALS(url::normalize("http://www.www.foo.com/", false),  "http://www.www.foo.com/");

        // [default] ports
        TS_ASSERT_EQUALS(url::normalize("http://foo.com:80/foo"),    "http://foo.com/foo");
        TS_ASSERT_EQUALS(url::normalize("https://foo.com:443/foo"),  "https://foo.com/foo");
        TS_ASSERT_EQUALS(url::normalize("http://foo.com:8080/foo"),  "http://foo.com:8080/foo");
        TS_ASSERT_EQUALS(url::normalize("http://foo.com:443/foo"),   "http://foo.com:443/foo");
        TS_ASSERT_EQUALS(url::normalize("https://foo.com:80/foo"),    "https://foo.com:80/foo");

        // remove trailing '.' in the hostname, add trailing '/' in the authority
        TS_ASSERT_EQUALS(url::normalize("http://foo.com./foo/bar.html"), "http://foo.com/foo/bar.html");
        TS_ASSERT_EQUALS(url::normalize("https://foo.com.:80/foo"),     "https://foo.com:80/foo");
        TS_ASSERT_EQUALS(url::normalize("https://foo.com."),       "https://foo.com/");
        TS_ASSERT_EQUALS(url::normalize("https://foo.com./"),      "https://foo.com/");
        TS_ASSERT_EQUALS(url::normalize("https://foo.com"),       "https://foo.com/");

        // remove directory index
        TS_ASSERT_EQUALS(url::normalize("https://foo.com/index.asp"),  "https://foo.com/");
        TS_ASSERT_EQUALS(url::normalize("https://foo.com/default.htm"),  "https://foo.com/");
        TS_ASSERT_EQUALS(url::normalize("https://foo.com/default.htm/"),  "https://foo.com/default.htm/");
        TS_ASSERT_EQUALS(url::normalize("http://foo.com/dir/index.htm"),  "http://foo.com/dir/");
        TS_ASSERT_EQUALS(url::normalize("https://foo.com/bar/index.htm?arg=val"),  "https://foo.com/bar/?arg=val");
        TS_ASSERT_EQUALS(url::normalize("https://foo.com/default.html#fragment'"), "https://foo.com/");
        TS_ASSERT_EQUALS(url::normalize("https://foo.com/index.txt"), "https://foo.com/index.txt");

        // remove fragment and '?' when the querystring is empty
        TS_ASSERT_EQUALS(url::normalize("https://foo.com#fragment"),  "https://foo.com/");
        TS_ASSERT_EQUALS(url::normalize("http://foo.com/bar?")     ,  "http://foo.com/bar");
        TS_ASSERT_EQUALS(url::normalize("http://foo.com/foo?aRg=Val#fragment"),  "http://foo.com/foo?aRg=Val");

        // %-escape path and query
        TS_ASSERT_EQUALS(url::normalize("http://foo.com/~bar?arg=v~al"),      "http://foo.com/%7Ebar?arg=v%7Eal");
        TS_ASSERT_EQUALS(url::normalize("http://foo.com/~bar?arg=v al"),      "http://foo.com/%7Ebar?arg=v+al");
        TS_ASSERT_EQUALS(url::normalize("http://foo.com/%7ebar?arg=v%7eal"),  "http://foo.com/%7Ebar?arg=v%7Eal");
        TS_ASSERT_EQUALS(url::normalize("http://foo.com/%7Ebar?arg=v%7Eal"),  "http://foo.com/%7Ebar?arg=v%7Eal");

        // %-escape query and sort arguments in query string
        TS_ASSERT_EQUALS(url::normalize("http://foo.com/?arg2=val2&arg0&arg1=val1"),     "http://foo.com/?arg0&arg1=val1&arg2=val2");
        TS_ASSERT_EQUALS(url::normalize("http://foo.com/foo?arg1=va l&arg~=val2"),  "http://foo.com/foo?arg%7E=val2&arg1=va+l");
        TS_ASSERT_EQUALS(url::normalize("http://foo.com/?arg==?"),     "http://foo.com/?arg=%3D%3F");

        // Check %2F and %2f is not converted to '/' in path and query
        TS_ASSERT_EQUALS(url::normalize("http://foo.com/pa%2Fth/?a%2Frg=va%2Fl"),     "http://foo.com/pa%2Fth/?a%2Frg=va%2Fl");
        TS_ASSERT_EQUALS(url::normalize("http://foo.com/pa%2Fth/?a%2frg=va%2fl"),     "http://foo.com/pa%2Fth/?a%2Frg=va%2Fl");

        // collapse path
        TS_ASSERT_EQUALS(url::normalize("http://host.com/bar/."), "http://host.com/bar/");
        TS_ASSERT_EQUALS(url::normalize("http://host.com/bar/./"), "http://host.com/bar/");
        TS_ASSERT_EQUALS(url::normalize("http://host.com/bar/.."), "http://host.com/");
        TS_ASSERT_EQUALS(url::normalize("http://host.com/bar/../"), "http://host.com/");
        TS_ASSERT_EQUALS(url::normalize("http://host.com/bar/../baz"), "http://host.com/baz");
        TS_ASSERT_EQUALS(url::normalize("http://host.com/bar/baz/../.."), "http://host.com/");
        TS_ASSERT_EQUALS(url::normalize("http://host.com/bar/baz/../../"), "http://host.com/");
        TS_ASSERT_EQUALS(url::normalize("http://host.com/bar/../.."), "http://host.com/");
        TS_ASSERT_EQUALS(url::normalize("http://host.com/bar/../../"), "http://host.com/");
        TS_ASSERT_EQUALS(url::normalize("http://host.com/bar/../../../baz"), "http://host.com/baz");
        TS_ASSERT_EQUALS(url::normalize("http://host.com/bar/../../../../baz"), "http://host.com/baz");
        TS_ASSERT_EQUALS(url::normalize("http://host.com/./bar"), "http://host.com/bar");
        TS_ASSERT_EQUALS(url::normalize("http://host.com//"), "http://host.com/");
        TS_ASSERT_EQUALS(url::normalize("http://host.com///bar//"), "http://host.com/bar/");

        // boundary inputs
        TS_ASSERT_EQUALS(url::normalize(""), "");

        // various combinations
        TS_ASSERT_EQUALS(url::normalize("httPs://www.foo.cOm:443/bar/Index.htM?aRg=Val"),  "https://foo.com/bar/Index.htM?aRg=Val");
        TS_ASSERT_EQUALS(url::normalize("http://www.foo.cOm/bar/../index.htm?#fragment"), "http://foo.com/");
        TS_ASSERT_EQUALS(url::normalize("http://www.foo.cOm/../../bar?arg"), "http://foo.com/bar?arg");
    }
    void testMakeNativePath()
    {
#ifdef _WIN32
        TS_ASSERT_EQUALS(url::makeNativePath("file://C:/prog.exe"), "C:\\prog.exe");
        TS_ASSERT_EQUALS(url::makeNativePath("file:///usr/bin/ls"), "\\usr\\bin\\ls");
#else
        TS_ASSERT_EQUALS(url::makeNativePath("file://C:/prog.exe"), "C:/prog.exe");
        TS_ASSERT_EQUALS(url::makeNativePath("file:///usr/bin/ls"), "/usr/bin/ls");
#endif
    }

	void testScheme()
	{
		using namespace ta;
		TS_ASSERT_EQUALS(url::getScheme("http://sioux.nl"), url::Http);
		TS_ASSERT_EQUALS(url::getScheme("https://sioux.nl"), url::Https);
		TS_ASSERT_EQUALS(url::getScheme("file://sioux.nl"), url::File);
		TS_ASSERT_EQUALS(url::getScheme("other://sioux.nl"), url::Other);
		TS_ASSERT_EQUALS(url::getScheme("not url"), url::Other);
	}

	void testParseUrl()
	{
        using std::string;
		using namespace ta;
        url::Parts myParsedUrl = url::parse("http://myuser:mypassword@www.sioux.nl:8080/folder/page.pl?name=val#section");
        TS_ASSERT_EQUALS(myParsedUrl.scheme, "http");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.user, "myuser");
        TS_ASSERT(myParsedUrl.authority_parts.has_password);
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.password, "mypassword");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.host, "www.sioux.nl");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.port, "8080");
        TS_ASSERT_EQUALS(myParsedUrl.path, "/folder/page.pl");
        TS_ASSERT(myParsedUrl.has_query);
        TS_ASSERT_EQUALS(myParsedUrl.query, "name=val");
        TS_ASSERT(myParsedUrl.has_fragment);
        TS_ASSERT_EQUALS(myParsedUrl.fragment, "section");

        myParsedUrl = url::parse("https://www.server.com/redirect.php?url=https://www.otherserver.com/data.php");
        TS_ASSERT_EQUALS(myParsedUrl.scheme, "https");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.user, "");
        TS_ASSERT(!myParsedUrl.authority_parts.has_password);
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.host, "www.server.com");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.port, "");
        TS_ASSERT_EQUALS(myParsedUrl.path, "/redirect.php");
        TS_ASSERT(myParsedUrl.has_query);
        TS_ASSERT_EQUALS(myParsedUrl.query, "url=https://www.otherserver.com/data.php");
        TS_ASSERT(!myParsedUrl.has_fragment);

        myParsedUrl = url::parse("http://www.sioux.nl:8080/folder/page.pl?name=val#section");
        TS_ASSERT_EQUALS(myParsedUrl.scheme, "http");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.user, "");
        TS_ASSERT(!myParsedUrl.authority_parts.has_password);
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.host, "www.sioux.nl");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.port, "8080");
        TS_ASSERT_EQUALS(myParsedUrl.path, "/folder/page.pl");
        TS_ASSERT(myParsedUrl.has_query);
        TS_ASSERT_EQUALS(myParsedUrl.query, "name=val");
        TS_ASSERT(myParsedUrl.has_fragment);
        TS_ASSERT_EQUALS(myParsedUrl.fragment, "section");

        myParsedUrl = url::parse("http://www.sioux.nl/folder/page.pl?name=val#section");
        TS_ASSERT_EQUALS(myParsedUrl.scheme, "http");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.user, "");
        TS_ASSERT(!myParsedUrl.authority_parts.has_password);
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.host, "www.sioux.nl");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.port, "");
        TS_ASSERT_EQUALS(myParsedUrl.path, "/folder/page.pl");
        TS_ASSERT(myParsedUrl.has_query);
        TS_ASSERT_EQUALS(myParsedUrl.query, "name=val");
        TS_ASSERT(myParsedUrl.has_fragment);
        TS_ASSERT_EQUALS(myParsedUrl.fragment, "section");

        myParsedUrl = url::parse("http://www.sioux.nl/folder/page.pl?name=val#");
        TS_ASSERT_EQUALS(myParsedUrl.scheme, "http");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.user, "");
        TS_ASSERT(!myParsedUrl.authority_parts.has_password);
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.host, "www.sioux.nl");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.port, "");
        TS_ASSERT_EQUALS(myParsedUrl.path, "/folder/page.pl");
        TS_ASSERT(myParsedUrl.has_query);
        TS_ASSERT_EQUALS(myParsedUrl.query, "name=val");
        TS_ASSERT(myParsedUrl.has_fragment);
        TS_ASSERT_EQUALS(myParsedUrl.fragment, "");

        myParsedUrl = url::parse("http://www.sioux.nl/folder/page.pl?name=val");
        TS_ASSERT_EQUALS(myParsedUrl.scheme, "http");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.user, "");
        TS_ASSERT(!myParsedUrl.authority_parts.has_password);
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.host, "www.sioux.nl");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.port, "");
        TS_ASSERT_EQUALS(myParsedUrl.path, "/folder/page.pl");
        TS_ASSERT(myParsedUrl.has_query);
        TS_ASSERT_EQUALS(myParsedUrl.query, "name=val");
        TS_ASSERT(!myParsedUrl.has_fragment);

        myParsedUrl = url::parse("http://www.sioux.nl/folder/page.pl?");
        TS_ASSERT_EQUALS(myParsedUrl.scheme, "http");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.user, "");
        TS_ASSERT(!myParsedUrl.authority_parts.has_password);
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.host, "www.sioux.nl");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.port, "");
        TS_ASSERT_EQUALS(myParsedUrl.path, "/folder/page.pl");
        TS_ASSERT(myParsedUrl.has_query);
        TS_ASSERT_EQUALS(myParsedUrl.query, "");
        TS_ASSERT(!myParsedUrl.has_fragment);

        myParsedUrl = url::parse("http://www.sioux.nl/folder/page.pl");
        TS_ASSERT_EQUALS(myParsedUrl.scheme, "http");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.user, "");
        TS_ASSERT(!myParsedUrl.authority_parts.has_password);
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.host, "www.sioux.nl");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.port, "");
        TS_ASSERT_EQUALS(myParsedUrl.path, "/folder/page.pl");
        TS_ASSERT(!myParsedUrl.has_query);
        TS_ASSERT(!myParsedUrl.has_fragment);

        myParsedUrl = url::parse("http://www.sioux.nl/");
        TS_ASSERT_EQUALS(myParsedUrl.scheme, "http");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.user, "");
        TS_ASSERT(!myParsedUrl.authority_parts.has_password);
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.host, "www.sioux.nl");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.port, "");
        TS_ASSERT_EQUALS(myParsedUrl.path, "/");
        TS_ASSERT(!myParsedUrl.has_query);
        TS_ASSERT(!myParsedUrl.has_fragment);

        myParsedUrl = url::parse("http://user:password@www.sioux.nl");
        TS_ASSERT_EQUALS(myParsedUrl.scheme, "http");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.user, "user");
        TS_ASSERT(myParsedUrl.authority_parts.has_password);
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.password, "password");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.host, "www.sioux.nl");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.port, "");
        TS_ASSERT_EQUALS(myParsedUrl.path, "");
        TS_ASSERT(!myParsedUrl.has_query);
        TS_ASSERT(!myParsedUrl.has_fragment);

        myParsedUrl = url::parse("http://user:@www.sioux.nl");
        TS_ASSERT_EQUALS(myParsedUrl.scheme, "http");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.user, "user");
        TS_ASSERT(myParsedUrl.authority_parts.has_password);
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.password, "");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.host, "www.sioux.nl");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.port, "");
        TS_ASSERT_EQUALS(myParsedUrl.path, "");
        TS_ASSERT(!myParsedUrl.has_query);
        TS_ASSERT(!myParsedUrl.has_fragment);

        myParsedUrl = url::parse("http://user@www.sioux.nl");
        TS_ASSERT_EQUALS(myParsedUrl.scheme, "http");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.user, "user");
        TS_ASSERT(!myParsedUrl.authority_parts.has_password);
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.host, "www.sioux.nl");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.port, "");
        TS_ASSERT_EQUALS(myParsedUrl.path, "");
        TS_ASSERT(!myParsedUrl.has_query);
        TS_ASSERT(!myParsedUrl.has_fragment);

        myParsedUrl = url::parse("http://www.sioux.nl#section");
        TS_ASSERT_EQUALS(myParsedUrl.scheme, "http");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.user, "");
        TS_ASSERT(!myParsedUrl.authority_parts.has_password);
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.host, "www.sioux.nl");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.port, "");
        TS_ASSERT_EQUALS(myParsedUrl.path, "");
        TS_ASSERT(!myParsedUrl.has_query);
        TS_ASSERT(myParsedUrl.has_fragment);
        TS_ASSERT_EQUALS(myParsedUrl.fragment, "section");

        myParsedUrl = url::parse("about:blank");
        TS_ASSERT_EQUALS(myParsedUrl.scheme, "about");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.user, "");
        TS_ASSERT(!myParsedUrl.authority_parts.has_password);
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.host, "");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.port, "");
        TS_ASSERT_EQUALS(myParsedUrl.path, "blank");
        TS_ASSERT(!myParsedUrl.has_query);
        TS_ASSERT(!myParsedUrl.has_fragment);

        // This looks misleading, but it is in fact the same as about:blank above
        myParsedUrl = url::parse("sioux.nl:8080");
        TS_ASSERT_EQUALS(myParsedUrl.scheme, "sioux.nl");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.user, "");
        TS_ASSERT(!myParsedUrl.authority_parts.has_password);
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.host, "");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.port, "");
        TS_ASSERT_EQUALS(myParsedUrl.path, "8080");
        TS_ASSERT(!myParsedUrl.has_query);
        TS_ASSERT(!myParsedUrl.has_fragment);

        myParsedUrl = url::parse("mailto:");
        TS_ASSERT_EQUALS(myParsedUrl.scheme, "mailto");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.user, "");
        TS_ASSERT(!myParsedUrl.authority_parts.has_password);
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.host, "");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.port, "");
        TS_ASSERT_EQUALS(myParsedUrl.path, "");
        TS_ASSERT(!myParsedUrl.has_query);
        TS_ASSERT(!myParsedUrl.has_fragment);

        myParsedUrl = url::parse("http://");
        TS_ASSERT_EQUALS(myParsedUrl.scheme, "http");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.user, "");
        TS_ASSERT(!myParsedUrl.authority_parts.has_password);
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.host, "");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.port, "");
        TS_ASSERT_EQUALS(myParsedUrl.path, "");
        TS_ASSERT(!myParsedUrl.has_query);
        TS_ASSERT(!myParsedUrl.has_fragment);

        const string fileurls[] = {"file://C:/Program Files/CoolSoft/CoolProg.exe", "file://C:/Program Files/CoolSoft/CoolProg.exe"};
        foreach (string url, fileurls)
        {
            myParsedUrl = url::parse(url);
            TS_ASSERT_EQUALS(myParsedUrl.scheme, "file");
            TS_ASSERT_EQUALS(myParsedUrl.authority_parts.user, "");
            TS_ASSERT(!myParsedUrl.authority_parts.has_password);
            TS_ASSERT_EQUALS(myParsedUrl.authority_parts.host, "C");
            TS_ASSERT_EQUALS(myParsedUrl.authority_parts.port, "");
            TS_ASSERT_EQUALS(myParsedUrl.path, "/Program Files/CoolSoft/CoolProg.exe");
            TS_ASSERT(!myParsedUrl.has_query);
            TS_ASSERT(!myParsedUrl.has_fragment);
        }

        myParsedUrl = url::parse("file://ipconfig.exe");
        TS_ASSERT_EQUALS(myParsedUrl.scheme, "file");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.user, "");
        TS_ASSERT(!myParsedUrl.authority_parts.has_password);
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.host, "ipconfig.exe");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.port, "");
        TS_ASSERT_EQUALS(myParsedUrl.path, "");
        TS_ASSERT(!myParsedUrl.has_query);
        TS_ASSERT(!myParsedUrl.has_fragment);

        myParsedUrl = url::parse("file:///etc/hosts");
        TS_ASSERT_EQUALS(myParsedUrl.scheme, "file");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.user, "");
        TS_ASSERT(!myParsedUrl.authority_parts.has_password);
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.host, "");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.port, "");
        TS_ASSERT_EQUALS(myParsedUrl.path, "/etc/hosts");
        TS_ASSERT(!myParsedUrl.has_query);
        TS_ASSERT(!myParsedUrl.has_fragment);

        myParsedUrl = url::parse("file://host/share");
        TS_ASSERT_EQUALS(myParsedUrl.scheme, "file");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.user, "");
        TS_ASSERT(!myParsedUrl.authority_parts.has_password);
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.host, "host");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.port, "");
        TS_ASSERT_EQUALS(myParsedUrl.path, "/share");
        TS_ASSERT(!myParsedUrl.has_query);
        TS_ASSERT(!myParsedUrl.has_fragment);

        TS_ASSERT_THROWS(url::parse("\\\\host\\share"), UrlParseError);
        myParsedUrl = url::parse(url::normalize("\\\\host\\share"));
        TS_ASSERT_EQUALS(myParsedUrl.scheme, "file");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.user, "");
        TS_ASSERT(!myParsedUrl.authority_parts.has_password);
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.host, "host");
        TS_ASSERT_EQUALS(myParsedUrl.authority_parts.port, "");
        TS_ASSERT_EQUALS(myParsedUrl.path, "/share");
        TS_ASSERT(!myParsedUrl.has_query);
        TS_ASSERT(!myParsedUrl.has_fragment);


        TS_ASSERT_THROWS(url::parse(""), UrlParseError);
        TS_ASSERT_THROWS(url::parse(":"), UrlParseError);
        TS_ASSERT_THROWS(url::parse("www.sioux.nl"), UrlParseError);
	}
    void testParseUrlAuthority()
    {
		using namespace ta;
        url::Authority::Parts myParsedAuthority = url::Authority::parse("myuser:mypassword@www.sioux.nl:8080");
        TS_ASSERT_EQUALS(myParsedAuthority.user, "myuser");
        TS_ASSERT(myParsedAuthority.has_password);
        TS_ASSERT_EQUALS(myParsedAuthority.password, "mypassword");
        TS_ASSERT_EQUALS(myParsedAuthority.host, "www.sioux.nl");
        TS_ASSERT_EQUALS(myParsedAuthority.port, "8080");

        myParsedAuthority = url::Authority::parse("myuser:mypassword@www.sioux.nl");
        TS_ASSERT_EQUALS(myParsedAuthority.user, "myuser");
        TS_ASSERT(myParsedAuthority.has_password);
        TS_ASSERT_EQUALS(myParsedAuthority.password, "mypassword");
        TS_ASSERT_EQUALS(myParsedAuthority.host, "www.sioux.nl");
        TS_ASSERT_EQUALS(myParsedAuthority.port, "");

        myParsedAuthority = url::Authority::parse("myuser:@www.sioux.nl");
        TS_ASSERT_EQUALS(myParsedAuthority.user, "myuser");
        TS_ASSERT(myParsedAuthority.has_password);
        TS_ASSERT_EQUALS(myParsedAuthority.password, "");
        TS_ASSERT_EQUALS(myParsedAuthority.host, "www.sioux.nl");
        TS_ASSERT_EQUALS(myParsedAuthority.port, "");

        myParsedAuthority = url::Authority::parse("www.sioux.nl");
        TS_ASSERT_EQUALS(myParsedAuthority.user, "");
        TS_ASSERT(!myParsedAuthority.has_password);
        TS_ASSERT_EQUALS(myParsedAuthority.host, "www.sioux.nl");
        TS_ASSERT_EQUALS(myParsedAuthority.port, "");

        myParsedAuthority = url::Authority::parse("127.0.0.1:8080");
        TS_ASSERT_EQUALS(myParsedAuthority.user, "");
        TS_ASSERT(!myParsedAuthority.has_password);
        TS_ASSERT_EQUALS(myParsedAuthority.host, "127.0.0.1");
        TS_ASSERT_EQUALS(myParsedAuthority.port, "8080");

        myParsedAuthority = url::Authority::parse("127.0.0.1:xyz");
        TS_ASSERT_EQUALS(myParsedAuthority.user, "");
        TS_ASSERT(!myParsedAuthority.has_password);
        TS_ASSERT_EQUALS(myParsedAuthority.host, "127.0.0.1");
        TS_ASSERT_EQUALS(myParsedAuthority.port, "");

        myParsedAuthority = url::Authority::parse("[fe80::20c:299ff:fe0d:1234]:8080");
        TS_ASSERT_EQUALS(myParsedAuthority.user, "");
        TS_ASSERT(!myParsedAuthority.has_password);
        TS_ASSERT_EQUALS(myParsedAuthority.host, "fe80::20c:299ff:fe0d:1234");
        TS_ASSERT_EQUALS(myParsedAuthority.port, "8080");

        myParsedAuthority = url::Authority::parse("[fd7c::192.168.1.1]:8080");
        TS_ASSERT_EQUALS(myParsedAuthority.user, "");
        TS_ASSERT(!myParsedAuthority.has_password);
        TS_ASSERT_EQUALS(myParsedAuthority.host, "fd7c::192.168.1.1");
        TS_ASSERT_EQUALS(myParsedAuthority.port, "8080");

        myParsedAuthority = url::Authority::parse("[::192.168.1.1]:8080");
        TS_ASSERT_EQUALS(myParsedAuthority.user, "");
        TS_ASSERT(!myParsedAuthority.has_password);
        TS_ASSERT_EQUALS(myParsedAuthority.host, "::192.168.1.1");
        TS_ASSERT_EQUALS(myParsedAuthority.port, "8080");

        myParsedAuthority = url::Authority::parse("");
        TS_ASSERT_EQUALS(myParsedAuthority.user, "");
        TS_ASSERT(!myParsedAuthority.has_password);
        TS_ASSERT_EQUALS(myParsedAuthority.host, "");
        TS_ASSERT_EQUALS(myParsedAuthority.port, "");

        myParsedAuthority = url::Authority::parse(":");
        TS_ASSERT_EQUALS(myParsedAuthority.user, "");
        TS_ASSERT(!myParsedAuthority.has_password);
        TS_ASSERT_EQUALS(myParsedAuthority.host, "");
        TS_ASSERT_EQUALS(myParsedAuthority.port, "");
    }

    void testWildcardHostMatch()
    {
		using namespace ta;
        TS_ASSERT(url::wildcardHostMatch("login.bank.nl", "login.bank.nl"));
        TS_ASSERT(url::wildcardHostMatch("192.168.1.1", "192.168.1.1"));
        TS_ASSERT(url::wildcardHostMatch("fe80:2::20c:29ff:fe6e:c10b", "fe80:2::20c:29ff:fe6e:c10b"));
        TS_ASSERT(url::wildcardHostMatch("login.bank.nl", "*.bank.nl"));
        TS_ASSERT(url::wildcardHostMatch("logiN.Bank.nl", "*.bAnk.nL"));
        TS_ASSERT(url::wildcardHostMatch("online.login.bank.nl", "*.bank.nl"));
        TS_ASSERT(url::wildcardHostMatch("oNline.lOgin.bAnk123.nl", "*.BanK123.nl"));

        TS_ASSERT(!url::wildcardHostMatch("bank.nl", "*.bank.nl"));
        TS_ASSERT(!url::wildcardHostMatch("login.bank.nl", "*bank.nl")); // not a DNS wildcard
        TS_ASSERT(!url::wildcardHostMatch("login.bank.nl", "login.*.nl")); // not a DNS wildcard
        TS_ASSERT(!url::wildcardHostMatch("nl", "*")); // top level domain
        TS_ASSERT(!url::wildcardHostMatch("bank.nl", "*.nl")); // 2nd level domain
        TS_ASSERT(!url::wildcardHostMatch("192.168.1.1", "*.168.1.1")); // IPv4
        TS_ASSERT(!url::wildcardHostMatch("fe80:2::20c:29ff:fe6e:c10b", "*:2::20c:29ff:fe6e:c10b")); // IPv6

        TS_ASSERT_THROWS(url::wildcardHostMatch("", ""), UrlParseError);
        TS_ASSERT_THROWS(url::wildcardHostMatch("", "*.bank.nl"), UrlParseError);
        TS_ASSERT_THROWS(url::wildcardHostMatch("login.bank.nl", ""), UrlParseError);
    }

    void testHasScheme()
    {
        using namespace ta;

        TS_ASSERT(url::hasScheme("https://nu.nl"));

        TS_ASSERT(!url::hasScheme("nu.nl"));
        TS_ASSERT(!url::hasScheme(""));
    }

    void test_join_url()
    {
        using namespace ta;

        TS_ASSERT_EQUALS(url::join("https://site.org/ ", "/path"), "https://site.org/path");
        TS_ASSERT_EQUALS(url::join("https://site.org/ ", " /path"), "https://site.org/path");
        TS_ASSERT_EQUALS(url::join("https://site.org", "/path"), "https://site.org/path");
        TS_ASSERT_EQUALS(url::join("https://site.org", "path"), "https://site.org/path");
        TS_ASSERT_EQUALS(url::join("https://site.org/ ", "path"), "https://site.org/path");
        TS_ASSERT_EQUALS(url::join("https://site.org/ ", " "), "https://site.org/");
    }
};
