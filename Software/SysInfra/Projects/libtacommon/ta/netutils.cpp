#include "netutils.h"
#include "url.h"
#include "strings.h"
#include "process.h"
#include "utils.h"
#include "scopedresource.hpp"
#include "ta/logger.h"
#include "common.h"

#ifdef _WIN32
# include <winsock2.h>
# include <Ws2tcpip.h>
# include <Rpc.h>
# include <Iphlpapi.h>
# include <memory>
#elif defined(__linux__)
# include <sys/types.h>
# include <arpa/inet.h>
# include <sys/socket.h>
# include <sys/ioctl.h>
# include <sys/types.h>
# include <unistd.h>
# include <netdb.h>
# include <net/if.h>
# include <netinet/in.h>
# include <ifaddrs.h>
# include <cstdlib>
# include <cstring>
# include <cstdio>
# include <fcntl.h>
# include <errno.h>
# include <sstream>
#else
# error "Unsupported platform"
#endif
#include <memory>
#include <vector>
#include "boost/format.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/tokenizer.hpp"
#include "boost/regex.hpp"

using std::string;
using std::vector;

namespace ta
{
    namespace NetUtils
    {
        // Private stuff
        namespace
        {
            enum IpType { IPV4, IPV6 };

#ifdef __linux__
            const string NetIfacesConfigFilePath = "/etc/network/interfaces";
            const string LoopbackIfaceName       = "lo";

            bool isIfaceConfigBlockStarted(const string& anIfaceName, const string& aLine)
            {
                const boost::regex regex(str(boost::format("auto %1%|allow-hotplug %1%|iface\\s+%1%\\s+inet(6)?\\s+\\w+") % ta::regexEscapeStr(anIfaceName)));
                boost::cmatch match;
                return regex_match(aLine.c_str(), match, regex);
            }

            bool isIfaceConfigBlockFinished(const string& aLine)
            {
                return boost::trim_copy(aLine).empty();
            }

            bool isGatewaySetting(const IpType anIpType, const string& aLine)
            {
                static const boost::regex regex("\\s*gateway\\s+(?<ip>[0-9a-fA-F\\:\\.]+)");
                boost::cmatch match;
                if (regex_match(aLine.c_str(), match, regex))
                {
                    const string myIp = match["ip"];
                    if (anIpType == IPV4 && isValidIpv4(myIp))
                    {
                        return true;
                    }
                    if (anIpType == IPV6 && isValidIpv6(myIp))
                    {
                        return true;
                    }
                }
                return false;
            }

            bool isIfaceConfigBlockHeader(const string& anIfaceName, const IpType anIpType, const string& aLine)
            {
                const boost::regex regex(str(boost::format("iface\\s+%1%\\s+%2%\\s+\\w+") % ta::regexEscapeStr(anIfaceName) % (anIpType == IPV4 ? "inet" : "inet6")));
                boost::cmatch match;
                return regex_match(aLine.c_str(), match, regex);
            }

            bool existIfaceWithAutoConfiguration(const IpType anIpType, const string& anIgnoredIfaceName = "")
            {
                foreach (const Iface& iface, getMyIfaces(skipLoopBackYes))
                {
                    const string myIfaceName = iface.first;
                    if (!anIgnoredIfaceName.empty() && myIfaceName == anIgnoredIfaceName)
                    {
                        continue;
                    }

                    const boost::tuple<IfaceConfigType::val, IfaceConfigType::val> myIfConfigType = getNetIfaceConfigType(myIfaceName);
                    if (anIpType == IPV4 && boost::get<0>(myIfConfigType) == IfaceConfigType::Auto)
                    {
                        return true;
                    }
                    if (anIpType == IPV6 && boost::get<1>(myIfConfigType) == IfaceConfigType::Auto)
                    {
                        return true;
                    }
                }
                return false;
            }

            void validateIpSettings(const Iface& anIface, IfaceConfigType::val anIPv4IfaceConfigType, IfaceConfigType::val anIPv6IfaceConfigType)
            {
                //
                // IPv4
                //
                if (anIPv4IfaceConfigType != IfaceConfigType::Auto && anIPv4IfaceConfigType != IfaceConfigType::Manual )
                {
                    TA_THROW_MSG(std::invalid_argument, boost::format("Unsupported IPv4 configuration type %d supplied for interface %s") % anIPv4IfaceConfigType % anIface.first);
                }
                if (anIPv4IfaceConfigType == IfaceConfigType::Manual)
                {
                    const IPv4 ipv4 = anIface.second.ipv4;
                    if (!isValidIpv4(ipv4.addr))
                    {
                        TA_THROW_MSG(std::invalid_argument, boost::format("Invalid IPv4 address '%s' supplied for interface %s") % ipv4.addr % anIface.first);
                    }
                    if (!isValidIpv4NetMask(ipv4.netmask))
                    {
                        TA_THROW_MSG(std::invalid_argument, boost::format("Invalid IPv4 netmask '%s' supplied for interface %s") % ipv4.netmask % anIface.first);
                    }
                }

                //
                // IPv6
                //
                if (anIPv6IfaceConfigType != IfaceConfigType::Manual)
                {
                    TA_THROW_MSG(std::invalid_argument, boost::format("Unsupported IPv6 configuration type %d supplied for interface %s") % anIPv6IfaceConfigType % anIface.first);
                }
                foreach (const IPv6& ipv6, anIface.second.ipsv6)
                {
                    if (hasDuplicates(anIface.second.ipsv6))
                    {
                        TA_THROW_MSG(std::invalid_argument, boost::format("Duplicate IPv6 addresses found among '%s' supplied for interface %s") % str(anIface.second.ipsv6) % anIface.first);
                    }
                    if (!isValidIpv6(ipv6.addr))
                    {
                        TA_THROW_MSG(std::invalid_argument, boost::format("Invalid IPv6 address '%s' supplied for interface %s") % ipv6.addr % anIface.first);
                    }
                    if (!isValidIpv6PrefixLength(ipv6.prefixlen))
                    {
                        TA_THROW_MSG(std::invalid_argument, boost::format("Invalid IPv6 prefix length %d supplied for interface %s") % ipv6.prefixlen % anIface.first);
                    }
                }
            }

            // read the network interface configuration file, skipping configuration blocks of the given interface
            string readNetIfaceConfigSkippingIfaceConfigBlocks(const string& anIfaceName)
            {
                std::ifstream myFile(NetIfacesConfigFilePath.c_str());
                if (!myFile.is_open() || myFile.fail())
                {
                    TA_THROW_MSG(std::runtime_error, boost::format("Failed to open '%1%' for reading") % NetIfacesConfigFilePath);
                }

                string myContents, line;
                bool myInsideIfaceConfigBlock = false;

                while (std::getline(myFile, line))
                {
                    if (myInsideIfaceConfigBlock)
                    {
                        myInsideIfaceConfigBlock = !isIfaceConfigBlockFinished(line);
                        // skip config block lines
                    }
                    else
                    {
                        if (isIfaceConfigBlockStarted(anIfaceName, line))
                        {
                            myInsideIfaceConfigBlock = true;
                            // skip config block lines
                        }
                        else
                        {
                            myContents += line + "\n";
                        }
                    }
                }
                return boost::trim_copy(myContents);
            }

            string readNetIfaceConfigSkippingDefaultGateway(const IpType anIpType)
            {
                std::ifstream myFile(NetIfacesConfigFilePath.c_str());
                if (!myFile.is_open() || myFile.fail())
                {
                    TA_THROW_MSG(std::runtime_error, boost::format("Failed to open '%1%' for reading") % NetIfacesConfigFilePath);
                }

                string myContents, line;
                while (std::getline(myFile, line))
                {
                    if (!isGatewaySetting(anIpType, line))
                    {
                        myContents += line + "\n";
                    }
                }
                return boost::trim_copy(myContents);
            }

            string addDefaultIpv4Gateway(const string& aNetIfacesConfigContents, const DefGateway& aDefGateway)
            {
                string myResult;
                foreach (const string& line, Strings::split(aNetIfacesConfigContents, '\n'))
                {
                    myResult += line + "\n";

                    if (isIfaceConfigBlockHeader(aDefGateway.iface, IPV4, line))
                    {
                        myResult += str(boost::format("    gateway %s\n") % aDefGateway.ip);
                    }
                }
                return myResult;
            }

