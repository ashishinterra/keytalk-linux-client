#include "ReseptConfigManagerNoUi.h"
#include "LoadSettingsBL.h"
#include "rclient/Common.h"
#include "rclient/Settings.h"
#ifdef _WIN32
#include "rclient/TaskSettings.h"
#endif
#include "ta/url.h"
#include "ta/logger.h"
#include "ta/utils.h"
#include "ta/common.h"

#include <iostream>
#include <boost/filesystem.hpp>

using std::string;

struct CookieAppParams
{
    CookieAppParams(): allowDowngrade(false), interactiveMode(false) {}
    CookieAppParams(bool anAllowDowngrade, bool anInteractiveMode): allowDowngrade(anAllowDowngrade), interactiveMode(anInteractiveMode) {}
    bool allowDowngrade;
    bool interactiveMode;
};

ReseptConfigManagerNoUi::ReseptConfigManagerNoUi()
{
    try
    {
        theOpenSSLAppPtr.reset(new ta::OpenSSLApp());
    }
    catch (std::exception& e)
    {
        ERRORLOG2("Error initializing RESEPT Configuration Manager", e.what());
        throw;
    }
}

ReseptConfigManagerNoUi::~ReseptConfigManagerNoUi()
{}

#ifdef _WIN32
bool ReseptConfigManagerNoUi::installTasksIni(const string& aTasksIniPath)
{
    try
    {
        INFOLOG(boost::format("Installing task configuration from '%s'.") % aTasksIniPath);
        if (!rclient::Settings::isScheduledTaskFeatureInstalled())
        {
            ERRORLOG("Scheduled tasks feature is not installed.");
            return false;
        }

        string myTasksIniPath = boost::trim_copy(aTasksIniPath);
        boost::filesystem::path myPath(myTasksIniPath);
        if (!myPath.is_absolute())
        {
            ERRORLOG(boost::format("'%s' is not an absolute path.") % myTasksIniPath);
            return false;
        }

        if (!boost::filesystem::exists(myPath))
        {
            ERRORLOG(boost::format("'%s' does not exist.") % myTasksIniPath);
            return false;
        }

        try
        {
            rclient::Settings::ScopedTaskConfiguration myTaskConfiguration(myTasksIniPath);
            if (!rclient::Settings::isAllTasksValid())
            {
                ERRORLOG(boost::format("'%s' contains invalid tasks. Is a task misconfigured or a required provider not installed?") % myTasksIniPath);
                return false;
            }
        }
        catch (...)
        {
            ERRORLOG(boost::format("Could not validate tasks configuration file '%s'") % aTasksIniPath);
            throw;
        }

        boost::filesystem::copy_file(myTasksIniPath, rclient::Settings::getTaskConfigPath(), boost::filesystem::copy_option::overwrite_if_exists);
        INFOLOG("Installed new tasks.ini file from '" + myTasksIniPath + "'.");
        return true;
    }
    catch (std::exception& e)
    {
        ERRORLOG2(boost::format("Error installing tasks configuration file '%s'.") % aTasksIniPath, e.what());
    }
    catch (...)
    {
        ERRORLOG(boost::format("Error installing tasks configuration file '%s'.") % aTasksIniPath);
    }
    return false;
}
#endif

bool ReseptConfigManagerNoUi::installRccd(const string& anRccdUrl, bool anAllowDowngrade, bool anInteractiveMode)
{
    try
    {
        std::vector<unsigned char> myRccdBlob;
        string myRccdUrl, myErrorMsg;

        const ta::url::Scheme myScheme = ta::url::getScheme(anRccdUrl);
        if (myScheme == ta::url::Http || myScheme == ta::url::Https)
        {
            if (!LoadSettingsBL::loadRccdFromUrl(anRccdUrl, myRccdBlob, myErrorMsg))
            {
                ERRORLOG(myErrorMsg);
                return false;
            }
        }
        else // handle the rest as file URL
        {
            if (!LoadSettingsBL::loadRccdFromFile(anRccdUrl, myRccdBlob, myErrorMsg))
            {
                ERRORLOG(myErrorMsg);
                return false;
            }
        }
        if (myRccdBlob.empty())
        {
            ERRORLOG("Empty RCCD file received from " + anRccdUrl);
            return false;
        }

        CookieAppParams myCookieAppParams(anAllowDowngrade, anInteractiveMode);
        if (!LoadSettingsBL::installRccd(myRccdBlob, anRccdUrl, downgradeConfirmationPrompt, &myCookieAppParams, myErrorMsg))
        {
            ERRORLOG(myErrorMsg);
            return false;
        }

        return true;
    }
    catch (std::exception& e)
    {
        ERRORLOG2("Error installing RCCD", e.what());
        return false;
    }
    catch (...)
    {
        ERRORLOG2("Unknown error installing RCCD", "RCCD path: " + anRccdUrl);
        return false;
    }
}

bool ReseptConfigManagerNoUi::downgradeConfirmationPrompt(const std::string& aMsgText, void* aCookie)
{
    if (aCookie)
    {
        CookieAppParams* myCookieAppParams = (CookieAppParams*)aCookie;

        // allowDowngrade has preference over user feedback
        if (myCookieAppParams->allowDowngrade)
        {
            DEBUGLOG("Downgrade is allowed by command line options");
            return true;
        }

        if (myCookieAppParams->interactiveMode)
        {
            string myAnswer;
            while (true)
            {
                std::cout << aMsgText << std::endl;
                std::cin >> myAnswer;
                if (myAnswer.empty() || boost::iequals(myAnswer, "y") || boost::iequals(myAnswer, "yes"))
                {
                    DEBUGLOG("Downgrade is allowed by user");
                    return true;
                }
                if (boost::iequals(myAnswer, "n") || boost::iequals(myAnswer, "no"))
                {
                    DEBUGLOG("Downgrade is disallowed by user");
                    return false;
                }
            }
        }
    }
    std::cout << "Disallowing settings downgrade. Please use --" << AllowDowngradeOpt << " or --" << InteractiveModeOpt << " to allow this." << std::endl;
    DEBUGLOG("No downgrade confirmation received, disallowing downgrade");
    return false;
}

