#pragma once

#include "ta/common.h"
#include "ta/netutils.h"
#include "ta/timeutils.h"
#include "cxxtest/TestSuite.h"
#include <string>
#include <vector>
#include <algorithm>
#include "boost/assign/list_of.hpp"
#include "boost/filesystem.hpp"

using std::string;

// Test of NetUtils accessors i.e. functions that only query system network configuration without modifying it
class NetUtilsAccessorsTest : public CxxTest::TestSuite
{
public:
    NetUtilsAccessorsTest()
    {
#ifdef _WIN32
        std::cout << "*** NOTICE. Please make sure at least one site-local (\"fdXX\") IPv6 is setup on Windows using \"ipv6 install\" and \"netsh\" tools.\n";
#endif
    }
    ~NetUtilsAccessorsTest()
    {}
    static NetUtilsAccessorsTest *createSuite()
    {
        return new NetUtilsAccessorsTest();
    }
    static void destroySuite( NetUtilsAccessorsTest *suite )
    {
        delete suite;
    }

    void testValidIpV4()
    {
        using namespace ta::NetUtils;

        TS_ASSERT(isValidIpv4("192.168.0.1"));
        TS_ASSERT(isValidIpv4("0.0.0.0"));

        TS_ASSERT(!isValidIpv4("localhost"));
        TS_ASSERT(!isValidIpv4(""));
        TS_ASSERT(!isValidIpv4("192.168.0.256"));
        TS_ASSERT(!isValidIpv4("0::0"));
        TS_ASSERT(!isValidIpv4("fd7c::192.168.1.1"));
        TS_ASSERT(!isValidIpv4("siouxdemo.trustalert.com"));
    }

    void testIsLoopbackIpv4()
    {
        using namespace ta::NetUtils;

        TS_ASSERT(isLoopbackIpv4("127.0.0.1"));
        TS_ASSERT(isLoopbackIpv4("\t127.0.0.1  "));
        TS_ASSERT(!isLoopbackIpv4("192.168.0.1"));

        TS_ASSERT_THROWS(isLoopbackIpv4("localhost"), std::exception);
        TS_ASSERT_THROWS(isLoopbackIpv4("192.168.0.256"), std::exception);
        TS_ASSERT_THROWS(isLoopbackIpv4("fd7c::192.168.1.1"), std::exception);
    }

    void testConvIpv4CidrNetmaskToDotDecimal()
    {
        using namespace ta::NetUtils;

        TS_ASSERT_EQUALS(convIpv4CidrNetmaskToDotDecimal(0), "0.0.0.0");
        TS_ASSERT_EQUALS(convIpv4CidrNetmaskToDotDecimal(8), "255.0.0.0");
        TS_ASSERT_EQUALS(convIpv4CidrNetmaskToDotDecimal(16), "255.255.0.0");
        TS_ASSERT_EQUALS(convIpv4CidrNetmaskToDotDecimal(24), "255.255.255.0");
        TS_ASSERT_EQUALS(convIpv4CidrNetmaskToDotDecimal(26), "255.255.255.192");
        TS_ASSERT_EQUALS(convIpv4CidrNetmaskToDotDecimal(32), "255.255.255.255");

        TS_ASSERT_THROWS(convIpv4CidrNetmaskToDotDecimal(33), std::exception);
        TS_ASSERT_THROWS(convIpv4CidrNetmaskToDotDecimal(-1), std::exception);
    }

    void testValidIpV6()
    {
        using namespace ta;

        TS_ASSERT(NetUtils::isValidIpv6("0::0"));
        TS_ASSERT(NetUtils::isValidIpv6("::1"));

        // The same link-local IPv6 with scopeid 2 in different notations
        TS_ASSERT(NetUtils::isValidIpv6("fe80:2::20c:29ff:fe6e:c10b"));
        TS_ASSERT(NetUtils::isValidIpv6("fe80::20c:29ff:fe6e:c10b%2"));

#ifndef _WIN32
        const string myNonLoopbackIfName = getFirstNonLoopbackIfName();
        TS_ASSERT(NetUtils::isValidIpv6("fe80::20c:29ff:fe6e:c10b%" + myNonLoopbackIfName));
#endif

        // ipv4 compatible ipv6 address
        TS_ASSERT(NetUtils::isValidIpv6("::192.168.1.1"));
        TS_ASSERT(NetUtils::isValidIpv6("fd7c::192.168.1.1"));

        TS_ASSERT(!NetUtils::isValidIpv6("localhost"));
        TS_ASSERT(!NetUtils::isValidIpv6(""));
        TS_ASSERT(!NetUtils::isValidIpv6("0.0.0.0"));
        TS_ASSERT(!NetUtils::isValidIpv6("192.168.0.1"));
        TS_ASSERT(!NetUtils::isValidIpv6("siouxdemo.trustalert.com"));
    }

    void testIsLoopbackIpv6()
    {
        using namespace ta;

        TS_ASSERT(NetUtils::isLoopbackIpv6("::1"));
        TS_ASSERT(NetUtils::isLoopbackIpv6("0:0:0:0:0:0:0:1"));

        TS_ASSERT(!NetUtils::isLoopbackIpv6("fe80::200:5eff:fe00:104"));
        TS_ASSERT(!NetUtils::isLoopbackIpv6("::1.2.3.4"));
        TS_ASSERT(!NetUtils::isLoopbackIpv6("fd7c::192.168.1.1"));

        TS_ASSERT_THROWS(NetUtils::isLoopbackIpv6("192.168.1.1"), std::exception);
        TS_ASSERT_THROWS(NetUtils::isLoopbackIpv6(""), std::exception);
    }

