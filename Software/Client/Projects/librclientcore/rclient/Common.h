#pragma once

#include "rclient/Version.h"
#include "resept/common.h"
#include "ta/version.h"
#include "ta/osinfoutils.h"
#include "ta/common.h"
#include <string>
#include <vector>
#include <stdexcept>

namespace rclient
{
    static const std::string ClientDescription = str(boost::format("%s %s Client-%s") % resept::ProductName
            % ta::OsInfoUtils::getPlatformShortName()
            % ta::version::toStr(ClientVersion));

#ifdef _WIN32
    static const std::string ReseptDesktopClient      = "ReseptDesktopClient";
    static const std::string ReseptConsoleClient      = "ReseptConsoleClient";
    static const std::string ReseptConfigManager      = "ReseptConfigManager";
    static const std::string ReseptPrGenerator        = "ReseptPrGenerator.exe";
#else
    static const std::string ReseptConsoleClient      = "ktclient";
    static const std::string ReseptConfigManager      = "ktconfig";
    static const std::string ReseptPrGenerator        = "ktprgen";
#endif
    static const std::string LogName                  = "ktclient.log";
    static const std::string ConfigManagerLogFileName = "ktconfig.log";
    static const std::string ConfigToolLogFileName    = "ktconfigtool.log";
    static const std::string ConfigUpdaterLogFileName = "ktconfupdater.log";
    static const std::string SavedPemName             = "ktusercert.pem";
    static const std::string SavedPemKeyPasswdName    = "ktusercert.passwd.txt";
    static const std::string ReseptIeBroker           = "ReseptIeBroker.exe";
    static const char       BrokerServiceName[]       = "ReseptBrokerService";
    static const std::string BrokerServiceLogName     = "ktbrokerservice.log";
    static const std::string IeBrokerProxyLogName     = "ktiebrokerproxy.log";
    static const std::string SweeperLogFileName       = "ktsweeper.log";
    static const std::string PrGeneratorLogName       = "ktprgenerator.log";

    // images
    static const std::string IconV10ImageName = "resept_ico.bmp";
    static const std::string LogoV10ImageName = "resept_logo.bmp";
    static const std::string LogoV11ImageName = "logo_v11.png";
    static const std::string LogoV20ImageName = "logo.png";
    static const unsigned int LogoV20ImageWidth  = 110;
    static const unsigned int LogoV20ImageHeight = 110;


    struct RcdpVersionMismatchError : std::runtime_error
    {
        explicit RcdpVersionMismatchError(const std::string& aMessage = "")	: std::runtime_error(aMessage) {}
    };

    struct Pfx
    {
        Pfx() {}
        Pfx(const std::vector<unsigned char>& aData, const std::string& aPassword): data(aData), password(aPassword) {}
        std::vector<unsigned char> data;
        std::string password;
    };

    struct UserRcdpSessionData
    {
        UserRcdpSessionData()
        {
            reset();
        }
        void reset(const resept::rcdpv2::State anRcdpState = resept::rcdpv2::stateClosed)
        {
            rcdpState = anRcdpState;
            sid.clear();
            sid_exist = false;
        }
        std::string sid;
        bool sid_exist;
        resept::rcdpv2::State rcdpState;
        ta::version::Version rcdpVersion;
    };

    struct AuthRequirements
    {
        AuthRequirements(): resolve_service_uris(false), calc_service_uris_digest(false) {}
        resept::CredentialTypes cred_types;
        std::string hwsig_formula;
        std::string password_prompt;
        std::vector<std::string> service_uris;
        bool resolve_service_uris;
        bool calc_service_uris_digest;
    };

    inline std::string str(const AuthRequirements& aReq)
    {
        return str(boost::format("credential types: %s, HWSIG formula: %s, password prompt: %s, service URIs: %s, resolve service URIs: %s, calculate service URIs digest: %s")
                   % fmtCredTypes(aReq.cred_types)
                   % aReq.hwsig_formula
                   % aReq.password_prompt
                   % ta::Strings::join(aReq.service_uris, ',')
                   % (aReq.resolve_service_uris ? "yes" : "no")
                   % (aReq.calc_service_uris_digest ? "yes" : "no"));
    }

    struct AuthResponse
    {
        AuthResponse() {}
        AuthResponse(const resept::AuthResult& anAuthResult, const ta::StringDict& aChallenges = ta::StringDict(), const ta::StringArray aResponseNames = ta::StringArray())
            : auth_result(anAuthResult), challenges(aChallenges), response_names(aResponseNames)
        {}

        resept::AuthResult auth_result;
        ta::StringDict challenges;
        ta::StringArray response_names;
    };

    struct Message
    {
        Message(): utc(0) {}
        Message(const time_t aUtc, const std::string& aText): utc(aUtc), text(aText) {}

        inline bool operator==(const Message& rhs) const { return utc == rhs.utc && text == rhs.text; }

        time_t utc;
        std::string text;
    };
    typedef std::vector<Message> Messages;
    std::string formatMessages(const Messages& aMessages);

    struct CertResponse
    {
        CertResponse(): execute_sync(false) {}
        std::vector<unsigned char> cert;
        std::string password;
        bool execute_sync;
    };

    std::string getLogDir();
    std::string getInstallerDataBackupDir();
}
