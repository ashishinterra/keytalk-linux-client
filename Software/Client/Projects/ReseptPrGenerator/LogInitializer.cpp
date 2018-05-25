#include "LogInitializer.h"
#include "rclient/Common.h"
#include "rclient/CommonUtils.h"
#ifdef _WIN32
#include "ta/InternetExplorer.h"
#endif
#include "ta/process.h"
#include "ta/version.h"
#include "ta/logconfiguration.h"
#include "ta/logger.h"
#include "ta/common.h"

using std::string;

LogInitializer::LogInitializer()
{
    string myAppName;
    try { myAppName = ta::Process::getSelfShortName(); }
    catch (...) {}

    const string myLogFilePath = ta::Process::getTempDir() + rclient::PrGeneratorLogName;
    string myEnvInfo = str(boost::format("%s Client-%s Problem Report Generator") % resept::ProductName % toStr(rclient::ClientVersion));

#ifdef _WIN32
    string myIeInfo  = "IE not installed";
    try
    {
        if (ta::InternetExplorer::isInstalled())
        {
            const ta::InternetExplorer::Version myVer = ta::InternetExplorer::getVersion();
            myIeInfo = str(boost::format("IE-%u.%u.%u") % myVer.major % myVer.minor % myVer.subminor);
        }
    }
    catch (std::exception& e)
    {
        myIeInfo = str(boost::format("Failed to retrieve IE version info. %s") % e.what());
    }
    myEnvInfo += " " + myIeInfo;
#endif

    ta::LogConfiguration::Config myMemConfig;
    myMemConfig.fileAppender = true;
    myMemConfig.fileAppenderLogThreshold = ta::LogLevel::Debug;
    myMemConfig.fileAppenderLogFileName = myLogFilePath;
    ta::LogConfiguration::instance().load(myMemConfig);
    PROLOG(myEnvInfo);
    logOsInfo();
}

void LogInitializer::logOsInfo()
{
    using namespace ta;
    try
    {
        OsInfoUtils::Version myVersion = OsInfoUtils::getVersion();
        DEBUGLOG(boost::format("\nOS info:\nName: %s\nVersion: %s") % myVersion.name % myVersion.ver);

        DEBUGLOG("\nEnvironment variables:");
        foreach (const string& myEnvVar, Process::getEnvVars())
        {
            DEBUGLOG(myEnvVar);
        }
#ifdef _WIN32
        DEBUGLOG(boost::format("IE %sinstalled") % (ta::InternetExplorer::isInstalled() ? "" : "not "));
        DEBUGLOG(boost::format(resept::ProductName + " IE add-on %sinstalled") % (rclient::isReseptIeAddonInstalled() ? "" : "not "));
#endif
    }
    catch (std::exception& e)
    {
        WARNLOG2("Failed to get OS info.", boost::format("Failed to get OS info. %s. Ignoring the error...") % e.what());
    }
}
