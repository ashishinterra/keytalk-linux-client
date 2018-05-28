#include "dnsutils.h"
#include "netutils.h"
#include "url.h"
#include "strings.h"
#include "process.h"
#include "utils.h"
#include "common.h"
#include "ta/logger.h"

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
# include <unistd.h>
# include <netdb.h>
# include <net/if.h>
# include <netinet/in.h>
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
#include <boost/assign/list_of.hpp>
#include "boost/tokenizer.hpp"
#include "boost/regex.hpp"

namespace ta
{
    namespace DnsUtils
    {
        using namespace ta::NetUtils;
        using std::string;
        using std::vector;

        // Private stuff
        namespace
        {
            /**
             * The returned array contains at least one address
            */
            vector<string> resolveIpsv4ByName(const string& aHostName)
            {
                SockInitializer mySockSockInitializer;
                vector<string> myIps;
                if (isValidIpv4(aHostName))
                {
                    myIps.push_back(aHostName);
                    return myIps;
                }
#ifdef _WIN32
                addrinfo hints = {0};
                addrinfo* result = NULL;
                hints.ai_family = AF_INET;
                hints.ai_socktype = SOCK_STREAM;
                hints.ai_protocol = IPPROTO_TCP;
                DWORD dwRetval = getaddrinfo(aHostName.c_str(), NULL, &hints, &result);
                if (dwRetval != 0)
                    TA_THROW_MSG(IpResolveError, boost::format("getaddrinfo failed with code %d") % dwRetval);
                for (addrinfo* ptr = result; ptr && ptr->ai_family == AF_INET; ptr = ptr->ai_next)
                {
                    char myIpSzBuf[INET_ADDRSTRLEN] = {};
                    sockaddr_in* sockaddr_ipv4 = (struct sockaddr_in*) ptr->ai_addr;
                    if (!inet_ntop(AF_INET, &sockaddr_ipv4->sin_addr, myIpSzBuf, sizeof(myIpSzBuf)))
                        TA_THROW_MSG(IpResolveError, boost::format("inet_ntop() failed. Last error: %d") % WSAGetLastError());
                    myIps.push_back(myIpSzBuf);
                }
#else
                hostent* myHost = ::gethostbyname(aHostName.c_str());
                if (!myHost)
                    TA_THROW_MSG(IpResolveError, boost::format("gethostbyname() failed for host %1%. %2%") % aHostName % hstrerror(h_errno));

                for (unsigned int i=0; myHost->h_addr_list[i]; ++i)
                {
                    string myIp = str(boost::format("%1%.%2%.%3%.%4%") %
                                      (unsigned int)(unsigned char)myHost->h_addr_list[i][0] %
                                      (unsigned int)(unsigned char)myHost->h_addr_list[i][1] %
                                      (unsigned int)(unsigned char)myHost->h_addr_list[i][2] %
                                      (unsigned int)(unsigned char)myHost->h_addr_list[i][3]);
                    myIps.push_back(myIp);
                }
#endif
                if (myIps.empty())
                {
                    TA_THROW_MSG(IpResolveError, boost::format("gethostbyname() returned zero IPv4 addreses for host %1%") % aHostName);
                }
                return myIps;
            }

