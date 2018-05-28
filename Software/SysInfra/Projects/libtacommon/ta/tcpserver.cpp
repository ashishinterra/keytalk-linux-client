#include "tcpserver.h"
#include "strings.h"
#include "tcpsocketutils.h"
#include "netutils.h"
#include "common.h"
#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <arpa/inet.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <unistd.h>
# include <netdb.h>
# include <errno.h>
#endif
#include "boost/range/algorithm.hpp"
#include <algorithm>
#include <list>
#include <cassert>


namespace ta
{
    using std::string;
    using std::list;

    namespace
    {
        /**
        * @pre all sockets are in listening state
        * @param aListenSockets [in, out] the input is a list of listening sockets to wait for connection on,
        *        the output contains only those sockets for which the subsequent call to accept() is guaranteed to complete without blocking
        * @throw TcpServerError
        */
        void waitForConnection(list<SOCKET>& aListenSockets, const int aTimeoutMsec = TcpServer::Infinity)
        {
            assert(!aListenSockets.empty());
            fd_set myReadSockets;
            FD_ZERO(&myReadSockets);
            foreach (SOCKET sock, aListenSockets)
            {
                FD_SET(sock, &myReadSockets);
            }
            int myNumSockets = *(boost::max_element(aListenSockets)) + 1;
            int myRet = -1;
            if (aTimeoutMsec == TcpServer::Infinity)
            {
                myRet =  ::select(myNumSockets, &myReadSockets, NULL, NULL, NULL);
            }
            else
            {
                timeval myTv;
                myTv.tv_sec = aTimeoutMsec / 1000;
                myTv.tv_usec = (aTimeoutMsec % 1000) * 1000;
                myRet =  ::select(myNumSockets, &myReadSockets, NULL, NULL, &myTv);
            }
            if (myRet < 0)
                TA_THROW_MSG(TcpServerError, boost::format("select(2) failed. %1%") % NetUtils::getLastErrorStr());
            if (myRet == 0)
            {
                aListenSockets.clear();
                return;
            }

            list<SOCKET>::const_iterator end = aListenSockets.end();
            for (list<SOCKET>::iterator it = aListenSockets.begin(); it!= end; )
                if (!FD_ISSET(*it, &myReadSockets))
                    it = aListenSockets.erase(it);
                else
                    ++it;
        }
    }// end of private API


    struct TcpServer::TcpServerImpl
    {
        TcpServerImpl()
            : listenIpv4Sock(INVALID_SOCKET), listenIpv6Sock(INVALID_SOCKET)
        {}

        void listenIpv4(const NetUtils::IP& anIp, unsigned int aPort)
        {
            listenIpv4Sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (listenIpv4Sock == INVALID_SOCKET)
                TA_THROW_MSG(TcpServerError, boost::format("Failed to create IPv4 socket. %1%") % NetUtils::getLastErrorStr());

            int myReuseAddr = 1;
            if (::setsockopt(listenIpv4Sock, SOL_SOCKET, SO_REUSEADDR, (char*)&myReuseAddr, sizeof(myReuseAddr)) == SOCKET_ERROR)
                TA_THROW_MSG(TcpServerError, boost::format("Failed to setsockopt (SO_REUSEADDR). %1%") % NetUtils::getLastErrorStr());
            int myTcpNoDelay = 1;
            if (::setsockopt(listenIpv4Sock, IPPROTO_TCP, TCP_NODELAY, (char*)&myTcpNoDelay, sizeof(myTcpNoDelay)) == SOCKET_ERROR)
                TA_THROW_MSG(TcpServerError, boost::format("Failed to setsockopt (TCP_NODELAY). %1%") % NetUtils::getLastErrorStr());

            sockaddr_in mySelfAddr;
            memset(&mySelfAddr, 0, sizeof(mySelfAddr));
            mySelfAddr.sin_family = AF_INET;
            mySelfAddr.sin_port = htons(aPort);
            int myRet = -1;
            if ((myRet = inet_pton(AF_INET, anIp.ipv4.c_str(), &mySelfAddr.sin_addr)) == 0)
                TA_THROW_MSG(TcpServerError, boost::format("%1% is invalid IPv4 address") % anIp.ipv4);
            if (myRet < 0)
                TA_THROW_MSG(TcpServerError, boost::format("Failed to initialize IPv4 address %1%. %2%") % anIp.ipv4 % NetUtils::getLastErrorStr());
            if (::bind(listenIpv4Sock, (sockaddr*)&mySelfAddr, sizeof(mySelfAddr)) == SOCKET_ERROR)
                TA_THROW_MSG(TcpServerError, boost::format("bind failed. %1%") % NetUtils::getLastErrorStr());
            if (::listen(listenIpv4Sock, SOMAXCONN) == SOCKET_ERROR)
                TA_THROW_MSG(TcpServerError, boost::format("listen failed. %1%") % NetUtils::getLastErrorStr());
        }

