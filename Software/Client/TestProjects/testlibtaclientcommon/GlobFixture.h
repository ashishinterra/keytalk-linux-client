#ifndef GlobFixture_H
#define GlobFixture_H

#include "cxxtest/TestSuite.h"
#include "cxxtest/GlobalFixture.h"
#include "ta/opensslapp.h"
#include <stdexcept>

#define HUNT_FOR_MEM_LEAKS

#if defined(WIN32) && defined (HUNT_FOR_MEM_LEAKS)
#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>
#endif

class GlobFixture : public CxxTest::GlobalFixture
{
	ta::OpenSSLApp* theOpenSSLAppPtr;

    static void enableMemLeakHunting()
    {
#if defined(WIN32) && defined (HUNT_FOR_MEM_LEAKS)
        _CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
        _CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_FILE);
        _CrtSetReportFile( _CRT_WARN, _CRTDBG_FILE_STDOUT );
#endif
    }
public:
    // throw std::runtime_error on error
	GlobFixture()
		: theOpenSSLAppPtr(new ta::OpenSSLApp())
	{}
    ~GlobFixture()
    {
        delete theOpenSSLAppPtr;
        theOpenSSLAppPtr = NULL;
    }
    virtual bool setUpWorld()
    {
        enableMemLeakHunting();
		return true;
    }
	virtual bool tearDownWorld()
	{
		return true;
	}
};

#endif
