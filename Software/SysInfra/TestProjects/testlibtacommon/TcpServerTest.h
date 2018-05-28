#pragma once

#include "ta/tcpserver.h"
#include "ta/tcpclient.h"
#include "ta/netutils.h"
#include "ta/timeutils.h"
#include "ta/netutils.h"
#include "cxxtest/TestSuite.h"
#include <string>
#include <vector>
#include <algorithm>

using std::string;
using std::vector;
using namespace ta;

class TcpServerTest : public CxxTest::TestSuite
{

/**
    * TCP Server listens on the given IPv4 and IPv6 addresses and port
    * Expects two connections, sends back the inverted string
    */
    class TcpFixedIpPortServerEngine
    {
    public:
        TcpFixedIpPortServerEngine(TcpServer& aTcpServer, const string& anIpv4, const string& anIpv6, unsigned int aPort, const string& anExpectedRxStrTempl)
            : theTcpServer(aTcpServer)
            , theIpv4(anIpv4)
            , theIpv6(anIpv6)
            , thePort(aPort)
            , theExpectedRxStrTempl(anExpectedRxStrTempl)
        {}
        void operator()()
        {
            TS_TRACE(str(boost::format("Start listening on IPv4 %s, IPv6 %s, port %d") % theIpv4 % theIpv6 % thePort).c_str());
            theTcpServer.listen(NetUtils::IP(theIpv4, theIpv6), thePort);
            for (unsigned int iConn=1; iConn <= 2; ++iConn)
            {
                std::auto_ptr<TcpClient> myClient(theTcpServer.accept());
                const string myExpectedStr = (boost::format(theExpectedRxStrTempl) % iConn).str();
                vector<char> myRxData =  myClient->receiveAll(myExpectedStr.size());
                string myRxStr(ta::getSafeBuf(myRxData), myRxData.size());
                TS_ASSERT_EQUALS(myRxStr, myExpectedStr);
                std::reverse(myRxStr.begin(), myRxStr.end());
                myClient->sendAll(vector<char>(myRxStr.c_str(), myRxStr.c_str() + myRxStr.size()));
            }

        }
    private:
        TcpServer& theTcpServer;
        const string theIpv4;
        const string theIpv6;
        unsigned int thePort;
        string theExpectedRxStrTempl;
    };


    /**
    * TCP Server listens on the loopback interface (IPv4 and IPv6) and the provided port
    * Expects two connections, sends back the inverted string
    */
    class TcpLoopbackFixedPortServerEngine
    {
    public:
        /*
        * @param anListenOnIP whether listen on explicitly indicated IP(s) or on the address type (loopack, all)
        */
        TcpLoopbackFixedPortServerEngine(TcpServer& aTcpServer, unsigned int aPort, const string& anExpectedRxStrTempl, bool anListenOnIP)
            : theTcpServer(aTcpServer)
            , thePort(aPort)
            , theExpectedRxStrTempl(anExpectedRxStrTempl)
            , theListenOnIP(anListenOnIP)
        {}
        void operator()()
        {
            if (theListenOnIP)
            {
                TS_TRACE(str(boost::format("Start listening on IPv4 127.0.0.1, IPv6 ::1, port %d") % thePort).c_str());
                theTcpServer.listen(NetUtils::IP("127.0.0.1", "::1"), thePort);
            }
            else
            {
                TS_TRACE(str(boost::format("Start listening on loopback interface, port %d") % thePort).c_str());
                theTcpServer.listen(TcpServer::AddrLoopback, thePort);
            }
            for (unsigned int iConn=1; iConn <= 2; ++iConn)
            {
                std::auto_ptr<TcpClient> myClient(theTcpServer.accept());
                const string myExpectedStr = (boost::format(theExpectedRxStrTempl) % iConn).str();
                vector<char> myRxData =  myClient->receiveAll(myExpectedStr.size());
                string myRxStr(ta::getSafeBuf(myRxData), myRxData.size());
                TS_ASSERT_EQUALS(myRxStr, myExpectedStr);
                std::reverse(myRxStr.begin(), myRxStr.end());
                myClient->sendAll(vector<char>(myRxStr.c_str(), myRxStr.c_str() + myRxStr.size()));
            }
        }
    private:
        TcpServer& theTcpServer;
        unsigned int thePort;
        string theExpectedRxStrTempl;
        bool theListenOnIP;
    };