            vector<string> resolveIpsv6ByName(const string& aHostName)
            {
                SockInitializer mySockSockInitializer;
                vector<string> myIps;
                if (isValidIpv6(aHostName))
                {
                    myIps.push_back(aHostName);
                    return myIps;
                }
#ifdef _WIN32
                addrinfo hints = {0};
                addrinfo* result = NULL;
                hints.ai_family = AF_INET6;
                hints.ai_socktype = SOCK_STREAM;
                hints.ai_protocol = IPPROTO_TCP;
                DWORD dwRetval = getaddrinfo(aHostName.c_str(), NULL, &hints, &result);
                if (dwRetval != 0)
                    TA_THROW_MSG(IpResolveError, boost::format("getaddrinfo failed with code %d") % dwRetval);
                for(addrinfo* ptr=result; ptr && ptr->ai_family == AF_INET6 ; ptr=ptr->ai_next)
                {
                    char myIpSzBuf[INET6_ADDRSTRLEN] = {};
                    sockaddr_in6* sockaddr_ipv6 = (struct sockaddr_in6*) ptr->ai_addr;
                    if (!inet_ntop(AF_INET6, &sockaddr_ipv6->sin6_addr, myIpSzBuf, sizeof(myIpSzBuf)) )
                        TA_THROW_MSG(IpResolveError, boost::format("inet_ntop() failed. Last error: %d") % WSAGetLastError());
                    myIps.push_back(myIpSzBuf);
                }
#else
                hostent* myHost = ::gethostbyname2(aHostName.c_str(), AF_INET6);
                if (!myHost)
                    TA_THROW_MSG(IpResolveError, boost::format("gethostbyname2() failed for host %1%. %2%") % aHostName % hstrerror(h_errno));

                for (unsigned int i=0; myHost->h_addr_list[i]; ++i)
                {
                    char myIpSzBuf[INET6_ADDRSTRLEN] = {};
                    if (!inet_ntop(myHost->h_addrtype, myHost->h_addr_list[i], myIpSzBuf, sizeof(myIpSzBuf)))
                        TA_THROW_MSG(IpResolveError, boost::format("inet_ntop() failed. %1%") % strerror(errno));
                    myIps.push_back(myIpSzBuf);
                }
#endif
                return myIps;
            }


#ifdef RESEPT_SERVER
            static const string ResolvConfPath = "/etc/resolv.conf";
            static const string ResolvConfdDir = "/etc/resolvconf/resolv.conf.d";
            static const string ResolvConfTailPath = ResolvConfdDir + "/tail";
            static const string ResolvConfNameServerOption = "nameserver";
            static const vector<char> ResolvConfSeps = boost::assign::list_of(' ')('\t');

            ta::StringArray loadNameServersFrom(const string& aFilePath)
            {
                ta::StringArray myNameSvrs;

                if (ta::isFileExist(aFilePath))
                {
                    std::ifstream myFile(aFilePath.c_str());
                    string myLine;

                    while (std::getline (myFile, myLine))
                    {
                        boost::trim(myLine);
                        ta::StringArray myKeyVal = ta::Strings::split(myLine, ResolvConfSeps, ta::Strings::sepsMergeOn);
                        if (myKeyVal.size() == 2 && myKeyVal[0] == ResolvConfNameServerOption)
                        {
                            const string myNameSvr = myKeyVal[1];
                            if (!ta::NetUtils::isValidIpv4(myNameSvr) && !ta::NetUtils::isValidIpv6(myNameSvr))
                            {
                                WARNLOG("Nameserver " + myNameSvr + "found in " + aFilePath + " is not a valid ip address.");
                            }
                            // be tolerant to what we get
                            myNameSvrs.push_back(myNameSvr);
                        }
                    }
                }
                return ta::removeDuplicates(myNameSvrs);
            }

            void saveNameServersTo(const string& aFilePath, const ta::StringArray& aNameServers)
            {
                ta::StringArray myOutFileLines;

                // Play nice and preserve existing entries other than nameserver-related of course
                if (ta::isFileExist(aFilePath))
                {
                    std::ifstream myInFile(aFilePath.c_str());
                    if (!myInFile.is_open() || myInFile.fail())
                    {
                        TA_THROW_MSG(std::runtime_error, boost::format("Failed to open %s for reading") % aFilePath);
                    }
                    string myLine;
                    while (std::getline (myInFile, myLine))
                    {
                        boost::trim(myLine);
                        ta::StringArray myKeyVal = ta::Strings::split(myLine, ResolvConfSeps, ta::Strings::sepsMergeOn);
                        if (myKeyVal.size() == 2 && myKeyVal[0] == ResolvConfNameServerOption)
                        {
                            continue; // skip
                        }
                        myOutFileLines.push_back(myLine);
                    }
                    myInFile.close();
                }

                foreach (const string& svr, aNameServers)
                {
                    const string mySvr = boost::trim_copy(svr);
                    if (!mySvr.empty())
                    {
                        myOutFileLines.push_back(ResolvConfNameServerOption + " " + mySvr);
                    }
                }
                const string myFileContent = myOutFileLines.empty() ? "" : ta::Strings::join(myOutFileLines, "\n") + "\n";
                ta::writeData(aFilePath, myFileContent);
            }

