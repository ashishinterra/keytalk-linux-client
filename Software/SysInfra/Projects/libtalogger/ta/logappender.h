#pragma once

#include <string>
#include <vector>

#ifdef _WIN32
# ifdef TA_LOGGER_EXPORTS
#  define TA_LOGAPPENDER_API __declspec(dllexport)
# else
#  define TA_LOGAPPENDER_API __declspec(dllimport)
# endif
#else
# define TA_LOGAPPENDER_API
#endif

namespace ta
{
    namespace LogLevel
    {
        enum val
        {
            _first,
            Error = _first,
            Warn, Warning = Warn,
            Info,
            Debug,
            _last = Debug
        };
        const std::string strs[] = {"ERROR", "WARN", "INFO", "DEBUG"};

        TA_LOGAPPENDER_API std::string str(val aVal);
        TA_LOGAPPENDER_API bool parse(const std::string& aLogLevelStr, val& aLogLevel);
        TA_LOGAPPENDER_API bool isLogLevel(int aVal);
        size_t getMaxStrLen();
    }

    struct LogEvent
    {
        LogEvent(const LogLevel::val aLogLevel, const size_t aCallDepth, const std::string& aFunc, const std::string& aFile, unsigned int aLine, const std::string& aMsg, const bool anIsDevel = false)
            : level(aLogLevel), callDepth(aCallDepth), func(aFunc), file(aFile), line(aLine), msg(aMsg), isDevel(anIsDevel)
        {}

        LogEvent(const LogLevel::val aLogLevel, const size_t aCallDepth, const std::string& aFunc, const std::string& aMsg, const bool anIsDevel = false)
            : level(aLogLevel), callDepth(aCallDepth), func(aFunc), line(0), msg(aMsg), isDevel(anIsDevel)
        {}

        LogEvent(const size_t aCallDepth, const std::string& aFunc, const bool anEnter)
            : level(LogLevel::Debug), callDepth(aCallDepth), func(aFunc), line(0), msg(anEnter?"entering":"exiting"), isDevel(false)
        {}

        LogLevel::val level;
        size_t callDepth;
        std::string func;
        std::string file;
        unsigned int line;
        std::string msg;
        bool isDevel;
    };

    struct ProLogEvent: LogEvent
    {
        ProLogEvent(const std::string& anAppName);
        static std::string makeMsg(const std::string& anAppName);
    };

    struct EpiLogEvent: LogEvent
    {
        EpiLogEvent(const std::string& anAppName);
    };

    const std::string LoggerDevelTag = "[DEVEL]";


    /**
      Log appender class
    */
    class LogAppender
    {
    public:
        LogAppender();
        virtual ~LogAppender();

        /**
          Send message to logger

          @param[in] aLogEvent Log event type
         */
        virtual void send(const LogEvent& aLogEvent) const = 0 ;

        /**
          Duplicates the object
         */
        virtual LogAppender* clone() const = 0;

        /**
          Retrieve application name

          @note It is recommended to call this function at runtime rather then
          during statics initialization time since the latter does not always work correctly
        */
        static std::string getSelfAppName();

        /**
            Retrieves local time formatted as a string, corrected with a difference between the local and the target timezone if necessary
        */
#ifdef _WIN32
        static std::string getTimeStamp();
#else
        static std::string getTimeStamp(const std::string& aTargetTz = "");
#endif

        static std::vector<std::string> splitMsg(const std::string& aMsg, size_t aMaxSize);

        // Filter out log entries tagged as "development"
        TA_LOGAPPENDER_API static std::string filterOutDevelEntries(const std::string& aText);

    };

    /**
      Create clone of object

      @note Cloneability is required to be able passing around boost::ptr_vector<LogAppender>
     */
    inline LogAppender* new_clone(const LogAppender& aLogAppender)
    {
        return aLogAppender.clone();
    }
}
