#include "ta/fileappender.h"
#include "ta/process.h"
#include "ta/thread.h"
#include "ta/common.h"
#include "ta/utils.h"

#include <iostream>
#ifdef _WIN32
# include <io.h>
# include <fcntl.h>
# include <sys/stat.h>
# include <sys/types.h>
# include <share.h>
#else
# include <unistd.h>
#endif
#include <cassert>

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
            const string myMsg = str(boost::format("%s(): %s") % aLogEvent.func
                                     % aLogEvent.msg);
            return str(boost::format(myTempl) % LogAppender::getTimeStamp()
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
            return  str(boost::format(myTempl) % LogAppender::getTimeStamp()
                        % LogAppender::getSelfAppName()
                        % Process::getSelfPid()
                        % ThreadUtils::getSelfId()
                        % str(aLogEvent.level)
                        % (aLogEvent.isDevel ? LoggerDevelTag : "")
                        % getCallDepthStr(aLogEvent.callDepth)
                        % myMsg);
        }
    }// namespace


    Mutex FileAppender::theFileMutex;
    static const size_t DefMaxLogSize = 512*1024;
    static const string DefOldFileSuffix = ".old";

    FileAppender::FileAppender(const Args& anArgs)
        : theLogFileName(anArgs.logFileName)
        , theLogThreshold(anArgs.logThreshold)
    {
        initLogFile();
    }


    FileAppender::FileAppender(const FileAppender& aFileAppender)
        : Clonable<FileAppender, LogAppender>()
        , theLogFileName(aFileAppender.theLogFileName)
        , theLogThreshold(aFileAppender.theLogThreshold)
    {
        initLogFile();
    }

    FileAppender::~FileAppender()
    {}

    void FileAppender::initLogFile()
    {
        if (theLogFile.is_open())
            theLogFile.close();
        theLogFile.clear();
        try { ta::createParentDir(theLogFileName); } catch  (...) {}
        theLogFile.open(theLogFileName.c_str(), std::ios::out | std::ios::app);
        if (theLogFile.is_open())
            theLogFile.seekp(0, std::ios_base::end);
    }

    /**
    * @pre logfile is opened
    */
    bool FileAppender::rotateLogFile() const
    {
        assert(theLogFile.is_open());
        size_t mySize = static_cast<size_t>(theLogFile.tellp());
        if (mySize > DefMaxLogSize)
        {
            theLogFile.close();
            std::ofstream myOldFile((theLogFileName + DefOldFileSuffix).c_str(), std::ios::out | std::ios::trunc);
            if (!myOldFile.is_open())
                return false;
            std::ifstream myLogFile(theLogFileName.c_str());
            myLogFile.seekg(0);
            myOldFile << myLogFile.rdbuf();
            myOldFile.close(), myLogFile.close();
            if (!clearLogFile())
                return false;
            theLogFile.open(theLogFileName.c_str(), std::ios::out | std::ios::app);
            if (!theLogFile.is_open())
                return false;
        }
        return true;
    }

    bool FileAppender::clearLogFile() const
    {
#ifdef _WIN32
        int myFileHandle = _sopen(theLogFileName.c_str(), _O_RDWR, _SH_DENYNO, _S_IREAD | _S_IWRITE );
        if (myFileHandle == -1)
            return false;
        if (_chsize( myFileHandle, 0) != 0)
            return  _close(myFileHandle), false;
        _close( myFileHandle);
        return true;
#else
        if (truncate(theLogFileName.c_str(), 0) != 0)
            return false;
        return true;
#endif
    }

    void FileAppender::send(const LogEvent& aLogEvent) const
    {
        if (theLogThreshold < aLogEvent.level)
            return;
        if (!theLogFile.is_open())
            return;
        if (!rotateLogFile())
            return;

        string myMsg;
        if (aLogEvent.level == LogLevel::Error && aLogEvent.isDevel)
            myMsg = formatLogMsgFull(aLogEvent);
        else
            myMsg = formatLogMsgBrief(aLogEvent);

        {
            ScopedLock lock(theFileMutex);
            theLogFile << myMsg << std::endl;
        }
    }
}