    void testIsLinkLocalIpv6()
    {
        using namespace ta;

        TS_ASSERT(NetUtils::isLinkLocalIpv6("fe80::200:5eff:fe00:104"));
        TS_ASSERT(NetUtils::isLinkLocalIpv6("FE80::200:5EFF:FE00:104"));
        TS_ASSERT(NetUtils::isLinkLocalIpv6("FEbf::200:5EFF:FE00:104"));

        // The same link-local IPv6 with scopeid 1 in different notations
        TS_ASSERT(NetUtils::isLinkLocalIpv6("FEbf::200:5EFF:FE00:104%1"));
#ifndef _WIN32
        const string myNonLoopbackIfName = getFirstNonLoopbackIfName();
        TS_ASSERT(NetUtils::isLinkLocalIpv6("FEbf::200:5EFF:FE00:104%" + myNonLoopbackIfName));
#endif
        TS_ASSERT(NetUtils::isLinkLocalIpv6("FEbf:1::200:5EFF:FE00:104"));

        TS_ASSERT(!NetUtils::isLinkLocalIpv6("::1"));
        TS_ASSERT(!NetUtils::isLinkLocalIpv6("FEcf::200:5EFF:FE00:104"));
        TS_ASSERT(!NetUtils::isLinkLocalIpv6("::1.2.3.4"));
        TS_ASSERT(!NetUtils::isLinkLocalIpv6("fd7c::192.168.1.1"));// site-local

        TS_ASSERT_THROWS(NetUtils::isLinkLocalIpv6("192.168.1.1"), std::exception);
        TS_ASSERT_THROWS(NetUtils::isLinkLocalIpv6(""), std::exception);
    }


    void testValidIpV6PrefixLength()
    {
        using namespace ta;

        TS_ASSERT(NetUtils::isValidIpv6PrefixLength(0));
        TS_ASSERT(NetUtils::isValidIpv6PrefixLength(64));
        TS_ASSERT(NetUtils::isValidIpv6PrefixLength(128));

        TS_ASSERT(!NetUtils::isValidIpv6PrefixLength(-1));
        TS_ASSERT(!NetUtils::isValidIpv6PrefixLength(130));
    }

    void testValidPort()
    {
        using namespace ta;
        unsigned int myPort;

        const unsigned int myMaxPort = 65535;
        // String version
        TS_ASSERT(!NetUtils::isValidPort("other80text"));
        TS_ASSERT(!NetUtils::isValidPort("0"));
        TS_ASSERT(NetUtils::isValidPort("80", &myPort));
        TS_ASSERT(myPort == 80);
        TS_ASSERT(NetUtils::isValidPort(str(boost::format("%u") % myMaxPort), &myPort));
        TS_ASSERT(myPort == myMaxPort);
        TS_ASSERT(!NetUtils::isValidPort(str(boost::format("%u") % (myMaxPort + 1))));

        // Integer version
        TS_ASSERT(!NetUtils::isValidPort(0));
        TS_ASSERT(NetUtils::isValidPort(80));
        TS_ASSERT(NetUtils::isValidPort(myMaxPort));
        TS_ASSERT(!NetUtils::isValidPort(myMaxPort + 1));
    }

    void test_IPv4_netmask_check_approves_valid_netmask()
    {
        using namespace ta::NetUtils;
        TS_ASSERT(isValidIpv4NetMask("255.0.0.0"));
        TS_ASSERT(isValidIpv4NetMask("255.255.255.0"));
        TS_ASSERT(isValidIpv4NetMask("255.255.255.224"));
        TS_ASSERT(isValidIpv4NetMask("255.192.0.0"));
        TS_ASSERT(isValidIpv4NetMask("252.0.0.0"));
    }

    void test_IPv4_netmask_check_rejects_invalid_netmask()
    {
        using namespace ta::NetUtils;
        TS_ASSERT(!isValidIpv4NetMask("256.0.0.0"));
        TS_ASSERT(!isValidIpv4NetMask("255.333.0.0"));
        TS_ASSERT(!isValidIpv4NetMask("253.0.0.0"));
        TS_ASSERT(!isValidIpv4NetMask("255.255.251.0"));
        TS_ASSERT(!isValidIpv4NetMask("255.255.255.223"));
    }

    void testGetMyIPv4()
    {
        using namespace ta;

        const std::vector<string> myIPs = NetUtils::getMyIpv4();
        TS_ASSERT(myIPs.size() > 0);
        TS_ASSERT(!isElemExist("127.0.0.1", myIPs));
        foreach (const std::string& ip, myIPs)
        {
            TS_ASSERT(NetUtils::isValidIpv4(ip));
            TS_TRACE(("Self IPv4 address: " + ip).c_str());
        }
    }

    void testIsLoopback()
    {
#ifndef _WIN32
        const string myLoopbackIfName = ta::NetUtils::getLoopbackIfName();
        TS_ASSERT(ta::NetUtils::isLoopback(myLoopbackIfName));

        const string myNonLoopbackIfName = getFirstNonLoopbackIfName();
        TS_ASSERT(!ta::NetUtils::isLoopback(myNonLoopbackIfName));
#else
        TS_SKIP("Not implemented on Windows");
#endif
    }