            void saveIpSettings(const Iface& anIface,
                                IfaceConfigType::val anIPv4IfaceConfigType,
                                IfaceConfigType::val anIPv6IfaceConfigType)
            {
                string myNetIfacesConfigContents = ta::readData(NetIfacesConfigFilePath);
                DEBUGDEVLOG(boost::format("Initial contents of %s:\n%s") % NetIfacesConfigFilePath % myNetIfacesConfigContents);

                const DefGateway myIpv4DefGateway = getDefIpv4Gateway();

                const string myIfaceName = anIface.first;
                myNetIfacesConfigContents = readNetIfaceConfigSkippingIfaceConfigBlocks(myIfaceName);

                // append new interface configuration to the end of file
                myNetIfacesConfigContents += "\n\n";
                myNetIfacesConfigContents += str(boost::format("auto %s\n") % myIfaceName);
                myNetIfacesConfigContents += str(boost::format("allow-hotplug %s\n") % myIfaceName);

                // add IPv4 configuration
                if (anIPv4IfaceConfigType == IfaceConfigType::Auto)
                {
                    DEBUGLOG("Applying IPv4 DHCP settings for " + myIfaceName);
                    myNetIfacesConfigContents += str(boost::format("iface %s inet dhcp\n") % myIfaceName);
                }
                else
                {
                    DEBUGLOG(boost::format("Applying IPv4 static settings for %s. Setting IP to %s") % myIfaceName % str(anIface.second.ipv4));
                    myNetIfacesConfigContents += str(boost::format("iface %s inet static\n") % myIfaceName);
                    myNetIfacesConfigContents += str(boost::format("    address %s\n") % anIface.second.ipv4.addr);
                    myNetIfacesConfigContents += str(boost::format("    netmask %s\n") % anIface.second.ipv4.netmask);
                    // keep default gateway if there are no DHCP interfaces around
                    if (myIpv4DefGateway.defined() && myIfaceName == myIpv4DefGateway.iface && !existIfaceWithAutoConfiguration(IPV4, myIfaceName))
                    {
                        myNetIfacesConfigContents += str(boost::format("    gateway %s\n") % myIpv4DefGateway.ip);
                    }
                }

                // add IPv6 configuration
                if (anIPv6IfaceConfigType == IfaceConfigType::Manual)
                {
                    foreach (const IPv6& ipv6, anIface.second.ipsv6)
                    {
                        if (!isLoopbackIpv6(ipv6.addr) && !isLinkLocalIpv6(ipv6.addr))
                        {
                            DEBUGLOG(boost::format("Applying IPv6 static settings for %s. Setting IP to %s") % myIfaceName % str(ipv6));
                            myNetIfacesConfigContents += "\n";
                            myNetIfacesConfigContents += str(boost::format("iface %s inet6 static\n") % myIfaceName);
                            myNetIfacesConfigContents += str(boost::format("    address %s\n") % ipv6.addr);
                            myNetIfacesConfigContents += str(boost::format("    netmask %d\n") % ipv6.prefixlen);
                        }
                    }
                }

                // save
                DEBUGDEVLOG(boost::format("New contents of %s:\n%s") % NetIfacesConfigFilePath % myNetIfacesConfigContents);
                ta::writeData(NetIfacesConfigFilePath, myNetIfacesConfigContents);
            }

            // @pre all non-loopback interfaces should have manual (non-DHCP) setting
            //@return the original configuration
            std::string saveDefaultIpv4Gateway(const DefGateway& aDefGateway)
            {
                if (existIfaceWithAutoConfiguration(IPV4))
                {
                    TA_THROW_MSG(std::runtime_error, "Cannot set IPv4 default gateway because at least one network interface has automatic (DHCP) configuration");
                }

                const string myOrigNetIfacesConfigContents = ta::readData(NetIfacesConfigFilePath);
                DEBUGDEVLOG(boost::format("Initial contents of %s:\n%s") % NetIfacesConfigFilePath % myOrigNetIfacesConfigContents);

                // remove current default gateway
                string myNewNetIfacesConfigContents = readNetIfaceConfigSkippingDefaultGateway(IPV4);

                //
                // apply new default gateway
                //
                if (aDefGateway.defined())
                {
                    DEBUGLOG("Setting IPv4 default gateway to " + str(aDefGateway));
                    myNewNetIfacesConfigContents = addDefaultIpv4Gateway(myNewNetIfacesConfigContents, aDefGateway);
                }
                else
                {
                    DEBUGLOG("Removing IPv4 default gateway");
                }

                DEBUGDEVLOG(boost::format("New contents of %s:\n%s") % NetIfacesConfigFilePath % myNewNetIfacesConfigContents);
                ta::writeData(NetIfacesConfigFilePath, myNewNetIfacesConfigContents);

                return myOrigNetIfacesConfigContents;
            }

            // Validate and normalize custom routes
            //@return normalized routes without duplicates
            IPv4Routes normalizeCustomIpv4Routes(const IPv4Routes& aRoutes)
            {
                IPv4Routes myNormalizedRoutes;

                foreach (const IPv4Route& route, ta::removeDuplicates(aRoutes))
                {
                    const string myAddr = boost::trim_copy(route.network.addr);
                    if (!isValidIpv4(myAddr))
                    {
                        TA_THROW_MSG(std::invalid_argument, boost::format("Invalid network IPv4 network '%s'") % myAddr);
                    }

                    const string myNetmask = boost::trim_copy(route.network.netmask);
                    if (!isValidIpv4NetMask(myNetmask))
                    {
                        TA_THROW_MSG(std::invalid_argument, boost::format("Invalid IPv4 netmask '%s'") % myNetmask);
                    }

                    const string myGw = boost::trim_copy(route.gateway);
                    if (!isValidIpv4(myGw))
                    {
                        TA_THROW_MSG(std::invalid_argument, boost::format("Invalid IPv4 gateway '%s'") % myGw);
                    }

                    myNormalizedRoutes.push_back(IPv4Route(IPv4(myAddr, myNetmask), myGw));
                }
                return myNormalizedRoutes;
            }

            // Validates and normalize default gateway
            //@return normalized gateway
            DefGateway normalizeIpv4DefGateway(const DefGateway& aDefGateway)
            {
                const string myGwIfacName = boost::trim_copy(aDefGateway.iface);
                const string myGwIp = boost::trim_copy(aDefGateway.ip);

                if (!myGwIp.empty() && !isValidIpv4(myGwIp))
                {
                    TA_THROW_MSG(std::invalid_argument, boost::format("'%s' is not valid Ipv4 address (to be used for default gateway)") % myGwIp);
                }
                return DefGateway(myGwIfacName, myGwIp);
            }

            string serializeIpv4Routes(const IfacesIPv4Routes& aRoutes)
            {
                string myRetVal;
                foreach (const IfaceIPv4Routes& routes, aRoutes)
                {
                    foreach (const IPv4Route& route, routes.second)
                    {
                        myRetVal += str(boost::format("ip -4 route replace %s/%s via %s dev %s\n") % route.network.addr % route.network.netmask % route.gateway % routes.first);
                    }
                }
                return myRetVal;
            }

            // Combines the current system routes with the supplied routes by replacing the routes for the given interface and keeping the routes for other interfaces
            IfacesIPv4Routes combineCurrentIpv4Routes(const string& anIfaceName, const IPv4Routes& aRoutes)
            {
                IfacesIPv4Routes myCombinedRoutes = getIpv4CustomRoutes();
                myCombinedRoutes[anIfaceName] = aRoutes;
                return myCombinedRoutes;
            }

            // @return old configuration
            template <class Routes>
            std::string saveIpv4CustomRoutes(const Routes& aRoutes, const string& aSaveScriptPath)
            {
                const bool mySaveScriptExist = ta::isFileExist(aSaveScriptPath);
                const string myContentsOrig = mySaveScriptExist ? (string)ta::readData(aSaveScriptPath) : "";
                const string myContentsNew = serializeIpv4Routes(aRoutes);
                DEBUGDEVLOG(boost::format("Initial contents of %s:\n%s") % aSaveScriptPath % (mySaveScriptExist ? myContentsOrig : "<file not exist>"));
                DEBUGDEVLOG(boost::format("New contents of %s:\n%s") % aSaveScriptPath % myContentsNew);
                ta::writeData(aSaveScriptPath, myContentsNew);
                return myContentsOrig;
            }

