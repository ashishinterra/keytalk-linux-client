#ifndef ComputerUuidTest_H
#define ComputerUuidTest_H

#include "resept/computeruuid.h"
#include "ta/common.h"
#include "cxxtest/TestSuite.h"
#include "boost/algorithm/string.hpp"

class ComputerUuidTest : public CxxTest::TestSuite
{
public:
    void testCalcCs()
    {
        using namespace resept::ComputerUuid;

#ifdef _WIN32
        const std::string myFormula = " 0,1,01, 2, -1,3,,4,5,6,7,8,  9,   , 10, 11, 12, 13,14,-000 ,15,foo,16,99,601";
		const std::string myExpectedParsedFormula = "0,1,1,2,3,4,5,6,7,8,9,10,11,12,13,14,0,15,16,0";  // everything beyond 0-99 range is ignored
#else
        const std::string myFormula = " 0,601,0601, 602, -601,603,,604,605,606,607,608,  689,   , 690, 691, 12, 13,614,-100 ,615,foo,616,999";
		const std::string myExpectedParsedFormula = "0,601,601,602,603,604,605,606,607,608,0,0,0,0,0,0"; // everything beyond 6XX range is ignored
#endif
        bool myIsFormulaOk;
        std::string myParsedFormula;

        std::string myCs = calcCs(myFormula, &myParsedFormula, &myIsFormulaOk);
        TS_ASSERT(!myIsFormulaOk);
        TS_ASSERT_EQUALS(myParsedFormula, myExpectedParsedFormula);

        TS_ASSERT_EQUALS(calcCs(myFormula), calcCs(myExpectedParsedFormula, &myParsedFormula, &myIsFormulaOk));
        TS_ASSERT(myIsFormulaOk);

        TS_ASSERT_EQUALS(calcCs("sometext"), calcCs(""));
        TS_ASSERT_EQUALS(calcCs("word1,word2"), calcCs(""));
        TS_ASSERT_EQUALS(calcCs("00"), calcCs("0"));

#ifdef _WIN32
        TS_ASSERT_EQUALS(calcCs("1 "), calcCs("1"));
        TS_ASSERT_EQUALS(calcCs("1,2"), calcCs("    1, 2    "));
        TS_ASSERT_EQUALS(calcCs("1,2,0,3"), calcCs("1,2,77,3, 101"));
        TS_ASSERT_EQUALS(calcCs("1,2"), calcCs("1,2,"));
        TS_ASSERT_EQUALS(calcCs("1,2"), calcCs("1,2,__nonnumber__"));

        TS_ASSERT_DIFFERS(calcCs("1"), calcCs("2"));
        TS_ASSERT_DIFFERS(calcCs("1"), calcCs("1,1"));
        TS_ASSERT_DIFFERS(calcCs("1,2"), calcCs("2,1"));
#else
        TS_ASSERT_EQUALS(calcCs("601 "), calcCs("601"));
        TS_ASSERT_EQUALS(calcCs("601,602"), calcCs("    601, 602    "));
        TS_ASSERT_EQUALS(calcCs("601,602,603"), calcCs("601,602,77,603, 101"));
        TS_ASSERT_EQUALS(calcCs("601,602"), calcCs("601,602,"));
        TS_ASSERT_EQUALS(calcCs("601,602"), calcCs("601,602,__nonnumber__"));

        TS_ASSERT_DIFFERS(calcCs("601"), calcCs("602"));
        TS_ASSERT_DIFFERS(calcCs("601"), calcCs("601,601"));
        TS_ASSERT_DIFFERS(calcCs("601,602"), calcCs("602,601"));
#endif
    }
};

#endif
