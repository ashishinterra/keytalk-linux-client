//----------------------------------------------------------------------------
//
//  Name          OpenSSLApp.h
//  Description : OpenSSLApp class is used to initialize and automatically deinintialize OpenSSL library in RAII-way
//                Normally the OpenSSLApp object is created once per app, during its lifetime OpenSSL stuff can be called
//
//----------------------------------------------------------------------------
#pragma once

namespace ta
{
    class OpenSSLApp
    {
    public:
        /**
        Initializes OpenSSL library
        The object of this class should be normally created once on the application startup.
        @nothrow
        */
        OpenSSLApp();
        ~OpenSSLApp();
    };
}
