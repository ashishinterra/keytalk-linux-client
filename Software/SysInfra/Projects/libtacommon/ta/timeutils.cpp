#include "timeutils.h"
#include "process.h"
#include "strings.h"
#include "common.h"

#include <cstdlib>
#include <cassert>
#include <stdexcept>
#include <sys/types.h>
#ifdef _WIN32
# include <windows.h>
#else
# include <sys/time.h>
#endif
#include "boost/regex.hpp"


using std::string;

namespace ta
{
    namespace TimeUtils
    {
        namespace
        {
            tm getLocalTime(const time_t aTimestamp = time(NULL))
            {
                tm myTm;
#ifdef _WIN32
                _localtime32_s(&myTm, &aTimestamp);
#else
                localtime_r(&aTimestamp, &myTm);
#endif
                return myTm;
            }

            int diffUtc()
            {
                time_t myTimestampNow = time(NULL);
                tm myUtcNow;
#ifdef _WIN32
                _gmtime32_s( &myUtcNow, &myTimestampNow);
#else
                gmtime_r( &myTimestampNow,  &myUtcNow);
#endif
                tm myLocalTime = getLocalTime(myTimestampNow);
                return (int)difftime(mktime(&myLocalTime), mktime(&myUtcNow));
            }

            string formatDays(const unsigned int days)
            {
                return str(boost::format("%u day%s") % days % (days!=1?"s":""));
            }

            string formatHours(const unsigned int hours)
            {
                return str(boost::format("%u hour%s") % hours % (hours!=1?"s":""));
            }

            string formatMinutes(const unsigned int minutes)
            {
                return str(boost::format("%u minute%s") % minutes % (minutes!=1?"s":""));
            }

            string formatSeconds(const unsigned int seconds)
            {
                return str(boost::format("%u second%s") % seconds % (seconds!=1?"s":""));
            }
        }


        void sleep(const unsigned int aMsec)
        {
#ifdef _WIN32
            ::Sleep(aMsec);
#else
            timespec myTime;
            myTime.tv_sec = aMsec / 1000;
            myTime.tv_nsec = ((long)(aMsec % 1000)) * 1000000;
            nanosleep (&myTime, NULL);
#endif
            // or, as an alternative:
            // boost::xtime delay;
            // to_time(aMsec, delay);
            // boost::thread().sleep(delay);
            // see also http://www.rsdn.ru/forum/message/2843715.aspx
        }


        string getUtcNowAsIso8601()
        {
            time_t myNow = time(NULL);
            tm myUtcNowTm;
#ifdef _WIN32
            _gmtime32_s( &myUtcNowTm, &myNow);
#else
            gmtime_r(&myNow, &myUtcNowTm);
#endif
            char myTimeBuf[64] = {};
            string myOrigLocale = setlocale(LC_TIME, NULL);
            setlocale(LC_TIME, "UTC");
            strftime(myTimeBuf, sizeof(myTimeBuf)-1, "%Y-%m-%dT%H:%M:%S+0000", &myUtcNowTm);
            setlocale(LC_TIME, myOrigLocale.c_str());
            return myTimeBuf;
        }

        string timestampToIso8601(const time_t aTimestamp)
        {
            tm myTm;
#ifdef _WIN32
            _gmtime32_s( &myTm, &aTimestamp);
#else
            gmtime_r(&aTimestamp, &myTm);
#endif
            char myTimeBuf[64] = {};
            string myOrigLocale = setlocale(LC_TIME, NULL);
            setlocale(LC_TIME, "UTC");
            strftime(myTimeBuf, sizeof(myTimeBuf)-1, "%Y-%m-%dT%H:%M:%S+0000", &myTm);
            setlocale(LC_TIME, myOrigLocale.c_str());
            return myTimeBuf;
        }