    /**
    * TCP Server listens on the loopback interface (IPv4 or IPv6), return the port it bounds to
    * Expects one connection, sends back the inverted string
    */
    class TcpLoopbackAdHocPortServerEngine
    {
    public:
        TcpLoopbackAdHocPortServerEngine(TcpServer& aTcpServer, unsigned int& aPort, const string& anExpectedRxStrTempl, bool aListenIpv6)
            : theTcpServer(aTcpServer)
            , thePort(aPort)
            , theExpectedRxStrTempl(anExpectedRxStrTempl)
            , theListenIpv6(aListenIpv6)
        {}
        void operator()()
        {
            if (theListenIpv6)
            {
                TS_TRACE("Start listening ::1");
                thePort = theTcpServer.listen("::1");
            }
            else
            {
                TS_TRACE("Start listening 127.0.0.1");
                thePort = theTcpServer.listen("127.0.0.1");
            }
            std::auto_ptr<TcpClient> myClient(theTcpServer.accept());
            const string myExpectedStr = (boost::format(theExpectedRxStrTempl) % 1).str();
            vector<char> myRxData =  myClient->receiveAll(myExpectedStr.size());
            string myRxStr(ta::getSafeBuf(myRxData), myRxData.size());
            TS_ASSERT_EQUALS(myRxStr, myExpectedStr);
            std::reverse(myRxStr.begin(), myRxStr.end());
            myClient->sendAll(vector<char>(myRxStr.c_str(), myRxStr.c_str() + myRxStr.size()));
        }
    private:
        TcpServer& theTcpServer;
        unsigned int& thePort;
        string theExpectedRxStrTempl;
        bool theListenIpv6;
    };

public:


    //
    // Test cases
    //

    void testServerConnectionTimedOut()
    {
        TcpServer mySvr;

        mySvr.listen(NetUtils::IP("127.0.0.1", ""), 9870);
        TS_ASSERT_THROWS(mySvr.accept(100), TcpServerConnectionTimedOut);

        mySvr.listen(NetUtils::IP("", "::1"), 9871);
        TS_ASSERT_THROWS(mySvr.accept(100), TcpServerConnectionTimedOut);

        mySvr.listen(NetUtils::IP("127.0.0.1", "::1"), 9872);
        TS_ASSERT_THROWS(mySvr.accept(100), TcpServerConnectionTimedOut);

        mySvr.listen(TcpServer::AddrLoopback, 9873);
        TS_ASSERT_THROWS(mySvr.accept(100), TcpServerConnectionTimedOut);

        TS_ASSERT(mySvr.listen("127.0.0.1") > 0);
        TS_ASSERT_THROWS(mySvr.accept(100), TcpServerConnectionTimedOut);

        TS_ASSERT_THROWS(mySvr.listen(NetUtils::IP("", ""), 9874), TcpServerError);
        TS_ASSERT_THROWS(mySvr.listen(""), TcpServerError);

    }

    void testServerListenOnFixedIpPort()
    {
#ifdef _WIN32
        TS_SKIP("Skip TCP server test since no IPv6 can be retrieved on this platform");
#else
        const unsigned int myPort = 19344;
        const string mySentStrTempl("This is #%1% chunk of data sent to the fixed port server listening on IP");

        // Run through all interfaces, this gives as a chance to test against different IP types
        foreach (const ta::NetUtils::Iface& iface, ta::NetUtils::getMyIfaces())
        {
            // Start TCP server
            TcpServer myTcpServer;
            TS_TRACE(("Start listening on interface " + iface.first).c_str());
            const ta::NetUtils::IPv4 ipv4(iface.second.ipv4);
            const ta::NetUtils::IPv6 ipv6(iface.second.ipsv6.at(0));

            TcpFixedIpPortServerEngine myEngine(myTcpServer, ipv4.addr, ipv6.addr, myPort, mySentStrTempl);
            Thread myTcpServerThread(myEngine);
            TimeUtils::sleep(100);

            TcpClient myTcpClient;

            // Connect to IPv4
            string myStr2Send = str(boost::format(mySentStrTempl) % 1);
            string myExpectedRxStr = myStr2Send;
            std::reverse(myExpectedRxStr.begin(), myExpectedRxStr.end());
            myTcpClient.open(ipv4.addr, myPort);
            myTcpClient.sendAll(vector<char>(myStr2Send.c_str(), myStr2Send.c_str() + myStr2Send.size()));
            vector<char> myRxData = myTcpClient.receiveAll(myExpectedRxStr.size());
            TS_ASSERT_EQUALS(string(ta::getSafeBuf(myRxData), myRxData.size()), myExpectedRxStr);

            // Connect to IPv6
            myStr2Send = str(boost::format(mySentStrTempl) % 2);
            myExpectedRxStr = myStr2Send;
            std::reverse(myExpectedRxStr.begin(), myExpectedRxStr.end());
            myTcpClient.open(ipv6.addr, myPort);
            myTcpClient.sendAll(vector<char>(myStr2Send.c_str(), myStr2Send.c_str() + myStr2Send.size()));
            myRxData = myTcpClient.receiveAll(myExpectedRxStr.size());
            TS_ASSERT_EQUALS(string(ta::getSafeBuf(myRxData), myRxData.size()), myExpectedRxStr);

            myTcpServerThread.join();
            myTcpServer.close();

            // The server is stopped, no more connection possible
            TS_ASSERT_THROWS(myTcpClient.open(ipv4.addr, myPort), TcpClientError);
            TS_ASSERT_THROWS(myTcpClient.open(ipv6.addr, myPort), TcpClientError);
        }// foreach
#endif // _WIN32
    }

