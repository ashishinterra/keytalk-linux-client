#pragma once

#include <stdexcept>
#include <string>
#include <vector>
#include "boost/utility.hpp"
#include "tcpsocketutils.h"

namespace ta
{
    struct TcpClientError : std::runtime_error
    {
        explicit TcpClientError(const std::string& aMessage = "") : std::runtime_error(aMessage) {}
    };

    /**
      TCP client class
     */
    class TcpClient: public boost::noncopyable
    {
    public:
        /**
          Constructor: initializes new TCP socket

          @throw TcpClientError on error
         */
        TcpClient();

        /**
          Constructor: initializes existing TCP socket

          @param[in] aConnectionSocket socket to initialize
          @throw TcpClientError on error
         */
        TcpClient(SOCKET aConnectionSocket);
        virtual ~TcpClient();

        /**
          Open TCP connection socket.
          If the symbolic hostname is provided as aHost argument and the hostname resolves
          both to IPv4 and IPv6 the following behavior applies:
           - First an attempt is made to establish a connection to the resolved IPv6 address
           - If for whaterver reason the IPv6 connection cannot be established, the second attempt is made for IPv4 address

          @param[in] aHost hostname/IP (IPv4 or IPv6) or the remote host to connect to
          @param[in] aPort port number used for connection
          @throw TcpClientError
         */
        virtual void open(const std::string& aHost, unsigned int aPort);

        /**
          Send the data to the connected endpoint

          @param[in] aData Data to be send
          @return the number of bytes sent
          @throw TcpClientError
         */
        virtual size_t send(const std::vector<char>& aData);

        /**
          Send the data to the connected endpoint, blocks until all the data is sent

          @param[in] aData Data to be send
          @throw TcpClientError
         */
        virtual void sendAll(const std::vector<char>& aData);

        /**
          Receives at most aMaxSize bytes from the connected endpoint

          @param[in] aMaxSize Maximum number of bytes to receive
          @return Received data
          @throw TcpClientError
         */
        virtual std::vector<char> receive(size_t aMaxSize);

        /**
          Receives the data from the connected endpoint, blocks until aSize bytes is received

          @param[in] aSize Number of bytes to receive
          @return Received data
          @throw TcpClientError
         */
        virtual std::vector<char> receiveAll(size_t aSize);

        /**
          Close TCP connection socket
         */
        virtual void close();

        /**
          Call this on your own risk
        */
        inline virtual SOCKET getSocket() { return theConnectionSocket; } ;
    private:
        SOCKET theConnectionSocket;
    };
}
