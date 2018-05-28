#include "syslogappender.h"
#include "ta/thread.h"
#include "ta/singletonholder.hpp"
#include "ta/strings.h"
#include "ta/timeutils.h"
#include "ta/common.h"

#include <syslog.h>

using std::string;
using std::vector;

namespace ta
{
    namespace
    {
        static const size_t SyslogMaxLineLen = 1024; //@todo see MAXLINE in syslogd.c
        static const size_t SafeMaxLineLen = SyslogMaxLineLen - 256; // subtract space used by date/time, pid, tid, log threshold (at most)

        template<class T>
        class SysLogCreationPolicy
        {
        public:
            static T* createInstance() 	{ return new T(theAppNamePrefix); }
            static string theAppNamePrefix;
        };
        template<class T> string SysLogCreationPolicy<T>::theAppNamePrefix;

        class SysLog: public SingletonHolder<SysLog, SysLogCreationPolicy<SysLog> >
        {
            friend class SingletonHolder<SysLog, SysLogCreationPolicy<SysLog> >;
            friend class SysLogCreationPolicy<SysLog>;
        public:
            SysLog(const string& anAppNamePrefix)
                : theAppName(anAppNamePrefix + LogAppender::getSelfAppName())
            {
                openlog(theAppName.c_str(), LOG_PID|LOG_NDELAY, LOG_USER);
            }
            ~SysLog()
            {
                closelog();
            }
        private:
            string theAppName;
        };
        void initSysLog(const string anAppNamePrefix)
        {
            SysLogCreationPolicy<SysLog>::theAppNamePrefix = anAppNamePrefix;
            SysLog::instance();
        }

        int translateLogLevel2SysLogPrio(LogLevel::val aLogLevel)
        {
            switch (aLogLevel)
            {
            case LogLevel::Debug:
                return LOG_DEBUG;
            case LogLevel::Info:
                return LOG_INFO;
            case LogLevel::Warn:
                return LOG_WARNING;
            case LogLevel::Error:
                return LOG_ERR;
            default:
                return LOG_INFO;
            }
        }

        //
        // Log messages formatting
        //

        string getCallDepthStr(size_t aCallDepth)
        {
            return string( 2*aCallDepth, ' ' );
        }

        //
        // Format log message splitting long message to fit syslog line length
        //
        vector<string> formatLogMsgBrief(const LogEvent& aLogEvent, const std::string& aRemoteLogSvrTimeZone)
        {
            vector<string> myRetVal;

            string myMsg = str(boost::format("%s(): %s") % aLogEvent.func % aLogEvent.msg);
            if (!aRemoteLogSvrTimeZone.empty() && aRemoteLogSvrTimeZone != TimeUtils::LocalTimeZone)
            {
                myMsg = "[[" + LogAppender::getTimeStamp(aRemoteLogSvrTimeZone) + "]] " + myMsg;
            }

            const string myTempl = "<<%5u>> [%-" + Strings::toString(LogLevel::getMaxStrLen()) + "s]%s %s%s";
            foreach (const string& msg, LogAppender::splitMsg(myMsg, SafeMaxLineLen))
            {
                const string myLogEntry = str(boost::format(myTempl)
                                              % ThreadUtils::getSelfId()
                                              % str(aLogEvent.level)
                                              % (aLogEvent.isDevel ? LoggerDevelTag : "")
                                              % getCallDepthStr(aLogEvent.callDepth)
                                              % msg);
                myRetVal.push_back(myLogEntry);
            }
            return myRetVal;
        }

        vector<string> formatLogMsgFull(const LogEvent& aLogEvent, const std::string& aRemoteLogSvrTimeZone)
        {
            vector<string> myRetVal;

            string myMsg = str(boost::format("%s(), file %s:%u: %s") % aLogEvent.func % aLogEvent.file  % aLogEvent.line % aLogEvent.msg);
            if (!aRemoteLogSvrTimeZone.empty() && aRemoteLogSvrTimeZone != TimeUtils::LocalTimeZone)
            {
                myMsg = "[[" + LogAppender::getTimeStamp(aRemoteLogSvrTimeZone) + "]] " + myMsg;
            }

            const string myTempl = "<<%5u>> [%-" + Strings::toString(LogLevel::getMaxStrLen()) + "s]%s %s%s";
            foreach (const string& msg, LogAppender::splitMsg(myMsg, SafeMaxLineLen))
            {
                const string myLogEntry = str(boost::format(myTempl)
                                              % ThreadUtils::getSelfId()
                                              % str(aLogEvent.level)
                                              % (aLogEvent.isDevel ? LoggerDevelTag : "")
                                              % getCallDepthStr(aLogEvent.callDepth)
                                              % msg);
                myRetVal.push_back(myLogEntry);
            }
            return myRetVal;
        }
    }

    Mutex SysLogAppender::theSysLogMutex;

    SysLogAppender::SysLogAppender(const Args& anArgs)
        : theAppNamePrefix(anArgs.appNamePrefix)
        , theRemoteLogSvrTimeZone(anArgs.remoteLogSvrTimeZone)
    {}

    SysLogAppender::~SysLogAppender()
    {}

    void SysLogAppender::send(const LogEvent& aLogEvent) const
    {
        initSysLog(theAppNamePrefix);
        int mySysLogPrio = translateLogLevel2SysLogPrio(aLogEvent.level);
        vector<string> myMsgs;
        if (aLogEvent.level == LogLevel::Error && aLogEvent.isDevel)
            myMsgs = formatLogMsgFull(aLogEvent, theRemoteLogSvrTimeZone);
        else
            myMsgs = formatLogMsgBrief(aLogEvent, theRemoteLogSvrTimeZone);

        {
            ScopedLock lock(theSysLogMutex);
            foreach (const string& msg, myMsgs)
            {
                syslog(mySysLogPrio, "%s", msg.c_str());
            }
        }
    }
}
