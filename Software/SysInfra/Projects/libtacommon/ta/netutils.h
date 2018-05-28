#pragma once

#include "ta/common.h"
#include "ta/strings.h"

#include <stdexcept>
#include <string>
#include <vector>
#include <map>
#include "boost/tuple/tuple.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/assign/list_of.hpp"
#include "boost/format.hpp"

struct sockaddr_in6;

namespace ta
{
    struct IpResolveError : std::runtime_error
    {
        explicit IpResolveError(const std::string& aMessage = ""): std::runtime_error(aMessage) {}
    };

    namespace NetUtils
    {
        enum DomainNameType
        {
            hostName,
            dnsName
        };

        enum DomainNameValidationResult
        {
            domainNameOk,
            domainNameEmpty,
            domainNameTooLong,
            domainNameLabelTooLong,
            domainNameLabelEmpty,
            domainNameInvalidCharacter
        };

        struct DomainNameValidationError : public std::runtime_error
        {
            explicit DomainNameValidationError(ta::NetUtils::DomainNameValidationResult aValidationResult, const std::string& aDomainName)
                :   std::runtime_error("Invalid domain name: '" + aDomainName + "'"),
                    validationResult(aValidationResult)
            {}

            DomainNameValidationResult validationResult;
        };

        struct IP
        {
            std::string ipv4;
            std::string ipv6;

            IP() {}
            IP(const std::string& anIpv4, const std::string& anIpv6) : ipv4(anIpv4), ipv6(anIpv6) {}
            inline bool operator==(const IP& rhs) const { return (rhs.ipv4 == ipv4 && boost::iequals(rhs.ipv6,ipv6)); }
        };
        inline std::string str(const IP& anIp)
        {
            std::string myRetVal;
            if (!anIp.ipv4.empty())
            {
                myRetVal = "IPv4: "+ anIp.ipv4;
            }
            if (!anIp.ipv6.empty())
            {
                if (myRetVal.empty())
                {
                    myRetVal = "IPv6: "+ anIp.ipv6;
                }
                else
                {
                    myRetVal += ", IPv6: " + anIp.ipv6;
                }
            }
            return myRetVal;
        }

        /**
          Retrieve MAC address
          @return MAC address of the primary NIC. The return value is the upper case HEX representation of MAC address of length 12.
          The second 'Formatted' return MAC formatted with ":"
         */
        std::string getPrimaryMacAddress();
        std::string getFormattedPrimaryMacAddress();

        /**
          Retrieve list of IPv4 addresses
          @return List of IPv4 addresses of the local host skipping loopback and down interfaces.
         */
        std::vector<std::string> getMyIpv4();

#ifndef _WIN32
        struct DefGateway
        {
            DefGateway() {}
            DefGateway(const std::string& anIfaceName, const std::string& anIp): iface(anIfaceName), ip(anIp) {}
            inline bool defined() const { return !iface.empty() && !ip.empty(); }
            inline bool operator==(const DefGateway& rhs) const { return rhs.ip == ip; }
            inline bool operator!=(const DefGateway& rhs) const { return !(*this == rhs); }
            std::string iface;
            std::string ip;
        };
        inline std::string str(const DefGateway& aDefGateway)
        {
            return aDefGateway.defined() ? str(boost::format("%1% via %2%") % aDefGateway.ip % aDefGateway.iface)
                   : "<not defined>";
        }
        /**
          Retrieve default IPv4 gateway
         */
        DefGateway getDefIpv4Gateway();

        /**
             Effectuates default gateway and makes the changes persistent across reboots
             @pre all non-loopback interfaces should have manual (non-DHCP) setting
        */
        void applyDefIpv4Gateway(const DefGateway& aDefGateway);
#endif
        struct IPv4
        {
            IPv4() {}
            IPv4(const std::string& anAddr, const std::string& aNetMask): addr(anAddr), netmask(aNetMask) {}
            std::string addr;
            std::string netmask;
        };
        inline bool operator==(const IPv4& lhs, const IPv4& rhs)
        {
            return (lhs.addr == rhs.addr && lhs.netmask == rhs.netmask);
        }
        inline bool operator!=(const IPv4& lhs, const IPv4& rhs)
        {
            return !(lhs == rhs);
        }
        inline std::string str(const IPv4& anIpv4)
        {
            return str(boost::format("%s/%s") % anIpv4.addr % anIpv4.netmask);
        }
        inline bool operator<(const IPv4& lhs, const IPv4& rhs)
        {
            if (lhs.addr != rhs.addr)
            {
                return lhs.addr < rhs.addr;
            }
            else
            {
                return lhs.netmask < rhs.netmask;
            }
        }