        void listenIpv4(AddrType anAddrType, unsigned int aPort)
        {
            listenIpv4Sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (listenIpv4Sock == INVALID_SOCKET)
                TA_THROW_MSG(TcpServerError, boost::format("Failed to create IPv4 socket. %1%") % NetUtils::getLastErrorStr());

            int myReuseAddr = 1;
            if (::setsockopt(listenIpv4Sock, SOL_SOCKET, SO_REUSEADDR, (char*)&myReuseAddr, sizeof(myReuseAddr)) == SOCKET_ERROR)
                TA_THROW_MSG(TcpServerError, boost::format("Failed to setsockopt (SO_REUSEADDR). %1%") % NetUtils::getLastErrorStr());
            int myTcpNoDelay = 1;
            if (::setsockopt(listenIpv4Sock, IPPROTO_TCP, TCP_NODELAY, (char*)&myTcpNoDelay, sizeof(myTcpNoDelay)) == SOCKET_ERROR)
                TA_THROW_MSG(TcpServerError, boost::format("Failed to setsockopt (TCP_NODELAY). %1%") % NetUtils::getLastErrorStr());

            sockaddr_in mySelfIpv4Addr;
            memset(&mySelfIpv4Addr, 0, sizeof(mySelfIpv4Addr));
            mySelfIpv4Addr.sin_family = AF_INET;
            mySelfIpv4Addr.sin_port = htons(aPort);
            mySelfIpv4Addr.sin_addr.s_addr = htonl((anAddrType == AddrAny)?INADDR_ANY:INADDR_LOOPBACK);
            if (::bind(listenIpv4Sock, (sockaddr*)&mySelfIpv4Addr, sizeof(mySelfIpv4Addr)) == SOCKET_ERROR)
                TA_THROW_MSG(TcpServerError, boost::format("bind(2) failed. %1%") % NetUtils::getLastErrorStr());
            if (::listen(listenIpv4Sock, SOMAXCONN) == SOCKET_ERROR)
                TA_THROW_MSG(TcpServerError, boost::format("listen(2) failed. %1%") % NetUtils::getLastErrorStr());
        }

        unsigned int listenIpv4(const string& anIp)
        {
            listenIpv4Sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (listenIpv4Sock == INVALID_SOCKET)
                TA_THROW_MSG(TcpServerError, boost::format("Failed to create IPv4 socket. %1%") % NetUtils::getLastErrorStr());

            int myReuseAddr = 1;
            if (::setsockopt(listenIpv4Sock, SOL_SOCKET, SO_REUSEADDR, (char*)&myReuseAddr, sizeof(myReuseAddr)) == SOCKET_ERROR)
                TA_THROW_MSG(TcpServerError, boost::format("Failed to setsockopt (SO_REUSEADDR). %1%") % NetUtils::getLastErrorStr());
            int myTcpNoDelay = 1;
            if (::setsockopt(listenIpv4Sock, IPPROTO_TCP, TCP_NODELAY, (char*)&myTcpNoDelay, sizeof(myTcpNoDelay)) == SOCKET_ERROR)
                TA_THROW_MSG(TcpServerError, boost::format("Failed to setsockopt (TCP_NODELAY). %1%") % NetUtils::getLastErrorStr());

            sockaddr_in mySelfAddr;
            memset(&mySelfAddr, 0, sizeof(mySelfAddr));
            mySelfAddr.sin_family = AF_INET;
            mySelfAddr.sin_port = htons(0);
            int myRet = inet_pton(AF_INET, anIp.c_str(), &mySelfAddr.sin_addr);
            if (myRet == 0)
                TA_THROW_MSG(TcpServerError, boost::format("%1% is invalid IPv4 address") % anIp);
            if (myRet < 0)
                TA_THROW_MSG(TcpServerError, boost::format("Failed to initialize address %1%. %2%") % anIp % NetUtils::getLastErrorStr());
            if (::bind(listenIpv4Sock, (sockaddr*)&mySelfAddr, sizeof(mySelfAddr)) == SOCKET_ERROR)
                TA_THROW_MSG(TcpServerError, boost::format("bind(2) failed. %1%") % NetUtils::getLastErrorStr());
            sockaddr_in mySockAddr;
            memset(&mySockAddr, 0, sizeof(mySockAddr));
#ifdef _WIN32
            int mySize = sizeof(mySockAddr);
#else
            socklen_t mySize = sizeof(mySockAddr);
#endif
            if (::getsockname(listenIpv4Sock, (sockaddr*)&mySockAddr, &mySize) == SOCKET_ERROR)
                TA_THROW_MSG(TcpServerError, boost::format("getsockname(2) failed. %1%") % NetUtils::getLastErrorStr());
            if (::listen(listenIpv4Sock, SOMAXCONN) == SOCKET_ERROR)
                TA_THROW_MSG(TcpServerError, boost::format("listen(2) failed. %1%") % NetUtils::getLastErrorStr());

            return ntohs(mySockAddr.sin_port);
        }