    void testGetMyIfaces()
    {
        using namespace ta;

        // when
#ifdef _WIN32
        const NetUtils::Ifaces myIfaces = NetUtils::getMyIfaces();
#else
        const NetUtils::Ifaces myIfaces = NetUtils::getMyIfaces(NetUtils::skipLoopBackNo);
#endif

        // then
        TS_ASSERT(!myIfaces.empty());
#ifdef _WIN32
        TS_ASSERT_EQUALS(myIfaces.size(), NetUtils::getMyIpv4faces().size());
#else
        TS_ASSERT_EQUALS(myIfaces.size(), NetUtils::getMyIpv4faces(NetUtils::skipLoopBackNo).size());
        TS_ASSERT_EQUALS(myIfaces.size(), NetUtils::getMyIpv6faces(NetUtils::skipLoopBackNo).size());
        TS_ASSERT_EQUALS(NetUtils::getMyIfaces(NetUtils::skipLoopBackNo).size(),
                         NetUtils::getMyIfaces(NetUtils::skipLoopBackYes).size() + 1);
        TS_ASSERT_EQUALS(NetUtils::getMyIpv4faces(NetUtils::skipLoopBackNo).size(),
                         NetUtils::getMyIpv4faces(NetUtils::skipLoopBackYes).size() + 1);
        TS_ASSERT_EQUALS(NetUtils::getMyIpv6faces(NetUtils::skipLoopBackNo).size(),
                         NetUtils::getMyIpv6faces(NetUtils::skipLoopBackYes).size() + 1);
#endif
        foreach (const NetUtils::Iface& iface, myIfaces)
        {
            TS_TRACE((boost::format("\nInterface name: %s\n%s\n") % iface.first % str(iface.second)).str().c_str());

            TS_ASSERT(!iface.first.empty());

            TS_ASSERT(NetUtils::isValidIpv4(iface.second.ipv4.addr));
            TS_ASSERT(NetUtils::isValidIpv4NetMask(iface.second.ipv4.netmask));
#ifdef __linux__
            if (NetUtils::isLoopback(iface.first))
            {
                TS_ASSERT_EQUALS(iface.second.ipsv6.size(), 1U);
                const NetUtils::IPv6 ipv6 = iface.second.ipsv6.at(0);
                TS_ASSERT(NetUtils::isLoopbackIpv6(ipv6.addr));
                TS_ASSERT_EQUALS(ipv6.addr, "::1");
                TS_ASSERT_EQUALS(ipv6.prefixlen, 128);
            }
            else
            {
                TS_ASSERT(iface.second.ipsv6.size() >= 1);
                bool myIsLinkLocalIpv6Found = false;
                foreach (const ta::NetUtils::IPv6& ipv6, iface.second.ipsv6)
                {
                    if (NetUtils::isLinkLocalIpv6(ipv6.addr))
                    {
                        myIsLinkLocalIpv6Found = true;
                        TS_ASSERT(NetUtils::isValidIpv6(ipv6.addr));
                        TS_ASSERT(NetUtils::isValidIpv6PrefixLength(ipv6.prefixlen));
                    }
                }
                TS_ASSERT(myIsLinkLocalIpv6Found);
            }
#endif
        }
    }

    void testGetIfaceConfigType()
    {
#ifndef _WIN32
        using namespace ta::NetUtils;

        IfaceConfigType::val myIPv4IfaceConfigType, myIPv6IfaceConfigType;

        // when
        boost::tie(myIPv4IfaceConfigType, myIPv6IfaceConfigType) = getNetIfaceConfigType("lo");
        // then
        TS_ASSERT_EQUALS(myIPv4IfaceConfigType, IfaceConfigType::Auto);
        TS_ASSERT_EQUALS(myIPv6IfaceConfigType, IfaceConfigType::Auto);

        // when-then
        TS_ASSERT_THROWS(getNetIfaceConfigType("__non_nexisting_iface__"), std::runtime_error);

        // given (iterate over local interfaces)
        foreach (const Iface& iface, getMyIfaces())
        {
            // when
            boost::tie(myIPv4IfaceConfigType, myIPv6IfaceConfigType) = getNetIfaceConfigType(iface.first);

            //
            // then
            //
            TS_TRACE((boost::format("Interface %1% has IPv4 %2% and %3% configuration") %
                    iface.first % str(iface.second.ipv4) % str(myIPv4IfaceConfigType)).str().c_str());
            TS_TRACE((boost::format("Interface %1% has IPv6 %2% and %3% configuration") %
                    iface.first % str(iface.second.ipsv6) %  str(myIPv6IfaceConfigType)).str().c_str());
        }
#else
        TS_SKIP("Not implemented on Windows");
#endif
    }

    void testGetIPv4DefGateway()
    {
#ifndef _WIN32
        // when
        const ta::NetUtils::DefGateway myDefGateway = ta::NetUtils::getDefIpv4Gateway();
        // then
        TS_ASSERT(ta::NetUtils::isValidIpv4(myDefGateway.ip));
        TS_ASSERT(isValidInterfaceName(myDefGateway.iface));
        TS_TRACE(("IPv4 default gateway: " + str(myDefGateway)).c_str());
#else
        TS_SKIP("Not implemented on Windows");
#endif
    }

