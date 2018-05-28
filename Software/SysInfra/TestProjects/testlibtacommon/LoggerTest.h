#pragma once

#include "ta/logger.h"
#include "ta/logconfiguration.h"
#include "ta/consoleappender.h"
#include "ta/fileappender.h"
#include "ta/syslogappender.h"
#include "ta/timeutils.h"
#include "ta/process.h"
#include "ta/common.h"

#include "cxxtest/TestSuite.h"
#include "boost/bind.hpp"
#include "boost/range/algorithm.hpp"
#include <algorithm>
#include <cstdio>
#include <string>
#include <fstream>
#include <sstream>

using std::string;

template <class T>
bool isType(const ta::LogAppender& aBase)
{
    return (typeid(aBase) == typeid(T));
}

// Redirect cout or cerr to a file
class ScopedStdDev2FileRedirector
{
    public:
        ScopedStdDev2FileRedirector(const std::string& anOutFileName, ta::ConsoleAppender::OutDev anOutDev)
            : theOutFileName(anOutFileName)
            , theOutDev(anOutDev)
            , theOutFileStream(anOutFileName.c_str())
            , theOrigStreamBuf((theOutDev==ta::ConsoleAppender::devStdOut)?std::cout.rdbuf(theOutFileStream.rdbuf()):std::cerr.rdbuf(theOutFileStream.rdbuf()))
        {}
        ~ScopedStdDev2FileRedirector()
        {
            if (theOutDev==ta::ConsoleAppender::devStdOut)
                std::cout.rdbuf(theOrigStreamBuf);
            else
                std::cerr.rdbuf(theOrigStreamBuf);
            remove(theOutFileName.c_str());
        }
    private:
        std::string theOutFileName;
        ta::ConsoleAppender::OutDev theOutDev;
        std::ofstream theOutFileStream;
        std::streambuf* theOrigStreamBuf;
};

class LoggerTest : public CxxTest::TestSuite
{
    void cleanLogs()
    {
        using namespace ta;
        remove(LogConfigFileName.c_str());
        const std::string DefaultFileLogOutputFileName(Process::getSelfShortName()+".log");
        remove(DefaultFileLogOutputFileName.c_str());
        remove(FileLogOutputFileName.c_str());
        const std::string FileLogOutputOldFileName(FileLogOutputFileName+".old");
        remove(FileLogOutputOldFileName.c_str());
    }
public:
    void setUp()
    {
        cleanLogs();
    }

    void tearDown()
    {
       cleanLogs();
    }

    void makeConsoleStdoutAppenderConfigFile()
    {
        std::ofstream myConfigFile(LogConfigFileName.c_str(), std::ios::out | std::ios::trunc);
        CxxTest::setAbortTestOnFail(true);
        TS_ASSERT(myConfigFile.is_open());
        CxxTest::setAbortTestOnFail(false);

        std::string myContent = "LogAppenders:\n\
                                {\n\
                                  SysLogAppender:{};\n\
                                  ConsoleAppender:\n\
                                  {\n\
                                    LogThreshold=\"DEBUG\";\n\
                                  };\n\
                                };\n";
        myConfigFile << myContent;
    }

    void makeConsoleStderrAppenderConfigFile()
    {
        std::ofstream myConfigFile(LogConfigFileName.c_str(), std::ios::out | std::ios::trunc);
        CxxTest::setAbortTestOnFail(true);
        TS_ASSERT(myConfigFile.is_open());
        CxxTest::setAbortTestOnFail(false);

        std::string myContent = "LogAppenders:\n\
                                {\n\
                                  SysLogAppender:{};\n\
                                  ConsoleAppender:\n\
                                  {\n\
                                    LogThreshold=\"DEBUG\";\n\
                                    OutDevice=\"stderr\";\n\
                                  };\n\
                                };\n";
        myConfigFile << myContent;
    }

    void makeFileAppenderConfigFile()
    {
        std::ofstream myConfigFile(LogConfigFileName.c_str(), std::ios::out | std::ios::trunc);
        CxxTest::setAbortTestOnFail(true);
        TS_ASSERT(myConfigFile.is_open());
        CxxTest::setAbortTestOnFail(false);

        std::string myContent = "LogAppenders:\n\
                                {\n\
                                  FileAppender:\n\
                                  {\n\
                                    LogThreshold=\"DEBUG\";\n\
                                    LogFileName=\""+FileLogOutputFileName+"\";\n\
                                  };\n\
                                };\n";
        myConfigFile << myContent;
    }

