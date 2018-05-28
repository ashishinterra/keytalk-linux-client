#pragma once

#include "ta/dynlibloader.h"
#include "cxxtest/TestSuite.h"
#include <string>

using namespace ta;
using std::string;

class DynLibLoaderTest : public CxxTest::TestSuite
{
public:
	void testLoad()
	{
        string myLibBaseName, myExportedSymbol, myNotExportedSymbol;
#ifdef _WIN32
        myLibBaseName = "msvcrt";
        myExportedSymbol = "strcmp";
        myNotExportedSymbol = "___strcmp___";
#elif defined(__linux__)
        myLibBaseName = "dl";
        myExportedSymbol = "dlsym";
        myNotExportedSymbol = "___dlsym___";
#else
# error "Unsupported platform"
#endif
        string myLibName = DynLibLoader::makeLibName(myLibBaseName);
        DynLibLoader myLoader(myLibName);

        TS_ASSERT(myLoader.getFuncPtr(myExportedSymbol));
        TS_ASSERT_THROWS(myLoader.getFuncPtr(myNotExportedSymbol), GetFuncPtrError);
	}
};