        void listenIpv6(AddrType anAddrType, unsigned int aPort)
        {
            listenIpv6Sock = ::socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
            if (listenIpv6Sock == INVALID_SOCKET)
                TA_THROW_MSG(TcpServerError, boost::format("Failed to create IPv6 socket. %1%") % NetUtils::getLastErrorStr());

            int myReuseAddr = 1;
            if (::setsockopt(listenIpv6Sock, SOL_SOCKET, SO_REUSEADDR, (char*)&myReuseAddr, sizeof(myReuseAddr)) == SOCKET_ERROR)
                TA_THROW_MSG(TcpServerError, boost::format("Failed to setsockopt (SO_REUSEADDR). %1%") % NetUtils::getLastErrorStr());
            int myTcpNoDelay = 1;
            if (::setsockopt(listenIpv6Sock, IPPROTO_TCP, TCP_NODELAY, (char*)&myTcpNoDelay, sizeof(myTcpNoDelay)) == SOCKET_ERROR)
                TA_THROW_MSG(TcpServerError, boost::format("Failed to setsockopt (TCP_NODELAY). %1%") % NetUtils::getLastErrorStr());

            sockaddr_in6 mySelfIpv6Addr;
            memset(&mySelfIpv6Addr, 0, sizeof(mySelfIpv6Addr));
            mySelfIpv6Addr.sin6_family = AF_INET6;
            mySelfIpv6Addr.sin6_port = htons(aPort);
            mySelfIpv6Addr.sin6_addr = ((anAddrType == AddrAny)?in6addr_any:in6addr_loopback);
            if (::bind(listenIpv6Sock, (sockaddr*)&mySelfIpv6Addr, sizeof(mySelfIpv6Addr)) == SOCKET_ERROR)
                TA_THROW_MSG(TcpServerError, boost::format("bind(2) failed. %1%") % NetUtils::getLastErrorStr());
            if (::listen(listenIpv6Sock, SOMAXCONN) == SOCKET_ERROR)
                TA_THROW_MSG(TcpServerError, boost::format("listen(2) failed. %1%") % NetUtils::getLastErrorStr());

        }

