#include "consoleappender.h"
#include "ta/process.h"
#include "ta/thread.h"
#include "ta/strings.h"
#include "ta/common.h"

#include "boost/format.hpp"
#include <iostream>

namespace ta
{
    using std::string;

    namespace
    {
        //
        // Log messages formatting
        //

        string getCallDepthStr(size_t aCallDepth)
        {
            return string( 2*aCallDepth, ' ' );
        }

        string formatLogMsgBrief(const LogEvent& aLogEvent)
        {
            const string myTempl = "%s %s <%5u> <<%5u>> [%-" + Strings::toString(LogLevel::getMaxStrLen()) + "s]%s %s%s";
            const string myMsg = str(boost::format("%s(): %s") % aLogEvent.func % aLogEvent.msg);
            return str(boost::format(myTempl)
                       % LogAppender::getTimeStamp()
                       % LogAppender::getSelfAppName()
                       % Process::getSelfPid()
                       % ThreadUtils::getSelfId()
                       % str(aLogEvent.level)
                       % (aLogEvent.isDevel ? LoggerDevelTag : "")
                       % getCallDepthStr(aLogEvent.callDepth)
                       % myMsg);
        }
        string formatLogMsgFull(const LogEvent& aLogEvent)
        {
            const string myTempl = "%s %s <%5u> <<%5u>> [%-" + Strings::toString(LogLevel::getMaxStrLen()) + "s]%s %s%s";
            const string myMsg = str(boost::format("%s(), file %s:%u: %s") % aLogEvent.func
                                     % aLogEvent.file
                                     % aLogEvent.line
                                     % aLogEvent.msg);
            return  str(boost::format(myTempl)
                        % LogAppender::getTimeStamp()
                        % LogAppender::getSelfAppName()
                        % Process::getSelfPid()
                        % ThreadUtils::getSelfId()
                        % str(aLogEvent.level)
                        % (aLogEvent.isDevel ? LoggerDevelTag : "")
                        % getCallDepthStr(aLogEvent.callDepth)
                        % myMsg);
        }
    }// namespace


    Mutex ConsoleAppender::theConsoleMutex;

    ConsoleAppender::ConsoleAppender(const Args& anArgs)
        : theLogThreshold(anArgs.logThreshold)
        , theOutDev(anArgs.outDev)
    {}

    ConsoleAppender::~ConsoleAppender()
    {}

    void ConsoleAppender::send(const LogEvent& aLogEvent) const
    {
        if (theLogThreshold < aLogEvent.level)
            return;
        string myMsg;
        if (aLogEvent.level == LogLevel::Error && aLogEvent.isDevel)
            myMsg = formatLogMsgFull(aLogEvent);
        else
            myMsg = formatLogMsgBrief(aLogEvent);

        {
            ScopedLock lock(theConsoleMutex);
            if (theOutDev == devStdOut)
                std::cout << myMsg << std::endl;
            else if (theOutDev == devStdErr)
                std::cerr << myMsg << std::endl;
        }
    }
}
