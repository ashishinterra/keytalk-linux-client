#pragma once

#include "resept/common.h"
#include "rclient/Common.h"
#include <string>
#include <vector>
#include <stdexcept>
#include <ctime>

namespace ta { namespace version { class Version; }}

namespace rclient
{
    namespace rcdpv2response
    {
        struct ParseError : std::logic_error
        {
            explicit ParseError(const std::string& aMessage = "") : std::logic_error(aMessage) {}
        };

        //
        // All public API raise ParseError on error
        //

        resept::rcdpv2::Response parseResponseStatus(const std::string& aResponse);

        // @return EOC reason or "" if not sent
        std::string parseEoc(const std::string& aResponse);

        // @return error code and optional error reason via anErrorDescription
        int parseError(const std::string& aResponse, std::string& anErrorDescription);

        // @return RCDP version proposed by the server
        ta::version::Version parseHello(const std::string& aResponse);

        // @return server UTC
        time_t parseHandshake(const std::string& aResponse);

        AuthRequirements parseAuthRequirements(const std::string& aResponse);
        AuthResponse parseAuthResponse(const std::string& aResponse);
        Messages parseLastMessages(const std::string& aResponse);
        CertResponse parseCert(const std::string& aResponse, const resept::CertFormat aCertFormat, const std::string& aSessionId);
    }
}
