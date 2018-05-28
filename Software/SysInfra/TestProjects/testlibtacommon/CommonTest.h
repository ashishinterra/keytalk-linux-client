#pragma once

#include "ta/common.h"
#include "cxxtest/TestSuite.h"
#include <string>
#include <stdexcept>

#include "boost/assign/list_of.hpp"
#include "boost/algorithm/string.hpp"

using std::string;

struct MyException1 : std::logic_error
{
    explicit MyException1(const std::string& aMessage = "")	: std::logic_error(aMessage) {}
};

struct MyException2 : std::logic_error
{
    explicit MyException2(int anErrorCode, const std::string& aMessage = "")	: std::logic_error(aMessage), theErrorCode(anErrorCode) {}
    virtual int getErrorCode() const { return theErrorCode; }
private:
    int theErrorCode;
};


class CommonTest : public CxxTest::TestSuite
{
public:
    void testSafeFormat()
    {
        // correct usage
        TS_ASSERT_EQUALS(str(ta::safe_format("abc %s %d") % "d" % 123), "abc d 123");

        // more args than expected
        TS_ASSERT_EQUALS(str(ta::safe_format("abc %s") % "first" % 123), "abc first");
        TS_ASSERT_EQUALS(str(ta::safe_format("abc %1%") % "first" % 123),  "abc first");

        // less args than expected
        TS_ASSERT_EQUALS(str(ta::safe_format("abc %s %d %u") % "first" % 123), "");
        TS_ASSERT_EQUALS(str(ta::safe_format("abc %1% %2% %3%") % "first" % 123), "");

        // type mismatch
        TS_ASSERT_EQUALS(str(ta::safe_format("abc %d") % "not-a-number"), "abc not-a-number");

        // ill-formed format string
        TS_ASSERT_THROWS_NOTHING(ta::safe_format(""));
        TS_ASSERT_THROWS_NOTHING(ta::safe_format("abc %1% %d"));
        TS_ASSERT_EQUALS(str(ta::safe_format("")), "");
        TS_ASSERT_EQUALS(str(ta::safe_format("abc %1% %d")), "");
        TS_ASSERT_EQUALS(str(ta::safe_format("abc %1% %d") % "first" % 2), "");
        TS_ASSERT_EQUALS(str(ta::safe_format("") % "first" % 2), "");

        // a mixture of the above
        TS_ASSERT_EQUALS(str(ta::safe_format("abc %d% %2%") % 2), "");
    }

