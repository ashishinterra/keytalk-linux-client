#pragma once

#include "ta/logappender.h"
#include "ta/thread.h"
#include "ta/logger.h"
#include "ta/common.h"

namespace ta
{
    /**
      Console appender class
     */
    class ConsoleAppender: public Clonable<ConsoleAppender, LogAppender>
    {
    public:
        enum OutDev
        {
            devStdOut,
            devStdErr
        };
        struct Args
        {
            Args(): logThreshold(LogLevel::Info), outDev(devStdOut) {}
            LogLevel::val logThreshold;
            OutDev outDev;
        };
        ConsoleAppender(const Args& anArgs);
        virtual ~ConsoleAppender();

        /**
          Send message to logger

          @param[in] aLogEvent Log event type
         */
        virtual void send(const LogEvent& aLogEvent) const;
    private:
        const LogLevel::val theLogThreshold;
        const OutDev theOutDev;
        static Mutex theConsoleMutex;
    };
}
