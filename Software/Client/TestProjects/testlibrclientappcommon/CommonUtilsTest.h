#pragma once

#include "rclient/CommonUtils.h"
#include "ta/url.h"
#include "cxxtest/TestSuite.h"
#include <string>

class CommonUtilsTest : public CxxTest::TestSuite
{
public:
    void testIsServiceUri()
    {
        using rclient::isServiceUri;

        TS_ASSERT(isServiceUri("https://bank.nl", "https://bank.nl"));
        TS_ASSERT(isServiceUri("https://bank.nl", "https://www.bank.nl"));
        TS_ASSERT(isServiceUri("https://www.bank.nl", "https://bank.nl"));
        TS_ASSERT(isServiceUri("https://bAnk.nl", "https://bank.nl"));
        TS_ASSERT(isServiceUri("https://bank.nl", "https://bAnk.nl"));
        TS_ASSERT(isServiceUri("https://bank.nl/", "https://bank.nl"));
        TS_ASSERT(isServiceUri("https://bank.nl", "https://bank.nl/"));
        TS_ASSERT(isServiceUri(" https://bank.nl\t\n", "\t\nhttps://bank.nl "));
        TS_ASSERT(isServiceUri("https://bank.nl/hypothec.htm", "https://bank.nl/"));
        TS_ASSERT(isServiceUri("https://bank.nl/hypothec.htm#section", "https://bank.nl/"));
        TS_ASSERT(isServiceUri("https://user:password@bank.nl#section", "https://bank.nl"));
        TS_ASSERT(isServiceUri("https://user:password@bank.nl", "https://bank.nl"));
        TS_ASSERT(isServiceUri("https://bank.nl#section", "https://bank.nl"));
        TS_ASSERT(isServiceUri("https://bank.nl", "https://user:password@bank.nl#section"));
        TS_ASSERT(isServiceUri("https://bank.nl", "https://user:password@bank.nl"));
        TS_ASSERT(isServiceUri("https://bank.nl", "https://bank.nl#section"));
        TS_ASSERT(isServiceUri("https://login.bank.nl", "https://*.bank.nl"));
        TS_ASSERT(isServiceUri("https://logiN.Bank.nl", "https://*.bAnk.nL"));
        TS_ASSERT(isServiceUri("https://online.login.bank.nl", "https://*.bank.nl"));
        TS_ASSERT(isServiceUri("https://oNline.lOgin.bAnk.nl", "https://*.BanK.nl"));
        TS_ASSERT(isServiceUri("https://bank.nl?query", "https://bank.nl"));
        TS_ASSERT(isServiceUri("https://bank.nl", "https://bank.nl?query"));
        TS_ASSERT(isServiceUri("https://bank.nl?blue", "https://bank.nl?blues"));

        TS_ASSERT(!isServiceUri("https://bank.nl", "https://bank.nl/hypothec.htm"));
        TS_ASSERT(!isServiceUri("http://bank.nl", "https://bank.nl"));
        TS_ASSERT(!isServiceUri("https://bank.nl:100", "https://bank.nl"));
        TS_ASSERT(!isServiceUri("https://bank.nl:100", "https://bank.nl:10"));
        TS_ASSERT(!isServiceUri("http://bank.nl:100", "https://bank.nl"));
        TS_ASSERT(!isServiceUri("https://bank.nl:100", "https://bank.nl"));
        TS_ASSERT(!isServiceUri("https://bank.nl.com", "https://bank.nl"));
        TS_ASSERT(!isServiceUri("https://bank.nl", "http://bank.nl.com"));

        TS_ASSERT(!isServiceUri("about:blank", "http://bank.nl"));
        TS_ASSERT(!isServiceUri("https://login.bank.nl", "https://bank.nl"));
        TS_ASSERT(!isServiceUri("https://bank.nl", "https://*.bank.nl"));
        TS_ASSERT(!isServiceUri("https://bank.nl", "https://*.nl"));
        TS_ASSERT(!isServiceUri("https://192.168.1.1", "https://*.168.1.1"));

        // ill-formed URIs
        TS_ASSERT(!isServiceUri("", "https://bank.nl"));
        TS_ASSERT(!isServiceUri("https://bank.nl", ""));
        TS_ASSERT(!isServiceUri("bank.nl", "http://bank.nl"));
        TS_ASSERT(!isServiceUri("", "http://bank.nl"));
        TS_ASSERT(!isServiceUri("about:blank", "bank.nl"));
        TS_ASSERT(!isServiceUri("about:blank", ""));
        TS_ASSERT(!isServiceUri(" ", ""));
        TS_ASSERT(!isServiceUri("", " "));
        TS_ASSERT(!isServiceUri(" ", " "));
        TS_ASSERT(!isServiceUri("", ""));
    }
};
