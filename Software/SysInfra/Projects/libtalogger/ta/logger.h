#pragma once

#include "ta/common.h"
#include "boost/format.hpp"
#include <string>

#ifdef _WIN32
# ifdef TA_LOGGER_EXPORTS
#  define TA_LOGGER_API __declspec(dllexport)
# else
#  define TA_LOGGER_API __declspec(dllimport)
# endif
#else
# define TA_LOGGER_API
#endif

namespace ta
{
    /**
      Logger class
     */
    class TA_LOGGER_API Logger
    {
    public:
        Logger();
        ~Logger();


        //
        // Functions ending with 'Devel' will mark the log entry with an additional 'developer' tag
        // which might be later used by log reading tooling e.g. to filter out such log entries
        //

        /**
          Send message to the log with error/warning/info/debug severity

          @param[in] aFuncName Function name from which log message is send
          @param[in] aFile File name from which log message is send
          @param[in] aLine Line number from which log message is send
          @param[in] aMessage Message to be logged
         */
        void error(const std::string& aFuncName, const std::string& aMessage);
        void error_devel(const std::string& aFuncName, const std::string& aMessage);
        void error(const std::string& aFuncName, const std::string& aFile, unsigned int aLine, const std::string& aMessage);
        void error_devel(const std::string& aFuncName, const std::string& aFile, unsigned int aLine, const std::string& aMessage);

        void warn(const std::string& aFuncName, const std::string& aMessage);
        void warn_devel(const std::string& aFuncName, const std::string& aMessage);

        void info(const std::string& aFuncName, const std::string& aMessage);
        void info_devel(const std::string& aFuncName, const std::string& aMessage);

        void debug(const std::string& aFuncName, const std::string& aMessage);
        void debug_devel(const std::string& aFuncName, const std::string& aMessage);

        void func(const std::string& aFuncName);

        /**
          Open message log

          @param[in] anAppName Application name used to identify log file
         */
        void start(const std::string& anAppName);

        /**
          Close message log

          @param[in] anAppName Application name used to identify log file
         */
        void finish(const std::string& anAppName);
    private:
        bool	theFuncLogFlag;
        char*	theFuncName;
    };
} // namespace ta

// Regular logging
#define ERRORLOG(msg) ta::Logger().error(TA_BARE_FUNC, ta::safeFmtMsg(msg))
#define WARNLOG(msg)  ta::Logger().warn(TA_BARE_FUNC, ta::safeFmtMsg(msg))
#define INFOLOG(msg)  ta::Logger().info(TA_BARE_FUNC, ta::safeFmtMsg(msg))
#define DEBUGLOG(msg) ta::Logger().debug(TA_BARE_FUNC, ta::safeFmtMsg(msg))

// "Developer" logging (normally more verbose)
#define ERRORDEVLOG(msg) ta::Logger().error_devel(TA_BARE_FUNC, __FILE__, __LINE__, ta::safeFmtMsg(msg))
#define WARNDEVLOG(msg)  ta::Logger().warn_devel(TA_BARE_FUNC, ta::safeFmtMsg(msg))
#define INFODEVLOG(msg)  ta::Logger().info_devel(TA_BARE_FUNC, ta::safeFmtMsg(msg))
#define DEBUGDEVLOG(msg) ta::Logger().debug_devel(TA_BARE_FUNC, ta::safeFmtMsg(msg))


#define ERRORLOG2(msg, dev_msg) do { ERRORLOG(msg); ERRORDEVLOG(dev_msg); } while(0)
#define WARNLOG2(msg, dev_msg)  do { WARNLOG(msg);  WARNDEVLOG(dev_msg); } while(0)
#define INFOLOG2(msg, dev_msg)  do { INFOLOG(msg);  INFODEVLOG(dev_msg); } while(0)
#define DEBUGLOG2(msg, dev_msg) do { DEBUGLOG(msg); DEBUGDEVLOG(dev_msg); } while(0)

/**
* @note FUNCLOG should be placed at the function entrance for pretty printing
*/
#define FUNCLOG       ta::Logger log_8B98715A; log_8B98715A.func(TA_BARE_FUNC)

#define PROLOG(appName)  ta::Logger().start(ta::safeFmtMsg(appName))
#define EPILOG(appName)  ta::Logger().finish(ta::safeFmtMsg(appName))