        struct IPv6
        {
            IPv6(): prefixlen((size_t)-1) {}
            IPv6(const std::string& anAddr, const size_t aPrefixLen): addr(anAddr), prefixlen(aPrefixLen) {}
            std::string addr;
            size_t prefixlen;
        };
        inline bool operator==(const IPv6& lhs, const IPv6& rhs)
        {
            return ( boost::iequals(lhs.addr, rhs.addr) && lhs.prefixlen == rhs.prefixlen);
        }
        inline bool operator!=(const IPv6& lhs, const IPv6& rhs)
        {
            return !(lhs == rhs);
        }
        inline bool operator<(const IPv6& lhs, const IPv6& rhs)
        {
            if (boost::to_upper_copy(lhs.addr) != boost::to_upper_copy(rhs.addr))
            {
                return boost::to_upper_copy(lhs.addr) < boost::to_upper_copy(rhs.addr);
            }
            else
            {
                return lhs.prefixlen < rhs.prefixlen;
            }
        }
        inline std::string str(const IPv6& anIpv6)
        {
            return str(boost::format("%s/%d") % anIpv6.addr % anIpv6.prefixlen);
        }

        typedef std::vector<IPv6> IPsv6;
        inline std::string str(const IPsv6& anIPsv6, const std::string& aSep = " ")
        {
            std::vector<std::string> ipsv6;
            foreach (const IPv6& ipv6, anIPsv6)
            {
                ipsv6.push_back(str(ipv6));
            }
            return ta::Strings::join(ipsv6, aSep);
        }

        struct IfaceInfo
        {
            IPv4 ipv4;
            IPsv6 ipsv6;
        };
        inline bool operator==(const IfaceInfo& lhs, const IfaceInfo& rhs)
        {
            return (lhs.ipv4 == rhs.ipv4 && ta::equalIgnoreOrder(lhs.ipsv6, rhs.ipsv6));
        }
        inline bool operator!=(const IfaceInfo& lhs, const IfaceInfo& rhs)
        {
            return !(lhs == rhs);
        }
        inline std::string str(const IfaceInfo& anIfaceInfo)
        {
            return str(boost::format("IPv4: %s. IPv6: %s") % str(anIfaceInfo.ipv4) % str(anIfaceInfo.ipsv6));
        }

        // interface name: interface info
        typedef std::pair<std::string, IfaceInfo> Iface;
        typedef std::map<std::string, IfaceInfo> Ifaces;

        inline bool operator==(const Iface& lhs, const Iface& rhs)
        {
            return (lhs.first == rhs.first && lhs.second == rhs.second);
        }
        inline bool operator!=(const Iface& lhs, const Iface& rhs)
        {
            return !(lhs == rhs);
        }

        struct SockInitializer
        {
            SockInitializer();
            ~SockInitializer();
        };