    void testMac()
    {
        using namespace ta;
        string myMac = NetUtils::getPrimaryMacAddress();
        string myFormattedMac = NetUtils::getFormattedPrimaryMacAddress();
        TS_ASSERT(myMac.size() == 12);
        TS_ASSERT(myFormattedMac.size() == 17);
        TS_TRACE((boost::format("Self MAC address: %s") % myMac).str().c_str());
        TS_TRACE((boost::format("Self MAC address (formatted): %s") % myFormattedMac).str().c_str());
    }

    void testParseHost()
    {
        TS_ASSERT_EQUALS(ta::NetUtils::parseHost("www.vertaalpraktijk.com"), ta::NetUtils::RemoteAddress("www.vertaalpraktijk.com", 80));
        TS_ASSERT_EQUALS(ta::NetUtils::parseHost("www.vertaalpraktijk.com:81"), ta::NetUtils::RemoteAddress("www.vertaalpraktijk.com", 81));
        TS_ASSERT_EQUALS(ta::NetUtils::parseHost("www.vertaalpraktijk.com", 81), ta::NetUtils::RemoteAddress("www.vertaalpraktijk.com", 81));
        TS_ASSERT_EQUALS(ta::NetUtils::parseHost("www.vertaalpraktijk.com:81", 82), ta::NetUtils::RemoteAddress("www.vertaalpraktijk.com", 81));

        TS_ASSERT_EQUALS(ta::NetUtils::parseHost("192.168.1.2"), ta::NetUtils::RemoteAddress("192.168.1.2", 80));
        TS_ASSERT_EQUALS(ta::NetUtils::parseHost("192.168.1.2:81"), ta::NetUtils::RemoteAddress("192.168.1.2", 81));
        TS_ASSERT_EQUALS(ta::NetUtils::parseHost("192.168.1.2", 81), ta::NetUtils::RemoteAddress("192.168.1.2", 81));
        TS_ASSERT_EQUALS(ta::NetUtils::parseHost("192.168.1.2:81", 82), ta::NetUtils::RemoteAddress("192.168.1.2", 81));

        TS_ASSERT_EQUALS(ta::NetUtils::parseHost("[fd7c:1111:1111:10::110]"), ta::NetUtils::RemoteAddress("fd7c:1111:1111:10::110", 80));
        TS_ASSERT_EQUALS(ta::NetUtils::parseHost("[fd7c:1111:1111:10::110]:81"), ta::NetUtils::RemoteAddress("fd7c:1111:1111:10::110", 81));
        TS_ASSERT_EQUALS(ta::NetUtils::parseHost("[fd7c:1111:1111:10::110]", 81), ta::NetUtils::RemoteAddress("fd7c:1111:1111:10::110", 81));
        TS_ASSERT_EQUALS(ta::NetUtils::parseHost("[fd7c:1111:1111:10::110]:81", 82), ta::NetUtils::RemoteAddress("fd7c:1111:1111:10::110", 81));

        TS_ASSERT_EQUALS(ta::NetUtils::parseHost("[fd7c::192.168.1.1]"), ta::NetUtils::RemoteAddress("fd7c::192.168.1.1", 80));
        TS_ASSERT_EQUALS(ta::NetUtils::parseHost("[fd7c::192.168.1.1]:81"), ta::NetUtils::RemoteAddress("fd7c::192.168.1.1", 81));
        TS_ASSERT_EQUALS(ta::NetUtils::parseHost("[::192.168.1.1]"), ta::NetUtils::RemoteAddress("::192.168.1.1", 80));
        TS_ASSERT_EQUALS(ta::NetUtils::parseHost("[::192.168.1.1]:81"), ta::NetUtils::RemoteAddress("::192.168.1.1", 81));

        TS_ASSERT_THROWS(ta::NetUtils::parseHost(""), std::exception);
        TS_ASSERT_THROWS(ta::NetUtils::parseHost("www.vertaalpraktijk.com", ta::NetUtils::NoDefaultPort), std::exception);
    }

    void testRemoteAddressToStr()
    {
        TS_ASSERT_EQUALS(toString(ta::NetUtils::RemoteAddress("www.vertaalpraktijk.com", 80), 80), "www.vertaalpraktijk.com");
        TS_ASSERT_EQUALS(toString(ta::NetUtils::RemoteAddress("www.vertaalpraktijk.com", 80), 81), "www.vertaalpraktijk.com:80");
        TS_ASSERT_EQUALS(toString(ta::NetUtils::RemoteAddress("www.vertaalpraktijk.com", 80)), "www.vertaalpraktijk.com:80");

        TS_ASSERT_EQUALS(toString(ta::NetUtils::RemoteAddress("192.168.1.2", 80), 80), "192.168.1.2");
        TS_ASSERT_EQUALS(toString(ta::NetUtils::RemoteAddress("192.168.1.2", 80), 81), "192.168.1.2:80");
        TS_ASSERT_EQUALS(toString(ta::NetUtils::RemoteAddress("192.168.1.2", 80)), "192.168.1.2:80");

        TS_ASSERT_EQUALS(toString(ta::NetUtils::RemoteAddress("fd7c:1111:1111:10::110", 80), 80), "[fd7c:1111:1111:10::110]");
        TS_ASSERT_EQUALS(toString(ta::NetUtils::RemoteAddress("fd7c:1111:1111:10::110", 80), 81), "[fd7c:1111:1111:10::110]:80");
        TS_ASSERT_EQUALS(toString(ta::NetUtils::RemoteAddress("fd7c:1111:1111:10::110", 80)), "[fd7c:1111:1111:10::110]:80");

        TS_ASSERT_EQUALS(toString(ta::NetUtils::RemoteAddress("0.0.0.0", 80)), "127.0.0.1:80");
        TS_ASSERT_EQUALS(toString(ta::NetUtils::RemoteAddress("::", 80)), "[::1]:80");
    }

