#pragma once

#include "ta/tcpclient.h"
#include "ta/netutils.h"
#include "ta/timeutils.h"
#include "ta/process.h"
#include "ta/utils.h"
#include "cxxtest/TestSuite.h"
#include <string>
#include <vector>
#include <cstdio>
#include <sys/types.h>
#ifndef _WIN32
#include <unistd.h>
#include <signal.h>
#endif
#include <errno.h>
#include "boost/lexical_cast.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/assign/list_of.hpp"

using std::string;
using std::vector;

class TcpClientTest : public CxxTest::TestSuite
{
static const string Ipv4WebSvrScriptName;
static const string Ipv4WebSvrScriptPidFile;
static const unsigned int Ipv4WebSvrPort;
static const string Ipv6WebSvrScriptName;
static const string Ipv6WebSvrScriptPidFile;
static const unsigned int Ipv6WebSvrPort;

#define NETUTILSTEST_TRACE
#ifdef _WIN32
//#define WIN32_ENABLE_TCPCLIENT_IPV6_TESTS
#endif

public:
    void stopWebSvrs()
    {
        try
        {
            string myPidStr = ta::readData(Ipv4WebSvrScriptPidFile);
            unsigned int myPid = boost::lexical_cast<unsigned int>(boost::trim_copy(myPidStr).c_str());
            if (myPid > 0)
                ta::Process::kill(myPid);
        }
        catch (...)
        {}
        remove(Ipv4WebSvrScriptPidFile.c_str());
#if !defined(_WIN32) || defined(WIN32_ENABLE_TCPCLIENT_IPV6_TESTS)
        try
        {
            string myPidStr = ta::readData(Ipv6WebSvrScriptPidFile);
            unsigned int myPid = boost::lexical_cast<unsigned int>(boost::trim_copy(myPidStr).c_str());
            if (myPid > 0)
                ta::Process::kill(myPid);
        }
        catch (...)
        {}
        remove(Ipv6WebSvrScriptPidFile.c_str());
#endif
    }

    TcpClientTest()
    {
        // Stop any possible leftovers
        stopWebSvrs();

        //@note TS_XXX macros cannot be used in suite c'tors
        try
        {
#ifdef _WIN32
            string myCommand = (boost::format("..\\..\\..\\Import\\python-2.7\\python %s localhost %d 0") % Ipv4WebSvrScriptName % Ipv4WebSvrPort).str();
            string myStdOut, myStdErr;
            unsigned int myExitCode;
            if (ta::Process::shellExecAsync(myCommand, myStdOut, myStdErr, myExitCode))
                TA_THROW_MSG(std::runtime_error, boost::format("Command %s failed with code %d. stderr: %d, stdout: %s") % myCommand % myExitCode % myStdErr % myStdOut);
#else
            string myCommand = (boost::format("(python %s localhost %d 0) &") % Ipv4WebSvrScriptName % Ipv4WebSvrPort).str();
            if (system(myCommand.c_str()) != 0)
                TA_THROW_MSG(std::runtime_error, boost::format("Command %s failed with errno %d") % myCommand % errno);
#endif
#if !defined(_WIN32) || defined(WIN32_ENABLE_TCPCLIENT_IPV6_TESTS)
            myCommand = (boost::format("(python %s ::1 %d 1) &") % Ipv6WebSvrScriptName % Ipv6WebSvrPort).str();
            if (system(myCommand.c_str()) != 0)
                TA_THROW_MSG(std::runtime_error, boost::format("Command %s failed with errno %d") % myCommand % errno);
#endif

			std::cout << "Wait for the webservers to start...";
            const unsigned int myWebServersStartTimeout = 5000;
            unsigned int myElapsed = 0;
            unsigned int myDelta = 500;
            while (myElapsed < myWebServersStartTimeout)
            {
                ta::TimeUtils::sleep(myDelta);
                if (ta::isFileExist(Ipv4WebSvrScriptPidFile)
#if !defined(_WIN32) || defined(WIN32_ENABLE_TCPCLIENT_IPV6_TESTS)
                    && ta::isFileExist(Ipv6WebSvrScriptPidFile)
#endif
                    )
                    break;
                myElapsed += myDelta;
            }
            if (myElapsed > myWebServersStartTimeout)
                TA_THROW_MSG(std::runtime_error, "Timeout reached waiting for webservers to start");

			std::cout << "started\n";
        }
        catch (const std::exception& e)
        {
            std::cerr << e.what() << "\n";
            stopWebSvrs();
            throw;
        }
    }
    virtual ~TcpClientTest()
    {
        stopWebSvrs();
    }

    static TcpClientTest *createSuite()
    {
        return new TcpClientTest();
    }