            bool isSystemOnlyNameServer(const string& aNameServer)
            {
                return ta::isElemExist(aNameServer, loadNameServersFrom(ResolvConfPath))
                       && (!ta::isElemExist(aNameServer, loadNameServersFrom(ResolvConfTailPath)));
            }
#endif // RESEPT_SERVER

        } // unnamed ns


        //
        // Public API
        //

        IP resolveIpByName(const string& aHostName)
        {
            // First, check if we have IP as argument
            IP myRetVal;
            if (isValidIpv4(aHostName))
            {
                myRetVal.ipv4 = aHostName;
                return myRetVal;
            }
            if (isValidIpv6(aHostName))
            {
                myRetVal.ipv6 = aHostName;
                return myRetVal;
            }

            // We have hosthame, try resolving it
            bool myIsIpv4Resolved = false;
            string myIpv4ErrorMsg;
            try
            {
                myRetVal.ipv4 = resolveIpsv4ByName(aHostName).at(0);
                myIsIpv4Resolved = true;
            }
            catch(std::runtime_error& e)
            {
                myIpv4ErrorMsg = e.what();
            }
            try
            {
                myRetVal.ipv6 = resolveIpsv6ByName(aHostName).at(0);
            }
            catch(std::runtime_error& e)
            {
                if (!myIsIpv4Resolved)
                    TA_THROW_MSG(IpResolveError, boost::format("Cannot resolve neither IPv4 nor IPv6 of %1%. %2%. %3%") % aHostName % myIpv4ErrorMsg % e.what());
            }
            return myRetVal;
        }

        vector<IP> resolveIpsByName(const string& aHostName)
        {
            // First, check if we have IP as argument
            vector<IP> myIps;
            if (isValidIpv4(aHostName))
            {
                myIps.push_back(IP(aHostName, ""));
                return myIps;
            }
            if (isValidIpv6(aHostName))
            {
                myIps.push_back(IP("", aHostName));
                return myIps;
            }

            // We have hosthame, try resolving it
            bool myIsIpv4Resolved = false;
            vector<string> myIpsV4;
            string myIpv4ErrorMsg;
            try
            {
                myIpsV4 = resolveIpsv4ByName(aHostName);
                myIsIpv4Resolved = true;
            }
            catch(IpResolveError& e)
            {
                myIpv4ErrorMsg = e.what();
            }

            vector<string> myIpsV6;
            try
            {
                myIpsV6 = resolveIpsv6ByName(aHostName);
            }
            catch(IpResolveError& e)
            {
                if (!myIsIpv4Resolved)
                    TA_THROW_MSG(IpResolveError, boost::format("Cannot resolve neither IPv4 nor IPv6 of %1%. %2%. %3%") % aHostName % myIpv4ErrorMsg % e.what());
            }

            size_t myMaxNumIps = (myIpsV4.size() > myIpsV6.size()) ? myIpsV4.size() : myIpsV6.size();
            myIpsV4.resize(myMaxNumIps);
            myIpsV6.resize(myMaxNumIps);
            myIps.clear();
            for (unsigned int i=0; i < myMaxNumIps; ++i)
                myIps.push_back(IP(myIpsV4[i], myIpsV6[i]));
            return myIps;
        }

        vector<string> getMyIpv4()
        {
            vector<string> myIPs;
            SockInitializer mySockSockInitializer;
#ifdef _WIN32
            SOCKET mySock = socket(AF_INET,SOCK_DGRAM,0);
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
                DWORD myLastError = ::WSAGetLastError();
                closesocket(mySock);
                TA_THROW_MSG(IpResolveError, boost::format("WSAIoctl(SIO_GET_INTERFACE_LIST) failed. Last error is %d") % myLastError);
            }
            closesocket(mySock);
            int myNumOfIfaces = myBytes / sizeof(INTERFACE_INFO);
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
            int mySock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (mySock < 0)
                TA_THROW_MSG(IpResolveError, boost::format("socket() failed. %s.") % strerror(errno));

            char myBuf[16*sizeof(struct ifreq)];
            struct ifconf ifConf;
            struct ifreq ifReq;

