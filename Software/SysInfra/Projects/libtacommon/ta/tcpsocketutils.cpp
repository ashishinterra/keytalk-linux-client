#include "tcpsocketutils.h"
#include "netutils.h"
#include "common.h"
#include "boost/numeric/conversion/cast.hpp"
#ifdef _WIN32
# include <Mstcpip.h>
#else
# include <arpa/inet.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <unistd.h>
# include <netdb.h>
# include <errno.h>
#endif
#include <cassert>

namespace ta
{
    namespace TcpSocketUtils
    {
        using std::vector;

        void init()
        {
#ifdef _WIN32
            WSADATA myWd;
            WORD myVersionRequested = MAKEWORD(2, 2);
            if (WSAStartup(myVersionRequested, &myWd) != 0)
                TA_THROW_MSG(TcpSocketError, boost::format("Failed to initialize sockets. %1%") % NetUtils::getLastErrorStr());
#endif
        }

        void uninit()
        {
#ifdef _WIN32
            ::WSACleanup();
#endif
        }

        size_t send(SOCKET aSocket, const vector<char>& aData)
        {
            if (aSocket == INVALID_SOCKET)
                TA_THROW_MSG(TcpSocketError, "Invalid socket");
            if (aData.empty())
                return 0;
            try
            {
                size_t mtTxBytes = ::send(aSocket, getSafeBuf(aData), boost::numeric_cast<int>(aData.size()), 0);
                if (mtTxBytes == (size_t)SOCKET_ERROR)
                    TA_THROW_MSG(TcpSocketError, boost::format("send(2) failed. %1%") % NetUtils::getLastErrorStr());
                assert(mtTxBytes <= aData.size());
                return mtTxBytes;
            }
            catch (boost::bad_numeric_cast& e)
            {
                TA_THROW_MSG(TcpSocketError, e.what());
            }
        }

        void sendAll(SOCKET aSocket, const vector<char>& aData)
        {
            if (aSocket == INVALID_SOCKET)
                TA_THROW_MSG(TcpSocketError, "Invalid socket");
            if (aData.empty())
                return;
            try
            {
                size_t myBytesRemain = aData.size();
                const char* myDataPtr = getSafeBuf(aData);
                while (myBytesRemain > 0)
                {
                    size_t mtTxBytes = ::send(aSocket, myDataPtr, boost::numeric_cast<int>(myBytesRemain), 0);
                    if (mtTxBytes == (size_t)SOCKET_ERROR)
                        TA_THROW_MSG(TcpSocketError, boost::format("send(2) failed. %1%") % NetUtils::getLastErrorStr());
                    assert(mtTxBytes <= myBytesRemain);
                    myDataPtr += mtTxBytes;
                    myBytesRemain -= mtTxBytes;
                }
            }
            catch (boost::bad_numeric_cast& e)
            {
                TA_THROW_MSG(TcpSocketError, e.what());
            }
        }

        vector<char> receive(SOCKET aSocket, size_t aMaxSize)
        {
            if (aSocket == INVALID_SOCKET)
                TA_THROW_MSG(TcpSocketError, "Invalid socket");
            if (!aMaxSize)
                return vector<char>();
            try
            {
                std::vector<char> myData(aMaxSize);
                size_t myRxBytes = ::recv(aSocket, getSafeBuf(myData), boost::numeric_cast<int>(aMaxSize), 0);
                if (myRxBytes == 0)
                    TA_THROW_MSG(TcpSocketError, "The connection has been gracefully closed");
                if (myRxBytes == (size_t)-1)
                    TA_THROW_MSG(TcpSocketError, boost::format("recv(2) failed. %1%") % NetUtils::getLastErrorStr());
                assert(myRxBytes <= aMaxSize);
                myData.resize(myRxBytes);
                return myData;
            }
            catch (boost::bad_numeric_cast& e)
            {
                TA_THROW_MSG(TcpSocketError, e.what());
            }
        }

        vector<char> receiveAll(SOCKET aSocket, size_t aSize)
        {
            if (aSocket == INVALID_SOCKET)
                TA_THROW_MSG(TcpSocketError, "Invalid socket");
            if (!aSize)
                return vector<char>();
            try
            {
                std::vector<char> myData(aSize);
                size_t myBytesRemain = aSize;
                char* myDataPtr = getSafeBuf(myData);
                while (myBytesRemain > 0)
                {
                    size_t myRxBytes = ::recv(aSocket, myDataPtr, boost::numeric_cast<int>(myBytesRemain), 0);
                    if (myRxBytes == 0)
                        TA_THROW_MSG(TcpSocketError, "The connection has been gracefully closed");
                    if (myRxBytes == (size_t)-1)
                        TA_THROW_MSG(TcpSocketError, boost::format("recv(2) failed. %1%") % NetUtils::getLastErrorStr());
                    assert(myRxBytes <= myBytesRemain);
                    myDataPtr += myRxBytes;
                    myBytesRemain -= myRxBytes;
                }
                return myData;
            }
            catch (boost::bad_numeric_cast& e)
            {
                TA_THROW_MSG(TcpSocketError, e.what());
            }
        }

        void close(SOCKET aSocket, Shutdown aShutDown)
        {
            if (aSocket == INVALID_SOCKET)
                return;
#ifdef _WIN32
            if (aShutDown == shutdownYes)
                ::shutdown(aSocket, SD_BOTH);
            ::closesocket(aSocket);
#else
            if (aShutDown == shutdownYes)
                ::shutdown(aSocket, SHUT_RDWR);
            ::close(aSocket);
#endif
        }

#if defined(_WIN32) || defined(__linux__)
        void enableKeepAlive(SOCKET aSocket, unsigned int anIdleTime, unsigned int anInterval)
        {
            if (aSocket == INVALID_SOCKET)
                TA_THROW_MSG(TcpSocketError, "Invalid socket");

#ifdef _WIN32
            DWORD nSize;
            struct tcp_keepalive alive;
            alive.onoff = 1;
            alive.keepalivetime = anIdleTime * 1000;
            alive.keepaliveinterval = anInterval * 1000;

            if (WSAIoctl(aSocket, SIO_KEEPALIVE_VALS, &alive,sizeof(alive), NULL,0,reinterpret_cast<DWORD*>(&nSize),NULL,NULL) == SOCKET_ERROR)
                TA_THROW_MSG(TcpSocketError, boost::format("Cannot enable TCP keepalives. WSAIoctl() failed. %1%") % NetUtils::getLastErrorStr());
            //@note we do not set keep-alive probes because on Windows this can only be done on the system scope, not per socket
#else
            // Enable keepalive on the socket
            int optval = 1;
            if (setsockopt(aSocket, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0)
                TA_THROW_MSG(TcpSocketError, boost::format("Cannot enable TCP keepalives. setsockopt() failed. %1%") % NetUtils::getLastErrorStr());

            // Configure keepalives
            optval = anIdleTime;
            if (setsockopt(aSocket, SOL_TCP, TCP_KEEPIDLE, &optval, sizeof(optval)) < 0)
                TA_THROW_MSG(TcpSocketError, boost::format("Cannot set TCP keepalive idle time to %1%. setsockopt() failed. %2%") % optval % NetUtils::getLastErrorStr());

            optval = anInterval;
            if (setsockopt(aSocket, SOL_TCP, TCP_KEEPINTVL, &optval, sizeof(optval)) < 0)
                TA_THROW_MSG(TcpSocketError, boost::format("Cannot set TCP keepalive interval to %1%. setsockopt() failed. %2%") % optval % NetUtils::getLastErrorStr());

#endif
        }
#endif

    }
}