            //@nothrow
            void tryRestoreCustomIpv4Routes(const string& anOrigPersistentConfiguration, const IPv4Routes& anOrigRoutes, const string& aSaveScriptPath)
            {
                try {
                    ta::writeData(aSaveScriptPath, anOrigPersistentConfiguration);
                } catch (std::exception& e) {
                    WARNLOG2("Failed to restore the original IPv4 persistent custom routing configuration", e.what());
                }

                foreach (const IPv4Route& route, anOrigRoutes)
                {
                    try {
                        Process::checkedShellExecSync(str(boost::format("sudo ip -4 route delete %s/%s") % route.network.addr % route.network.netmask));
                    } catch (std::exception& e) {
                        WARNLOG2("Failed to restore the original IPv4 custom routing configuration", e.what());
                    }
                }
                foreach (const IPv4Route& route, anOrigRoutes)
                {
                    try {
                        Process::checkedShellExecSync(str(boost::format("sudo ip -4 route replace %s/%s via %s") % route.network.addr % route.network.netmask % route.gateway));
                    } catch (std::exception& e) {
                        WARNLOG2("Failed to restore the original IPv4 custom routing configuration", e.what());
                    }
                }
            }

            //@nothrow
            void tryRestoreIpv4DefGateway(const string& anOrigPersistentConfiguration, const DefGateway& aDefGateway)
            {
                try
                {
                    ta::writeData(NetIfacesConfigFilePath, anOrigPersistentConfiguration);
                }
                catch (std::exception& e)
                {
                    WARNLOG2("Failed to restore the original IPv4 persistent default gateway configuration", e.what());
                }

                if (aDefGateway.defined())
                {
                    try
                    {
                        Process::checkedShellExecSync(str(boost::format("sudo ip -4 route replace default via %s dev %s") % aDefGateway.ip % aDefGateway.iface));
                    }
                    catch (std::exception& e)
                    {
                        WARNLOG2("Failed to restore the original IPv4 default gateway configuration", e.what());
                    }
                }
                else if (getDefIpv4Gateway().defined())
                {
                    try
                    {
                        Process::checkedShellExecSync("sudo ip -4 route delete default");
                    }
                    catch (std::exception& e)
                    {
                        WARNLOG2("Failed to restore the original IPv4 default gateway configuration", e.what());
                    }
                }
            }

            void effectuateSavedIpSettings(const string& anIfaceName)
            {
                // flush existing IP info first
                if (isKeyExist(anIfaceName, getMyIfaces()))
                {
                    try {
                        ta::Process::checkedShellExecSync(str(boost::format("sudo ip address flush dev %s") % anIfaceName));
                    } catch (std::exception& e) {
                        WARNLOG(boost::format("Failed to flush IP information for interface %s. %s. Tolerating the error.") % anIfaceName % e.what());
                    }
                }

                // apply the previously saved settings
                try {
                    ta::Process::checkedShellExecSync("sudo service networking restart");
                } catch (std::exception& e) {
                    // add extra diagnostics
                    string myStdOut, myStdErr;
                    ta::Process::shellExecSync("sudo service networking status", myStdOut, myStdErr);
                    TA_THROW_MSG(std::runtime_error, boost::format("%s. 'service networking status' says: stdout: %s. stderr: %s") % e.what() % myStdOut % myStdErr);
                }
            }

            size_t hexStr2Number(const string& aHexStr)
            {
                size_t parsed;
                std::stringstream ss;
                ss << std::hex << aHexStr;
                ss >> parsed;
                return parsed;
            }

            string normalizeIpv6(const string& anIpv6, const string& anIfaceNameHint)
            {
                // convert ipv6 to binary form and back

                struct sockaddr_in sa = {0};
                int myRet = inet_pton(AF_INET6, anIpv6.c_str(), &sa.sin_addr);
                if (myRet == 0)
                {
                    TA_THROW_MSG(std::runtime_error, boost::format("Cannot normalize IPv6 address %s for interface %s. inet_pton() says the given IPv6 address is invalid")% anIpv6 %  anIfaceNameHint);
                }
                else if (myRet != 1)
                {
                    TA_THROW_MSG(std::runtime_error, boost::format("Cannot normalize IPv6 address %s for interface %s. inet_pton() says: %s") % anIpv6 % anIfaceNameHint % strerror(errno));
                }

                char normalizedIpv6[INET6_ADDRSTRLEN] = {};
                if (!inet_ntop(AF_INET6, &sa.sin_addr, normalizedIpv6, sizeof(normalizedIpv6)))
                {
                    TA_THROW_MSG(std::runtime_error, boost::format("Cannot normalize IPv6 address %s for interface %s. inet_ntop() says: %s") % anIpv6 % anIfaceNameHint % strerror(errno));
                }

                return normalizedIpv6;
            }

            size_t parseIpv6PrefixLength(const string& aPrefixLenHex, const string& anIfaceNameHint)
            {
                size_t myPrefixLen = 0;
                try {
                    myPrefixLen = hexStr2Number(aPrefixLenHex);
                } catch (std::exception& e) {
                    TA_THROW_MSG(std::runtime_error, boost::format("Cannot parse IPv6 prefix length from %s for interface %s. %s") % aPrefixLenHex % anIfaceNameHint % e.what());
                }
                if (!isValidIpv6PrefixLength(myPrefixLen))
                {
                    TA_THROW_MSG(std::runtime_error, boost::format("Invalid IPv6 prefix length %d for interface %s") % myPrefixLen % anIfaceNameHint);
                }
                return myPrefixLen;
            }

            string parseIpv6(const string& anIpv6Str, const string& anIfaceName)
            {
                if (anIpv6Str.size() != 32)
                {
                    TA_THROW_MSG(std::runtime_error, boost::format("Cannot parse IPv6 from %s for interface %s. Invalid length.") % anIpv6Str % anIfaceName);
                }

                // parse
                vector<size_t> myIpv6Parts(8);
                try
                {
                    size_t myPartPos = 0;
                    foreach (size_t& part, myIpv6Parts)
                    {
                        part = hexStr2Number(anIpv6Str.substr(myPartPos, 4));
                        myPartPos += 4;
                    }
                }
                catch (std::exception& e)
                {
                    TA_THROW_MSG(std::runtime_error, boost::format("Cannot parse IPv6 address from %s for interface %s. %s") % anIpv6Str % anIfaceName % e.what());
                }

                // format
                string myIpv6;
                foreach (const size_t part, myIpv6Parts)
                {
                    const string partHex = str(boost::format("%x") % part);
                    myIpv6 += (myIpv6.empty()) ? partHex : ":" + partHex;
                }

                myIpv6 = normalizeIpv6(myIpv6, anIfaceName);
                if (isLinkLocalIpv6(myIpv6))
                {
                    myIpv6 += "%" + anIfaceName; // or if_nametoindex()
                }
                return myIpv6;
            }

#endif
        } // end of private stuff


        //
        // Public stuff
        //

        SockInitializer::SockInitializer()
        {
#ifdef _WIN32
            WSADATA myWd;
            WORD myVersionRequested = MAKEWORD(2, 2);
            if (WSAStartup(myVersionRequested, &myWd) != 0)
            {
                throw std::runtime_error(str(boost::format("Failed to initialize sockets. Last error is %d") % ::WSAGetLastError()));
            }
#endif
        }
        SockInitializer::~SockInitializer()
        {
#ifdef _WIN32
            ::WSACleanup();
#endif
        }