    void testDotDecimalIpv4EmbeddedInIpv6()
    {
        using namespace ta::NetUtils;

        string myCanonicalIpv6;

        TS_ASSERT(isDotDecimalIpv4EmbeddedInIpv6("::192.168.1.1", myCanonicalIpv6));
        TS_ASSERT_EQUALS(myCanonicalIpv6, "::c0a8:0101");
        TS_ASSERT(isDotDecimalIpv4EmbeddedInIpv6("fd7c::192.168.1.1", myCanonicalIpv6));
        TS_ASSERT_EQUALS(myCanonicalIpv6, "fd7c::c0a8:0101");
        TS_ASSERT(isDotDecimalIpv4EmbeddedInIpv6("2001:db8::FFFF:192.168.1.1", myCanonicalIpv6));
        TS_ASSERT_EQUALS(myCanonicalIpv6, "2001:db8::FFFF:c0a8:0101");

        TS_ASSERT(!isDotDecimalIpv4EmbeddedInIpv6("fd7c:1111:1111:10::110", myCanonicalIpv6));
        TS_ASSERT(!isDotDecimalIpv4EmbeddedInIpv6("::1", myCanonicalIpv6));

        TS_ASSERT_THROWS(isDotDecimalIpv4EmbeddedInIpv6("192.168.1.1", myCanonicalIpv6), std::exception);
        TS_ASSERT_THROWS(isDotDecimalIpv4EmbeddedInIpv6("www.google.com", myCanonicalIpv6), std::exception);

    }

    void test_internet_connectivity()
    {
        using namespace ta::NetUtils;
#ifndef _WIN32
        TS_ASSERT_EQUALS(checkConnectivity(), connectivityOk);
        TS_ASSERT_EQUALS(checkConnectivity(boost::assign::list_of(RemoteAddress("google.com", 666))), connectivityTcpServersNotAccessible);
#else
        TS_SKIP("Not implemented on Windows");
#endif
    }

    void test_ping_running_ipv4_host_succeeds()
    {
#ifndef _WIN32
        TS_TRACE(__FUNCTION__);
        // Given
        string host("127.0.0.1");
        // When
        bool pingSuccess = ta::NetUtils::ping(host);
        // Then
        TS_ASSERT(pingSuccess);
#else
        TS_SKIP("Not implemented on Windows");
#endif
    }

    void test_ping_unreachable_ipv4_host_fails()
    {
#ifndef _WIN32
        TS_TRACE(__FUNCTION__);
        // Given
        string host("128.1.2.3");
        // When
        bool pingSuccess = ta::NetUtils::ping(host);
        // Then
        TS_ASSERT(!pingSuccess);
#else
        TS_SKIP("Not implemented on Windows");
#endif
    }

    void test_ping_incorrect_ipv4_address_fails()
    {
#ifndef _WIN32
        TS_TRACE(__FUNCTION__);
        // Given
        string host("333.0.0.1");
        // When
        bool pingSuccess = ta::NetUtils::ping(host);
        // Then
        TS_ASSERT(!pingSuccess);
#else
        TS_SKIP("Not implemented on Windows");
#endif
    }

    void test_ping_running_ipv6_host_succeeds()
    {
#ifndef _WIN32
        TS_TRACE(__FUNCTION__);
        // Given
        string host("::1");
        // When
        bool pingSuccess = ta::NetUtils::ping6(host);
        // Then
        TS_ASSERT(pingSuccess);
#else
        TS_SKIP("Not implemented on Windows");
#endif
    }

    void test_ping_unreachable_ipv6_host_fails()
    {
#ifndef _WIN32
        // When
        bool pingSuccess = ta::NetUtils::ping6("::9");
        // Then
        TS_ASSERT(!pingSuccess);
#else
        TS_SKIP("Not implemented on Windows");
#endif
    }

    void test_ping_incorrect_ipv6_address_fails()
    {
#ifndef _WIN32
        // When
        bool pingSuccess = ta::NetUtils::ping6("::z");
        // Then
        TS_ASSERT(!pingSuccess);
#else
        TS_SKIP("Not implemented on Windows");
#endif
    }


