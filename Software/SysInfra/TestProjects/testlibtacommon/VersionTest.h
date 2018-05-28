#pragma once

#include "ta/version.h"
#include "cxxtest/TestSuite.h"
#include <stdexcept>

class VersionTest : public CxxTest::TestSuite
{
public:
    void testParseVersion()
    {
        using namespace ta::version;

        TS_ASSERT_EQUALS(parse("5.3"), Version(5, 3, 0, ""));
        TS_ASSERT_EQUALS(parse("5.3"), Version(5, 3, 0, ""));
        TS_ASSERT_EQUALS(parse("5.3.99"), Version(5, 3, 99, ""));
        TS_ASSERT_EQUALS(parse("5.3.99.a4"), Version(5, 3, 99, "a4"));
        TS_ASSERT_EQUALS(parse(" \r\n 5.3.99.a4 \t "), Version(5, 3, 99, "a4"));
        TS_ASSERT_EQUALS(parse("05.03.099.a04"), Version(5, 3, 99, "a4"));

        TS_ASSERT_THROWS(parse(""), std::exception);
        TS_ASSERT_THROWS(parse("invalid"), std::exception);
        TS_ASSERT_THROWS(parse("5.invalid"), std::exception);
        TS_ASSERT_THROWS(parse("5"), std::exception);
        TS_ASSERT_THROWS(parse("5."), std::exception);
        TS_ASSERT_THROWS(parse("5.2.invalid"), std::exception);
        TS_ASSERT_THROWS(parse("5.2."), std::exception);
        TS_ASSERT_THROWS(parse("5.3.1.invalid"), std::exception);
        TS_ASSERT_THROWS(parse("5.3.1."), std::exception);
        TS_ASSERT_THROWS(parse("0.0.1"), std::exception);
        TS_ASSERT_THROWS(parse("-5.2.1"), std::exception);
        TS_ASSERT_THROWS(parse("5.-2.1"), std::exception);
        TS_ASSERT_THROWS(parse("5.2.-1"), std::exception);

        TS_ASSERT_THROWS(Version(-5, 2, 1), std::exception);
        TS_ASSERT_THROWS(Version(5, -2, 1), std::exception);
        TS_ASSERT_THROWS(Version(5, 2, -1), std::exception);
        TS_ASSERT_THROWS(Version(0, 0, 1), std::exception);
        TS_ASSERT_THROWS(Version(5, 2, 1, "invalid"), std::exception);
    }

    void testCompareVersions()
    {
        using namespace ta::version;

        TS_ASSERT_EQUALS(Version(), Version(1, 0, 0, ""));
        TS_ASSERT_EQUALS(Version(5, 3, 0, ""), Version(5, 3));
        TS_ASSERT_EQUALS(Version(5, 3, 0, "p1"), Version(5, 3, 0, "p001"));

        TS_ASSERT_LESS_THAN(Version(5, 99, 88), Version(6, 0, 1));
        TS_ASSERT_LESS_THAN(Version(5, 1, 99), Version(5, 2, 0));
        TS_ASSERT_LESS_THAN(Version(5, 1, 2), Version(5, 2, 1));
        TS_ASSERT_LESS_THAN(Version(5, 1, 2), Version(5, 1, 2, "p1"));
        TS_ASSERT_LESS_THAN(Version(5, 1, 2, "a4"), Version(5, 1, 2, "b1"));

        TS_ASSERT(Version(5, 1, 2, "p2") > Version(5, 1, 2, "p1"));
        TS_ASSERT(Version(5, 1, 2) > Version(5, 1, 2, "b9"));

        TS_ASSERT(Version(5, 1, 2) >= Version(5, 1, 2));
        TS_ASSERT(Version(5, 1, 2) <= Version(5, 1, 2));
        TS_ASSERT(!(Version(5, 1, 2) > Version(5, 1, 2)));
        TS_ASSERT(!(Version(5, 1, 2) < Version(5, 1, 2)));
    }

    void testVertionToStr()
    {
        using namespace ta::version;

        TS_ASSERT_EQUALS(toStr(Version(0,1,23,"p1")), "0.1.23.p1");
        TS_ASSERT_EQUALS(toStr(Version(0,1)), "0.1.0");

        TS_ASSERT_EQUALS(toStr(Version(0,1,23,"p1"), fmtMajorMinorSubminorDevstage), "0.1.23.p1");
        TS_ASSERT_EQUALS(toStr(Version(0,1), fmtMajorMinorSubminorDevstage), "0.1.0");

        TS_ASSERT_EQUALS(toStr(Version(0,1,23,"p1"), fmtMajorMinorSubminor), "0.1.23");
        TS_ASSERT_EQUALS(toStr(Version(0,1), fmtMajorMinorSubminor), "0.1.0");

        TS_ASSERT_EQUALS(toStr(Version(0,1,23,"p1"), fmtMajorMinor), "0.1");
        TS_ASSERT_EQUALS(toStr(Version(0,1), fmtMajorMinor), "0.1");
    }
};