    void makeCorrectConfigFile()
    {
        std::ofstream myConfigFile(LogConfigFileName.c_str(), std::ios::out | std::ios::trunc);
        CxxTest::setAbortTestOnFail(true);
        TS_ASSERT(myConfigFile.is_open());
        CxxTest::setAbortTestOnFail(false);

        std::string myContent = "LogAppenders:\n\
                                {\n\
                                  SysLogAppender:\n\
                                  {\n\
                                    RemoteLogSvrTimeZone = \"CEDT\";\n\
                                  };\n\
                                  ConsoleAppender:\n\
                                  {\n\
                                    LogThreshold=\"DEBUG\";\n\
                                  };\n\
                                  FileAppender:\n\
                                  {\n\
                                    LogThreshold=\"DEBUG\";\n\
                                  };\n\
                                };\n";
        myConfigFile << myContent;
    }

    void testGoodConfiguration()
    {
        CxxTest::setAbortTestOnFail(true);
        makeCorrectConfigFile();

        ta::LogConfiguration& myLogConfiguration = ta::LogConfiguration::instance();
        TS_ASSERT(myLogConfiguration.load(LogConfigFileName));

        boost::ptr_vector<ta::LogAppender> myAppenders = myLogConfiguration.getAppenders();
#ifndef _WIN32
        TS_ASSERT_EQUALS(myAppenders.size(), 3U);
#else
        TS_ASSERT_EQUALS(myAppenders.size(), 2U);
#endif
        TS_ASSERT_EQUALS(boost::count_if(myAppenders, boost::bind(&isType<ta::ConsoleAppender>, _1) ), 1);
        TS_ASSERT_EQUALS(boost::count_if(myAppenders, boost::bind(&isType<ta::FileAppender>, _1) ), 1);
#ifndef _WIN32
        TS_ASSERT_EQUALS(boost::count_if(myAppenders, boost::bind(&isType<ta::SysLogAppender>, _1) ), 1);
        boost::ptr_vector<ta::LogAppender>::const_iterator it = boost::find_if(myAppenders, boost::bind(&isType<ta::SysLogAppender>, _1));
        if (it != myAppenders.end())
        {
            const ta::SysLogAppender* appender = dynamic_cast<const ta::SysLogAppender*>(&(*it));
            TS_ASSERT_EQUALS(appender->getRemoteLogSvrTimeZone(), "CEDT");
        }
        else
        {
            TS_FAIL("No syslog appender found");
        }
#endif

        ta::LogConfiguration::Config myMemConfig;
        myLogConfiguration.load(myMemConfig);
        TS_ASSERT(myLogConfiguration.getAppenders().empty());

        myMemConfig.fileAppender = true;
        myLogConfiguration.load(myMemConfig);
        myAppenders = myLogConfiguration.getAppenders();
        TS_ASSERT_EQUALS(myAppenders.size(), 1U);
        TS_ASSERT_EQUALS(boost::count_if(myAppenders, boost::bind(&isType<ta::FileAppender>, _1) ), 1);

        myMemConfig.consoleAppender = true;
        myLogConfiguration.load(myMemConfig);
        myAppenders = myLogConfiguration.getAppenders();
        TS_ASSERT_EQUALS(myAppenders.size(), 2U);
        TS_ASSERT_EQUALS(boost::count_if(myAppenders, boost::bind(&isType<ta::ConsoleAppender>, _1) ), 1);
        TS_ASSERT_EQUALS(boost::count_if(myAppenders, boost::bind(&isType<ta::FileAppender>, _1) ), 1);

#ifndef _WIN32
        myMemConfig.syslogAppender = true;
        myLogConfiguration.load(myMemConfig);
        myAppenders = myLogConfiguration.getAppenders();
        TS_ASSERT_EQUALS(myAppenders.size(), 3U);
        TS_ASSERT_EQUALS(boost::count_if(myAppenders, boost::bind(&isType<ta::ConsoleAppender>, _1) ), 1);
        TS_ASSERT_EQUALS(boost::count_if(myAppenders, boost::bind(&isType<ta::FileAppender>, _1) ), 1);
        TS_ASSERT_EQUALS(boost::count_if(myAppenders, boost::bind(&isType<ta::SysLogAppender>, _1) ), 1);
#endif

        myMemConfig.fileAppender = false;
        myLogConfiguration.load(myMemConfig);
        myAppenders = myLogConfiguration.getAppenders();
#ifndef _WIN32
        TS_ASSERT_EQUALS(myAppenders.size(), 2U);
        TS_ASSERT_EQUALS(boost::count_if(myAppenders, boost::bind(&isType<ta::ConsoleAppender>, _1) ), 1);
        TS_ASSERT_EQUALS(boost::count_if(myAppenders, boost::bind(&isType<ta::FileAppender>, _1) ), 0);
        TS_ASSERT_EQUALS(boost::count_if(myAppenders, boost::bind(&isType<ta::SysLogAppender>, _1) ), 1);
#else
        TS_ASSERT_EQUALS(myAppenders.size(), 1U);
        TS_ASSERT_EQUALS(boost::count_if(myAppenders, boost::bind(&isType<ta::ConsoleAppender>, _1) ), 1);
        TS_ASSERT_EQUALS(boost::count_if(myAppenders, boost::bind(&isType<ta::FileAppender>, _1) ), 0);
        TS_ASSERT_EQUALS(boost::count_if(myAppenders, boost::bind(&isType<ta::SysLogAppender>, _1) ), 0);
#endif
    }