    void testIsValidDomainName()
    {
        using namespace ta::NetUtils;

        DomainNameValidationResult validationResult;

        TS_TRACE("--- Testing valid domain names as hostname");
        TS_ASSERT(isValidDomainName("abcd", validationResult, hostName));
        TS_ASSERT_EQUALS(validationResult, domainNameOk);

        TS_ASSERT(isValidDomainName("a.b.c", validationResult, hostName));
        TS_ASSERT_EQUALS(validationResult, domainNameOk);

        TS_ASSERT(isValidDomainName("0123456789", validationResult, hostName));
        TS_ASSERT_EQUALS(validationResult, domainNameOk);

        TS_TRACE("--- Testing valid domain names as DNS name");
        TS_ASSERT(isValidDomainName("_abcd", validationResult, dnsName));
        TS_ASSERT_EQUALS(validationResult, domainNameOk);

        TS_ASSERT(isValidDomainName("_a.b.c", validationResult, dnsName));
        TS_ASSERT_EQUALS(validationResult, domainNameOk);

        TS_ASSERT(isValidDomainName("_0123456789", validationResult, dnsName));
        TS_ASSERT_EQUALS(validationResult, domainNameOk);

        TS_TRACE("--- Testing valid domain names as hostname (valid after normalization)");
        TS_ASSERT(isValidDomainName("  abcd   ", validationResult, hostName));
        TS_ASSERT_EQUALS(validationResult, domainNameOk);

        TS_ASSERT(isValidDomainName("aBCd", validationResult, hostName));
        TS_ASSERT_EQUALS(validationResult, domainNameOk);

        TS_TRACE("--- Testing valid domain names as DNS name (valid after normalization)");
        TS_ASSERT(isValidDomainName("  _abcd   ", validationResult, dnsName));
        TS_ASSERT_EQUALS(validationResult, domainNameOk);

        TS_ASSERT(isValidDomainName("_aBCd", validationResult, dnsName));
        TS_ASSERT_EQUALS(validationResult, domainNameOk);

        TS_TRACE("--- Testing domain name without any characters");
        TS_ASSERT(!isValidDomainName("", validationResult, hostName));
        TS_ASSERT_EQUALS(validationResult, domainNameEmpty);

        TS_TRACE("--- Testing domain name without any characters (after normalization)");
        TS_ASSERT(!isValidDomainName("  ", validationResult, hostName));
        TS_ASSERT_EQUALS(validationResult, domainNameEmpty);

        TS_TRACE("--- Testing domain name as hostname with invalid characters");
        TS_ASSERT(!isValidDomainName("a.b.c^", validationResult, hostName));
        TS_ASSERT_EQUALS(validationResult, domainNameInvalidCharacter);
        TS_ASSERT(!isValidDomainName("a@", validationResult, hostName));
        TS_ASSERT_EQUALS(validationResult, domainNameInvalidCharacter);

        TS_TRACE("--- Testing domain name as DNS name with invalid characters");
        TS_ASSERT(!isValidDomainName("a.b.c^", validationResult, dnsName));
        TS_ASSERT_EQUALS(validationResult, domainNameInvalidCharacter);
        TS_ASSERT(!isValidDomainName("a@", validationResult, dnsName));
        TS_ASSERT_EQUALS(validationResult, domainNameInvalidCharacter);

        TS_TRACE("--- Testing domain name with an invalid label (without a character)");
        TS_ASSERT(!isValidDomainName("a..c", validationResult, hostName));
        TS_ASSERT_EQUALS(validationResult, domainNameLabelEmpty);
        TS_ASSERT(!isValidDomainName("a.b..", validationResult, hostName));
        TS_ASSERT_EQUALS(validationResult, domainNameLabelEmpty);
        TS_ASSERT(!isValidDomainName("..a.b", validationResult, hostName));
        TS_ASSERT_EQUALS(validationResult, domainNameLabelEmpty);

        TS_TRACE("--- Testing domain name with a maximum label size");
        // A label with 63 characters is allowed
        TS_ASSERT(isValidDomainName("a.012345678901234567890123456789012345678901234567890123456789123.c", validationResult, hostName));
        TS_ASSERT_EQUALS(validationResult, domainNameOk);

        TS_TRACE("--- Testing domain name with an invalid label size");
        // A label with 64 characters is not allowed
        TS_ASSERT(!isValidDomainName("a.0123456789012345678901234567890123456789012345678901234567891234.c", validationResult, hostName));
        TS_ASSERT_EQUALS(validationResult, domainNameLabelTooLong);

        TS_TRACE("--- Testing domain name with a maximum number of characters");
        // A domain name with 255 characters is allowed
        string domainNameWithMaximumSize =
            "0123456789012345678901234567890123456789012345678."
            "0123456789012345678901234567890123456789012345678."
            "0123456789012345678901234567890123456789012345678."
            "0123456789012345678901234567890123456789012345678."
            "0123456789012345678901234567890123456789012345678."
            "01234";
        TS_ASSERT(isValidDomainName(domainNameWithMaximumSize, validationResult, hostName));
        TS_ASSERT_EQUALS(validationResult, domainNameOk);

        TS_TRACE("--- Testing domain name with an invalid number of characters");
        // A domain name with more than 255 characters is not allowed
        string domainNameWithInvalidMaximumSize =
            "0123456789012345678901234567890123456789012345678."
            "0123456789012345678901234567890123456789012345678."
            "0123456789012345678901234567890123456789012345678."
            "0123456789012345678901234567890123456789012345678."
            "0123456789012345678901234567890123456789012345678."
            "012345";
        TS_ASSERT(!isValidDomainName(domainNameWithInvalidMaximumSize, validationResult, hostName));
        TS_ASSERT_EQUALS(validationResult, domainNameTooLong);
    }

