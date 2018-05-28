#pragma once

#include "ta/hostutils.h"
#include <string>


class HostUtilsHostnameTest : public CxxTest::TestSuite
{
#ifdef RESEPT_SERVER
    std::string theHostName;
#endif
public:
    void setUp()
    {
#ifdef RESEPT_SERVER
        theHostName = ta::HostUtils::hostname::get();
#endif
    }
    void tearDown()
    {
#ifdef RESEPT_SERVER
         ta::HostUtils::hostname::set(theHostName);
#endif
    }

    void testGetHostname()
    {
        TS_ASSERT(!ta::HostUtils::hostname::get().empty());
    }

    void testSetHostname()
    {
#ifdef RESEPT_SERVER
        using namespace ta::HostUtils;

        TS_TRACE("--- Testing valid hostnames");
        hostname::set("abcd");
        TS_ASSERT_EQUALS(hostname::get(), "abcd");

        hostname::set("a.b.c");
        TS_ASSERT_EQUALS(hostname::get(), "a.b.c");

        hostname::set("0123456789");
        TS_ASSERT_EQUALS(hostname::get(), "0123456789");

        TS_TRACE("--- Testing valid hostnames (valid after normalization)");
        hostname::set("  abcd  ");
        TS_ASSERT_EQUALS(hostname::get(), "abcd");

        hostname::set("aBCd");
        TS_ASSERT_EQUALS(hostname::get(), "abcd");

        TS_TRACE("--- Testing hostname without any characters");
        TS_ASSERT_THROWS_EQUALS(hostname::set(""), const ta::NetUtils::DomainNameValidationError& error, error.validationResult, ta::NetUtils::domainNameEmpty);

        TS_TRACE("--- Testing hostname without any characters (after normalization)");
        TS_ASSERT_THROWS_EQUALS(hostname::set("  "), const ta::NetUtils::DomainNameValidationError& error, error.validationResult, ta::NetUtils::domainNameEmpty);

        TS_TRACE("--- Testing hostname with invalid characters");
        TS_ASSERT_THROWS_EQUALS(hostname::set("a.b.c^"), const ta::NetUtils::DomainNameValidationError& error, error.validationResult, ta::NetUtils::domainNameInvalidCharacter);
        TS_ASSERT_THROWS_EQUALS(hostname::set("a@"), const ta::NetUtils::DomainNameValidationError& error, error.validationResult, ta::NetUtils::domainNameInvalidCharacter);

        TS_TRACE("--- Testing hostname with an invalid label (without a character)");
        TS_ASSERT_THROWS_EQUALS(hostname::set("a..c"), const ta::NetUtils::DomainNameValidationError& error, error.validationResult, ta::NetUtils::domainNameLabelEmpty);
        TS_ASSERT_THROWS_EQUALS(hostname::set("a.b.."), const ta::NetUtils::DomainNameValidationError& error, error.validationResult, ta::NetUtils::domainNameLabelEmpty);
        TS_ASSERT_THROWS_EQUALS(hostname::set("..a.b"), const ta::NetUtils::DomainNameValidationError& error, error.validationResult, ta::NetUtils::domainNameLabelEmpty);

        TS_TRACE("--- Trying to set hostname with too long label size");
        TS_ASSERT_THROWS_EQUALS(hostname::set("a.0123456789012345678901234567890123456789012345678901234567890123.c"),
            const ta::NetUtils::DomainNameValidationError& error, error.validationResult, ta::NetUtils::domainNameLabelTooLong);


        TS_TRACE("--- Testing hostname with an invalid number of characters");
        // A hostname with more than 255 characters is not allowed
        std::string hostnameWithInvalidMaximumSize =
            "0123456789012345678901234567890123456789012345678."
            "0123456789012345678901234567890123456789012345678."
            "0123456789012345678901234567890123456789012345678."
            "0123456789012345678901234567890123456789012345678."
            "0123456789012345678901234567890123456789012345678."
            "012345";
        TS_ASSERT_THROWS_EQUALS(hostname::set(hostnameWithInvalidMaximumSize),
            const ta::NetUtils::DomainNameValidationError& error, error.validationResult, ta::NetUtils::domainNameTooLong);
#else
        TS_SKIP("This test is for KeyTalk server only");
#endif
    }
};