        Ipv6AddrType getIpv6AddrInfo(const std::string& anAddrStr, sockaddr_in6& anAddr, bool aServerUse)
        {
            SockInitializer mySockSockInitializer;
            memset(&anAddr, 0, sizeof(anAddr));
            anAddr.sin6_family = AF_INET6;

            struct addrinfo hints, *res;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family   = AF_INET6;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;
            hints.ai_flags = AI_NUMERICHOST;
            if (aServerUse)
                hints.ai_flags |= AI_PASSIVE;

            int myRet = getaddrinfo(anAddrStr.c_str(), NULL, &hints, &res);
            if (myRet != 0)
            {
                throw std::runtime_error(str(boost::format("Failed to parse IPv6 from %s. %s") % anAddrStr % gai_strerror(myRet)));
            }
            memcpy(&anAddr, res->ai_addr, res->ai_addrlen);
            freeaddrinfo(res);

            if (IN6_IS_ADDR_LOOPBACK(&anAddr.sin6_addr))
            {
                return ipv6AddrLoopback;
            }
            if (IN6_IS_ADDR_LINKLOCAL(&anAddr.sin6_addr))
            {
                return ipv6AddrLinkLocal;
            }
            return ipv6AddrOther;
        }

        string getPrimaryMacAddress()
        {
            string myRetVal;
#ifdef _WIN32
            std::auto_ptr<IP_ADAPTER_INFO> myAdapterInfo(static_cast<IP_ADAPTER_INFO*>(::operator new(sizeof(IP_ADAPTER_INFO))));
            ULONG myOutBufLen = sizeof(IP_ADAPTER_INFO);
            DWORD myErrCode = ::GetAdaptersInfo(myAdapterInfo.get(),&myOutBufLen);
            if (myErrCode == ERROR_BUFFER_OVERFLOW)
            {
                myAdapterInfo.reset(static_cast<IP_ADAPTER_INFO*>(::operator new(myOutBufLen)));
                myErrCode = ::GetAdaptersInfo(myAdapterInfo.get(),&myOutBufLen);
            }
            if (myErrCode != ERROR_SUCCESS)
                TA_THROW_MSG(std::runtime_error, boost::format("GetAdaptersInfo failed with error code %u") % myErrCode);
            if (myAdapterInfo->AddressLength != 6)
                TA_THROW_MSG(std::runtime_error, boost::format("Invalid address length. Expected: 6, actual: %d") % myAdapterInfo->AddressLength);
            // MS$ claims GetAdaptersInfo does not retrieve loobback interfaces
            myRetVal = Strings::toHex(&myAdapterInfo->Address[0], myAdapterInfo->AddressLength);
#elif defined(__linux__)
            ///@todo: is this indeed *primary* MAC? What is primary MAC in Linux?
            // Note: to filter out non-physical ifaces, for each interface from /proc/net/dev, check if the folder /sys/class/net/<interface>/device exists. If so, get MAC from /sys/class/net/<interface>/address.
            int mySocket = socket(AF_INET, SOCK_DGRAM, 0);
            if (mySocket == -1)
            {
                TA_THROW_MSG(std::runtime_error, "Failed to create socket");
            }
            char buf[1024];
            ifconf ifc;
            ifc.ifc_len = sizeof(buf);
            ifc.ifc_buf = buf;
            ioctl(mySocket, SIOCGIFCONF, &ifc);

            ifreq ifr;
            ifreq* IFR = ifc.ifc_req;
            bool myIsFound = false;
            for (int i = ifc.ifc_len / sizeof(struct ifreq); --i >= 0; IFR++)
            {
                strcpy(ifr.ifr_name, IFR->ifr_name);
                if (ioctl(mySocket, SIOCGIFFLAGS, &ifr) == 0)
                {
                    if (!(ifr.ifr_flags & IFF_LOOPBACK))
                    {
                        if (ioctl(mySocket, SIOCGIFHWADDR, &ifr) == 0)
                        {
                            myIsFound = true;
                            break;
                        }
                    }
                }
            }
            close(mySocket);
            if (!myIsFound)
            {
                TA_THROW_MSG(std::runtime_error, "MAC not found");
            }
            unsigned char myAddressBuf[6];
            memcpy( myAddressBuf, ifr.ifr_hwaddr.sa_data, sizeof(myAddressBuf));
            myRetVal = Strings::toHex(myAddressBuf, sizeof(myAddressBuf));
#endif
            return boost::to_upper_copy(myRetVal);
        }

        string getFormattedPrimaryMacAddress()
        {
            const string myMac = getPrimaryMacAddress();
            string myFormattedMac;
            for (size_t i = 0; i < myMac.size(); ++i)
            {
                myFormattedMac += myMac[i];
                if (i !=  myMac.size()-1 && (i+1) % 2 == 0)
                    myFormattedMac += ":";
            }
            return myFormattedMac;
        }

        vector<string> getMyIpv4()
        {
            vector<string> myIPs;
            SockInitializer mySockSockInitializer;
#ifdef _WIN32
            ScopedResource<SOCKET> mySock(socket(AF_INET,SOCK_DGRAM,0), closesocket, INVALID_SOCKET);
            if (mySock == INVALID_SOCKET)
            {
                DWORD myLastError = ::WSAGetLastError();
                TA_THROW_MSG(IpResolveError, boost::format("socket() failed. Last error is %d") % myLastError);
            }
            static const unsigned short MaxNumOfIfaces = 64;
            INTERFACE_INFO myIfaceInfo[MaxNumOfIfaces];
            DWORD myBytes;
            if (::WSAIoctl(mySock, SIO_GET_INTERFACE_LIST, 0, 0, myIfaceInfo, sizeof(myIfaceInfo), &myBytes, 0, 0) == SOCKET_ERROR)
            {
                TA_THROW_MSG(IpResolveError, boost::format("WSAIoctl(SIO_GET_INTERFACE_LIST) failed. Last error is %d") % ::WSAGetLastError());
            }
            const int myNumOfIfaces = myBytes / sizeof(INTERFACE_INFO);
            for (int i = 0; i < myNumOfIfaces; ++i)
            {
                LPINTERFACE_INFO lpii = &myIfaceInfo[i];
                if (!(lpii->iiFlags & IFF_UP))
                    continue;
                if (lpii->iiFlags & IFF_LOOPBACK)
                    continue;

                sockaddr_in* myAddrIn = reinterpret_cast<struct sockaddr_in*> (&lpii->iiAddress.AddressIn);
                if (myAddrIn->sin_addr.s_addr == INADDR_ANY)
                    continue;
                if (myAddrIn->sin_family != AF_INET)
                    continue;

                char mySzIp[16];
                if (!inet_ntop(AF_INET, &myAddrIn->sin_addr, mySzIp, sizeof(mySzIp)))
                    TA_THROW_MSG(IpResolveError, boost::format("inet_ntop() failed with error %d") % ::WSAGetLastError());
                myIPs.push_back(mySzIp);
            }
#elif defined(__linux__)
            ScopedResource<int> mySock(socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP), close, -1);
            if (mySock < 0)
            {
                TA_THROW_MSG(IpResolveError, boost::format("socket() failed. %s.") % strerror(errno));
            }

            char myBuf[16*sizeof(struct ifreq)];
            struct ifconf ifConf;
            struct ifreq ifReq;

            ifConf.ifc_len = sizeof myBuf;
            ifConf.ifc_ifcu.ifcu_buf = (caddr_t)myBuf;
            if (ioctl(mySock, SIOCGIFCONF, &ifConf) < 0)
            {
                TA_THROW_MSG(IpResolveError, boost::format("ioctl(SIOCGIFCONF) failed. %s.") % strerror(errno));
            }
            for (int i = 0; i < ifConf.ifc_len;)
            {
                // First cast to void* in order to silence the alignment warnings.
                struct ifreq* pifReq = (struct ifreq*)(void*)((caddr_t)ifConf.ifc_req + i);
                i += sizeof *pifReq;
                /* See if this is the sort of interface we want to deal with. */
                strcpy (ifReq.ifr_name, pifReq -> ifr_name);
                if (ioctl (mySock, SIOCGIFFLAGS, &ifReq) < 0)
                {
                    TA_THROW_MSG(IpResolveError, boost::format("ioctl(SIOCGIFFLAGS) failed. %s.") % strerror(errno));
                }
                /* Skip loopback and down interfaces, */
                if ((ifReq.ifr_flags & IFF_LOOPBACK) || (!(ifReq.ifr_flags & IFF_UP)))
                    continue;
                if (pifReq -> ifr_addr.sa_family == AF_INET)
                {
                    sockaddr_in myAddr  = {0};
                    memcpy (&myAddr, &pifReq -> ifr_addr, sizeof pifReq -> ifr_addr);
                    if (myAddr.sin_addr.s_addr != htonl (INADDR_LOOPBACK))
                    {
                        char* mySzIp = inet_ntoa(myAddr.sin_addr);
                        if (!mySzIp)
                        {
                            TA_THROW_MSG(IpResolveError, "inet_ntoa() failed");
                        }
                        myIPs.push_back(mySzIp);
                    }
                }
            }
#else
# error "Unsupported platform"
#endif
            return myIPs;
        }

#ifndef _WIN32
        DefGateway getDefIpv4Gateway()
        {
            // get the default route
            string myStdOut, myStdErr;
            const string myCommand = "ip -4 route show default" ;
            const int myExecCode = Process::shellExecSync(myCommand, myStdOut, myStdErr);
            if (myExecCode != 0)
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Command %1% finished with error code %2%. Stderr: %3%") % myCommand % myExecCode % myStdErr);
            }

