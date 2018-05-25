#pragma once

#include "ta/opensslapp.h"
#include <string>
#include <memory>

static const std::string RccdPathOpt         = "rccd-path";
static const std::string TasksIniPathOpt     = "tasks-ini-path";
static const std::string AllowDowngradeOpt   = "allow-downgrade";
static const std::string InteractiveModeOpt  = "interactive";

class ReseptConfigManagerNoUi
{
public:
    ReseptConfigManagerNoUi();
    ~ReseptConfigManagerNoUi();

    //@nothrow
    //@return success flag
    bool installRccd(const std::string& anRccdUrl, bool anAllowDowngrade, bool anInteractiveMode);

#ifdef _WIN32
    //@nothrow
    //@return success flag
    bool installTasksIni(const std::string& aTasksIniPath);
#endif

private:
    static bool downgradeConfirmationPrompt(const std::string& aMsgText, void* aCookie);

private:
    std::auto_ptr<ta::OpenSSLApp> theOpenSSLAppPtr;
};