    static void destroySuite( TcpClientTest *suite )
    {
        delete suite;
    }

    void testIpv4Ipv6InternalServer()
    {
        using namespace ta;
        TS_TRACE("-- Testing against IPv4/IPv6 internal webservers");

        TcpClient myTcpClient;

        myTcpClient.open("127.0.0.1", Ipv4WebSvrPort);
        myTcpClient.close();

#if !defined(_WIN32) || defined(WIN32_ENABLE_TCPCLIENT_IPV6_TESTS)
        myTcpClient.open("::1", Ipv6WebSvrPort);
        myTcpClient.close();
#endif

        myTcpClient.open("localhost", Ipv4WebSvrPort);
        myTcpClient.close();

        TS_ASSERT_THROWS(myTcpClient.open("127.0.0.1", Ipv6WebSvrPort), TcpClientError);
#if !defined(_WIN32) || defined(WIN32_ENABLE_TCPCLIENT_IPV6_TESTS)
        TS_ASSERT_THROWS(myTcpClient.open("::1", Ipv4WebSvrPort), TcpClientError);
#endif
        TS_ASSERT_THROWS(myTcpClient.open("__i_hope_this_server_does_not_exist__", Ipv4WebSvrPort), TcpClientError);
    }


     void testExternalServer()
    {
        using namespace ta;
        TS_TRACE("-- Testing against external webservers");

		// try several times, fail only if all attempts fail
		const std::vector<string> mySvrs = boost::assign::list_of("www.nu.nl")("www.sioux.eu");
		foreach (string svr, mySvrs)
		{
			try
			{
				TcpClient myTcpClient;
				TS_TRACE(("  -- Connecting to " + svr + "...").c_str());
				myTcpClient.open(svr, 80);
				return;
			}
			catch (TcpClientError& e)
			{
				TS_TRACE(e.what());
				TimeUtils::sleep(500);
			}
		}
		TS_FAIL("Fail to connect to any of webservers, giving up");
    }

    void testSendReceiveInternalServer()
    {
        using namespace ta;
        TS_TRACE("-- Testing send/receive against internal webserver");

        static const string myHttpReq = "GET /test.html HTTP/1.1\r\n\r\n";
        TcpClient myTcpClient;
        myTcpClient.open("localhost", Ipv4WebSvrPort);
        size_t myTotalSent = 0;
        while (myTotalSent < myHttpReq.size())
        {
            myTotalSent += myTcpClient.send(vector<char>(myHttpReq.begin() + myTotalSent, myHttpReq.end()));
        }

        static const size_t myMinRespSize = 256;
        size_t myTotalReceived = 0;
        vector<char> myHttpResp;
        while (myTotalReceived < myMinRespSize)
        {
            vector<char> myIntermediateResp = myTcpClient.receive(myMinRespSize - myTotalReceived);
            myHttpResp += myIntermediateResp;
            myTotalReceived += myIntermediateResp.size();
        }
        string myHttpRespStr(ta::getSafeBuf(myHttpResp), myHttpResp.size());
        TS_ASSERT(myHttpRespStr.find("HTTP/1.") != string::npos);
        TS_ASSERT(myHttpRespStr.find("<title>TA Test page</title>") != string::npos);
    }

    void testSendAllReceiveAllInternalServer()
    {
        using namespace ta;
        TS_TRACE("-- Testing send/receive against internal webserver");

        static const string myHttpReq = "GET /test.html HTTP/1.1\r\n\r\n";
        TcpClient myTcpClient;
        myTcpClient.open("localhost", Ipv4WebSvrPort);
        myTcpClient.sendAll(vector<char>(myHttpReq.begin(), myHttpReq.end()));

        static const size_t myMinRespSize = 256;
        vector<char> myHttpResp = myTcpClient.receiveAll(myMinRespSize);
        string myHttpRespStr(ta::getSafeBuf(myHttpResp), myHttpResp.size());
        TS_ASSERT(myHttpRespStr.find("HTTP/1.") != string::npos);
        TS_ASSERT(myHttpRespStr.find("<title>TA Test page</title>") != string::npos);
    }
};

const string TcpClientTest::Ipv4WebSvrScriptName("naive_http_server.py");
const string TcpClientTest::Ipv4WebSvrScriptPidFile("naive_http_server_ipv4.pid");
const unsigned int TcpClientTest::Ipv4WebSvrPort = 9092;
const string TcpClientTest::Ipv6WebSvrScriptName("naive_http_server.py");
const string TcpClientTest::Ipv6WebSvrScriptPidFile("naive_http_server_ipv6.pid");
const unsigned int TcpClientTest::Ipv6WebSvrPort = 9093;