            ifConf.ifc_len = sizeof myBuf;
            ifConf.ifc_ifcu.ifcu_buf = (caddr_t)myBuf;
            if (ioctl(mySock, SIOCGIFCONF, &ifConf) < 0)
            {
                int myErr = errno;
                close(mySock);
                TA_THROW_MSG(IpResolveError, boost::format("ioctl(SIOCGIFCONF) failed. %s.") % strerror(myErr));
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
                    int myErr = errno;
                    close(mySock);
                    TA_THROW_MSG(IpResolveError, boost::format("ioctl(SIOCGIFFLAGS) failed. %s.") % strerror(myErr));
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
                            close(mySock);
                            TA_THROW_MSG(IpResolveError, "inet_ntoa() failed");
                        }
                        myIPs.push_back(mySzIp);
                    }
                }
            }
            close(mySock);
#endif
            return myIPs;
        }

#ifdef RESEPT_SERVER
        ta::StringArray loadNameServers(const NsFilter aFilter)
        {
            // Distro-dependent heuristics:
            // if /etc/resolvconf/resolv.conf.d/ directory exists (e.g. on Ubuntu):
            // then:
            //     DNS entries in /etc/resolv.conf are effective name servers and are managed by the system. These name servers typically originate from DHCP servers and from user-specified location /etc/resolvconf/resolv.conf.d/tail. Any changes made to this file will be overwritten by the system and therefore lost.
            // else:
            //    use /etc/resolv.conf to read and write from

            const ta::StringArray myEffectiveNameServers = loadNameServersFrom(ResolvConfPath);
            if (ta::isDirExist(ResolvConfdDir))
            {
                switch (aFilter)
                {
                case nsAll: return myEffectiveNameServers;
                case nsUserOnly: return ta::intersect(loadNameServersFrom(ResolvConfTailPath), myEffectiveNameServers);
                default: TA_THROW_MSG(std::invalid_argument, boost::format("Unsupported NS filter %d") % aFilter);
                }
            }
            else
            {
                return myEffectiveNameServers;
            }
        }

        void applyUserNameServers(const ta::StringArray& aNameServers)
        {
            ta::StringArray myNameServers = ta::removeDuplicates(aNameServers);
            if (ta::isDirExist(ResolvConfdDir))
            {
                // skip nameservers managed by the system only
                myNameServers = ta::filterOutWhen(isSystemOnlyNameServer, myNameServers);
                saveNameServersTo(ResolvConfTailPath, myNameServers);
            }
            else
            {
                saveNameServersTo(ResolvConfPath, myNameServers);
            }
            // effectuate the changes
            ta::Process::checkedShellExecSync("sudo resolvconf -u");
        }


        namespace HostsFile
        {
            namespace
            {
                static const char* hostsFilePath = "/etc/hosts";
            }

            Entry::Entry(const std::string& aIpaddress, const std::string& aHostName, const std::string& aAliases)
                :   ipAddress(aIpaddress),
                    hostName(aHostName),
                    aliases(aAliases)
            {}

            Entry::ValidationResult Entry::mapHostNameValidationResult(const ta::NetUtils::DomainNameValidationResult e) const
            {
                using namespace ta::NetUtils;

                switch (e)
                {
                case domainNameOk:
                    return ok;
                case domainNameEmpty:
                    return hostnameEmpty;
                case domainNameTooLong:
                    return hostnameTooLong;
                case domainNameLabelTooLong:
                    return labelTooLong;
                case domainNameLabelEmpty:
                    return labelEmpty;
                case domainNameInvalidCharacter:
                    return invalidCharacter;
                default:
                    TA_THROW_MSG(std::invalid_argument, boost::format("unsupported Domain Name Validation Result %1%.") % e);
                }
            }

            bool Entry::operator==(const Entry& rhs) const
            {
                return (rhs.ipAddress == ipAddress && rhs.hostName == hostName && rhs.aliases == aliases);
            }

            string Entry::format() const
            {
                return ipAddress + "\t" + hostName + "\t" + aliases + "\n";
            }

            bool Entry::isValid(ValidationResult& aValidationResult, string& aValidationData) const
            {
                if (boost::trim_copy(ipAddress) == "")
                {
                    aValidationResult = emptyIpAddress;
                    aValidationData = ipAddress;
                    return false;
                }

                if ((!isValidIpv4(ipAddress)) && (!isValidIpv6(ipAddress)))
                {
                    aValidationResult = invalidIpAddress;
                    aValidationData = ipAddress;
                    return false;
                }

                ta::NetUtils::DomainNameValidationResult domainNameValidationResult;
                if (!ta::NetUtils::isValidDomainName(hostName, domainNameValidationResult, ta::NetUtils::dnsName))
                {
                    aValidationResult = mapHostNameValidationResult(domainNameValidationResult);
                    aValidationData = hostName;
                    return false;
                }

                const vector<char> separators = boost::assign::list_of(' ')('\t');
                vector<string> lineParts = ta::Strings::split(aliases, separators, ta::Strings::sepsMergeOn, ta::Strings::emptyTokensDrop);
                foreach (string alias, lineParts)
                {
                    ta::NetUtils::DomainNameValidationResult domainNameValidationResult;
                    if (!ta::NetUtils::isValidDomainName(alias, domainNameValidationResult, ta::NetUtils::dnsName))
                    {
                        if (domainNameValidationResult != ta::NetUtils::domainNameEmpty)
                        {
                            aValidationResult = mapHostNameValidationResult(domainNameValidationResult);
                            aValidationData = alias;
                            return false;
                        }
                    }
                }

                aValidationResult = ok;
                aValidationData = "";
                return true;
            }


            bool isValid(const HostEntries& aHostsFile,
                         Entry::ValidationResult& aValidationResult,
                         string& aValidationMsg)
            {
                foreach (const Entry& hostEntry, aHostsFile)
                {
                    HostsFile::Entry::ValidationResult validationResult;
                    string validationMsg;
                    if (!hostEntry.isValid(validationResult, validationMsg))
                    {
                        aValidationResult = validationResult;
                        aValidationMsg = validationMsg;
                        return false;
                    }
                }

                aValidationResult = Entry::ok;
                aValidationMsg = "";
                return true;
            }

            void save(const HostEntries& aNewHostFileEntries)
            {
                HostsFile::Entry::ValidationResult validationResult;
                string validationMsg;
                if (!isValid(aNewHostFileEntries, validationResult, validationMsg))
                {
                    throw HostsFileValidationError(validationResult, validationMsg);
                }

                string hostsFileAsString;
                foreach (Entry hostEntry, aNewHostFileEntries)
                {
                    boost::trim(hostEntry.ipAddress);
                    boost::to_lower(hostEntry.ipAddress);
                    boost::trim(hostEntry.hostName);
                    boost::to_lower(hostEntry.hostName);
                    boost::trim(hostEntry.aliases);
                    boost::to_lower(hostEntry.aliases);
                    if ((hostEntry.ipAddress.empty()) && (hostEntry.hostName.empty()) && (hostEntry.aliases.empty()))
                    {
                        continue;
                    }
                    hostsFileAsString += hostEntry.format();
                }
                ta::writeData(hostsFilePath, hostsFileAsString);
            }


            HostEntries load()
            {
                const string hostsFile = ta::readData(hostsFilePath);
                HostEntries hostsEntries;

                foreach (const string& hostsLine, Strings::split(hostsFile, '\n', Strings::sepsMergeOn, Strings::emptyTokensDrop))
                {
                    // Skip comments
                    boost::regex myRegEx("^\\s*#");
                    boost::cmatch myMatch;
                    if (!regex_search(hostsLine, myRegEx))
                    {
                        static const vector<char> linePartSeparators = boost::assign::list_of(' ')('\t');
                        vector<string> lineParts = ta::Strings::split(hostsLine, linePartSeparators, ta::Strings::sepsMergeOn, ta::Strings::emptyTokensDrop);
                        if (lineParts.size() >= 2)
                        {
                            string aliases = "";
                            if (lineParts.size() > 2)
                            {
                                for (size_t i = 2; i < lineParts.size(); i++)
                                {
                                    aliases += lineParts.at(i) + ((i+1 < lineParts.size()) ? " " : "");
                                }
                            }
                            hostsEntries.push_back(Entry(lineParts.at(0), lineParts.at(1), aliases));
                        }
                    }
                }

                return hostsEntries;
            }

            string getPath()
            {
                return hostsFilePath;
            }

            string format(const HostEntries& aHostFileEntries)
            {
                string hostsFileAsString;

                foreach (const Entry& hostEntry, aHostFileEntries)
                {
                    hostsFileAsString += hostEntry.format();
                }

                return hostsFileAsString;
            }

        } // namespace HostsFile
#endif //RESEPT_SERVER

    } // DnsUtils
}// ta