    void testExceptionMacroMsg()
    {
        TS_TRACE("-- Test regular usage");
        try
        {
            TA_THROW(MyException1);
            TS_FAIL("Exception not thrown");
        }
        catch (MyException1& e)
        {
            string myWhat = e.what();
            TS_ASSERT(myWhat.find("MyException1") != string::npos);
            TS_ASSERT(myWhat.find("testExceptionMacroMsg") != string::npos);
        }

        try
        {
            TA_THROW_MSG(MyException1, NULL);
            TS_FAIL("Exception not thrown");
        }
        catch (MyException1& e)
        {
            string myWhat = e.what();
            TS_ASSERT(myWhat.find("MyException1") != string::npos);
            TS_ASSERT(myWhat.find("testExceptionMacroMsg") != string::npos);
        }

        try
        {
            TA_THROW_MSG(MyException1, "Error!");
            TS_FAIL("Exception not thrown");
        }
        catch (MyException1& e)
        {
            string myWhat = e.what();
            TS_ASSERT(myWhat.find("MyException1") != string::npos);
            TS_ASSERT(myWhat.find("testExceptionMacroMsg") != string::npos);
            TS_ASSERT(myWhat.find("Error!") != string::npos);
        }

        try
        {
            TA_THROW_MSG(MyException1, boost::format("errno: %1%") % -1);
            TS_FAIL("Exception not thrown");
        }
        catch (MyException1& e)
        {
            string myWhat = e.what();
            TS_ASSERT(myWhat.find("MyException1") != string::npos);
            TS_ASSERT(myWhat.find("testExceptionMacroMsg") != string::npos);
            TS_ASSERT(myWhat.find("errno: -1") != string::npos);
        }

        try
        {
            TA_THROW_MSG(MyException1, "https://ta?%1%");
            TS_FAIL("Exception not thrown");
        }
        catch (MyException1& e)
        {
            string myWhat = e.what();
            TS_ASSERT(myWhat.find("MyException1") != string::npos);
            TS_ASSERT(myWhat.find("testExceptionMacroMsg") != string::npos);
            TS_ASSERT(myWhat.find("https://ta?%1%") != string::npos);
        }

        try
        {
            TA_THROW_MSG(MyException1, "https://ta?%s");
            TS_FAIL("Exception not thrown");
        }
        catch (MyException1& e)
        {
            string myWhat = e.what();
            TS_ASSERT(myWhat.find("MyException1") != string::npos);
            TS_ASSERT(myWhat.find("testExceptionMacroMsg") != string::npos);
            TS_ASSERT(myWhat.find("https://ta?%s") != string::npos);
        }

        TS_TRACE("-- Test that bad format strings are not tolerated by boost::format");
        try
        {
            TA_THROW_MSG(MyException1, boost::format("errno: %1% %2%") % -1);
            TS_FAIL("Exception not thrown");
        }
        catch (MyException1&)
        {
            TS_FAIL("MyException1 is not expected to be thrown");
        }
        catch (std::exception&)
        {
            // ok
        }

        try
        {
            TA_THROW_MSG(MyException1, boost::format("errno: %1%") % -1 % "-2");
            TS_FAIL("Exception not thrown");
        }
        catch (MyException1&)
        {
            TS_FAIL("MyException1 is not expected to be thrown");
        }
        catch (std::exception&)
        {
            // ok
        }

        TS_TRACE("-- Test that bad format strings are tolerated by ta::safe_format");
        try
        {
            TA_THROW_MSG(MyException1, ta::safe_format("errno: %1% %2%") % -1);
            TS_FAIL("Exception not thrown");
        }
        catch (MyException1& e)
        {
            string myWhat = e.what();
            TS_ASSERT_DIFFERS(myWhat.find("MyException1"), string::npos);
            TS_ASSERT_DIFFERS(myWhat.find("testExceptionMacroMsg"), string::npos);
            TS_ASSERT_EQUALS(myWhat.find("errno:"), string::npos);
        }

        try
        {
            TA_THROW_MSG(MyException1, ta::safe_format("errno: %1%") % -1 % "-2");
            TS_FAIL("Exception not thrown");
        }
        catch (MyException1& e)
        {
            string myWhat = e.what();
            TS_ASSERT_DIFFERS(myWhat.find("MyException1"), string::npos);
            TS_ASSERT_DIFFERS(myWhat.find("testExceptionMacroMsg"), string::npos);
            TS_ASSERT_DIFFERS(myWhat.find("errno: -1"), string::npos);
        }
    }

