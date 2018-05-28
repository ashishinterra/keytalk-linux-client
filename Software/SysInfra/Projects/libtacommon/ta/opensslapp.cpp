#include "opensslapp.h"
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
#include "openssl/ssl.h"
#include "openssl/err.h"
#include "openssl/engine.h"
#include "openssl/conf.h"
#endif

namespace ta
{
    OpenSSLApp::OpenSSLApp()
    {
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
        SSL_load_error_strings();
        SSL_library_init();
        OpenSSL_add_all_algorithms();
        //@note no need to seed entropy because it is already done by RAND_poll() and called by default implementation of RAND_bytes() (see RAND_SSLeay())
#endif
    }
    OpenSSLApp::~OpenSSLApp()
    {
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
        ERR_remove_state(0);
        ENGINE_cleanup();
        CONF_modules_unload(1);
        ERR_free_strings();
        EVP_cleanup();
        CRYPTO_cleanup_all_ex_data();
#endif
    }
}