        time_t parseUtcIso8601(const std::string& anUtcTimeStr)
        {
            try
            {
                boost::regex myRegEx("^(?<year>\\d{4})-(?<month>\\d{1,2})-(?<mday>\\d{1,2})(?:T|\\s+)(?<hour>\\d{1,2}):(?<minute>\\d{1,2}):(?<second>\\d{1,2})(?:\\.\\d{1,6})?(?:Z|\\+0000|\\+00:00|\\+00)$");
                boost::cmatch myMatch;
                if (!regex_match(anUtcTimeStr.c_str(), myMatch, myRegEx))
                {
                    TA_THROW_MSG(std::invalid_argument, boost::format("Cannot parse RFC 8601 datetime (1) from string '%1%'") % anUtcTimeStr);
                }

                tm myTmTime;
                myTmTime.tm_year = Strings::parse<int>(myMatch["year"]) - 1900;
                myTmTime.tm_mon = Strings::parse<int>(myMatch["month"]) - 1;
                myTmTime.tm_mday = Strings::parse<int>(myMatch["mday"]);
                myTmTime.tm_hour = Strings::parse<int>(myMatch["hour"]);
                myTmTime.tm_min = Strings::parse<int>(myMatch["minute"]);
                myTmTime.tm_sec = Strings::parse<int>(myMatch["second"]) + diffUtc();

                //@todo tm_gmtoff and tm_isdst may become deprecated, so relying on them may not be robust
#ifndef _WIN32
                myTmTime.tm_gmtoff = 0;
#endif
                myTmTime.tm_isdst = 0;

                time_t myRetVal = mktime(&myTmTime);
                if (myRetVal == (time_t)(-1))
                {
                    TA_THROW_MSG(std::invalid_argument, boost::format("Cannot parse RFC 8601 datetime (2) from string '%1%'") % anUtcTimeStr);
                }

                return myRetVal;
            }
            catch (const std::invalid_argument&)
            {
                throw;
            }
            catch (const std::exception& e)
            {
                TA_THROW_MSG(std::logic_error, boost::format("Cannot parse RFC 8601 datetime from string '%1%'. %2%") % anUtcTimeStr % e.what());
            }
        }

        std::string getUtcNowAsYYYYmmdd()
        {
            time_t myNow = time(NULL);
            tm myUtcNowTm;
#ifdef _WIN32
            _gmtime32_s( &myUtcNowTm, &myNow);
#else
            gmtime_r(&myNow, &myUtcNowTm);
#endif
            char myTimeBuf[16] = {};
            string myOrigLocale = setlocale(LC_TIME, NULL);
            setlocale(LC_TIME, "UTC");
            strftime(myTimeBuf, sizeof(myTimeBuf)-1, "%Y%m%d", &myUtcNowTm);
            setlocale(LC_TIME, myOrigLocale.c_str());
            return myTimeBuf;
        }

        std::string getNowAsLocalStr(const bool aWithSecondPrecision)
        {
            const LocalTime lt;
            string myFormattedLocalTime = str(boost::format("%02d-%02d-%04d %02d:%02d") % lt.getDay() % lt.getMonth() % lt.getYear() % lt.getHour() % lt.getMinute());
            if (aWithSecondPrecision)
            {
                myFormattedLocalTime += str(boost::format(":%02d") % lt.getSecond());
            }
            return myFormattedLocalTime;
        }

        string timestampToUtcStr(const time_t aTimestamp, const bool aWithSecondPrecision)
        {
            tm myUtcTm;
#ifdef _WIN32
            _gmtime32_s(&myUtcTm, &aTimestamp);
#else
            gmtime_r(&aTimestamp, &myUtcTm);
#endif
            string myRetVal = str(boost::format("%02d-%02d-%04d %02d:%02d") % myUtcTm.tm_mday % (myUtcTm.tm_mon+1) % (myUtcTm.tm_year+1900) % myUtcTm.tm_hour % myUtcTm.tm_min);
            if (aWithSecondPrecision)
            {
                myRetVal += str(boost::format(":%02d") % myUtcTm.tm_sec);
            }
            return myRetVal;
        }

        string timestampToLocalStr(const time_t aTimestamp, const bool aWithSecondPrecision)
        {
            const tm myLocalTime = getLocalTime(aTimestamp);
            string myRetVal = str(boost::format("%02d-%02d-%04d %02d:%02d") % myLocalTime.tm_mday % (myLocalTime.tm_mon+1) % (myLocalTime.tm_year+1900) % myLocalTime.tm_hour % myLocalTime.tm_min);
            if (aWithSecondPrecision)
            {
                myRetVal += str(boost::format(":%02d") % myLocalTime.tm_sec);
            }
            return myRetVal;
        }