    void testExceptionMacroErrMsg()
    {
        TS_TRACE("-- Test regular usage");
        int myErrorCode = 13;

        try
        {
            TA_THROW_ARG(MyException2, myErrorCode);
            TS_FAIL("Exception not thrown");
        }
        catch (MyException2& e)
        {
            string myWhat = e.what();
            TS_ASSERT(myWhat.find("MyException2") != string::npos);
            TS_ASSERT(myWhat.find("testExceptionMacroErrMsg") != string::npos);
            TS_ASSERT_EQUALS(e.getErrorCode(), myErrorCode);
        }

        try
        {
            TA_THROW_ARG_MSG(MyException2, myErrorCode, NULL);
            TS_FAIL("Exception not thrown");
        }
        catch (MyException2& e)
        {
            string myWhat = e.what();
            TS_ASSERT(myWhat.find("MyException2") != string::npos);
            TS_ASSERT(myWhat.find("testExceptionMacroErrMsg") != string::npos);
            TS_ASSERT_EQUALS(e.getErrorCode(), myErrorCode);
        }

        try
        {
            TA_THROW_ARG_MSG(MyException2, myErrorCode, "Error!");
            TS_FAIL("Exception not thrown");
        }
        catch (MyException2& e)
        {
            string myWhat = e.what();
            TS_ASSERT(myWhat.find("MyException2") != string::npos);
            TS_ASSERT(myWhat.find("testExceptionMacroErrMsg") != string::npos);
            TS_ASSERT(myWhat.find("Error!") != string::npos);
            TS_ASSERT_EQUALS(e.getErrorCode(), myErrorCode);
        }

        try
        {
            TA_THROW_ARG_MSG(MyException2, myErrorCode, boost::format("Error! File '%1%' is ill-formed.") % "file.bad");
            TS_FAIL("Exception not thrown");
        }
        catch (MyException2& e)
        {
            string myWhat = e.what();
            TS_ASSERT(myWhat.find("MyException2") != string::npos);
            TS_ASSERT(myWhat.find("testExceptionMacroErrMsg") != string::npos);
            TS_ASSERT(myWhat.find("Error! File 'file.bad' is ill-formed.") != string::npos);
            TS_ASSERT_EQUALS(e.getErrorCode(), myErrorCode);
        }


        try
        {
            TA_THROW_ARG_MSG(MyException2, myErrorCode, "Error! https://ta?%1%");
            TS_FAIL("Exception not thrown");
        }
        catch (MyException2& e)
        {
            string myWhat = e.what();
            TS_ASSERT(myWhat.find("MyException2") != string::npos);
            TS_ASSERT(myWhat.find("testExceptionMacroErrMsg") != string::npos);
            TS_ASSERT(myWhat.find("Error! https://ta?%1%") != string::npos);
            TS_ASSERT_EQUALS(e.getErrorCode(), myErrorCode);
        }

        try
        {
            TA_THROW_ARG_MSG(MyException2, myErrorCode, "Error! https://ta?%s");
            TS_FAIL("Exception not thrown");
        }
        catch (MyException2& e)
        {
            string myWhat = e.what();
            TS_ASSERT(myWhat.find("MyException2") != string::npos);
            TS_ASSERT(myWhat.find("testExceptionMacroErrMsg") != string::npos);
            TS_ASSERT(myWhat.find("Error! https://ta?%s") != string::npos);
            TS_ASSERT_EQUALS(e.getErrorCode(), myErrorCode);
        }

        TS_TRACE("-- Test that bad format strings are not tolerated by boost::format");
        try
        {
            TA_THROW_ARG_MSG(MyException2, -1, boost::format("errno: %1% %2%") % -1);
            TS_FAIL("Exception not thrown");
        }
        catch (MyException2&)
        {
            TS_FAIL("MyException2 is not expected to be thrown");
        }
        catch (std::exception&)
        {
            // ok
        }

        try
        {
            TA_THROW_ARG_MSG(MyException2, -1, boost::format("errno: %1%") % -1 % "-2");
            TS_FAIL("Exception not thrown");
        }
        catch (MyException2&)
        {
            TS_FAIL("MyException2 is not expected to be thrown");
        }
        catch (std::exception&)
        {
            // ok
        }

        TS_TRACE("-- Test that bad format strings are tolerated by ta::safe_format");
        try
        {
            TA_THROW_ARG_MSG(MyException2, -1, ta::safe_format("errno: %1% %d") % -1);
            TS_FAIL("Exception not thrown");
        }
        catch (MyException2& e)
        {
            string myWhat = e.what();
            TS_ASSERT_DIFFERS(myWhat.find("MyException2"), string::npos);
            TS_ASSERT_DIFFERS(myWhat.find("testExceptionMacroErrMsg"), string::npos);
            TS_ASSERT_EQUALS(myWhat.find("errno:"), string::npos);
        }

        try
        {
            TA_THROW_ARG_MSG(MyException2, -1, ta::safe_format("errno: %1%") % -1 % "-2");
            TS_FAIL("Exception not thrown");
        }
        catch (MyException2& e)
        {
            string myWhat = e.what();
            TS_ASSERT_DIFFERS(myWhat.find("MyException2"), string::npos);
            TS_ASSERT_DIFFERS(myWhat.find("testExceptionMacroErrMsg"), string::npos);
            TS_ASSERT_DIFFERS(myWhat.find("errno: -1"), string::npos);
        }
    }

