#pragma once

#include "ta/logappender.h"
#include "ta/thread.h"
#include "ta/logger.h"
#include "ta/common.h"
#include <string>
#include <fstream>

namespace ta
{
    /**
      File appender class
    */
    class FileAppender: public Clonable<FileAppender, LogAppender>
    {
    public:
        struct Args
        {
            Args(): logThreshold(LogLevel::Info), logFileName(getSelfAppName()+".log") {}
            LogLevel::val logThreshold;
            std::string logFileName;
        };
        FileAppender(const Args& anArgs);

        /**
          Copy c'tor is required to pass around theLogFile in clone()
         */
        FileAppender(const FileAppender& aFileAppender);
        virtual ~FileAppender();

        /**
          Send message to logger

          @param[in] aLogEvent Log event type
         */
        virtual void send(const LogEvent& aLogEvent) const;
    private:
        void initLogFile();
        bool rotateLogFile() const;
        bool clearLogFile() const;
    private:
        const std::string theLogFileName;
        LogLevel::val theLogThreshold;
        static Mutex theFileMutex;
        mutable std::ofstream theLogFile;
    };
}