        time_t parseTimestampFromLocalStr(const string& aDateTimeStr)
        {
            try
            {
                boost::regex myRegEx("\\s*(?<day>\\d{2})-(?<month>\\d{2})-(?<year>\\d{4})\\s+(?<hour>\\d{2})\\:(?<min>\\d{2})(?:\\:(?<sec>\\d{2}))?\\s*");
                boost::cmatch myMatch;
                if (!regex_match(aDateTimeStr.c_str(), myMatch, myRegEx))
                {
                    TA_THROW_MSG(std::invalid_argument, boost::format("Cannot parse dd-mm-YYYY HH:MM[:SS] datetime (1) from string '%1%'") % aDateTimeStr);
                }

                tm myTm = getLocalTime();// init with local time just to make sure tm_gmtoff is properly set
                myTm.tm_isdst = 0; // ignore daylight saving setting for now, we will correct it later
                myTm.tm_mday = Strings::parse<unsigned int>(myMatch["day"]);
                myTm.tm_mon = Strings::parse<unsigned int>(myMatch["month"]) - 1;
                myTm.tm_year = Strings::parse<unsigned int>(myMatch["year"]) - 1900;
                myTm.tm_hour = Strings::parse<unsigned int>(myMatch["hour"]);
                myTm.tm_min = Strings::parse<unsigned int>(myMatch["min"]);
                myTm.tm_sec = myMatch["sec"].matched ? Strings::parse<unsigned int>(myMatch["sec"]) : 0;

                time_t myTimestamp = mktime(&myTm);
                if (myTimestamp == (time_t)(-1))
                {
                    TA_THROW_MSG(std::invalid_argument, boost::format("Cannot parse dd-mm-YYYY HH:MM[:SS] datetime (2) from string '%1%'") % aDateTimeStr);
                }

                // correct for daylight saving
                if (getLocalTime(myTimestamp).tm_isdst > 0)
                {
                    myTimestamp -= SecondsInHour;
                }

                return myTimestamp;
            }
            catch (const std::invalid_argument&)
            {
                throw;
            }
            catch (const std::exception& e)
            {
                TA_THROW_MSG(std::logic_error, boost::format("Cannot parse dd-mm-YYYY HH:MM[:SS] datetime from string '%1%'. %2%") % aDateTimeStr % e.what());
            }
        }


        void breakTimeInterval(const unsigned int aTotalSeconds, unsigned int& aDays, unsigned int& aHours, unsigned int& aMinutes, unsigned int& aSeconds)
        {
            std::div_t myTtlParts = div((int)aTotalSeconds, (int)SecondsInDay);
            aDays = myTtlParts.quot;
            myTtlParts = div((int)myTtlParts.rem, (int)SecondsInHour);
            aHours = myTtlParts.quot;
            myTtlParts = div((int)myTtlParts.rem, (int)SecondsInMinute);
            aMinutes = myTtlParts.quot;
            aSeconds = myTtlParts.rem;
        }

        unsigned int makeTimeInterval(const unsigned int aDays, const unsigned int aHours, const unsigned int aMinutes, const unsigned int aSeconds)
        {
            return (aDays * SecondsInDay) + (aHours * SecondsInHour) + (aMinutes * SecondsInMinute) + aSeconds;
        }

        string formatTimeInterval(const unsigned int aTotalSeconds, const Precision aPrecision)
        {
            unsigned int mySeconds = 0, myMinutes = 0, myHours = 0, myDays = 0;
            breakTimeInterval(aTotalSeconds, myDays, myHours, myMinutes, mySeconds);

            std::vector<std::string> myTimeComponents;

            if (myDays != 0)
            {
                myTimeComponents.push_back(formatDays(myDays));
            }

            if (myHours != 0)
            {
                myTimeComponents.push_back(formatHours(myHours));
            }

            if (aPrecision == precisionFull || myDays == 0)
            {
                if (myMinutes != 0)
                {
                    myTimeComponents.push_back(formatMinutes(myMinutes));
                }

                if (aPrecision == precisionFull || myHours == 0)
                {
                    if (mySeconds != 0)
                    {
                        myTimeComponents.push_back(formatSeconds(mySeconds));
                    }
                }
            }

            switch (myTimeComponents.size())
            {
            case 4:
                return myTimeComponents[0] + ", " + myTimeComponents[1] + ", " + myTimeComponents[2] + " and " + myTimeComponents[3];
            case 3:
                return myTimeComponents[0] + ", " + myTimeComponents[1] + " and " + myTimeComponents[2];
            case 2:
                return myTimeComponents[0] + " and " + myTimeComponents[1];
            case 1:
                return myTimeComponents[0];
            case 0:
                return "0 seconds";
            default:
                TA_THROW_MSG(std::logic_error, boost::format("Too many time components: %u") % myTimeComponents.size());
            }
        }


        LocalTime::LocalTime()
        {
            init();
        }

        LocalTime::LocalTime(const long long anOffsetMsec)
        {
            init(anOffsetMsec);
        }

        LocalTime::~LocalTime()
        {}

