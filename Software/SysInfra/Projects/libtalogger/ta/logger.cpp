#include "logger.h"
#include "logappender.h"
#include "logconfiguration.h"
#include "ta/common.h"
#include "ta/strings.h" // for strlcpy
#include "boost/format.hpp"
#include <iostream>
#include <new>
#include "boost/thread/tss.hpp"

namespace ta
{
    using std::string;

    // Internal stuff
    namespace
    {
        boost::thread_specific_ptr<size_t> theCallDepthPtr;

        size_t getCallDepth()
        {
            if (!theCallDepthPtr.get())
                theCallDepthPtr.reset(new size_t(0));
            return *theCallDepthPtr;
        }
        void incCallDepth()
        {
            if (!theCallDepthPtr.get())
                theCallDepthPtr.reset(new size_t(0));
            (*theCallDepthPtr)++;
        }
        void decCallDepth()
        {
            if (!theCallDepthPtr.get())
                theCallDepthPtr.reset(new size_t(0));
            if (*theCallDepthPtr != 0)
                (*theCallDepthPtr)--;
        }

    }// unnamed namespace


    Logger::Logger()
        :theFuncLogFlag(false)
        , theFuncName(NULL)
    {}

    Logger::~Logger()
    {
        if (theFuncLogFlag)
        {
            decCallDepth();
            const LogEvent myLogEvent(getCallDepth(), theFuncName ? theFuncName : "", false);
            foreach (const LogAppender& appender, LogConfiguration::instance().getAppenders())
            {
                appender.send(myLogEvent);
            }
        }
        delete[]theFuncName;
    }

    void Logger::error(const string& aFuncName, const string& aFile, unsigned int aLine, const string& aMessage)
    {
        const LogEvent myLogEvent(LogLevel::Error, getCallDepth(), aFuncName, aFile, aLine, aMessage);
        foreach (const LogAppender& appender, LogConfiguration::instance().getAppenders())
        {
            appender.send(myLogEvent);
        }
    }

    void Logger::error_devel(const string& aFuncName, const string& aFile, unsigned int aLine, const string& aMessage)
    {
        const LogEvent myLogEvent(LogLevel::Error, getCallDepth(), aFuncName, aFile, aLine, aMessage, true);
        foreach (const LogAppender& appender, LogConfiguration::instance().getAppenders())
        {
            appender.send(myLogEvent);
        }
    }

    void Logger::error(const string& aFuncName, const string& aMessage)
    {
        const LogEvent myLogEvent(LogLevel::Error, getCallDepth(), aFuncName, aMessage);
        foreach(const LogAppender& appender, LogConfiguration::instance().getAppenders())
        {
            appender.send(myLogEvent);
        }
    }

    void Logger::error_devel(const string& aFuncName, const string& aMessage)
    {
        const LogEvent myLogEvent(LogLevel::Error, getCallDepth(), aFuncName, aMessage, true);
        foreach(const LogAppender& appender, LogConfiguration::instance().getAppenders())
        {
            appender.send(myLogEvent);
        }
    }

    void Logger::warn(const string& aFuncName, const string& aMessage)
    {
        const LogEvent myLogEvent(LogLevel::Warn, getCallDepth(), aFuncName, aMessage);
        foreach (const LogAppender& appender, LogConfiguration::instance().getAppenders())
        {
            appender.send(myLogEvent);
        }
    }
    void Logger::warn_devel(const string& aFuncName, const string& aMessage)
    {
        const LogEvent myLogEvent(LogLevel::Warn, getCallDepth(), aFuncName, aMessage, true);
        foreach (const LogAppender& appender, LogConfiguration::instance().getAppenders())
        {
            appender.send(myLogEvent);
        }
    }

    void Logger::info(const string& aFuncName, const string& aMessage)
    {
        const LogEvent myLogEvent(LogLevel::Info, getCallDepth(), aFuncName, aMessage);
        foreach (const LogAppender& appender, LogConfiguration::instance().getAppenders())
        {
            appender.send(myLogEvent);
        }
    }
    void Logger::info_devel(const string& aFuncName, const string& aMessage)
    {
        const LogEvent myLogEvent(LogLevel::Info, getCallDepth(), aFuncName, aMessage, true);
        foreach (const LogAppender& appender, LogConfiguration::instance().getAppenders())
        {
            appender.send(myLogEvent);
        }
    }

    void Logger::debug(const string& aFuncName, const string& aMessage)
    {
        const LogEvent myLogEvent(LogLevel::Debug, getCallDepth(), aFuncName, aMessage);
        foreach (const LogAppender& appender, LogConfiguration::instance().getAppenders())
        {
            appender.send(myLogEvent);
        }
    }
    void Logger::debug_devel(const string& aFuncName, const string& aMessage)
    {
        const LogEvent myLogEvent(LogLevel::Debug, getCallDepth(), aFuncName, aMessage, true);
        foreach (const LogAppender& appender, LogConfiguration::instance().getAppenders())
        {
            appender.send(myLogEvent);
        }
    }

    void Logger::func(const string& aFuncName)
    {
        theFuncLogFlag = true;
        delete[] theFuncName;
        theFuncName = new char[aFuncName.size() + 1];
        strlcpy(theFuncName, aFuncName.c_str(), aFuncName.size() + 1);

        const LogEvent myLogEvent(getCallDepth(), aFuncName, true);
        foreach (const LogAppender& appender, LogConfiguration::instance().getAppenders())
        {
            appender.send(myLogEvent);
        }
        incCallDepth();
    }

    void Logger::start(const string& anAppName)
    {
        ProLogEvent myProLogEvent(anAppName);
        foreach (const LogAppender& appender, LogConfiguration::instance().getAppenders())
        {
            appender.send(myProLogEvent);
        }
    }

    void Logger::finish(const string& anAppName)
    {
        EpiLogEvent myEpiLogEvent(anAppName);
        foreach (const LogAppender& appender, LogConfiguration::instance().getAppenders())
        {
            appender.send(myEpiLogEvent);
        }
    }
} // namespace ta
