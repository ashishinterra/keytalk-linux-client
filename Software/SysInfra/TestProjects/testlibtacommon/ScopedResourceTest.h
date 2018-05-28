#pragma once

#include "ta/scopedresource.hpp"
#include "cxxtest/TestSuite.h"

namespace DefResource
{
    typedef unsigned int* MyHandle;
    static bool theIsCreated = false;
    static MyHandle createValidResource()
    {
        theIsCreated = true;
        static unsigned int myResource = 1234;
        return &myResource;
    }
    static MyHandle createInvalidResource()
    {
        theIsCreated = false;
        return NULL;
    }
    static void closeResource(MyHandle aResource)
    {
        if (!aResource)
            throw std::logic_error("Trying to close invalid resource");
        theIsCreated = false;
    }
}

namespace Socket
{
    typedef int MyHandle;
    static bool theIsCreated = false;

    static MyHandle createValidSocket()
    {
        theIsCreated = true;
        return 1234;
    }
    static MyHandle createInvalidSocket()
    {
        theIsCreated = false;
        return -1;
    }
    static void closeSocket(MyHandle aSocketHandle)
    {
        if (aSocketHandle == -1)
            throw std::logic_error("Trying to close invalid socket");
        theIsCreated = false;
    }
}

class ScopedResourceTest : public CxxTest::TestSuite
{
public:
    void testValidDefResource()
    {
        using namespace DefResource;
        using namespace ta;

        theIsCreated = false;
        {
            ScopedResource<MyHandle> myValidResource(createValidResource(), closeResource);
            TS_ASSERT(myValidResource);
            TS_ASSERT(theIsCreated);
        }
        TS_ASSERT(!theIsCreated);
    }
    void testInvalidDefResource()
    {
        using namespace DefResource;
        using namespace ta;

        theIsCreated = false;
        {
            ScopedResource<MyHandle> myInvalidResource(createInvalidResource(), closeResource);
            TS_ASSERT(!myInvalidResource);
            TS_ASSERT(!theIsCreated);
        }
        TS_ASSERT(!theIsCreated);
    }
    void testValidSocket()
    {
        using namespace Socket;
        using namespace ta;

        theIsCreated = false;

        {
            ScopedResource<MyHandle> myValidSocket(createValidSocket(), closeSocket, -1);
            TS_ASSERT(myValidSocket != -1);
            TS_ASSERT(theIsCreated);
        }
        TS_ASSERT(!theIsCreated);
    }
    void testInvalidSocket()
    {
        using namespace Socket;
        using namespace ta;

        theIsCreated = false;

        {
            ScopedResource<MyHandle> myInvalidSocket(createInvalidSocket(), closeSocket, -1);
            TS_ASSERT(myInvalidSocket == -1);
            TS_ASSERT(!theIsCreated);
        }
        TS_ASSERT(!theIsCreated);
    }
    void testInvalidSocketBadInvalidHandle()
    {
        using namespace Socket;
        using namespace ta;

        theIsCreated = false;

        {
            ScopedResource<MyHandle> myInvalidSocket(createInvalidSocket(), closeSocket, 0);
            TS_ASSERT(myInvalidSocket != 0);
            TS_ASSERT(!theIsCreated);
        }
        TS_ASSERT(!theIsCreated);
    }
    void testAssign()
    {
        using namespace Socket;
        using namespace ta;

        theIsCreated = false;

        {
            ScopedResource<MyHandle> mySocket(createValidSocket(), closeSocket, -1);
            TS_ASSERT(mySocket != -1);
            mySocket.assign(createValidSocket(), closeSocket);
            TS_ASSERT(mySocket != -1);
            mySocket.assign(createInvalidSocket(), closeSocket);
            TS_ASSERT(mySocket == -1);
        }
    }
    void testDetach()
    {
        using namespace Socket;
        using namespace ta;

        theIsCreated = false;

        {
            {
                ScopedResource<MyHandle> mySocket(createValidSocket(), closeSocket, -1);
                TS_ASSERT(mySocket != -1);
                TS_ASSERT(theIsCreated);
                MyHandle mySocketHandle = mySocket;

                TS_ASSERT_EQUALS(mySocket.detach(), mySocketHandle);
                TS_ASSERT(mySocket == -1);
                TS_ASSERT(theIsCreated);
            }
            TS_ASSERT(theIsCreated);
        }
    }
};