        void LocalTime::init(const long long anOffsetMsec)
        {
#ifdef _WIN32
            SYSTEMTIME mySysTime;
            ::GetLocalTime(&mySysTime);
            FILETIME myFileTime;
            ::SystemTimeToFileTime(&mySysTime, &myFileTime);
            theTotalMsec = ((LARGE_INTEGER*)&myFileTime)->QuadPart / 10000 + anOffsetMsec;

            theYear     = mySysTime.wYear;
            theMonth    = mySysTime.wMonth;
            theDay      = mySysTime.wDay;
            theHour     = mySysTime.wHour;
            theMinute   = mySysTime.wMinute;
            theSecond   = mySysTime.wSecond;
            theMillisec = mySysTime.wMilliseconds;
#else
            timeval myTmNow;
            gettimeofday (&myTmNow, 0);
            theTotalMsec = ((unsigned long long int )myTmNow.tv_usec / 1000) + ((unsigned long long int )myTmNow.tv_sec) * 1000 + anOffsetMsec;

            timeval myTv;
            myTv.tv_sec = theTotalMsec / 1000;
            myTv.tv_usec = (theTotalMsec % 1000) * 1000;

            tm myLocTime = getLocalTime((time_t)myTv.tv_sec);
            theYear     = myLocTime.tm_year + 1900;
            theMonth    = myLocTime.tm_mon + 1;
            theDay      = myLocTime.tm_mday;
            theHour     = myLocTime.tm_hour;
            theMinute   = myLocTime.tm_min;
            theSecond   = myLocTime.tm_sec;
            theMillisec = theTotalMsec % 1000;
#endif
        }

        unsigned short LocalTime::getYear() const
        {
            return theYear;
        }

        unsigned short LocalTime::getMonth() const
        {
            return theMonth;
        }

        unsigned short LocalTime::getDay() const
        {
            return theDay;
        }

        unsigned short LocalTime::getHour() const
        {
            return theHour;
        }

        unsigned short LocalTime::getMinute() const
        {
            return theMinute;
        }

        unsigned short LocalTime::getSecond() const
        {
            return theSecond;
        }

        unsigned short LocalTime::getMillisec() const
        {
            return theMillisec;
        }

        unsigned long long int LocalTime::getTotalMillisec() const
        {
            return theTotalMsec;
        }

        long long LocalTime::operator- (const LocalTime& aTime) const
        {
            return theTotalMsec - aTime.theTotalMsec;
        }

        int getTimeZoneAsUtcOffset(const string& aTimeZoneId)
        {
            for (size_t i = 0; i < sizeof(TimeZones)/sizeof(TimeZones[0]); ++i)
            {
                if (TimeZones[i].id == aTimeZoneId)
                {
                    return TimeZones[i].utc_offset_minutes;
                }
            }
            TA_THROW_MSG(std::invalid_argument, "Invalid timezone id " + aTimeZoneId);
        }

        string fmtTz(const int anUtcOffsetMinutes)
        {
            const std::div_t res = std::div(anUtcOffsetMinutes, 60);
            const unsigned int hours = abs(res.quot);
            const unsigned int minutes = abs(res.rem);
            const char sign = anUtcOffsetMinutes >= 0 ? '+' : '-';
            return str(ta::safe_format("UTC%c%u:%02u") % sign % hours % minutes);
        }

#ifndef _WIN32
        int getLocalTimeZoneAsUtcOffset()
        {
            const string myCmd = "date +\"%z\"";
            string myStdOut, myStdErr;
            const int myExitCode = Process::shellExecSync(myCmd, myStdOut, myStdErr);
            if (myExitCode != 0)
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Command '%s' finished with error code %d. %s") % myCmd % myExitCode % myStdErr);
            }

            // Expected output is (+|-)HHMM
            const string myUtcOffset =  boost::trim_copy(myStdOut);
            if (myUtcOffset.size() != 5)
            {
                TA_THROW_MSG(std::runtime_error, boost::format("Invalid UTC offset: %s") % myUtcOffset);
            }

            bool positive = false;
            switch (myUtcOffset[0])
            {
            case '+': positive = true; break;
            case '-': positive = false; break;
            default: TA_THROW_MSG(std::runtime_error, boost::format("Cannot parse UTC offset sign from %c") % myUtcOffset[0]);
            }
            const unsigned int myHours = Strings::parse<unsigned int>(myUtcOffset.substr(1,2));
            const unsigned int myMinutes = Strings::parse<unsigned int>(myUtcOffset.substr(3,2));
            return (positive ? 1 : -1 ) * (myHours*60 + myMinutes);
        }
#endif

    }// TimeUtils
}// ta