    void testLoadSaveSysLogTimeZoneConfiguration()
    {
#ifdef _WIN32
        TS_SKIP("Skip syslog tests on Windows");
#else
        makeCorrectConfigFile();

        TS_ASSERT_EQUALS(ta::LogConfiguration::parseSysLogAppenderRemoteLogSvrTimeZone(LogConfigFileName), "CEDT");

        ta::LogConfiguration::saveSysLogAppenderRemoteLogSvrTimeZone(LogConfigFileName, "CET");
        TS_ASSERT_EQUALS(ta::LogConfiguration::parseSysLogAppenderRemoteLogSvrTimeZone(LogConfigFileName), "CET");

        const string myNonExistingFile = "/this/file/does/not/exist";
        remove(myNonExistingFile.c_str());
        TS_ASSERT_EQUALS(ta::LogConfiguration::parseSysLogAppenderRemoteLogSvrTimeZone(myNonExistingFile), "");
#endif
    }

    void testConsoleStdoutLogger()
    {
        CxxTest::setAbortTestOnFail(true);

        makeConsoleStdoutAppenderConfigFile();

        ta::LogConfiguration& myLogConfiguration = ta::LogConfiguration::instance();
        TS_ASSERT(myLogConfiguration.load(LogConfigFileName));

        boost::ptr_vector<ta::LogAppender> myAppenders = myLogConfiguration.getAppenders();
        TS_ASSERT_EQUALS(boost::count_if(myAppenders, boost::bind(&isType<ta::ConsoleAppender>, _1) ), 1);

        // Redirect cout to a file, thus we can grab logging to console and validate it
        const std::string ConsoleLogOutputFileName("consolelog.out");
        ScopedStdDev2FileRedirector myScopedCout2FileRedirector(ConsoleLogOutputFileName, ta::ConsoleAppender::devStdOut);

        FUNCLOG;
        DEBUGLOG("Some debug message");
        INFOLOG("Some info message");
        WARNLOG("Some warning message");
        ERRORLOG("Some error message");

        DEBUGLOG(boost::format("Some debug message with %1%") % "formatting");
        INFOLOG(boost::format("Some info message with %1%") % "formatting");
        WARNLOG(boost::format("Some warn message with %1%") % "formatting");
        ERRORLOG(boost::format("Some error message with %1%") % "formatting");

        DEBUGLOG("Some debug message with http://www.ta?%1%");
        INFOLOG("Some info message with http://www.ta?%1%");
        WARNLOG("Some warn message with http://www.ta?%1%");
        ERRORLOG("Some error message with http://www.ta?%1%");

        DEBUGLOG("Some debug message with http://www.ta?%s");
        INFOLOG("Some info message with http://www.ta?%s");
        WARNLOG("Some warn message with http://www.ta?%s");
        ERRORLOG("Some error message with http://www.ta?%s");

        std::ifstream myConsoleLogFile(ConsoleLogOutputFileName.c_str());
        TS_ASSERT(myConsoleLogFile.is_open());
        std::stringstream myStringStream;
        myStringStream << myConsoleLogFile.rdbuf();
        std::string myLogData = myStringStream.str();
        TS_ASSERT(myLogData.find("Some debug message") != std::string::npos);
        TS_ASSERT(myLogData.find("Some debug message with formatting") != std::string::npos);
        TS_ASSERT(myLogData.find("Some debug message with http://www.ta?%1%") != std::string::npos);
        TS_ASSERT(myLogData.find("Some error message with http://www.ta?%s") != std::string::npos);
        TS_ASSERT(myLogData.find(TA_BARE_FUNC) != std::string::npos);
        myConsoleLogFile.close();
    }

