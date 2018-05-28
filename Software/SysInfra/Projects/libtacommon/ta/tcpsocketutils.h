#pragma once

#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
# define SOCKET int
# define INVALID_SOCKET -1
# define SOCKET_ERROR   -1
#endif

namespace ta
{
    struct TcpSocketError : std::runtime_error
    {
        explicit TcpSocketError(const std::string& aMessage = "") : std::runtime_error(aMessage) {}
    };

    namespace TcpSocketUtils
    {
        /**
          Initializes socket subsystem

          @throw TcpSocketError
         */
        void init();

        /**
          Uninitialize socket subsystem
         */
        void uninit();

        /**
          Send the data to the socket.

          @param[in] aSocket Socket to send data to
          @param[in] aData Data to be send
          @return the number of bytes sent
          @throw TcpSocketError
         */
        size_t send(SOCKET aSocket, const std::vector<char>& aData);

        /**
          Send the data to the socket. Blocks until all the data is sent

          @param[in] aSocket Socket to send data to
          @param[in] aData Data to be send
          @throw TcpSocketError
         */
        void sendAll(SOCKET aSocket, const std::vector<char>& aData);

        /**
          Receive at most aMaxSize bytes from the socket.

          @param[in] aSocket Socket to send data to
          @param[in] aMaxSize Maximum number of bytes to receive
          @return Received data
          @throw TcpSocketError
         */
        std::vector<char> receive(SOCKET aSocket, size_t aMaxSize);

        /**
          Receive the data from the socket. Blocks until aSize bytes is received.

          @param[in] aSocket Socket to send data to
          @param[in] aSize Number of bytes to receive
          @return Received data
          @throw TcpSocketError
         */
        std::vector<char> receiveAll(SOCKET aSocket, size_t aSize);

        /**
          Close socket

          @param[in] aSocket Socket to be closed
         */
        enum Shutdown { shutdownYes, shutdownNo};
        void close(SOCKET aSocket, Shutdown aShutDown = shutdownYes);
#if defined(_WIN32) || defined(__linux__)
        /**
            Enabled TCP keep-alives on the given TCP socket
            @param[in] anIdleTime (seconds) how long the TCP connection sits idle, with no traffic, before TCP sends a keep-alive packet
            @param[in] anInterval (seconds) how long to wait for a response after sending a keep-alive before repeating the keep-alive
        */
        void enableKeepAlive(SOCKET aSocket, unsigned int anIdleTime, unsigned int anInterval);
#endif
    }
}
