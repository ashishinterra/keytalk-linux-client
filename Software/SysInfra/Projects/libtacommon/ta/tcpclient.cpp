#include "tcpclient.h"
#include "dnsutils.h"
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
#include <cassert>

namespace ta
{
    using std::string;
    using std::vector;

    TcpClient::TcpClient()
        : theConnectionSocket(INVALID_SOCKET)
    {
        TcpSocketUtils::init();
    }
    TcpClient::TcpClient(SOCKET aConnectionSocket)
        : theConnectionSocket(aConnectionSocket)
    {
        TcpSocketUtils::init();
    }
    TcpClient::~TcpClient()
    {
        close();
        TcpSocketUtils::uninit();
    }

    void TcpClient::open(const string& aHost, unsigned int aPort)
    {
        close();
        NetUtils::IP myHostIp;
        bool myIsIpv6 = false;
        bool myIsIpv4 = false;

        try
        {
            myHostIp = DnsUtils::resolveIpByName(aHost);
            myIsIpv6 = NetUtils::isValidIpv6(myHostIp.ipv6);
            myIsIpv4 = NetUtils::isValidIpv4(myHostIp.ipv4);
        }
        catch (std::exception& e)
        {
            TA_THROW_MSG(TcpClientError, e.what());
        }
        assert(myIsIpv6 || myIsIpv4);

        // Try IPv6
        if (myIsIpv6)
        {
            try
            {
                theConnectionSocket = ::socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
                if (theConnectionSocket == INVALID_SOCKET)
                    TA_THROW_MSG(TcpClientError, boost::format("Failed to create IPv6 socket. %1%") % NetUtils::getLastErrorStr());
                int myTcpNoDelay = 1;
                if (::setsockopt(theConnectionSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&myTcpNoDelay, sizeof(myTcpNoDelay)) == SOCKET_ERROR)
                    TA_THROW_MSG(TcpClientError, boost::format("Failed to setsockopt (TCP_NODELAY). %1%") % NetUtils::getLastErrorStr());
                sockaddr_in6 mySelfAddr;
                NetUtils::getIpv6AddrInfo(myHostIp.ipv6, mySelfAddr);
                mySelfAddr.sin6_port = htons(aPort);
                if (::connect(theConnectionSocket, (struct sockaddr*)&mySelfAddr, sizeof(mySelfAddr)) == SOCKET_ERROR)
                    TA_THROW_MSG(TcpClientError, boost::format("Connect to IPv6 '%1%:%2%' failed. %3%") % myHostIp.ipv6 % aPort % NetUtils::getLastErrorStr());
                return;
            }
            catch (std::exception& e)
            {
                close();
                if (!myIsIpv4)
                    TA_THROW_MSG(TcpClientError, e.what());
                // Can't connect to IPv6 (e.g. non-IPv6-enabled router), fallback to IPv4
            }
        }

        // Try IPv4
        theConnectionSocket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (theConnectionSocket == INVALID_SOCKET)
            TA_THROW_MSG(TcpClientError, boost::format("Failed to create IPv4 socket. %1%") % NetUtils::getLastErrorStr());
        int myTcpNoDelay = 1;
        if (::setsockopt(theConnectionSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&myTcpNoDelay, sizeof(myTcpNoDelay)) == SOCKET_ERROR)
            TA_THROW_MSG(TcpClientError, boost::format("Failed to setsockopt (TCP_NODELAY). %1%") % NetUtils::getLastErrorStr());
        sockaddr_in mySelfAddr = {0};
        mySelfAddr.sin_family = AF_INET;
        int myRet = inet_pton(AF_INET, myHostIp.ipv4.c_str(), &mySelfAddr.sin_addr);
        if (myRet == 0)
            TA_THROW_MSG(TcpClientError, boost::format("'%1%' is invalid IPv4 address") % myHostIp.ipv4);
        if (myRet < 0)
            TA_THROW_MSG(TcpClientError, boost::format("Failed to initialize IPv4 address '%1%'. %2%") % myHostIp.ipv4 % NetUtils::getLastErrorStr());
        mySelfAddr.sin_port = htons(aPort);
        if (::connect(theConnectionSocket, (struct sockaddr*)&mySelfAddr, sizeof(mySelfAddr)) == SOCKET_ERROR)
            TA_THROW_MSG(TcpClientError, boost::format("Connect to IPv4 '%1%:%2%' failed. %3%") % myHostIp.ipv4 % aPort % NetUtils::getLastErrorStr());

    }

    size_t TcpClient::send(const vector<char>& aData)
    {
        try {
            return TcpSocketUtils::send(theConnectionSocket, aData);
        }   catch (TcpSocketError& e)  {
            TA_THROW_MSG(TcpClientError, e.what());
        }
    }

    void TcpClient::sendAll(const vector<char>& aData)
    {
        try {
            TcpSocketUtils::sendAll(theConnectionSocket, aData);
        }   catch (TcpSocketError& e)  {
            TA_THROW_MSG(TcpClientError, e.what());
        }
    }

    vector<char> TcpClient::receive(size_t aMaxSize)
    {
        try {
            return TcpSocketUtils::receive(theConnectionSocket, aMaxSize);
        }   catch (TcpSocketError& e)  {
            TA_THROW_MSG(TcpClientError, e.what());
        }
    }

    vector<char> TcpClient::receiveAll(size_t aSize)
    {
        try {
            return TcpSocketUtils::receiveAll(theConnectionSocket, aSize);
        }   catch (TcpSocketError& e)  {
            TA_THROW_MSG(TcpClientError, e.what());
        }
    }

    void TcpClient::close()
    {
        TcpSocketUtils::close(theConnectionSocket);
        theConnectionSocket = INVALID_SOCKET;
    }
}
