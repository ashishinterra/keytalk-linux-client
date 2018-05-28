#pragma once

#include <string>
#include <ctime>
#include "boost/cstdint.hpp"

namespace ta
{
    namespace TimeUtils
    {
        static const unsigned int MsecsInSecond   = 1000;
        static const unsigned int SecondsInMinute = 60;
        static const unsigned int SecondsInHour   = 60 * SecondsInMinute;
        static const unsigned int SecondsInDay    = 24 * SecondsInHour;

        void sleep(const unsigned int aMsec);

        // ISO 8601 date/time example: "2008-11-05T13:15:30+0000"
        std::string getUtcNowAsIso8601();
        std::string timestampToIso8601(const time_t aTimestamp);

        time_t parseUtcIso8601(const std::string& anUtcTimeStr);

        //@nothrow
        std::string getUtcNowAsYYYYmmdd();

        //
        // Format/parse date/time as "30-10-2010 19:38[:59]"
        //
        //@nothrow
        std::string getNowAsLocalStr(const bool aWithSecondPrecision = false);
        //@nothrow
        std::string timestampToUtcStr(const time_t aTimestamp, const bool aWithSecondPrecision = false);
        //@nothrow
        std::string timestampToLocalStr(const time_t aTimestamp, const bool aWithSecondPrecision = false);

        time_t parseTimestampFromLocalStr(const std::string& aDateTimeStr);

        void breakTimeInterval(const unsigned int aTotalSeconds, unsigned int& aDays, unsigned int& aHours, unsigned int& aMinutes, unsigned int& aSeconds);
        unsigned int makeTimeInterval(const unsigned int aDays, const unsigned int aHours = 0, const unsigned int aMinutes = 0, const unsigned int aSeconds = 0);

        /*
            Return string representation of time delta as "d days, h hours, m minutes and s seconds" for pretty-printing.
            For example: "10 days, 1 hour, 20 minutes and 1 second" or "1 minute and 12 seconds"
            @param aPrecision when set to compact, at most 2 major time components are used e.g. "10 days, 1 hour, 20 minutes and 1 second" will be output as "10 days and 1 hour"
        */
        enum Precision
        {
            precisionFull,
            precisionCompact
        };
        std::string formatTimeInterval(const unsigned int aTotalSeconds, const Precision aPrecision = precisionFull);


        class LocalTime
        {
        public:
            LocalTime();
            LocalTime(const long long anOffsetMsec);
            ~LocalTime();

            // @return year [1970..2038]
            unsigned short getYear() const;

            // @return month [1..12]
            unsigned short getMonth() const;

            //@return day of month [1..31]
            unsigned short getDay() const;

            // @return hour of day [0..23]
            unsigned short getHour() const;

            // @return minute [0..59]
            unsigned short getMinute() const;

            // @return second [0..59]
            unsigned short getSecond() const;

            // @return millisecond [0..999]
            unsigned short getMillisec() const;

            // @return the total number of milliseconds
            unsigned long long int getTotalMillisec() const;

            // return the difference in milliseconds between two local times
            long long operator- (const LocalTime& aTime) const;
        private:
            void init(const long long anOffsetMsec = 0);
        private:
            unsigned short theYear;
            unsigned short theMonth;
            unsigned short theDay;
            unsigned short theHour;
            unsigned short theMinute;
            unsigned short theSecond;
            unsigned short theMillisec;

            unsigned long long int theTotalMsec;
        };


        // Time zone represented as UTC offset in minutes
        struct TimeZone
        {
            std::string id;
            int utc_offset_minutes;
        };
        static const std::string LocalTimeZone = "LOCAL";
        static const TimeZone TimeZones[] =
        {
            {"UTC", 0},
            {"CET", 60},
            {"CEDT", 120},
            {"EEDT", 180},
            {"IRST", 210},
            {"AMT", 240},
            {"AFT", 270},
            {"AMST", 300},
            {"IST", 330},
            {"NPT", 345},
            {"BDT", 360},
            {"CCT", 390},
            {"CXT", 420},
            {"AWST", 480},
            {"CWST", 525},
            {"AWDT", 540},
            {"ACST", 570},
            {"AEST", 600},
            {"ACDT", 630},
            {"AEDT", 660},
            {"NFT", 690},
            {"FJT", 720},
            {"CHAST", 765},
            {"NZDT", 780},
            {"CHADT", 825},
            {"LINT", 840},
            {"AZOST", -60},
            {"BRST", -120},
            {"NDT", -150},
            {"ADT", -180},
            {"NST", -210},
            {"AST", -240},
            {"VET", -270},
            {"ACT", -300},
            {"CST", -360},
            {"MST", -420},
            {"AKDT", -480},
            {"AKST", -540},
            {"MART", -570},
            {"CKT", -600},
            {"NUT", -660},
            {"BIT", -720}
        };

        //@return UTC offset in minutes for the given timezone
        int getTimeZoneAsUtcOffset(const std::string& aTimeZoneId);

        //@nothrow
        //@return timezone as "UTC(+|-)HH::MM"
        std::string fmtTz(const int anUtcOffsetMinutes);

#ifndef _WIN32
        //@return UTC offset in minutes for the local timezone
        int getLocalTimeZoneAsUtcOffset();
#endif
    }
}