        enum SkipLoopBack
        {
            skipLoopBackYes,
            skipLoopBackNo

        };
#ifdef _WIN32
        /**
          Retrieve interfaces of local host skipping down and loopback
          Only IPv4 interfaces are returned on Windows
         */
        Ifaces getMyIpv4faces();
        Ifaces getMyIfaces();
#else
        /**
          Retrieve interfaces of local host skipping down interfaces
          @param aSkipLoopBack skip loopback interfaces
         */
        Ifaces getMyIpv6faces(SkipLoopBack aSkipLoopBack = skipLoopBackYes);
        Ifaces getMyIpv4faces(SkipLoopBack aSkipLoopBack = skipLoopBackYes);
        Ifaces getMyIfaces(SkipLoopBack aSkipLoopBack = skipLoopBackYes);
#endif

#ifndef _WIN32
        struct IfaceConfigType
        {
            enum val
            {
                _First,
                Auto = _First,  // Automatic configuration (IP is received from DHCP server or configured by system, e.g. loopback)
                Manual,         // Manual configuration (static IP)
                _Last = Manual
            };
        };
        const std::string IfaceConfigTypeStrs[] = {"auto", "manual"};
        inline std::string str(const IfaceConfigType::val anIfaceConfigType)
        {
            return IfaceConfigTypeStrs[anIfaceConfigType];
        }
        inline IfaceConfigType::val parseIfaceConfigType(const std::string& aVal)
        {
            for (int i = IfaceConfigType::_First; i <= IfaceConfigType::_Last; ++i)
            {
                const IfaceConfigType::val configType = static_cast<IfaceConfigType::val>(i);
                if (str(configType) == aVal)
                {
                    return configType;
                }
            }
            TA_THROW_MSG(std::invalid_argument, aVal + " is invalid interface configuration type");
        }

        /**
         Retrieve network interface configuration types as tuple (ipv4-net-type, ipv6-net-type) given the interface name (such as "eth0")
        */
        boost::tuple<IfaceConfigType::val, IfaceConfigType::val> getNetIfaceConfigType(const std::string& anIfaceName);

        /*
            Just a handy shortcut to check whether at least one interface with automatic (DHCP) configuration exist, skipping loopback interfaces.
        */
        bool existIfaceWithAutoIpv4Configuration();
        bool existIfaceWithAutoIpv6Configuration();

        /**
         Apply network configuration to the given interface skipping loopback and link-local interface.
         For IPv6 only non-linklocal addresses are applied
        */
        void applyNetIfaceConfig(const Iface& anIface, IfaceConfigType::val anIPv4IfaceConfigType, IfaceConfigType::val anIPv6IfaceConfigType);

        struct IPv4Route
        {
            IPv4Route(const IPv4& aNetwork, const std::string& aGateway)
                : network(aNetwork), gateway(aGateway)
            {}
            inline bool operator==(const IPv4Route& other) const
            {
                return (network == other.network) && (gateway == other.gateway);
            }
            inline bool operator!=(const IPv4Route& other) const
            {
                return !(*this  == other);
            }
            inline bool operator<(const IPv4Route& other) const
            {
                if (network != other.network)
                {
                    return network < other.network;
                }
                else
                {
                    return gateway < other.gateway;
                }
            }

            IPv4 network;
            std::string gateway;
        };

        struct IPv6Route
        {
            IPv6Route(const IPv6& aNetwork, const std::string& aGateway)
                : network(aNetwork), gateway(aGateway)
            {}
            inline bool operator==(const IPv6Route& other) const
            {
                return (network == other.network) && (gateway == other.gateway);
            }
            inline bool operator!=(const IPv6Route& other) const
            {
                return !(*this  == other);
            }
            inline bool operator<(const IPv6Route& other) const
            {
                if (network != other.network)
                {
                    return network < other.network;
                }
                else
                {
                    return gateway < other.gateway;
                }
            }

            IPv6 network;
            std::string gateway;
        };

        typedef std::vector<IPv4Route> IPv4Routes;
        typedef std::pair<std::string, std::vector<IPv4Route> > IfaceIPv4Routes;
        typedef std::map<std::string, std::vector<IPv4Route> > IfacesIPv4Routes;

        template <class Route>
        std::string str_impl(const Route& aRoute)
        {
            return str(boost::format("route to %s is via %s") % str(aRoute.network) % aRoute.gateway);
        }
        inline std::string str(const IPv4Route& aRoute)  { return str_impl<IPv4Route>(aRoute); }

        template <class Routes>
        std::string str_impl(const Routes& aRoutes, const std::string& aSep)
        {
            std::string myRetVal;
            for (size_t i = 0, size = aRoutes.size(); i < size; ++i)
            {
                myRetVal += str(aRoutes[i]);
                if (i < size-1)
                {
                    myRetVal += aSep;
                }
            }
            return myRetVal;
        }
        inline std::string str(const IPv4Routes& aRoutes, const std::string& aSep = ", ") { return str_impl<IPv4Routes>(aRoutes, aSep); }