    void test_that_self_fqdn_can_be_retrieved()
    {
#ifdef _WIN32
        const std::string mySelfFQDN = ta::NetUtils::getSelfFqdn();
        TS_TRACE(("Self FQDN: " + mySelfFQDN).c_str());
        TS_ASSERT(!mySelfFQDN.empty());
#else
        TS_SKIP("This test is for Windows only");
#endif
    }


private:
#ifndef _WIN32
    bool isValidInterfaceName(const string& anIfaceName)
    {
        try
        {
            ta::NetUtils::getNetIfaceConfigType(anIfaceName);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
    string getFirstNonLoopbackIfName()
    {
        using namespace ta::NetUtils;

        foreach (const Iface& iface, getMyIfaces(skipLoopBackYes))
        {
            return iface.first;
        }
        TA_THROW_MSG(std::runtime_error, "No non-loopback interface found");
    }
#endif

}; // NetUtilsAccessorsTest



// Test of NetUtils modifiers i.e. functions that modify system network configuration and therefore need to restore the original configuration at the end
class NetUtilsModifiersTest : public CxxTest::TestSuite
{
    void setUp()
    {
#ifdef RESEPT_SERVER
        try
        {
            namespace fs = boost::filesystem;
            fs::copy_file("/etc/network/interfaces", "./interfaces.bak", fs::copy_option::overwrite_if_exists);
            ta::TimeUtils::sleep(2*ta::TimeUtils::MsecsInSecond); // sleep a bit between tests to let network interfaces properly recover
        }
        catch(std::exception& e)
        {
            TS_TRACE(e.what());
            throw;
        }
        catch(...)
        {
            TS_TRACE("setUp() failed with unknown error");
            throw;
        }
#endif
    }

    void tearDown()
    {
#ifdef RESEPT_SERVER
        try
        {
            namespace fs = boost::filesystem;
            if (fs::exists("./interfaces.bak"))
            {
                fs::copy_file("./interfaces.bak", "/etc/network/interfaces", fs::copy_option::overwrite_if_exists);
                try {
                    ta::Process::checkedShellExecSync("sudo service networking restart");
                } catch (std::exception& e) {
                    // add extra diagnostics
                    std::string myStdOut, myStdErr;
                    ta::Process::shellExecSync("sudo service networking status", myStdOut, myStdErr);
                    TA_THROW_MSG(std::runtime_error, boost::format("%s. 'service networking status' says: stdout: %s. stderr: %s") % e.what() % myStdOut % myStdErr);
                }
            }
        }
        catch(std::exception& e)
        {
            TS_TRACE(e.what());
            throw;
        }
        catch(...)
        {
            TS_TRACE("tearDown() failed with unknown error");
            throw;
        }
#endif
    }
public:
    void testApplyNetIfaceConfig()
    {
#ifdef RESEPT_SERVER
        using namespace ta::NetUtils;

        unsigned int myIndex = 1;

        const DefGateway myOrigIpv4DefGateway = getDefIpv4Gateway();

        foreach (const Iface& ifaceOrig, getMyIfaces())
        {
            // given
            const string myIfaceName = ifaceOrig.first;
            IfaceConfigType::val myOrigIPv4IfaceConfigType, myOrigIPv6IfaceConfigType;
            boost::tie(myOrigIPv4IfaceConfigType, myOrigIPv6IfaceConfigType) = getNetIfaceConfigType(myIfaceName);

            // For IPv4 we change interface config type only
            // For IPv6 we add new IP, leaving interface configuration 'manual'
            Iface myNewIface = ifaceOrig;
            IfaceConfigType::val myNewIPv4IfaceConfigType = (myOrigIPv4IfaceConfigType == IfaceConfigType::Auto)? IfaceConfigType::Manual: IfaceConfigType::Auto;
            IfaceConfigType::val myNewIPv6IfaceConfigType = IfaceConfigType::Manual;
            myNewIface.second.ipsv6.push_back(IPv6("2002:c000:203::" + ta::Strings::toString(myIndex++), 64));

            TS_TRACE((boost::format("Applying changes to %s.\nIPv4 configuration: %s -> %s.\nIPv6: %s -> %s, no configuration change") %
                    myIfaceName % str(myOrigIPv4IfaceConfigType) % str(myNewIPv4IfaceConfigType) % str(ifaceOrig.second.ipsv6) % str(myNewIface.second.ipsv6)).str().c_str());
            // when
            applyNetIfaceConfig(myNewIface, myNewIPv4IfaceConfigType, myNewIPv6IfaceConfigType);

            // then
            IfaceInfo myNewActualIfaceInfo;
            if (ta::findValueByKey(myIfaceName, getMyIfaces(), myNewActualIfaceInfo))
            {
                IfaceConfigType::val myNewActualIPv4IfaceConfigType, myNewActualIPv6IfaceConfigType;
                boost::tie(myNewActualIPv4IfaceConfigType, myNewActualIPv6IfaceConfigType) = getNetIfaceConfigType(myIfaceName);
                TS_ASSERT_EQUALS(myNewActualIPv4IfaceConfigType, myNewIPv4IfaceConfigType);
                TS_ASSERT_EQUALS(myNewActualIPv6IfaceConfigType, IfaceConfigType::Manual);
                if (myNewIPv4IfaceConfigType == IfaceConfigType::Manual)
                {
                    // DHCP -> static
                    if (myNewActualIfaceInfo != myNewIface.second)
                    {
                        ERRORLOG(boost::format("Actual settings of interface %s differs from the expected.\nActual: %s\nExpected: %s") %
                                myIfaceName % str(myNewActualIfaceInfo) % str(myNewIface.second));
                        TS_ASSERT(false);
                    }
                }
                else
                {
                    // static -> DHCP.
                    // Couldn't know beforehand what IPv4 DHCP would assign, so test IPv6 only
                    if (!ta::equalIgnoreOrder(myNewActualIfaceInfo.ipsv6, myNewIface.second.ipsv6))
                    {
                        ERRORLOG(boost::format("Expected IPv6 configuration of interface %s does not equal to the actual.\nActual: %s\nExpected: %s") %
                                myIfaceName % str(myNewActualIfaceInfo.ipsv6) % str(myNewIface.second.ipsv6));
                        TS_ASSERT(false);
                    }
                }
            }
            else
            {
                ERRORLOG(boost::format("%s interface not found after the new configuration has been applied") % myIfaceName);
                TS_ASSERT(false);
            }

            // Restore the original network configuration
            INFOLOG(boost::format("Restoring configuration for %s") % myIfaceName);
            // when
            applyNetIfaceConfig(ifaceOrig, myOrigIPv4IfaceConfigType, myOrigIPv6IfaceConfigType);


            // then
            IfaceInfo myRestoredIfaceInfo;
            if (ta::findValueByKey(myIfaceName, getMyIfaces(), myRestoredIfaceInfo))
            {
                IfaceConfigType::val myRestoredIPv4IfaceConfigType, myRestoredIPv6IfaceConfigType;
                boost::tie(myRestoredIPv4IfaceConfigType, myRestoredIPv6IfaceConfigType) = getNetIfaceConfigType(myIfaceName);
                TS_ASSERT_EQUALS(myRestoredIPv4IfaceConfigType, myOrigIPv4IfaceConfigType);
                TS_ASSERT_EQUALS(myRestoredIfaceInfo, ifaceOrig.second);
            }
            else
            {
                ERRORLOG(boost::format("%s interface not found after an original configuration has been restored") % myIfaceName);
                TS_ASSERT(false);
            }
        } //foreach
#else
    TS_SKIP("Network interface modification tests are only needed by KeyTalk server, skipping them for this platform");
#endif
    }

    void testApplyIpv4CustomRoutes()
    {
        using namespace ta::NetUtils;

#ifdef RESEPT_SERVER
        // given
        const DefGateway myDefGw = getDefIpv4Gateway();
        const string myIfaceName = myDefGw.iface;
        const IPv4Routes myOrigRoutes = getIpv4CustomRoutes(myIfaceName);
        printCustomRoutes("Original custom IPv4 routes on " + myIfaceName, myOrigRoutes);

        // given
        IPv4Routes myNewRoutesSet = myOrigRoutes;
        myNewRoutesSet.push_back(IPv4Route(IPv4("192.168.34.0", "255.255.255.0"), myDefGw.ip));
        const string mySaveScriptPath = "./custom-routes.temp";
        // when (add new route)
        applyIpv4CustomRoutes(myIfaceName, myNewRoutesSet, mySaveScriptPath);
        // then
        IPv4Routes myNewRoutesQueried = getIpv4CustomRoutes(myIfaceName);
        printCustomRoutes("New custom IPv4 routes on " + myIfaceName, myNewRoutesQueried);
        TS_ASSERT(ta::equalIgnoreOrder(myNewRoutesQueried, myNewRoutesSet));

        // when (restore the original routes)
        ta::TimeUtils::sleep(2*ta::TimeUtils::MsecsInSecond); // sleep a bit to not annoy service manager with too frequent service restarts
        applyIpv4CustomRoutes(myIfaceName, myOrigRoutes, mySaveScriptPath);
        // then
        myNewRoutesQueried = getIpv4CustomRoutes(myIfaceName);
        printCustomRoutes("Restored custom IPv4 routes on " + myIfaceName, myNewRoutesQueried);
        TS_ASSERT(ta::equalIgnoreOrder(myNewRoutesQueried, myOrigRoutes));

        TS_ASSERT_THROWS(applyIpv4CustomRoutes("non-existing-inferface", myOrigRoutes, mySaveScriptPath), std::exception);
        ta::TimeUtils::sleep(2*ta::TimeUtils::MsecsInSecond); // sleep a bit to not annoy service manager with too frequent service restarts
        const IfacesIPv4Routes myInvalidRoutes = boost::assign::map_list_of("non-existing-inferface", myOrigRoutes);
        TS_ASSERT_THROWS(applyIpv4CustomRoutes(myInvalidRoutes, mySaveScriptPath), std::exception);
        ta::TimeUtils::sleep(2*ta::TimeUtils::MsecsInSecond); // sleep a bit to not annoy service manager with too frequent service restarts
#else
        TS_SKIP("Network interface modification tests are only needed by KeyTalk server, skipping them for this platform");
#endif
    }


#ifdef RESEPT_SERVER
private:

    template <class Routes>
    void printCustomRoutes(const string& aHint, const Routes& aRoutes)
    {
        TS_TRACE((aHint+":").c_str());
        if (aRoutes.empty())
        {
            TS_TRACE("<no custom routes defined>");
        }
        else
        {
            TS_TRACE(str(aRoutes, "\n").c_str());
        }
    }

    struct IfaceConfigInfo
    {
        IfaceConfigInfo() {}
        IfaceConfigInfo(const ta::NetUtils::Iface& anIface, const ta::NetUtils::IfaceConfigType::val anIpv4ConfigType, const ta::NetUtils::IfaceConfigType::val anIpv6ConfigType)
        : iface(anIface), ipv4_config_type(anIpv4ConfigType), ipv6_config_type(anIpv6ConfigType)
        {}
        ta::NetUtils::Iface iface;
        ta::NetUtils::IfaceConfigType::val ipv4_config_type;
        ta::NetUtils::IfaceConfigType::val ipv6_config_type;
    };
    typedef std::map<string, IfaceConfigInfo> IfacesConfigInfo;


#endif
}; // NetUtilsModifiersTest
