#pragma once

#include "ta/tcpclient.h"
#include "ta/tcpsocketutils.h"
#include <stdexcept>
#include <string>
#include <memory>
#include "boost/utility.hpp"

namespace ta
{
    struct TcpServerError : std::runtime_error
    {
        explicit TcpServerError(const std::string& aMessage = "") : std::runtime_error(aMessage) {}
    };

    struct TcpServerConnectionTimedOut : TcpServerError
    {
        explicit TcpServerConnectionTimedOut(const std::string& aMessage = "") : TcpServerError(aMessage) {}
    };

    namespace NetUtils // forward declaration
    {
        struct IP;
    }

    /**
      TCP server class
     */
    class TcpServer: public boost::noncopyable
    {
    public:
        /**
          @throw TcpClientError on error
         */
        TcpServer();
        virtual ~TcpServer();

        enum AddrType { AddrAny, AddrLoopback };
        static const int Infinity = -1;

        /**
          Listen on the specified IP and port.

          The server listens on all non-empty IP addresses of the provided IP structure.

          @param[in] anIp IP to listen to
          @param[in] aPort port number to listen to
          @throw TcpServerError
         */
        virtual void listen(const NetUtils::IP& anIp, unsigned int aPort);

        /**
          Listen on the specified address type and port, both on IPv4 and on IPv6.

          @param[in] anAddrType IP (IPv4 or IPv6) to listen to
          @param[in] aPort port number to listen to
          @throw TcpServerError
         */
        virtual void listen(AddrType anAddrType, unsigned int aPort);

        /**
          Listen on the specified IP.

          IP address family (IPv4 or IPv6) to listen on is detected from the provided IP address.

          @param[in] anIp IP (IPv4 or IPv6) to listen to
          @return Port number of the listening address is bound to
          @throw TcpServerError
         */
        virtual unsigned int listen(const std::string& anIp);

        /**
          Wait for a connection on the listening port and address type.

          Prefer IPv6 to IPv4 if we have both pending connections.

          @param[in] aTimeoutMsec Timeout in milliseconds to wait for incoming connection
          @throw TcpServerError as generic error
          @throw TcpServerConnectionTimedOut if timeout reached while waiting for connection
          @return TCP connection socket
         */
        virtual std::auto_ptr<TcpClient> accept(const int aTimeoutMsec = Infinity);

        /**
          Close TCP listen sockets
         */
        virtual void close(TcpSocketUtils::Shutdown aShutDown = TcpSocketUtils::shutdownYes);
    private:
        struct TcpServerImpl;
        TcpServerImpl* theImplPtr;
    };
}
