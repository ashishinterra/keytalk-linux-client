#include "ta/logappender.h"
#include "ta/process.h"
#include "ta/encodingutils.h"
#include "ta/timeutils.h"
#include "ta/utils.h"
#include "ta/singletonholder.hpp"
#include "ta/osinfoutils.h"
#include "ta/common.h"

#include "boost/static_assert.hpp"
#include "boost/format.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/date_time/posix_time/posix_time.hpp"
#include "boost/ref.hpp"

using std::string;
using std::vector;

namespace ta
{
    // Internal stuff
    namespace
    {
        class AppNameSingleton: public SingletonHolder<AppNameSingleton>
        {
            friend class SingletonHolder<AppNameSingleton>;
            friend class DefaultCreationPolicy<AppNameSingleton>;
        public:
            AppNameSingleton()
            {
                try {
                    theAppName = Process::getSelfShortName();
                } catch (ProcessGetNameError&)  {
                    theAppName = "<Unknown application>";
                }
            }
            inline string get() const {return theAppName;};
        private:
            string theAppName;
        };

    } // internal ns

    namespace LogLevel
    {
        BOOST_STATIC_ASSERT(_last-_first+1 == sizeof(strs)/sizeof(strs[0]));

        string str(val aVal)
        {
            return strs[aVal-_first];
        }
        size_t getMaxStrLen()
        {
            size_t myRetVal = 0;
            foreach(const string& level, strs)
            {
                if (myRetVal < level.length())
                {
                    myRetVal = level.length();
                }
            }
            return myRetVal;
        }
        bool parse(const string& aLogLevelStr, val& aLogLevel)
        {
            for (int iLevel = _first; iLevel <= _last; ++iLevel)
            {
                const val myLevel = static_cast<val>(iLevel);
                if (str(myLevel) == aLogLevelStr)
                {
                    aLogLevel = myLevel;
                    return true;
                }
            }
            aLogLevel = Info;
            return false;
        }

        bool isLogLevel(int aVal)
        {
            return (aVal >= _first && aVal <= _last);
        }
    } // namespace LogLevel

    LogAppender::LogAppender()
    {}

    LogAppender::~LogAppender()
    {}

    string LogAppender::getSelfAppName()
    {
        // Normally retrieving app name is a relatively expensive operation (fork),
        // thus we do it once per process (singleton).
        AppNameSingleton& appNameSingleton  = AppNameSingleton::instance();
        return appNameSingleton.get();
    }

#ifdef _WIN32
    string LogAppender::getTimeStamp()
#else
    string LogAppender::getTimeStamp(const string& aTargetTz)
#endif
    {
        boost::posix_time::ptime myNow = boost::posix_time::microsec_clock::local_time();
#ifndef _WIN32
        if (!aTargetTz.empty() && aTargetTz != TimeUtils::LocalTimeZone)
        {
            try {
                const int myDeltaMinutes = TimeUtils::getTimeZoneAsUtcOffset(aTargetTz) - TimeUtils::getLocalTimeZoneAsUtcOffset();
                myNow += boost::posix_time::minutes(myDeltaMinutes);
            } catch (...) {
                // best-effort
            }
        }
#endif
        string myNowStr = to_simple_string(myNow);
        if (myNowStr.length() == 20)
        {
            // fractional seconds are not shown when zero, but we want it to be shown
            myNowStr += ".000";
        }
        myNowStr.resize(24);  // cut off precision from microseconds to milliseconds
        return myNowStr;
    }

    vector<string> LogAppender::splitMsg(const string& aMsg, size_t aMaxSize)
    {
        vector<string> myRetVal;
        for (size_t start = 0; start < aMsg.size(); start += aMaxSize)
        {
            myRetVal.push_back(aMsg.substr(start, aMaxSize));
        }
        return myRetVal;
    }

    string LogAppender::filterOutDevelEntries(const string& aText)
    {
        vector<string> myFilteredLines;
        foreach (const string& line, ta::Strings::split(aText, '\n'))
        {
            if (line.find(LoggerDevelTag) == string::npos)
            {
                myFilteredLines.push_back(line);
            }
        }
        return ta::Strings::join(myFilteredLines, '\n');
    }

    ProLogEvent::ProLogEvent(const string& anAppName)
        : LogEvent(LogLevel::Info, 0, "", "**************************** "+ makeMsg(anAppName) + ". Logging started ****************************", false)
    {}
    string ProLogEvent::makeMsg(const string& anAppName)
    {
        string myOsVer = "OS: <unknown>";
        try
        {
            OsInfoUtils::Version myVer = OsInfoUtils::getVersion();
            myOsVer = str(boost::format("OS: %s %s") % myVer.name % myVer.ver);
        }
        catch (std::runtime_error&)
        {}
        return str(boost::format("%s. %s") % anAppName % myOsVer);
    }
    EpiLogEvent::EpiLogEvent(const string& anAppName)
        : LogEvent(LogLevel::Info, 0, "", "**************************** "+ anAppName + ". Logging finished ****************************", false)
    {}

}
