#include "ta/Browser.h"
#include "cxxtest/TestSuite.h"

class BrowserTest : public CxxTest::TestSuite
{
public:
    void testGetDefault()
    {
        TS_ASSERT_THROWS_NOTHING(ta::Browser::getDefault());
    }
};