    void testServerListenOnLoopbackIpFixedPort()
    {
        const unsigned int myPort = 19345;
        const string mySentStrTempl("This is #%1% chunk of data sent to the fixed port server listening on IP");

        // Start the TCP server
        TcpServer myTcpServer;
        TcpLoopbackFixedPortServerEngine myEngine(myTcpServer, myPort, mySentStrTempl, true);
        Thread myTcpServerThread(myEngine);
        TimeUtils::sleep(100);

        TcpClient myTcpClient;

        // Connect to IPv4
        string myStr2Send = str(boost::format(mySentStrTempl) % 1);
        string myExpectedRxStr = myStr2Send;
        std::reverse(myExpectedRxStr.begin(), myExpectedRxStr.end());
        myTcpClient.open("127.0.0.1", myPort);
        myTcpClient.sendAll(vector<char>(myStr2Send.c_str(), myStr2Send.c_str() + myStr2Send.size()));
        vector<char> myRxData = myTcpClient.receiveAll(myExpectedRxStr.size());
        TS_ASSERT_EQUALS(string(ta::getSafeBuf(myRxData), myRxData.size()), myExpectedRxStr);

        // Connect to IPv6
        myStr2Send = str(boost::format(mySentStrTempl) % 2);
        myExpectedRxStr = myStr2Send;
        std::reverse(myExpectedRxStr.begin(), myExpectedRxStr.end());
        myTcpClient.open("::1", myPort);
        myTcpClient.sendAll(vector<char>(myStr2Send.c_str(), myStr2Send.c_str() + myStr2Send.size()));
        myRxData = myTcpClient.receiveAll(myExpectedRxStr.size());
        TS_ASSERT_EQUALS(string(ta::getSafeBuf(myRxData), myRxData.size()), myExpectedRxStr);

        myTcpServerThread.join();
        myTcpServer.close();

        // The server is stopped, no more connection possible
        TS_ASSERT_THROWS(myTcpClient.open("127.0.0.1", myPort), TcpClientError);
        TS_ASSERT_THROWS(myTcpClient.open("::1", myPort), TcpClientError);
    }

    void testServerListenOnLoopbackAddressFixedPort()
    {
        const unsigned int myPort = 19346;
        const string mySentStrTempl("This is #%1% chunk of data sent to the fixed port server listening on address");

        // Start the TCP server
        TcpServer myTcpServer;
        TcpLoopbackFixedPortServerEngine myEngine(myTcpServer, myPort, mySentStrTempl, false);
        Thread myTcpServerThread(myEngine);
        TimeUtils::sleep(100);

        TcpClient myTcpClient;

        // Connect to IPv4
        string myStr2Send = str(boost::format(mySentStrTempl) % 1);
        string myExpectedRxStr = myStr2Send;
        std::reverse(myExpectedRxStr.begin(), myExpectedRxStr.end());
        myTcpClient.open("127.0.0.1", myPort);
        myTcpClient.sendAll(vector<char>(myStr2Send.c_str(), myStr2Send.c_str() + myStr2Send.size()));
        vector<char> myRxData = myTcpClient.receiveAll(myExpectedRxStr.size());
        TS_ASSERT_EQUALS(string(ta::getSafeBuf(myRxData), myRxData.size()), myExpectedRxStr);

        // Connect to IPv6
        myStr2Send = str(boost::format(mySentStrTempl) % 2);
        myExpectedRxStr = myStr2Send;
        std::reverse(myExpectedRxStr.begin(), myExpectedRxStr.end());
        myTcpClient.open("::1", myPort);
        myTcpClient.sendAll(vector<char>(myStr2Send.c_str(), myStr2Send.c_str() + myStr2Send.size()));
        myRxData = myTcpClient.receiveAll(myExpectedRxStr.size());
        TS_ASSERT_EQUALS(string(ta::getSafeBuf(myRxData), myRxData.size()), myExpectedRxStr);

        myTcpServerThread.join();
        myTcpServer.close();

        // The server is stopped, no more connection possible
        TS_ASSERT_THROWS(myTcpClient.open("127.0.0.1", myPort), TcpClientError);
        TS_ASSERT_THROWS(myTcpClient.open("::1", myPort), TcpClientError);
    }

