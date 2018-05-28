#pragma once

#include "ta/common.h"
#include "ta/dnsutils.h"
#include "ta/netutils.h"
#include "cxxtest/TestSuite.h"
#include <string>
#include <vector>
#include "boost/assign/list_of.hpp"

class DnsUtilsClientTest : public CxxTest::TestSuite
{
public:

    void testGetIpByName()
    {
        // to test:
        // host -t A <hostname>
        // host -t AAAA <hostname>

        // given
        std::string myHostName = "worldipv6launch.org";
        // when
        ta::NetUtils::IP myHostIp = ta::DnsUtils::resolveIpByName(myHostName);
        // then (IPv4)
        TS_TRACE((boost::format("IPv4 address of %1% is %2%.\nIPv6 address of %1% is %3%\n") % myHostName % myHostIp.ipv4 % myHostIp.ipv6).str().c_str());
        TSM_ASSERT((myHostName + string(" has invalid IPv4: \'") + myHostIp.ipv4 + string("\'")).c_str(), ta::NetUtils::isValidIpv4(myHostIp.ipv4));
        // @note the IP checks below may sometimes fail if the remote server changes its IP address
        const ta::StringArray myExpectedIpv4s = boost::assign::list_of("212.110.167.157");
        TS_ASSERT(ta::isElemExist(myHostIp.ipv4, myExpectedIpv4s));

        // when
        ta::NetUtils::IP myHostIpv4 = ta::DnsUtils::resolveIpByName(myHostIp.ipv4);
        // then
        TS_ASSERT(ta::NetUtils::isValidIpv4(myHostIpv4.ipv4));
        TS_ASSERT(myHostIpv4.ipv6.empty());
        TS_ASSERT(ta::isElemExist(myHostIp.ipv4, myExpectedIpv4s));

#ifndef _WIN32
        // then (IPv6)
        TSM_ASSERT((myHostName + string(" has invalid IPv6:  \'") + myHostIp.ipv6 + string("\'")).c_str(), ta::NetUtils::isValidIpv6(myHostIp.ipv6));
        const ta::StringArray myExpectedIpv6s = boost::assign::list_of("2001:41c8:20::19");
        TS_ASSERT(ta::isElemExist(myHostIp.ipv6, myExpectedIpv6s));
        ta::NetUtils::IP myHostIpv6 = ta::DnsUtils::resolveIpByName(myHostIp.ipv6);
        TS_ASSERT(ta::NetUtils::isValidIpv6(myHostIpv6.ipv6));
        TS_ASSERT(myHostIpv6.ipv4.empty());
        TS_ASSERT(ta::isElemExist(myHostIp.ipv6, myExpectedIpv6s));
#endif
    }

    void testGetIpsByName()
    {
        //to check IPv4: #host -t A <hostname>
        //to check IPv6: #host -t AAAA <hostname>
        const std::string myHostName = "cnn.com";
        size_t myNumIpv4 = 0, myNumIpv6 = 0;
        foreach (const ta::NetUtils::IP& ip, ta::DnsUtils::resolveIpsByName(myHostName))
        {
            TS_TRACE((boost::format("Host %1%. %2%\n") % myHostName % str(ip)).str().c_str());
            if (!ip.ipv4.empty())
                ++myNumIpv4;
            if (!ip.ipv6.empty())
                ++myNumIpv6;
        }
        TS_ASSERT(myNumIpv4 >= (size_t)3);
#ifndef _WIN32
        TS_ASSERT(myNumIpv6 >= (size_t)1);
#endif
    }

    void testGetIpByName_Unresolvable()
    {
        TS_ASSERT_THROWS(ta::DnsUtils::resolveIpByName("i_hope_this_domain_does_not_exist"), ta::IpResolveError);
    }

};