        unsigned int listenIpv6(const string& anIp)
        {
            listenIpv6Sock = ::socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
            if (listenIpv6Sock == INVALID_SOCKET)
                TA_THROW_MSG(TcpServerError, boost::format("Failed to create IPv6 socket. %1%") % NetUtils::getLastErrorStr());

            int myReuseAddr = 1;
            if (::setsockopt(listenIpv6Sock, SOL_SOCKET, SO_REUSEADDR, (char*)&myReuseAddr, sizeof(myReuseAddr)) == SOCKET_ERROR)
                TA_THROW_MSG(TcpServerError, boost::format("Failed to setsockopt (SO_REUSEADDR). %1%") % NetUtils::getLastErrorStr());
            int myTcpNoDelay = 1;
            if (::setsockopt(listenIpv6Sock, IPPROTO_TCP, TCP_NODELAY, (char*)&myTcpNoDelay, sizeof(myTcpNoDelay)) == SOCKET_ERROR)
                TA_THROW_MSG(TcpServerError, boost::format("Failed to setsockopt (TCP_NODELAY). %1%") % NetUtils::getLastErrorStr());

            sockaddr_in6 mySelfAddr;
            try { NetUtils::getIpv6AddrInfo(anIp, mySelfAddr, true); }
            catch (std::exception& e) { TA_THROW_MSG(TcpServerError, e.what()); }
            mySelfAddr.sin6_port = htons(0);
            if (::bind(listenIpv6Sock, (sockaddr*)&mySelfAddr, sizeof(mySelfAddr)) == SOCKET_ERROR)
                TA_THROW_MSG(TcpServerError, boost::format("Cannot bind IPv6 to address %s. bind(2) failed. %s") % anIp % NetUtils::getLastErrorStr());
            sockaddr_in6 mySockAddr;
            memset(&mySockAddr, 0, sizeof(mySockAddr));
#ifdef _WIN32
            int mySize = sizeof(mySockAddr);
#else
            socklen_t mySize = sizeof(mySockAddr);
#endif
            if (::getsockname(listenIpv6Sock, (sockaddr*)&mySockAddr, &mySize) == SOCKET_ERROR)
                TA_THROW_MSG(TcpServerError, boost::format("getsockname(2) failed. %1%") % NetUtils::getLastErrorStr());

            if (::listen(listenIpv6Sock, SOMAXCONN) == SOCKET_ERROR)
                TA_THROW_MSG(TcpServerError, boost::format("Cannot listen IPv6 on address %s. listen(2) failed. %s") % anIp % NetUtils::getLastErrorStr());

            return ntohs(mySockAddr.sin6_port);
        }

        void listenIpv6(const NetUtils::IP& anIp, unsigned int aPort)
        {
            listenIpv6Sock = ::socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
            if (listenIpv6Sock == INVALID_SOCKET)
                TA_THROW_MSG(TcpServerError, boost::format("Failed to create IPv6 socket. %1%") % NetUtils::getLastErrorStr());

            int myReuseAddr = 1;
            if (::setsockopt(listenIpv6Sock, SOL_SOCKET, SO_REUSEADDR, (char*)&myReuseAddr, sizeof(myReuseAddr)) == SOCKET_ERROR)
                TA_THROW_MSG(TcpServerError, boost::format("Failed to setsockopt (SO_REUSEADDR). %1%") % NetUtils::getLastErrorStr());
            int myTcpNoDelay = 1;
            if (::setsockopt(listenIpv6Sock, IPPROTO_TCP, TCP_NODELAY, (char*)&myTcpNoDelay, sizeof(myTcpNoDelay)) == SOCKET_ERROR)
                TA_THROW_MSG(TcpServerError, boost::format("Failed to setsockopt (TCP_NODELAY). %1%") % NetUtils::getLastErrorStr());

            sockaddr_in6 mySelfAddr;
            memset(&mySelfAddr, 0, sizeof(mySelfAddr));
            try { NetUtils::getIpv6AddrInfo(anIp.ipv6, mySelfAddr, true); }
            catch (std::exception& e) { TA_THROW_MSG(TcpServerError, e.what()); }
            mySelfAddr.sin6_port = htons(aPort);
            if (::bind(listenIpv6Sock, (sockaddr*)&mySelfAddr, sizeof(mySelfAddr)) == SOCKET_ERROR)
                TA_THROW_MSG(TcpServerError, boost::format("Cannot bind IPv6 to address %s and port %d. bind(2) failed. %s") % anIp.ipv6 % aPort % NetUtils::getLastErrorStr());

            if (::listen(listenIpv6Sock, SOMAXCONN) == SOCKET_ERROR)
                TA_THROW_MSG(TcpServerError, boost::format("Cannot listen IPv6 on address %s and port %d. listen(2) failed. %s") % anIp.ipv6 % aPort % NetUtils::getLastErrorStr());
        }

        SOCKET listenIpv4Sock;
        SOCKET listenIpv6Sock;
    };// TcpServerImpl

    TcpServer::TcpServer()
        : theImplPtr(new TcpServerImpl)
    {
        TcpSocketUtils::init();
    }

    TcpServer::~TcpServer()
    {
        close();
        delete theImplPtr;
        TcpSocketUtils::uninit();
    }