            // parse the output
            boost::trim(myStdOut);
            if (myStdOut.empty())
            {
                return DefGateway();
            }
            static const boost::regex myRegEx("^\\s*default\\s+via\\s+(?<ip>[0-9a-fA-F\\:\\.]+)\\s+dev\\s+(?<device>\\w+)");
            boost::cmatch myMatch;
            if (!regex_search(myStdOut.c_str(), myMatch, myRegEx))
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Failed to parse default IPv4 gateway from '%s'") % myStdOut);
            }
            const string myIp = myMatch["ip"];
            const string myDevice = myMatch["device"];
            if (!isValidIpv4(myIp))
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Invalid default gateway IPv4 '%s'") % myIp);
            }
            return DefGateway(myDevice, myIp);
        }

        void applyDefIpv4Gateway(const DefGateway& aDefGateway)
        {
            const DefGateway myNewDefGateway = normalizeIpv4DefGateway(aDefGateway);
            const DefGateway myOrigDefGateway = getDefIpv4Gateway();
            DEBUGLOG("Applying default IPv4 gateway: " + str(aDefGateway));

            // save settings
            const string myOrigPersistentConfiguration = saveDefaultIpv4Gateway(myNewDefGateway);

            try
            {
                // effectuate the changes
                if (myNewDefGateway.defined())
                {
                    Process::checkedShellExecSync(str(boost::format("sudo ip -4 route replace default via %s dev %s") % myNewDefGateway.ip % myNewDefGateway.iface));
                }
                else if (myOrigDefGateway.defined())
                {
                    Process::checkedShellExecSync("sudo ip -4 route delete default");
                }
            }
            catch (...)
            {
                WARNLOG("Failed to effectuate IPv4 default gateway, trying to restore the original values");
                tryRestoreIpv4DefGateway(myOrigPersistentConfiguration, myOrigDefGateway);
                throw;
            }
        }

#endif // !_WIN32

#ifdef _WIN32
        Ifaces getMyIpv4faces()
        {
            Ifaces myIfaces;
            std::auto_ptr<IP_ADAPTER_INFO> myAdapterInfo(static_cast<IP_ADAPTER_INFO*>(::operator new(sizeof(IP_ADAPTER_INFO))));
            ULONG myOutBufLen = sizeof(IP_ADAPTER_INFO);
            //@note ::GetAdaptersInfo does not retrieve loobback interfaces
            DWORD myErrCode = ::GetAdaptersInfo(myAdapterInfo.get(),&myOutBufLen);
            if (myErrCode == ERROR_BUFFER_OVERFLOW)
            {
                myAdapterInfo.reset(static_cast<IP_ADAPTER_INFO*>(::operator new(myOutBufLen)));
                myErrCode = ::GetAdaptersInfo(myAdapterInfo.get(),&myOutBufLen);
            }
            if (myErrCode != ERROR_SUCCESS)
            {
                TA_THROW_MSG(std::runtime_error, boost::format("GetAdaptersInfo failed with error code %u") % myErrCode);
            }
            for (IP_ADAPTER_INFO* myAdapterInfoPtr = myAdapterInfo.get(); myAdapterInfoPtr; myAdapterInfoPtr = myAdapterInfoPtr->Next)
            {
                IfaceInfo myIfaceInfo;
                findValueByKey(myAdapterInfoPtr->AdapterName, myIfaces, myIfaceInfo);
                myIfaceInfo.ipv4 = IPv4(myAdapterInfoPtr->IpAddressList.IpAddress.String,
                                        myAdapterInfoPtr->IpAddressList.IpMask.String);
                myIfaces[myAdapterInfoPtr->AdapterName] = myIfaceInfo;
            }

            return myIfaces;
        }

        Ifaces getMyIfaces()
        {
            return getMyIpv4faces();
        }
#endif // WIN32