        /**
          Retrieve effective custom IPv4 routes skipping the default one
         */
        IPv4Routes getIpv4CustomRoutes(const std::string& anIfaceName);
        IfacesIPv4Routes getIpv4CustomRoutes();

        /**
          Effectuate custom IPv4 routes and make the changes persistent across reboots
          @note to make the changes persistent across reboots you should make sure that
          the supplied routes creation script aSaveScriptPath is executed on the system boot
          (e.g. sourced from /etc/rc.local)
         */
        void applyIpv4CustomRoutes(const std::string& anIfaceName, const IPv4Routes& aRoutes, const std::string& aSaveScriptPath);
        void applyIpv4CustomRoutes(const IfacesIPv4Routes& aRoutes, const std::string& aSaveScriptPath);
#endif

        bool isValidIpv4(const std::string& anAddr);

        /**
        Return whether the given IPv6 address is loopback address
        @throw std::exception if the provided address is not a correct IPv4 address
        */
        bool isLoopbackIpv4(const std::string& anAddr);

        bool isValidIpv4NetMask(const std::string& aNetMask);

        // Convert prefix length of IPv4 subnet mask (CIDR notation) to dot-decimal form
        // 24 => "255.255.255.0"
        std::string convIpv4CidrNetmaskToDotDecimal(const unsigned int aPrefixLen);

#ifndef _WIN32
        bool isLoopback(const std::string& anIfaceName);
        std::string getLoopbackIfName();
#endif

        bool isValidIpv6(const std::string& anAddr);

        /**
        Return whether the given IPv6 address is loopback address
        @throw std::exception if the provided address is not a correct IPv6 address
        */
        bool isLoopbackIpv6(const std::string& anAddr);

        /**
        Return whether the given IPv6 address is link-local address
        @throw std::exception if the provided address is not a correct IPv6 address
        */
        bool isLinkLocalIpv6(const std::string& anAddr);

        /**
        Return whether the given IPv6 address contains dotted-decimal IPv4 at the end e.g. ::192.168.1.1 or fd7c::192.168.1.1 or 2001:db8::FFFF:192.168.1.1
        @param[out] aCanonicalIpv6 when the function return true, this value contains the provided IPv6 converted to the canonocal (hexadecimal) notation
        @throw std::exception if the provided address is not a correct IPv6 address
        */
        bool isDotDecimalIpv4EmbeddedInIpv6(const std::string& anAddr, std::string& aCanonicalIpv6);

        /**
          Validate IPv6 prefix length

          @param[in] aPrefixLength IPv6 prefix length
          @return true if valid
          @throw std::runtime_error on system error (normally should not happen)
        */
        bool isValidIpv6PrefixLength(int aPrefixLength);

        enum Ipv6AddrType
        {
            ipv6AddrLoopback, ipv6AddrLinkLocal, ipv6AddrOther
        };

        //@return return IPv6 address type
        //@note this function supersedes the originally used inet_pton() because the latter does not understand scopeid percent notation like fe80::1%em1 or fe80::1%2
        Ipv6AddrType getIpv6AddrInfo(const std::string& anAddrStr, sockaddr_in6& anAddr, bool aServerUse = false);

        /**
          Validate port

          @param[in] aPort port number as string
          @param[in] aPortPtr port number as integer
          @return true if valid
        */
        bool isValidPort(const std::string& aPort, unsigned int* aPortPtr = NULL);

        /**
        Validate port

        @param[in] aPort port number
        @return true if valid
        */
        bool isValidPort(const int aPort);

        struct LocalAddress
        {
            LocalAddress(const IP& anIp, int aPort) : ip(anIp), port(aPort) {};
            LocalAddress() : port(0) {};

            IP	ip;
            int	port;

            inline bool operator==(const LocalAddress& rhs) const
            {
                return (ip == rhs.ip) && (port == rhs.port);
            }
        };
        inline std::string str(const LocalAddress& anAddress)
        {
            return str(boost::format("IP %s, port %d") % str(anAddress.ip) % anAddress.port);
        }