    void testIPv6LoopbackServerAdHocPort()
    {
        const string mySentStrTempl("This is #%1% chunk of data sent to the IPv6 add-hoc port server");

        // Start the TCP server, get the port it listens on
        TcpServer myTcpServer;
        unsigned int myPort = 0;
        TcpLoopbackAdHocPortServerEngine myEngine(myTcpServer, myPort, mySentStrTempl, true);
        Thread myTcpServerThread(myEngine);
        TimeUtils::sleep(100);
        TS_ASSERT(myPort > 0);

        TcpClient myTcpClient;

        // Connect to IPv6
        string myStr2Send = str(boost::format(mySentStrTempl) % 1);
        string myExpectedRxStr = myStr2Send;
        std::reverse(myExpectedRxStr.begin(), myExpectedRxStr.end());
        myTcpClient.open("::1", myPort);
        myTcpClient.sendAll(vector<char>(myStr2Send.c_str(), myStr2Send.c_str() + myStr2Send.size()));
        vector<char> myRxData = myTcpClient.receiveAll(myExpectedRxStr.size());
        TS_ASSERT_EQUALS(string(ta::getSafeBuf(myRxData), myRxData.size()), myExpectedRxStr);

        // The server is not listening on IPv4, connection to IPv4 should fail
        TS_ASSERT_THROWS(myTcpClient.open("127.0.0.1", myPort), TcpClientError);

        myTcpServerThread.join();
        myTcpServer.close();

        // The server is stopped, no more connection possible
        TS_ASSERT_THROWS(myTcpClient.open("127.0.0.1", myPort), TcpClientError);
        TS_ASSERT_THROWS(myTcpClient.open("::1", myPort), TcpClientError);

    }

    void testIPv4LoopbackServerAdHocPort()
    {
        const string mySentStrTempl("This is #%1% chunk of data sent to the IPv4 add-hoc port server");

        // Start the TCP server, get the port it listens on
        TcpServer myTcpServer;
        unsigned int myPort = 0;
        TcpLoopbackAdHocPortServerEngine myEngine(myTcpServer, myPort, mySentStrTempl, false);
        Thread myTcpServerThread(myEngine);
        TimeUtils::sleep(100);
        TS_ASSERT(myPort > 0);

        TcpClient myTcpClient;

        // Connect to IPv4
        string myStr2Send = str(boost::format(mySentStrTempl) % 1);
        string myExpectedRxStr = myStr2Send;
        std::reverse(myExpectedRxStr.begin(), myExpectedRxStr.end());
        myTcpClient.open("127.0.0.1", myPort);
        myTcpClient.sendAll(vector<char>(myStr2Send.c_str(), myStr2Send.c_str() + myStr2Send.size()));
        vector<char> myRxData = myTcpClient.receiveAll(myExpectedRxStr.size());
        TS_ASSERT_EQUALS(string(ta::getSafeBuf(myRxData), myRxData.size()), myExpectedRxStr);

        // The server is not listening on IPv6, connection to IPv6 should fail
        TS_ASSERT_THROWS(myTcpClient.open("::1", myPort), TcpClientError);

        myTcpServerThread.join();
        myTcpServer.close();

        // The server is stopped, no more connection possible
        TS_ASSERT_THROWS(myTcpClient.open("127.0.0.1", myPort), TcpClientError);
        TS_ASSERT_THROWS(myTcpClient.open("::1", myPort), TcpClientError);

    }

};
