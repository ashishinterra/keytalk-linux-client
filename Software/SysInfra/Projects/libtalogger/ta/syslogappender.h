#pragma once

#include "ta/logappender.h"
#include "ta/thread.h"
#include "ta/common.h"
#include <string>

namespace ta
{
    /**
      Syslog appender class
     */
    class SysLogAppender: public Clonable<SysLogAppender, LogAppender>
    {
    public:
        struct Args
        {
            std::string appNamePrefix;
            std::string remoteLogSvrTimeZone;
        };
        SysLogAppender(const Args& anArgs);
        virtual ~SysLogAppender();

        /**
          Send message to logger

          @param[in] aLogEvent Log event type
         */
        virtual void send(const LogEvent& aLogEvent) const;

        inline std::string getRemoteLogSvrTimeZone() const { return theRemoteLogSvrTimeZone; }
    private:
        static Mutex theSysLogMutex;
        const std::string theAppNamePrefix;
        const std::string theRemoteLogSvrTimeZone;
    };
}