    void testElemExist()
    {
        std::vector<string> myVec = boost::assign::list_of("a")("bc")("d");

        TS_ASSERT(ta::isElemExist("a", myVec));
        TS_ASSERT(!ta::isElemExist("A", myVec));
        TS_ASSERT(ta::isElemExistWhen(isNotEmpty, myVec));
        TS_ASSERT(!ta::isElemExistWhenNot(isNotEmpty, myVec));
        TS_ASSERT(!ta::isElemExistWhen(&string::empty, myVec));
        TS_ASSERT(ta::isElemExistWhenNot(&string::empty, myVec));

        myVec.push_back("");
        TS_ASSERT(ta::isElemExistWhen(isNotEmpty, myVec));
        TS_ASSERT(ta::isElemExistWhenNot(isNotEmpty, myVec));
        TS_ASSERT(ta::isElemExistWhen(&string::empty, myVec));
        TS_ASSERT(ta::isElemExistWhenNot(&string::empty, myVec));
    }

    void testElemExistInPodArray()
    {
        const string myArr[] = {"a", "bc", "d"};

        TS_ASSERT(ta::isPodArrayElemExist(string("a"), myArr));
        TS_ASSERT(!ta::isPodArrayElemExist(string("A"), myArr));
    }

    void testGetDictVal()
    {
        const std::map<string, string> myNumbers = boost::assign::map_list_of<string, string>("1", "one")("3", "three");

        TS_ASSERT(ta::isKeyExist("1", myNumbers));
        TS_ASSERT(!ta::isKeyExist("2", myNumbers));
        TS_ASSERT(ta::isKeyExist("3", myNumbers));

        string myNumberName;
        TS_ASSERT(ta::findValueByKey("1", myNumbers, myNumberName));
        TS_ASSERT_EQUALS(myNumberName, "one");
        TS_ASSERT(!ta::findValueByKey("2", myNumbers, myNumberName));
        TS_ASSERT(ta::findValueByKey("3", myNumbers, myNumberName));
        TS_ASSERT_EQUALS(myNumberName, "three");
    }

    void testFilter()
    {
        using boost::assign::list_of;

        std::vector<string> myVec = list_of<string>("")("a")("")("bc");

        TS_ASSERT_EQUALS(ta::filterWhen(isNotEmpty, myVec), list_of<string>("a")("bc"));
        TS_ASSERT_EQUALS(ta::filterOutWhen(&string::empty, myVec), list_of<string>("a")("bc"));

        myVec = list_of<string>("A")("a")("b")("a");
        TS_ASSERT_EQUALS(ta::filterOut("a", myVec), list_of<string>("A")("b"));
    }

    void testIntersect()
    {
        using boost::assign::list_of;

        // given
        std::vector<int> mySec1 = list_of(1)(2)(3)(4);
        std::vector<int> mySec2 = list_of(4)(1)(5);

        // when-then
        TS_ASSERT_EQUALS(ta::intersect(mySec1, mySec2), list_of<int>(1)(4));
        TS_ASSERT_EQUALS(ta::intersect(mySec1, std::vector<int>()), std::vector<int>());
        TS_ASSERT_EQUALS(ta::intersect(std::vector<int>(), mySec2), std::vector<int>());
        // when-then
        TS_ASSERT_EQUALS(ta::intersectSets(ta::vec2Set(mySec1), ta::vec2Set(mySec2)), list_of<int>(1)(4));
        TS_ASSERT_EQUALS(ta::intersectSets(ta::vec2Set(mySec1), std::set<int>()), std::set<int>());
        TS_ASSERT_EQUALS(ta::intersectSets(std::set<int>(), ta::vec2Set(mySec2)), std::set<int>());

        // given
        mySec2 = boost::assign::list_of(5)(6)(7);
        // when-then
        TS_ASSERT_EQUALS(ta::intersect(mySec1, mySec2), std::vector<int>());
        TS_ASSERT_EQUALS(ta::intersectSets(ta::vec2Set(mySec1), ta::vec2Set(mySec2)), std::set<int>());
    }