        struct RemoteAddress
        {
            RemoteAddress()
                : port(0)
            {};
            RemoteAddress(const std::string& aHost, const int aPort)
                : host(aHost), port(aPort)
            {
                if (host == "0.0.0.0")
                    host = "127.0.0.1";
                else if (host == "::")
                    host = "::1";
            };
            RemoteAddress(const LocalAddress& aLocalAddress)
                : host(!aLocalAddress.ip.ipv6.empty() ? aLocalAddress.ip.ipv6 : aLocalAddress.ip.ipv4), port(aLocalAddress.port)
            {
                if (host == "0.0.0.0")
                    host = "127.0.0.1";
                else if (host == "::")
                    host = "::1";
            };

            std::string	host;	///< host name or IP
            int			port;

            inline bool operator==(const RemoteAddress& rhs) const
            {
                return (boost::iequals(rhs.host,host) && rhs.port == port);
            }
            inline bool operator==(const LocalAddress& rhs) const
            {
                RemoteAddress ra;
                ra = rhs;
                return (*this == ra);
            }
            inline RemoteAddress& operator=(const LocalAddress& rhs)
            {
                host = (!rhs.ip.ipv6.empty()) ? rhs.ip.ipv6 : rhs.ip.ipv4;
                if (host == "0.0.0.0")
                    host = "127.0.0.1";
                else if (host == "::")
                    host = "::1";
                port = rhs.port;
                return *this;
            }
        };
        // just for pretty-printing, use toString() for more strict formatting
        inline std::string str(const RemoteAddress& anAddress)
        {
            return str(boost::format("host %s, port %d") % anAddress.host % anAddress.port);
        }

        // @param[in] aHost is <host>[:port] where host is either FQDN or IPv4 or IPv6.
        // username or password are removed from host if present
        // IPv6 address must be surrounded with brackets as "[fd7c:1111:1111:10::110]" or "[fd7c:1111:1111:10::110]:8080"
        // @param[in] aDefaultPort port to use when port is not present as a part of host; when set to NoDefaultPort, port MUST be present in host otherwise exception is thrown
        // @throws std::exception
        //
        static const int NoDefaultPort = -1;
        RemoteAddress parseHost(const std::string& aHost, int aDefaultPort = 80);

        // format remote address as string
        // @param[in] aRemovePort when matches port passed in anAddr argument, port will NOT appear in the resulted format string.
        // Default value -1 is set to invalid port to make sure that port always appear in the resulted string by default.
        std::string toString(const RemoteAddress& anAddr, int aRemovePort = -1);

        int getLastError();
        std::string getLastErrorStr();

#ifndef _WIN32
        bool ping(const std::string& aAddr, int aCount = 2, int aMaxWait = 2);
        bool ping6(const std::string& aAddr);

        enum ConnectivityStatus
        {
            connectivityOk, // Internet connectivity is present and DNS works
            connectivityDnsError, // DNS problem
            connectivityDefGwNotConfigured, // Default gateway is not configured
            connectivityTcpServersNotAccessible, // The indicated test servers are not accessible (firewall?)
            connectivityInternetServersNotPingable // The known Internet servers cannot be pinged
        };
        const std::vector<RemoteAddress> DefaultTestTcpServers = boost::assign::list_of(RemoteAddress("google.com", 80))(RemoteAddress("google.com", 443));
        ConnectivityStatus checkConnectivity(const std::vector<RemoteAddress>& aTestTcpServers = DefaultTestTcpServers);
#endif

        std::string normalizeDomainName(const std::string& aDomainName);
        bool isValidDomainName(const std::string& aDomainName, DomainNameValidationResult& aValidationResult, const DomainNameType aDomainNameType);

#ifdef _WIN32
        // Retrieves fully qualified DNS name that uniquely identifies the local computer such as test.keytalk.com
        std::string getSelfFqdn();
#endif
    } // namespace NetUtils

} // namespace ta

namespace boost
{
    namespace serialization
    {
        template<class Archive>
        void serialize(Archive& ar, ta::NetUtils::RemoteAddress& aRemoteAddress, const unsigned int UNUSED(version))
        {
            ar & aRemoteAddress.host;
            ar & aRemoteAddress.port;
        }
    }
}