#ifndef _WIN32
        Ifaces getMyIpv4faces(SkipLoopBack aSkipLoopBack)
        {
            Ifaces myIfaces;
            int mySock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (mySock < 0)
            {
                TA_THROW_MSG(std::runtime_error, boost::format("socket() failed. %s") % strerror(errno));
            }
            ScopedResource<int> sock(mySock, close); // to ensure proper cleanup

            char myBuf[16*sizeof(struct ifreq)];
            struct ifconf ifConf;
            struct ifreq ifReq;
            ifConf.ifc_len = sizeof myBuf;
            ifConf.ifc_ifcu.ifcu_buf = (caddr_t)myBuf;
            if (ioctl(mySock, SIOCGIFCONF, &ifConf) < 0)
            {
                TA_THROW_MSG(std::runtime_error, boost::format("ioctl(SIOCGIFCONF) failed. %s") % strerror(errno));
            }
            for (int i = 0; i < ifConf.ifc_len;)
            {
                IfaceInfo myIfaceInfo;
                // First cast to void* in order to silence the alignment warnings.
                struct ifreq* pifReq = (struct ifreq*)(void*)((caddr_t)ifConf.ifc_req + i);
                i += sizeof *pifReq;

                findValueByKey(pifReq -> ifr_name, myIfaces, myIfaceInfo);

                strcpy (ifReq.ifr_name, pifReq -> ifr_name);
                if (ioctl (mySock, SIOCGIFFLAGS, &ifReq) < 0)
                {
                    TA_THROW_MSG(std::runtime_error, boost::format("ioctl(SIOCGIFFLAGS) failed. %s") % strerror(errno));
                }
                if (!(ifReq.ifr_flags & IFF_UP))
                {
                    continue;
                }
                if (pifReq -> ifr_addr.sa_family != AF_INET)
                {
                    continue;
                }
                if (aSkipLoopBack == skipLoopBackYes && (ifReq.ifr_flags & IFF_LOOPBACK))
                {
                    continue;
                }

                // Retrieve IPv4
                sockaddr_in myIp = {0};
                memcpy (&myIp, &pifReq -> ifr_addr, sizeof pifReq -> ifr_addr);
                char* mySzIp = inet_ntoa(myIp.sin_addr);
                if (!mySzIp)
                {
                    TA_THROW_MSG(std::runtime_error, "inet_ntoa() failed");
                }
                myIfaceInfo.ipv4.addr = mySzIp;

                // Retrieve netmask
                struct ifreq netmaskReq;
                strcpy(netmaskReq.ifr_name, pifReq->ifr_name);
                memcpy(&(netmaskReq.ifr_addr), &(myIp.sin_addr), sizeof(netmaskReq.ifr_addr));
                if (ioctl(mySock, SIOCGIFNETMASK, &netmaskReq) < 0)
                {
                    TA_THROW_MSG(std::runtime_error, boost::format("ioctl(SIOCGIFNETMASK) failed. %s") % strerror(errno));
                }
                sockaddr_in myNetmask = {0};
                memcpy (&myNetmask, &(netmaskReq.ifr_addr), sizeof(netmaskReq.ifr_addr));
                char* mySzNetmask = inet_ntoa(myNetmask.sin_addr);
                if (!mySzNetmask )
                {
                    TA_THROW_MSG(std::runtime_error, "inet_ntoa() failed");
                }
                myIfaceInfo.ipv4.netmask= mySzNetmask;
                myIfaces[pifReq->ifr_name] = myIfaceInfo;
            }
            if (myIfaces.empty())
            {
                TA_THROW_MSG(std::runtime_error, "The list of ipv4 interfaces cannot be empty (at least loopback interface expected)");
            }
            return myIfaces;
        }

        Ifaces getMyIpv6faces(SkipLoopBack aSkipLoopBack)
        {
            Ifaces myIfaces;
            string myStdOut, myStdErr;
            const int myExecCode = ta::Process::shellExecSync("cat /proc/net/if_inet6", myStdOut, myStdErr);
            if (myExecCode != 0)
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Failed to read inet 6 interfaces info. Error code %d. %s") % myExecCode % myStdErr);
            }

            static boost::regex myRegEx("^(?<address>[0-9a-fA-F]{32})\\s+(?<dev_index>[0-9a-fA-F]+)\\s+(?<prefix_length>[0-9a-fA-F]{1,2})\\s+(?<scope>[0-9a-fA-F]+)\\s+(?<flags>[0-9a-fA-F]+)\\s+(?<dev_name>[\\w]+)\\s*$");
            boost::match_results<string::const_iterator> myMatch;
            string::const_iterator myBeg = myStdOut.begin(), myEnd = myStdOut.end();

            while (regex_search(myBeg, myEnd, myMatch, myRegEx))
            {
                const string myIpv6AddrStr = myMatch["address"];
                const string myPrefixLenHex = myMatch["prefix_length"];
                const string myIfaceName = myMatch["dev_name"];
                myBeg = myMatch[0].second;

                if (aSkipLoopBack == skipLoopBackYes && isLoopback(myIfaceName))
                {
                    continue;
                }
                const IPv6 ipv6(parseIpv6(myIpv6AddrStr, myIfaceName),
                                parseIpv6PrefixLength(myPrefixLenHex, myIfaceName));

                IfaceInfo myIfaceInfo;
                findValueByKey(myIfaceName, myIfaces, myIfaceInfo);
                myIfaceInfo.ipsv6.push_back(ipv6);
                myIfaces[myIfaceName] = myIfaceInfo;
            }

            if (myIfaces.empty())
            {
                TA_THROW_MSG(std::runtime_error, "The list of ipv6 interfaces cannot be empty (at least loopback interface expected)");
            }
            return myIfaces;
        }

        Ifaces getMyIfaces(SkipLoopBack aSkipLoopBack)
        {
            Ifaces myIfaces = getMyIpv4faces(aSkipLoopBack);
            foreach (const Iface& iface, getMyIpv6faces(aSkipLoopBack))
            {
                const string myIfaceName = iface.first;
                IfaceInfo myIfaceInfo;
                ta::findValueByKey(myIfaceName, myIfaces, myIfaceInfo);
                // add new or update existing
                myIfaceInfo.ipsv6 = iface.second.ipsv6;
                myIfaces[myIfaceName] = myIfaceInfo;
            }
            return myIfaces;
        }

        boost::tuple<IfaceConfigType::val, IfaceConfigType::val> getNetIfaceConfigType(const string& anIfaceName)
        {
            if (isLoopback(anIfaceName))
            {
                return boost::make_tuple(IfaceConfigType::Auto, IfaceConfigType::Auto);
            }

            const string myNetIfacesConfig = ta::readData(NetIfacesConfigFilePath);
            const boost::regex myRegEx("^iface\\s+" + ta::regexEscapeStr(anIfaceName) + "\\s+inet\\s+(?<iface_type>dhcp|static)$");
            boost::cmatch match;
            if (!regex_search(myNetIfacesConfig.c_str(), match, myRegEx))
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Cannot retrieve IPv4 configuration type for network interface '%s'") % anIfaceName);
            }
            const string myIfaceType = match["iface_type"];
            if (myIfaceType == "dhcp")
            {
                return boost::make_tuple(IfaceConfigType::Auto, IfaceConfigType::Manual);
            }
            else if (myIfaceType == "static")
            {
                return boost::make_tuple(IfaceConfigType::Manual, IfaceConfigType::Manual);
            }
            else
            {
                // this should not happen
                TA_THROW_MSG(std::runtime_error, boost::format("Failed to parse IPv4 configuration type from '%s' for network interface '%s'") % myIfaceType % anIfaceName);
            }
        }

        bool existIfaceWithAutoIpv4Configuration()
        {
            return existIfaceWithAutoConfiguration(IPV4);
        }

        bool existIfaceWithAutoIpv6Configuration()
        {
            return existIfaceWithAutoConfiguration(IPV6);
        }

        void applyNetIfaceConfig(const Iface& anIface, IfaceConfigType::val anIPv4IfaceConfigType, IfaceConfigType::val anIPv6IfaceConfigType)
        {
            DEBUGLOG(boost::format("Applying network settings for %s. %s. IPv4 configuration: %s. IPv6 configuration: %s") % anIface.first
                     % str(anIface.second)
                     % str(anIPv4IfaceConfigType)
                     % str(anIPv6IfaceConfigType));

            if (isLoopback(anIface.first))
            {
                return;
            }

            validateIpSettings(anIface, anIPv4IfaceConfigType, anIPv6IfaceConfigType);
            saveIpSettings(anIface, anIPv4IfaceConfigType, anIPv6IfaceConfigType);
            effectuateSavedIpSettings(anIface.first);
        }

        IPv4Routes getIpv4CustomRoutes(const std::string& anIfaceName)
        {
            string myStdOut, myStdErr;
            const string myCommand = "ip -4 route show";
            const int myExecCode = Process::shellExecSync(myCommand, myStdOut, myStdErr);
            if (myExecCode != 0)
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Command %1% finished with error code %2%. Stderr: %3%") % myCommand % myExecCode % myStdErr);
            }

            boost::trim(myStdOut);
            if (myStdOut.empty())
            {
                return IPv4Routes();
            }

            IPv4Routes myRoutes;

            const boost::regex myRegEx(str(boost::format("^\\s*(?<network-ip>[\\d\\.]+)/(?<network-mask>\\d+)\\s+via\\s+(?<gateway-ip>[\\d\\.]+)\\s+dev\\s+(?<device>%s)") % ta::regexEscapeStr(anIfaceName)));
            boost::match_results<string::const_iterator> myMatch;
            string::const_iterator myBeg = myStdOut.begin();
            string::const_iterator myEnd = myStdOut.end();
            while (regex_search(myBeg, myEnd, myMatch, myRegEx))
            {
                const string myNetworkIp = myMatch["network-ip"];
                const unsigned int myNetMaskCidr = Strings::parse<unsigned int>(myMatch["network-mask"]);
                const string myGatewayIp = myMatch["gateway-ip"];
                const string myNetMask = convIpv4CidrNetmaskToDotDecimal(myNetMaskCidr);
                if (!isValidIpv4(myNetworkIp))
                {
                    TA_THROW_MSG(std::runtime_error, boost::format("Invalid network IP '%s' found in the output of %s command: %s") % myNetworkIp % myCommand % myStdOut);
                }
                if (!isValidIpv4(myGatewayIp))
                {
                    TA_THROW_MSG(std::runtime_error, boost::format("Invalid gateway IP '%s' found in the output of %s command: %s") % myGatewayIp % myCommand % myStdOut);
                }
                myBeg = myMatch[0].second;

                const IPv4Route myRoute(IPv4(myNetworkIp, myNetMask), myGatewayIp);
                myRoutes.push_back(myRoute);
            }

            return myRoutes;
        }

        IfacesIPv4Routes getIpv4CustomRoutes()
        {
            IfacesIPv4Routes myRetVal;
            foreach (const Iface& iface, getMyIpv4faces())
            {
                const string myIfaceName = iface.first;
                const IPv4Routes myRoutes = getIpv4CustomRoutes(myIfaceName);
                if (!myRoutes.empty())
                {
                    myRetVal[myIfaceName] = myRoutes;
                }
            }
            return myRetVal;
        }

        void applyIpv4CustomRoutes(const string& anIfaceName, const IPv4Routes& aRoutes, const string& aSaveScriptPath)
        {
            if (!ta::isKeyExist(anIfaceName, getMyIpv4faces()))
            {
                TA_THROW_MSG(std::invalid_argument, "Unknown network interface " + anIfaceName + " supplied for applying custom IPv4 routes");
            }
            const IPv4Routes myOrigRoutes = getIpv4CustomRoutes(anIfaceName);
            const IPv4Routes myNewRoutes = normalizeCustomIpv4Routes(aRoutes);
            DEBUGLOG("Applying IPv4 custom routes for " + anIfaceName + ": " + str(myNewRoutes));

            // save to disk
            const IfacesIPv4Routes myCombinedRoutes = combineCurrentIpv4Routes(anIfaceName, myNewRoutes);
            const string myOrigPersistentConfiguration = saveIpv4CustomRoutes(myCombinedRoutes, aSaveScriptPath);

            // effectuate the changes
            try
            {
                foreach (const IPv4Route& route, myOrigRoutes)
                {
                    Process::checkedShellExecSync(str(boost::format("sudo ip -4 route delete %s/%s") % route.network.addr % route.network.netmask));
                }
                foreach (const IPv4Route& route, myNewRoutes)
                {
                    Process::checkedShellExecSync(str(boost::format("sudo ip -4 route replace %s/%s via %s dev %s") % route.network.addr % route.network.netmask % route.gateway % anIfaceName));
                }
            }
            catch (...)
            {
                WARNLOG("Failed to effectuate IPv4 custom routes, trying to restore the original values");
                // omit interface name letting the system to figure it out; this should minimize the chance of errors
                tryRestoreCustomIpv4Routes(myOrigPersistentConfiguration, myOrigRoutes, aSaveScriptPath);
                throw;
            }
        }

        void applyIpv4CustomRoutes(const IfacesIPv4Routes& aRoutes, const string& aSaveScriptPath)
        {
            // validate
            const Ifaces myIfaces = getMyIpv4faces();
            foreach (const IfaceIPv4Routes& route, aRoutes)
            {
                if (!ta::isKeyExist(route.first, myIfaces))
                {
                    TA_THROW_MSG(std::invalid_argument, "Unknown network interface " + route.first + " supplied for applying custom IPv4 routes");
                }
            }

            // apply
            foreach (const Iface& iface, myIfaces)
            {
                const string myIfaceName = iface.first;
                IfacesIPv4Routes::const_iterator routesIt = aRoutes.find(myIfaceName);
                if (routesIt != aRoutes.end())
                {
                    applyIpv4CustomRoutes(myIfaceName, routesIt->second, aSaveScriptPath);
                }
                else
                {
                    // remove custom routes for the remaining interfaces
                    applyIpv4CustomRoutes(myIfaceName, IPv4Routes(), aSaveScriptPath);
                }
            }
        }


