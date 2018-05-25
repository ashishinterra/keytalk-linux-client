#pragma once

#include "rclient/ReseptClientApp.h"
#include "resept/common.h"
#include "ta/utils.h"
#include "ta/strings.h"

#include "boost/cstdint.hpp"
#include "boost/regex.hpp"
#include "boost/algorithm/string.hpp"
#include <string>

// enclose in namespace because these values are also used by tests
namespace rclient
{
    static const std::string PfxFileName     = boost::to_lower_copy(resept::ProductName) + ".pfx";
    static const std::string PfxPassFileName = boost::to_lower_copy(resept::ProductName) + ".pfx.pass";

    static const char HelpOpt[]          = "help";
    static const char BatchModeOpt[]     = "batch";
    static const char InteractiveModeOpt[] = "interactive";
    static const char ProviderOpt[]      = "provider";
    static const char ServiceOpt[]       = "service";
    static const char UserOpt[]          = "user";
    static const char B64UserOpt[]       = "b64-user";
    static const char PasswordOpt[]      = "password";
    static const char B64PasswordOpt[]   = "b64-password";
    static const char NewPasswordOpt[]   = "new-password";
    static const char B64NewPasswordOpt[]= "b64-new-password";
    static const char PincodeOpt[]       = "pincode";
    static const char B64PincodeOpt[]    = "b64-pincode";
    static const char SavePfxOpt[]       = "save-pfx";
    static const char ShowVersionOpt[]   = "version";
    static const char CrFileOpt[]        = "cr-file";

    static const char DelayPrefix[] = "delay "; // when the app exits with exitAuthDelay the actual delay is printed to stderr as <DelayPrefix><seconds>

    inline bool parseExitCode(int anExitCode, const std::string& anStdErr, ReseptClientApp::ExitCode& aParsedCode, int& aParsedDelaySecs)
    {
        if (anExitCode >= ReseptClientApp::_FirstExitCode && anExitCode <= ReseptClientApp::_LastExitCode)
        {
            aParsedCode = static_cast<ReseptClientApp::ExitCode>(anExitCode);
            if (aParsedCode == ReseptClientApp::exitAuthDelay)
            {
                boost::regex myRegEx( str(boost::format("\\s*%s(\\d+)\\s*") % ta::regexEscapeStr(DelayPrefix)) );
                boost::cmatch match;
                if (!regex_match(anStdErr.c_str(), match, myRegEx))
                    return false;
                aParsedDelaySecs = ta::Strings::parse<int>(match[1]);
            }
            return true;
        }
        else
        {
            return false;
        }
    }
}
