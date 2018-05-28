/**
@brief Log Configuration singleton

An application should setup log configuration to configure log appenders before any it actually starts with logging.
The log configuration settings may reside in a separate file or share the app. config file.

@code
#include "ta/common/logconfiguration.h"
...
ta::LogConfiguration::instance().load(logConfigPath)
...
@endcode

Trying to log without preliminary setting up the log configuration is safe and will have no effect because no appenders exist.
*/
#pragma once

#include "ta/logappender.h"
#include "ta/singletonholder.hpp"
#include "ta/common.h"
#include "boost/ptr_container/ptr_vector.hpp"
#include <string>

#ifdef _WIN32
# ifdef TA_LOGGER_EXPORTS
#  define TA_LOGCONFIGURATION_API __declspec(dllexport)
# else
#  define TA_LOGCONFIGURATION_API __declspec(dllimport)
# endif
#else
# define TA_LOGCONFIGURATION_API
#endif

namespace ta
{
    /**
      Log configuration class
    */
    class TA_LOGCONFIGURATION_API LogConfiguration: public SingletonHolder<LogConfiguration>
    {
        friend class SingletonHolder<LogConfiguration>;
        friend class DefaultCreationPolicy<LogConfiguration>;
    public:
        enum ConsoleOutDev
        {
            conDevStdOut,
            conDevStdErr
        };

        struct Config
        {
            Config(): fileAppender(false), fileAppenderLogThreshold(LogLevel::Error)
                , consoleAppender(false), consoleAppenderLogThreshold(LogLevel::Error), consoleAppenderOutDev(conDevStdOut)
#ifndef _WIN32
                , syslogAppender(false)
#endif
            {}
            bool fileAppender;
            LogLevel::val fileAppenderLogThreshold;
            std::string fileAppenderLogFileName;
            bool consoleAppender;
            LogLevel::val consoleAppenderLogThreshold;
            ConsoleOutDev consoleAppenderOutDev;
#ifndef _WIN32
            bool syslogAppender;
            std::string syslogAppenderAppNamePrefix; // syslog matches app name in syslog.conf up to the first whitespace. This allows for someapp to use "otherapp " prefix, so someapp can log as it is otherapp
#endif
        };

        /**
          Load log configuration from config file

          @param[in] aConfigPath path to the configuration file
          @return whether the log configuration has been successfully loaded
         */
        bool load(const std::string& aConfigPath);

        /**
          Load log configuration from memory configuration
          @param[in] aConfig memory configuration
         */
        void load(const Config& aConfig);

        /**
          Retrieve list of appenders
          @return List of appenders
         */
        boost::ptr_vector<LogAppender> getAppenders() const;

#ifndef _WIN32
        /**
            Parses remote log server timezone id from the given log configuration file
            @return remote log server timezone id provided the given config contains syslog configuration with such setting
                    empty string on all errors (config does not exist, syslog section not found or it does not contain timezone section)
            @nothrow
        */
        static std::string parseSysLogAppenderRemoteLogSvrTimeZone(const std::string& aConfigFilePath);
        /**
            Saves remote log server timezone id to the given log configuration file
        */
        static void saveSysLogAppenderRemoteLogSvrTimeZone(const std::string& aConfigFilePath, const std::string& aTimeZoneId);
#endif
    private:
        /**
          @note It is crucial to explicitly define c'tor and d'tor in this class even if they are trivial
                since otherwise default ones will be generated in each module using LogConfiguration, so
                LogConfiguration will not be singleton any more
         */
        LogConfiguration();
        ~LogConfiguration();

        struct LogConfigurationImpl;
        LogConfigurationImpl* pImpl;
    };
}
