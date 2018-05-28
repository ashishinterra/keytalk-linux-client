#include "ta/logconfiguration.h"
#ifndef _WIN32
#include "ta/syslogappender.h"
#include "ta/timeutils.h"
#endif
#include "ta/consoleappender.h"
#include "ta/fileappender.h"
#include "libconfig.h++"
#include <cassert>

using std::string;


namespace ta
{
    static const std::string LogAppenderSettingPath                         = "LogAppenders";
    static const std::string SysLogAppenderSettingPath                      = LogAppenderSettingPath + ".SysLogAppender";
    static const std::string SysLogAppenderAppNamePrefixSettingPath         = SysLogAppenderSettingPath + ".AppNamePrefix";
    static const std::string RemoteLogSvrTimeZoneSettingName                = "RemoteLogSvrTimeZone";
    static const std::string SysLogAppenderRemoteLogSvrTimeZoneSettingPath  = SysLogAppenderSettingPath + "." + RemoteLogSvrTimeZoneSettingName;
    static const std::string ConsoleAppenderSettingPath                     = LogAppenderSettingPath + ".ConsoleAppender";
    static const std::string ConsoleAppenderOutDeviceName                   = ConsoleAppenderSettingPath + ".OutDevice";
    static const std::string ConsoleAppenderLogThresholdSettingPath         = ConsoleAppenderSettingPath + ".LogThreshold";
    static const std::string FileAppenderSettingPath                        = LogAppenderSettingPath + ".FileAppender";
    static const std::string FileAppenderLogThresholdSettingPath            = FileAppenderSettingPath + ".LogThreshold";
    static const std::string FileAppenderLogFileNameSettingPath             = FileAppenderSettingPath + ".LogFileName";

    struct LogConfiguration::LogConfigurationImpl
    {
        boost::ptr_vector<LogAppender> appenders;
    };

    LogConfiguration::LogConfiguration()
        : pImpl(new LogConfigurationImpl())
    {}

    LogConfiguration::~LogConfiguration()
    {
        delete pImpl;
    }

    bool LogConfiguration::load(const string& aConfigPath)
    {
        assert(pImpl);
        try
        {
            libconfig::Config myConfig;
            myConfig.readFile(aConfigPath.c_str());

            // Get Log appenders
            boost::ptr_vector<LogAppender> myAppenders;
#ifndef _WIN32
            if (myConfig.exists(SysLogAppenderSettingPath))
            {
                SysLogAppender::Args myArgs;
                string myVal;

                if (myConfig.lookupValue(SysLogAppenderAppNamePrefixSettingPath, myVal))
                    myArgs.appNamePrefix = myVal;
                if (myConfig.lookupValue(SysLogAppenderRemoteLogSvrTimeZoneSettingPath, myVal))
                    myArgs.remoteLogSvrTimeZone = myVal;
                myAppenders.push_back(new SysLogAppender(myArgs));
            }
#endif
            if (myConfig.exists(ConsoleAppenderSettingPath))
            {
                ConsoleAppender::Args myArgs;
                string myLogThresholdStr;
                LogLevel::val myLogThreshold;
                string myOutDevStr;

                if (myConfig.lookupValue(ConsoleAppenderOutDeviceName, myOutDevStr) && myOutDevStr == "stderr")
                    myArgs.outDev = ConsoleAppender::devStdErr;
                if (myConfig.lookupValue(ConsoleAppenderLogThresholdSettingPath, myLogThresholdStr) && LogLevel::parse(myLogThresholdStr, myLogThreshold))
                    myArgs.logThreshold = myLogThreshold;
                myAppenders.push_back(new ConsoleAppender(myArgs));
            }
            if (myConfig.exists(FileAppenderSettingPath))
            {
                FileAppender::Args myArgs;
                string myLogThresholdStr;
                LogLevel::val myLogThreshold;
                string myLogFileName;

                if (myConfig.lookupValue(FileAppenderLogThresholdSettingPath, myLogThresholdStr) && LogLevel::parse(myLogThresholdStr, myLogThreshold))
                    myArgs.logThreshold = myLogThreshold;
                if (myConfig.lookupValue(FileAppenderLogFileNameSettingPath, myLogFileName))
                    myArgs.logFileName = myLogFileName;
                myAppenders.push_back(new FileAppender(myArgs));
            }
            pImpl->appenders = myAppenders;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    void LogConfiguration::load(const Config& aConfig)
    {
        assert(pImpl);
        boost::ptr_vector<LogAppender> myAppenders;
        if (aConfig.fileAppender)
        {
            FileAppender::Args myArgs;
            myArgs.logThreshold = aConfig.fileAppenderLogThreshold;
            myArgs.logFileName = aConfig.fileAppenderLogFileName;
            myAppenders.push_back(new FileAppender(myArgs));
        }
        if (aConfig.consoleAppender)
        {
            ConsoleAppender::Args myArgs;
            myArgs.logThreshold = aConfig.consoleAppenderLogThreshold;
            if (aConfig.consoleAppenderOutDev == conDevStdErr)
                myArgs.outDev = ConsoleAppender::devStdErr;
            myAppenders.push_back(new ConsoleAppender(myArgs));
        }
#ifndef _WIN32
        if (aConfig.syslogAppender)
        {
            SysLogAppender::Args myArgs;
            myArgs.appNamePrefix = aConfig.syslogAppenderAppNamePrefix;
            myAppenders.push_back(new SysLogAppender(myArgs));
        }
#endif

        pImpl->appenders = myAppenders;
    }


    boost::ptr_vector<LogAppender> LogConfiguration::getAppenders() const
    {
        assert(pImpl);
        return pImpl->appenders;
    }


#ifndef _WIN32
    string LogConfiguration::parseSysLogAppenderRemoteLogSvrTimeZone(const string& aConfigFilePath)
    {
        try
        {
            libconfig::Config myConfig;
            myConfig.readFile(aConfigFilePath.c_str());

            if (myConfig.exists(SysLogAppenderSettingPath))
            {
                string myVal;
                if (myConfig.lookupValue(SysLogAppenderRemoteLogSvrTimeZoneSettingPath, myVal))
                {
                    return myVal;
                }
            }
        }
        catch (...)
        {}

        return "";
    }

    void LogConfiguration::saveSysLogAppenderRemoteLogSvrTimeZone(const string& aConfigFilePath, const string& aTimeZoneId)
    {
        libconfig::Config myConfig;
        myConfig.readFile(aConfigFilePath.c_str());

        if (!myConfig.exists(SysLogAppenderSettingPath))
        {
            TA_THROW_MSG(std::invalid_argument, "No syslog appender found in " + aConfigFilePath);
        }

        if (myConfig.exists(SysLogAppenderRemoteLogSvrTimeZoneSettingPath))
        {
            myConfig.lookup(SysLogAppenderRemoteLogSvrTimeZoneSettingPath) = aTimeZoneId;
        }
        else
        {
            myConfig.lookup(SysLogAppenderSettingPath).add(RemoteLogSvrTimeZoneSettingName, libconfig::Setting::TypeString) = aTimeZoneId;
        }

        myConfig.writeFile(aConfigFilePath.c_str());
    }
#endif

} // ta