    void testConsoleStderrLogger()
    {
        CxxTest::setAbortTestOnFail(true);

        makeConsoleStderrAppenderConfigFile();

        ta::LogConfiguration& myLogConfiguration = ta::LogConfiguration::instance();
        TS_ASSERT(myLogConfiguration.load(LogConfigFileName));

        boost::ptr_vector<ta::LogAppender> myAppenders = myLogConfiguration.getAppenders();
        TS_ASSERT_EQUALS(boost::count_if(myAppenders, boost::bind(&isType<ta::ConsoleAppender>, _1) ), 1);

        // Redirect stderr to a file, thus we can grab loging to console and validate it
        const std::string ConsoleLogOutputFileName("consolelog.out");
        ScopedStdDev2FileRedirector myScopedCerr2FileRedirector(ConsoleLogOutputFileName, ta::ConsoleAppender::devStdErr);

        FUNCLOG;
        DEBUGLOG("Some debug message");
        INFOLOG("Some info message");
        WARNLOG("Some warning message");
        ERRORLOG("Some error message");

        DEBUGLOG(boost::format("Some debug message with %1%") % "formatting");
        INFOLOG(boost::format("Some info message with %1%") % "formatting");
        WARNLOG(boost::format("Some warn message with %1%") % "formatting");
        ERRORLOG(boost::format("Some error message with %1%") % "formatting");

        DEBUGLOG("Some debug message with http://www.ta?%1%");
        INFOLOG("Some info message with http://www.ta?%1%");
        WARNLOG("Some warn message with http://www.ta?%1%");
        ERRORLOG("Some error message with http://www.ta?%1%");

        DEBUGLOG("Some debug message with http://www.ta?%s");
        INFOLOG("Some info message with http://www.ta?%s");
        WARNLOG("Some warn message with http://www.ta?%s");
        ERRORLOG("Some error message with http://www.ta?%s");

        std::ifstream myConsoleLogFile(ConsoleLogOutputFileName.c_str());
        TS_ASSERT(myConsoleLogFile.is_open());
        std::stringstream myStringStream;
        myStringStream << myConsoleLogFile.rdbuf();
        std::string myLogData = myStringStream.str();
        TS_ASSERT(myLogData.find("Some debug message") != std::string::npos);
        TS_ASSERT(myLogData.find("Some debug message with formatting") != std::string::npos);
        TS_ASSERT(myLogData.find("Some debug message with http://www.ta?%1%") != std::string::npos);
        TS_ASSERT(myLogData.find("Some error message with http://www.ta?%s") != std::string::npos);
        TS_ASSERT(myLogData.find(TA_BARE_FUNC) != std::string::npos);
        myConsoleLogFile.close();
    }

    void testFileLogger()
    {
        CxxTest::setAbortTestOnFail(true);

        makeFileAppenderConfigFile();

        ta::LogConfiguration& myLogConfiguration = ta::LogConfiguration::instance();
        TS_ASSERT(myLogConfiguration.load(LogConfigFileName));

        boost::ptr_vector<ta::LogAppender> myAppenders = myLogConfiguration.getAppenders();
        TS_ASSERT_EQUALS(boost::count_if(myAppenders, boost::bind(&isType<ta::FileAppender>, _1) ), 1);

        FUNCLOG;
        DEBUGLOG("Some debug message");
        INFOLOG("Some info message");
        WARNLOG("Some warning message");
        ERRORLOG("Some error message");

        DEBUGLOG(boost::format("Some debug message with %1%") % "formatting");
        INFOLOG(boost::format("Some info message with %1%") % "formatting");
        WARNLOG(boost::format("Some warn message with %1%") % "formatting");
        ERRORLOG(boost::format("Some error message with %1%") % "formatting");

        DEBUGLOG("Some debug message with http://www.ta?%1%");
        INFOLOG("Some info message with http://www.ta?%1%");
        WARNLOG("Some warn message with http://www.ta?%1%");
        ERRORLOG("Some error message with http://www.ta?%1%");

        DEBUGLOG("Some debug message with http://www.ta?%s");
        INFOLOG("Some info message with http://www.ta?%s");
        WARNLOG("Some warn message with http://www.ta?%s");
        ERRORLOG("Some error message with http://www.ta?%s");

        std::ifstream myFileLogFile(FileLogOutputFileName.c_str());
        TS_ASSERT(myFileLogFile.is_open());
        std::stringstream myStringStream;
        myStringStream << myFileLogFile.rdbuf();
        std::string myLogData = myStringStream.str();
        TS_ASSERT(myLogData.find("Some debug message") != std::string::npos);
        TS_ASSERT(myLogData.find("Some debug message with formatting") != std::string::npos);
        TS_ASSERT(myLogData.find("Some debug message with http://www.ta?%1%") != std::string::npos);
        TS_ASSERT(myLogData.find("Some error message with http://www.ta?%s") != std::string::npos);
        TS_ASSERT(myLogData.find(TA_BARE_FUNC) != std::string::npos);
        myFileLogFile.close();
    }

