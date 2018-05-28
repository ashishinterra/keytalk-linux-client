#pragma once

#include "ta/pngutils.h"
#include "ta/strings.h"
#include "ta/utils.h"
#include "cxxtest/TestSuite.h"
#include "boost/static_assert.hpp"
#include <string>
#include <vector>

class PngUtilsTest : public CxxTest::TestSuite
{
public:
	void test_that_png_info_on_nonexisting_file_throws_exception()
	{
        TS_ASSERT_THROWS(ta::PngUtils::getPngInfo("non-existing-file"), std::exception);
    }
	void test_that_png_info_on_nonbmp_file_throws_exception()
	{
        TS_ASSERT_THROWS(ta::PngUtils::getPngInfo("blob.tst"), std::exception);
    }
    void test_get_pngp_info_on_correct_image()
	{
        TS_ASSERT_EQUALS(ta::PngUtils::getPngInfo("30x32.png"), ta::PngUtils::PngInfo(30, 32));
        TS_ASSERT_EQUALS(ta::PngUtils::getPngInfo("49x55.png"), ta::PngUtils::PngInfo(49, 55));
        TS_ASSERT_EQUALS(ta::PngUtils::getPngInfo("2038x21.png"), ta::PngUtils::PngInfo(2038, 21));
    }
};
