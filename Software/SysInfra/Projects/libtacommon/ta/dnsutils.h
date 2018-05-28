#pragma once

#include <stdexcept>
#include <string>
#include <vector>
#include "ta/common.h"
#include "ta/netutils.h"

namespace ta
{
    namespace DnsUtils
    {
        /**
          Resolves IPv4 and/or IPv6 addresses from the provided host name.
          If the provided host name is already a valid IPv4 or IPv6, no resolution is performed
          and the appropriate ipv4 or ipv6 is just filled in the returned IP structure.
          The second function (resolveIpByName) is provided for convenience and resolves only one IP which is sufficient for most cases.
         */
        std::vector<NetUtils::IP> resolveIpsByName(const std::string& aHostName);
        NetUtils::IP resolveIpByName(const std::string& aHostName);

#ifdef RESEPT_SERVER
        enum NsFilter
        {
            nsAll,     // retrieve all effective name servers
            nsUserOnly // retrieve only those effective nameservers that can be managed by user
        };
        ta::StringArray loadNameServers(const NsFilter aFilter = nsAll);

        // The function does not affect existing nameservers that are not user-manageable
        // Empty nameservers are ignored
        void applyUserNameServers(const ta::StringArray& aNameServers);

        namespace HostsFile
        {
            struct Entry
            {
                enum ValidationResult
                {
                    ok,
                    hostnameEmpty,
                    hostnameTooLong,
                    labelTooLong,
                    labelEmpty,
                    invalidCharacter,
                    invalidIpAddress,
                    emptyIpAddress
                };

                std::string	ipAddress;
                std::string hostName;
                std::string aliases;

                Entry(const std::string& aIpaddress, const std::string& aHostName, const std::string& aAliases);
                ValidationResult mapHostNameValidationResult(const ta::NetUtils::DomainNameValidationResult e) const;
                bool operator==(const Entry& lhs) const;
                std::string format() const;
                bool isValid(ValidationResult& aValidationResult, std::string& aValidationData) const;
            };


            struct HostsFileValidationError : public std::runtime_error
            {
                HostsFileValidationError(const Entry::ValidationResult& aValidationResult, const std::string& aValidationData)
                    :   std::runtime_error("HostsFile format failure")
                    ,   validationResult(aValidationResult)
                    ,   validationData(aValidationData)
                {}
                ~HostsFileValidationError() throw() {}

                Entry::ValidationResult    validationResult;
                std::string                     validationData;
            };


            typedef std::vector<Entry> HostEntries;

            void save(const HostEntries& aHostsFile);
            HostEntries load();
            bool isValid(const HostEntries& aHostsFile,
                         Entry::ValidationResult& aValidationResult,
                         std::string& aValidationData);
            std::string getPath();
            std::string format(const HostEntries& aHostsFile);

        } // HostsFile

#endif // RESEPT_SERVER

    } // DnsUtils
} // ta