    void testSubtract()
    {
        using boost::assign::list_of;

        // given
        std::vector<int> mySec1 = list_of(1)(2)(3)(4);
        std::vector<int> mySec2 = list_of(4)(1)(5);

        // when-then (vector)
        TS_ASSERT_EQUALS(ta::subtract(mySec1, mySec2), list_of<int>(2)(3));
        TS_ASSERT_EQUALS(ta::subtract(mySec1, mySec1), std::vector<int>());
        TS_ASSERT_EQUALS(ta::subtract(mySec1, std::vector<int>()), mySec1);
        TS_ASSERT_EQUALS(ta::subtract(std::vector<int>(), mySec2), std::vector<int>());
        // when-then (set)
        TS_ASSERT_EQUALS(ta::subtractSets(ta::vec2Set(mySec1), ta::vec2Set(mySec2)), list_of<int>(2)(3));
        TS_ASSERT_EQUALS(ta::subtractSets(ta::vec2Set(mySec1), ta::vec2Set(mySec1)), std::set<int>());
        TS_ASSERT_EQUALS(ta::subtractSets(ta::vec2Set(mySec1), std::set<int>()), ta::vec2Set(mySec1));
        TS_ASSERT_EQUALS(ta::subtractSets(std::set<int>(), ta::vec2Set(mySec2)), std::set<int>());

        // given
        mySec2 = list_of(1)(2)(3)(4)(5);
        // when-then
        TS_ASSERT_EQUALS(ta::subtract(mySec1, mySec2), std::vector<int>());
        TS_ASSERT_EQUALS(ta::subtractSets(ta::vec2Set(mySec1), ta::vec2Set(mySec2)), std::set<int>());
    }


    void testGetFirstElem()
    {
        using boost::assign::list_of;

        std::vector<string> myVec = list_of("")("a")("")("bc");
        TS_ASSERT_EQUALS(ta::getFirstElem(isNotEmpty, myVec), "a");

        myVec = list_of("")("");
        TS_ASSERT_THROWS(ta::getFirstElem(isNotEmpty, myVec), std::exception);
    }

    void testGetUniqueElem()
    {
        std::vector<string> myVec = boost::assign::list_of("")("a")("");
        TS_ASSERT_EQUALS(ta::getUniqueElem(isNotEmpty, myVec), "a");

        // no such element
        myVec = boost::assign::list_of("")("");
        TS_ASSERT_THROWS(ta::getUniqueElem(isNotEmpty, myVec), std::exception);

        // not unique
        myVec = boost::assign::list_of("")("a")("")("bc");
        TS_ASSERT_THROWS(ta::getUniqueElem(isNotEmpty, myVec), std::exception);
    }

    void testDuplicates()
    {
        std::vector<string> myVec = boost::assign::list_of("1")("2")("3");
        TS_ASSERT(!ta::hasDuplicates(myVec));
        TS_ASSERT(!ta::hasDuplicates(ta::removeDuplicates(myVec)));
        TS_ASSERT_EQUALS(ta::removeDuplicates(myVec), myVec);

        myVec = boost::assign::list_of("1")("3")("1")("3")("2")("2");
        TS_ASSERT(ta::hasDuplicates(myVec));
        TS_ASSERT(!ta::hasDuplicates(ta::removeDuplicates(myVec)));
        TS_ASSERT_EQUALS(ta::removeDuplicates(myVec), boost::assign::list_of("1")("2")("3"));

        myVec = boost::assign::list_of("1")("1")("1");
        TS_ASSERT(ta::hasDuplicates(myVec));
        TS_ASSERT(!ta::hasDuplicates(ta::removeDuplicates(myVec)));
        TS_ASSERT_EQUALS(ta::removeDuplicates(myVec), boost::assign::list_of("1"));

        myVec.clear();
        TS_ASSERT(!ta::hasDuplicates(myVec));
        TS_ASSERT(!ta::hasDuplicates(ta::removeDuplicates(myVec)));
        TS_ASSERT_EQUALS(ta::removeDuplicates(myVec), myVec);
    }

    void testConversion()
    {
        using boost::assign::list_of;

        {
            const char* arr[] = {"one", "two", "three", NULL};
            TS_ASSERT_EQUALS(ta::podArray2StringArray(arr), list_of("one")("two")("three"));
        }

        {
            const char* arr[] = {"one", "two", "three", NULL, "four"};
            TS_ASSERT_EQUALS(ta::podArray2StringArray(arr), list_of("one")("two")("three"));
        }

        {
            const char* arr[] = {NULL};
            TS_ASSERT_EQUALS(ta::podArray2StringArray(arr), ta::StringArray());
        }
    }

private:
    static bool isNotEmpty(const std::string& anStr) { return !anStr.empty(); }
};
