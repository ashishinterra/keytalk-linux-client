#pragma once

#include "ta/opensslapp.h"
#include "cxxtest/TestSuite.h"
#include "cxxtest/GlobalFixture.h"
#include <stdexcept>
#include <iostream>
#include "openssl/crypto.h"


static bool theMemLeaksFound = false;
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
static void* memleak_cb(unsigned long, const char*, int, int, void *)
{
    theMemLeaksFound = true;
	return NULL;
}
#endif

#ifdef WIN32
#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>
#endif

class GlobFixture : public CxxTest::GlobalFixture
{
	ta::OpenSSLApp* theOpenSSLAppPtr;

    static void enableMemLeakHunting()
    {
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
        CRYPTO_set_mem_debug(1);
#else
        CRYPTO_malloc_debug_init();
        CRYPTO_dbg_set_options(V_CRYPTO_MDEBUG_ALL);
#endif
        CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);
#ifdef WIN32
        _CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
        _CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_FILE);
        _CrtSetReportFile( _CRT_WARN, _CRTDBG_FILE_STDOUT );
#endif
    }
public:
    GlobFixture()
        : theOpenSSLAppPtr(new ta::OpenSSLApp())
    {}
    ~GlobFixture()
    {
        delete theOpenSSLAppPtr;
        theOpenSSLAppPtr = NULL;
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
#ifndef OPENSSL_NO_CRYPTO_MDEBUG
        const int ret = CRYPTO_mem_leaks_fp(stderr);
        theMemLeaksFound = (ret == 0);
#else
        fprintf(stderr, "WARNING. Memory leak checks are disabled, rebuild OpenSSL to enable\n");
       // pre-built OpenSSL-1.1.0 is normally shipped with OPENSSL_NO_CRYPTO_MDEBUG defined hence no memory leak reports will be shown
       // to enable memleak hunting you should build OpenSSL yourself
#endif
#else
	    CRYPTO_mem_leaks_cb(memleak_cb);
	    CRYPTO_mem_leaks_fp(stderr);
#endif
        if (theMemLeaksFound)
        {
            exit(1); //@todo find more graceful way to fail the test because of memleaks
        }
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