    void testFileLoggerRotation()
    {
        CxxTest::setAbortTestOnFail(true);

        remove(FileLogOutputFileName.c_str());
        makeFileAppenderConfigFile();

        ta::LogConfiguration& myLogConfiguration = ta::LogConfiguration::instance();
        TS_ASSERT(myLogConfiguration.load(LogConfigFileName));

        boost::ptr_vector<ta::LogAppender> myAppenders = myLogConfiguration.getAppenders();
        TS_ASSERT_EQUALS(boost::count_if(myAppenders, boost::bind(&isType<ta::FileAppender>, _1) ), 1);

        FUNCLOG;
        for (int i=0; i<2048; i++)
        {
            DEBUGLOG(boost::format("%Test debug: %i") % i);
            INFOLOG(boost::format("Test info: %i") % i);
            WARNLOG(boost::format("Test warn: %i") % i);
            ERRORLOG(boost::format("Test error: %i") % i);
        }

        std::ifstream myFileLogFile(FileLogOutputFileName.c_str());
        TS_ASSERT(myFileLogFile.is_open());

        std::string FileLogOutputFileNameOld = FileLogOutputFileName + ".old";
        std::ifstream myFileLogFileOld(FileLogOutputFileNameOld.c_str());
        TS_ASSERT(myFileLogFileOld.is_open());
    }

    void testDeveloperLogging()
    {
        CxxTest::setAbortTestOnFail(true);

        // given
        makeFileAppenderConfigFile();

        // when
        ta::LogConfiguration& myLogConfiguration = ta::LogConfiguration::instance();
        // then
        TS_ASSERT(myLogConfiguration.load(LogConfigFileName));

        /// when
        boost::ptr_vector<ta::LogAppender> myAppenders = myLogConfiguration.getAppenders();
        // then
        TS_ASSERT_EQUALS(boost::count_if(myAppenders, boost::bind(&isType<ta::FileAppender>, _1) ), 1);

        // when
        FUNCLOG;
        DEBUGLOG2("Some user debug message", "Some developer debug message");
        INFOLOG("Some user info message");
        WARNDEVLOG("Some developer warning message");
        ERRORLOG2("Some user error message", "Some developer error message");
        // then
        std::ifstream myFileLogFile(FileLogOutputFileName.c_str());
        TS_ASSERT(myFileLogFile.is_open());
        std::stringstream myStringStream;
        myStringStream << myFileLogFile.rdbuf();
        const std::string myLogText = myStringStream.str();
        TS_ASSERT(myLogText.find("Some user debug message") != std::string::npos);
        TS_ASSERT(myLogText.find("Some developer debug message") != std::string::npos);
        TS_ASSERT(myLogText.find("Some user info message") != std::string::npos);
        TS_ASSERT(myLogText.find("Some developer warning message") != std::string::npos);
        TS_ASSERT(myLogText.find("Some user error message") != std::string::npos);
        TS_ASSERT(myLogText.find("Some developer error message") != std::string::npos);
        TS_ASSERT(myLogText.find(TA_BARE_FUNC) != std::string::npos);
        myFileLogFile.close();
        remove(FileLogOutputFileName.c_str());

        // when
        const std::string myFilteredLogText = ta::LogAppender::filterOutDevelEntries(myLogText);
        TS_TRACE(myLogText.c_str());
        TS_TRACE(myFilteredLogText.c_str());
        // then
        TS_ASSERT(myFilteredLogText.find("Some user debug message") != std::string::npos);
        TS_ASSERT(myFilteredLogText.find("Some developer debug message") == std::string::npos);
        TS_ASSERT(myFilteredLogText.find("Some user info message") != std::string::npos);
        TS_ASSERT(myFilteredLogText.find("Some developer warning message") == std::string::npos);
        TS_ASSERT(myFilteredLogText.find("Some user error message") != std::string::npos);
        TS_ASSERT(myFilteredLogText.find("Some developer error message") == std::string::npos);
        TS_ASSERT(myFilteredLogText.find(TA_BARE_FUNC) != std::string::npos);
    }

private:
    static const std::string LogConfigFileName;
    static const std::string FileLogOutputFileName;

};
const std::string LoggerTest::LogConfigFileName("logconfig.ini");
const std::string LoggerTest::FileLogOutputFileName("fileappender.log");