    void TcpServer::listen(const NetUtils::IP& anIp, unsigned int aPort)
    {
        assert(theImplPtr);

        if (!NetUtils::isValidPort(Strings::toString(aPort)))
            TA_THROW_MSG(TcpServerError, boost::format("The provided port %u is not a valid TCP port") % aPort);
        bool myListenIpv4 = false, myListenIpv6 = false;
        try
        {
            myListenIpv4 = NetUtils::isValidIpv4(anIp.ipv4);
            myListenIpv6 = NetUtils::isValidIpv6(anIp.ipv6);
        }
        catch (std::exception& e)
        {
            TA_THROW_MSG(TcpServerError, e.what());
        }
        if (!myListenIpv4 && !myListenIpv6)
            TA_THROW_MSG(TcpServerError, boost::format("The provided listening IP address is invalid. ipv4: '%1%', ipv6: '%2%'") % anIp.ipv4 % anIp.ipv6);

        close();

        if (myListenIpv4)
        {
            theImplPtr->listenIpv4(anIp, aPort);
        }
        if (myListenIpv6)
        {
            theImplPtr->listenIpv6(anIp, aPort);
        }
    }

    void TcpServer::listen(AddrType anAddrType, unsigned int aPort)
    {
        assert(theImplPtr);

        if (anAddrType != AddrAny && anAddrType != AddrLoopback)
            TA_THROW_MSG(TcpServerError, boost::format("Unsupported address type %1%") % anAddrType);
        if (!NetUtils::isValidPort(Strings::toString(aPort)))
            TA_THROW_MSG(TcpServerError, boost::format("The provided port %u is not a valid TCP port") % aPort);

        close();

        theImplPtr->listenIpv4(anAddrType, aPort);
        theImplPtr->listenIpv6(anAddrType, aPort);
    }

    unsigned int TcpServer::listen(const string& anIp)
    {
        assert(theImplPtr);

        close();

        try
        {
            if (NetUtils::isValidIpv4(anIp))
            {
                return theImplPtr->listenIpv4(anIp);
            }
            else if (NetUtils::isValidIpv6(anIp))
            {
                return theImplPtr->listenIpv6(anIp);
            }
        }
        catch (TcpServerError&)
        {
            throw;
        }
        catch (std::exception& e)
        {
            TA_THROW_MSG(TcpServerError, e.what());
        }

        TA_THROW_MSG(TcpServerError, boost::format("Invalid IP '%1%'") % anIp);
    }


    std::auto_ptr<TcpClient> TcpServer::accept(const int aTimeoutMsec)
    {
        assert(theImplPtr);
        list<SOCKET> myListenSockets;
        if (theImplPtr->listenIpv4Sock != INVALID_SOCKET)
            myListenSockets.push_back(theImplPtr->listenIpv4Sock);
        if (theImplPtr->listenIpv6Sock != INVALID_SOCKET)
            myListenSockets.push_back(theImplPtr->listenIpv6Sock);
        if (myListenSockets.empty())
            TA_THROW_MSG(TcpServerError, "The server is not listening, please call listen() before accept()");
        waitForConnection(myListenSockets, aTimeoutMsec);
        if (myListenSockets.empty())
        {
            if (aTimeoutMsec != Infinity)
                TA_THROW_MSG(TcpServerConnectionTimedOut, boost::format("Timeout of %ld msec reached while waiting for connection") % aTimeoutMsec);
            assert(!"Infinite timeout reached while waiting for connection?!");
        }

        SOCKET myConnectionSocket = INVALID_SOCKET;
        if (ta::isElemExist(theImplPtr->listenIpv6Sock, myListenSockets))
            myConnectionSocket = ::accept(theImplPtr->listenIpv6Sock, NULL, NULL);
        else
            myConnectionSocket = ::accept(theImplPtr->listenIpv4Sock, NULL, NULL);
        if (myConnectionSocket == INVALID_SOCKET)
            TA_THROW_MSG(TcpServerError, boost::format("accept(2) failed. %1%") % NetUtils::getLastErrorStr());
        return std::auto_ptr<TcpClient>(new TcpClient(myConnectionSocket));
    }

    void TcpServer::close(TcpSocketUtils::Shutdown aShutDown)
    {
        assert(theImplPtr);
        TcpSocketUtils::close(theImplPtr->listenIpv4Sock, aShutDown);
        theImplPtr->listenIpv4Sock = INVALID_SOCKET;
        TcpSocketUtils::close(theImplPtr->listenIpv6Sock, aShutDown);
        theImplPtr->listenIpv6Sock = INVALID_SOCKET;
    }

}// ta