#endif // !_WIN32

        bool isValidIpv4(const string& anAddr)
        {
            const string myAddr = boost::trim_copy(anAddr);
            SockInitializer mySockSockInitializer;
            sockaddr_in myAddrIn = {0};
            myAddrIn.sin_family = AF_INET;
            int myRet = inet_pton(AF_INET, myAddr.c_str(), &myAddrIn.sin_addr);
            if (myRet == 1)
                return true;
            if (myRet == 0)
                return false;
            TA_THROW_MSG(std::runtime_error, boost::format("inet_pton() failed. %1%") % getLastErrorStr());
        }

        bool isLoopbackIpv4(const std::string& anAddr)
        {
            const string myAddr = boost::trim_copy(anAddr);
            if (!isValidIpv4(myAddr))
            {
                TA_THROW_MSG(std::invalid_argument, myAddr + " is not a valid IPv4 address");
            }
            return myAddr == "127.0.0.1";
        }

#ifndef _WIN32
        bool isLoopback(const string& anIfaceName)
        {
            return anIfaceName == LoopbackIfaceName;
        }

        string getLoopbackIfName()
        {
            return LoopbackIfaceName;
        }
#endif

        bool isBitSet(const unsigned int aInput, const size_t aBitNumber)
        {
            if (aBitNumber >= sizeof(aInput) * 8)
                TA_THROW_MSG(std::runtime_error, boost::format("Attempt to read bit number %d of a %d bit integer. (0-based indexing)") % aBitNumber % sizeof(aInput));

            unsigned int myMaskedValue = aInput & (0x1 << aBitNumber);
            return myMaskedValue != 0;
        }

        bool isValidIpv4NetMask(const string& aNetMask)
        {
            const string myNetMask = boost::trim_copy(aNetMask);

            if (!isValidIpv4(myNetMask))
                return false;

            // Parse IP components
            using namespace boost;
            char_separator<char> mySeparator(".");
            tokenizer< char_separator<char> > myTokens(myNetMask, mySeparator);
            vector<unsigned char> myIpComponents;
            foreach (const string& myIpComponent, myTokens)
            {
                short c = Strings::parse<short>(myIpComponent);
                if (c < 0 || c > 255)
                    TA_THROW_MSG(std::runtime_error, boost::format("Invalid IPv4 netmask component: %d. Expected a number in the range [0-255]") % myIpComponent);
                myIpComponents.push_back(static_cast<unsigned short>(c));
            }

            // Construct network mask
            unsigned int myNetMaskInt = 0;
            myNetMaskInt |= myIpComponents[0] << 24;
            myNetMaskInt |= myIpComponents[1] << 16;
            myNetMaskInt |= myIpComponents[2] << 8;
            myNetMaskInt |= myIpComponents[3];

            // Determine if the network mask is a proper prefix of ones
            bool isValidPrefix = true;
            bool isCurrentlyInPrefix = true;
            for (int i = 31; i >= 0; --i)
            {
                bool myBitIsSet = isBitSet(myNetMaskInt, i);
                if (isCurrentlyInPrefix && !myBitIsSet)
                    isCurrentlyInPrefix = false; // We're not in the prefix, but in the tail

                if (isCurrentlyInPrefix)
                    isValidPrefix &= myBitIsSet;
                else
                    isValidPrefix &= !myBitIsSet;
            }

            return isValidPrefix;
        }

        string convIpv4CidrNetmaskToDotDecimal(const unsigned int aPrefixLen)
        {
            if (aPrefixLen > 32)
            {
                TA_THROW_MSG(std::invalid_argument, boost::format("Invalid IPv4 subnet prefix length %u. Should be 0-32.") % aPrefixLen);
            }
            const unsigned short ocets = (aPrefixLen + 7) / 8;
            unsigned long mySAddr = 0;
            if (ocets > 0)
            {
                memset(&mySAddr, 255, (size_t)ocets - 1);
                memset((unsigned char *)&mySAddr + (ocets - 1), 256 - (1 << ((32 - aPrefixLen) % 8)), 1);
            }
            const string myNetMask = str(boost::format("%d.%d.%d.%d") % (mySAddr&0xFF)
                                         % ((mySAddr>>8)&0xFF)
                                         % ((mySAddr>>16)&0xFF)
                                         % ((mySAddr>>24)&0xFF));

            if (!isValidIpv4NetMask(myNetMask))
            {
                TA_THROW_MSG(std::invalid_argument, boost::format("Invalid IPv4 subnet prefix length %u (derived dot-decimal form %s is invalid)") % aPrefixLen % myNetMask);
            }
            return myNetMask;
        }

        bool isValidIpv6(const string& anAddr)
        {
            const string myAddr = boost::trim_copy(anAddr);
            try
            {
                sockaddr_in6 myDummyAddr;
                getIpv6AddrInfo(myAddr, myDummyAddr);
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        bool isLoopbackIpv6(const string& anAddr)
        {
            const string myAddr = boost::trim_copy(anAddr);
            sockaddr_in6 myDummyAddr;
            return (getIpv6AddrInfo(myAddr, myDummyAddr) == ipv6AddrLoopback);
        }

        bool isLinkLocalIpv6(const string& anAddr)
        {
            const string myAddr = boost::trim_copy(anAddr);
            sockaddr_in6 myDummyAddr;
            return (getIpv6AddrInfo(myAddr, myDummyAddr) == ipv6AddrLinkLocal);
        }

        bool isDotDecimalIpv4EmbeddedInIpv6(const string& anAddr, string& aCanonicalIpv6)
        {
            const string myAddr = boost::trim_copy(anAddr);
            if (!isValidIpv6(myAddr))
                TA_THROW_MSG(std::invalid_argument, boost::format("%s is not a valid IPv6") % myAddr);

            static boost::regex myRegEx("([\\:\\da-fA-F]+?\\:)(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3})");
            boost::cmatch myMatch;
            if (!regex_match(myAddr.c_str(), myMatch, myRegEx))
                return false;
            assert(myMatch.size() == 3);
            const string myHead = myMatch[1];
            const string myIpv4 = myMatch[2];
            if (!isValidIpv4(myIpv4))
                return false;

            const vector<string> myIpv4Parts = Strings::split(myIpv4, '.');
            assert(myIpv4Parts.size() == 4);
            aCanonicalIpv6 = str(boost::format("%s%02x%02x:%02x%02x") % myHead
                                 % Strings::parse<int>(myIpv4Parts[0])
                                 % Strings::parse<int>(myIpv4Parts[1])
                                 % Strings::parse<int>(myIpv4Parts[2])
                                 % Strings::parse<int>(myIpv4Parts[3]));
            return true;
        }

        bool isValidIpv6PrefixLength(int aPrefixLength)
        {
            if (aPrefixLength >= 0 && aPrefixLength <= 128)
                return true;
            return false;
        }

        bool isValidPort(const string& aPort, unsigned int* aPortPtr)
        {
            try
            {
                int myPort = Strings::parse<int>(aPort);
                bool myIsValid = isValidPort(myPort);
                if (myIsValid && aPortPtr)
                    *aPortPtr = myPort;
                return myIsValid;
            }
            catch (...)
            {
                return false;
            }
        }

        bool isValidPort(const int aPort)
        {
            return aPort > 0 && aPort < (1 << 16);
        }

        RemoteAddress parseHost(const string& aHost, int aDefaultPort)
        {
            const url::Authority::Parts myParts = url::Authority::parse(aHost);
            if (myParts.host.empty())
                TA_THROW_MSG(std::invalid_argument, boost::format("Empty host in %s") % aHost);
            int myPort;
            if (!myParts.port.empty())
            {
                if (!isValidPort(myParts.port, (unsigned int*)&myPort))
                    TA_THROW_MSG(std::logic_error, boost::format("Bad port in %s") % aHost);
            }
            else
            {
                if (aDefaultPort == NoDefaultPort)
                    TA_THROW_MSG(std::invalid_argument, boost::format("Host '%s' should include port as host:port") % aHost);
                myPort = aDefaultPort;
            }
            return RemoteAddress(myParts.host, myPort);
        }

        string toString(const RemoteAddress& anAddr, int aRemovePort)
        {
            string myRetVal = anAddr.host;
            if (isValidIpv6(anAddr.host))
                myRetVal = "[" + myRetVal + "]";
            if (anAddr.port != aRemovePort)
                myRetVal = str(boost::format("%s:%d") % myRetVal % anAddr.port);
            return myRetVal;
        }

        int getLastError()
        {
#ifdef _WIN32
            return ::WSAGetLastError();
#else
            return errno;
#endif
        }

        string getLastErrorStr()
        {
#ifdef _WIN32
            DWORD myLastError = ::WSAGetLastError();
            LPSTR lpMsgBuf = NULL;
            int ok = FormatMessage(
                         FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                         NULL,
                         myLastError,
                         MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                         (LPSTR)&lpMsgBuf,
                         0,
                         NULL);

            if (ok && lpMsgBuf)
            {
                const string myRetVal(lpMsgBuf);
                LocalFree(lpMsgBuf);
                return myRetVal;
            }
            return str(boost::format("Unknown error occurred. Last error is %d") % myLastError);
#else
            return strerror(errno);
#endif
        }

#ifndef _WIN32
        bool ping(const string& aAddr, int aCount, int aMaxWait)
        {
            const string command = str(ta::safe_format("ping -c %d -w %d \"%s\"") % aCount % aMaxWait % aAddr);
            return ta::Process::shellExecSync(command) == 0;
        }

        bool ping6(const string& aAddr)
        {
            const string command = str(ta::safe_format("ping6 -c 1 \"%s\"") % aAddr);
            return ta::Process::shellExecSync(command) == 0;
        }

        ConnectivityStatus checkConnectivity(const vector<RemoteAddress>& aTestTcpServers)
        {
            // Check default gateway
            string myDefGwIp;
            try {
                myDefGwIp = getDefIpv4Gateway().ip;
            } catch (std::exception& e) {
                WARNDEVLOG(e.what());
                return connectivityDefGwNotConfigured;
            }
            if (!ping(myDefGwIp, 1, 1))
            {
                // prefer less accurate connectivity root causes to false positives when gw is configured to deny pinging itself.
                WARNLOG("Cannot test accessibility to the default gateway at " + myDefGwIp + " because it is not pingable. The subsequent connectivity root cause detections might be inaccurate.");
            }

            // Def gateway is ok, check DNS
            try {
                // nslookup ignores /etc/hosts, which is what we need
                ta::Process::checkedShellExecSync("nslookup -timeout=1 -retry=0 google.com");
            } catch (std::exception& e) {
                return connectivityDnsError;
            }
            if (!ping("8.8.8.8", 1, 1))
            {
                return connectivityDnsError;
            }

            // Check TCP connections
            foreach (const RemoteAddress& svr, aTestTcpServers)
            {
                try {
                    ta::Process::checkedShellExecSync(str(boost::format("nc -z -w 1 %s %s") % svr.host % svr.port));
                } catch (std::exception& e) {
                    WARNDEVLOG("Failed to connect to " + str(svr));
                    return connectivityTcpServersNotAccessible;
                }
            }

            if (!ping("google.com", 1, 1))
            {
                return connectivityInternetServersNotPingable;
            }

            return connectivityOk;
        }
#endif

        static string getValidCharactersForDomainName(DomainNameType aDomainNameType)
        {
            static const string myHostNameValidChars = ".-0123456789abcdefghijklmnopqrstuvwxyz";
            static const string myDnsNameValidChars = myHostNameValidChars + "_";

            switch (aDomainNameType)
            {
            case hostName:
                return myHostNameValidChars;
            case dnsName:
                return myDnsNameValidChars;
            default:
                TA_THROW_MSG(std::invalid_argument, "Unknown domain name type");
            }
        }

        string normalizeDomainName(const string& aDomainName)
        {
            return boost::trim_copy( boost::to_lower_copy(aDomainName) );
        }

        bool isValidDomainName(const string& aDomainName, DomainNameValidationResult& aValidationResult, const DomainNameType aDomainNameType)
        {
            string domainName = normalizeDomainName(aDomainName);

            // The domain name must have a maximum of 255 characters
            size_t hostnameLength = domainName.length();
            if (hostnameLength < 1)
            {
                aValidationResult = domainNameEmpty;
                return false;
            }

            if (hostnameLength > 255)
            {
                aValidationResult = domainNameTooLong;
                return false;
            }

            // Check for valid characters
            string validCharacters = getValidCharactersForDomainName(aDomainNameType);
            if (domainName.find_first_not_of(validCharacters) != string::npos)
            {
                aValidationResult = domainNameInvalidCharacter;
                return false;
            }

            foreach(const string& label, Strings::split(domainName, '.'))
            {
                // Each hostname label size must be between 1 and 63 characters
                size_t labelLength = label.length();
                if (labelLength < 1)
                {
                    aValidationResult = domainNameLabelEmpty;
                    return false;
                }

                if (labelLength > 63)
                {
                    aValidationResult = domainNameLabelTooLong;
                    return false;
                }
            }

            aValidationResult = domainNameOk;
            return true;
        }

#ifdef _WIN32
        string getSelfFqdn()
        {
            DWORD dwSize = 0;
            if (!GetComputerNameEx(ComputerNameDnsFullyQualified, NULL, &dwSize))
            {
                const int myLastError = ::GetLastError();
                if (myLastError != ERROR_MORE_DATA)
                {
                    TA_THROW_MSG(std::runtime_error, boost::format("Failed to retrieve buffer size to hold self FQDN. Last error: %d") % myLastError);
                }
            }
            vector<char> myFQDN(dwSize);
            if (!GetComputerNameEx(ComputerNameDnsFullyQualified, &myFQDN[0], &dwSize))
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Failed to retrieve self FQDN. Last error: %d") % ::GetLastError());
            }
            if (myFQDN.size() <= 1) // empty due to the NUL-terminant
            {
                TA_THROW_MSG(std::runtime_error, "Self FQDN is empty");
            }
            return string(&myFQDN[0], myFQDN.size() - 1);
        }
#endif

    } // NetUtils
} // ta
