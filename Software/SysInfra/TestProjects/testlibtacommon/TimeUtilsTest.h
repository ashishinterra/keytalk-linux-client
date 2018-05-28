#pragma once

#include "ta/timeutils.h"
#include "ta/common.h"
#include "cxxtest/TestSuite.h"
#include <cstdio>
#include <string>
#include <vector>

class TimeUtilsTest : public CxxTest::TestSuite
{
public:
    void testTimestampToIso8601()
    {
        using namespace ta::TimeUtils;
        using std::string;

        TS_ASSERT_EQUALS(timestampToIso8601(parseUtcIso8601("2008-11-05T13:15:30Z")), "2008-11-05T13:15:30+0000");
        TS_ASSERT_EQUALS(timestampToIso8601(parseUtcIso8601("2008-11-05T13:15:30+0000")), "2008-11-05T13:15:30+0000");
        TS_ASSERT_EQUALS(timestampToIso8601(parseUtcIso8601("2008-11-05T13:15:30+00:00")), "2008-11-05T13:15:30+0000");
        TS_ASSERT_EQUALS(timestampToIso8601(parseUtcIso8601("2008-11-05T13:15:30+00")), "2008-11-05T13:15:30+0000");
        TS_ASSERT_EQUALS(timestampToIso8601(parseUtcIso8601("2008-01-05T13:15:3Z")), "2008-01-05T13:15:03+0000");
        TS_ASSERT_EQUALS(timestampToIso8601(parseUtcIso8601("2008-11-05  13:15:30Z")), "2008-11-05T13:15:30+0000");

        string myNowStr = getUtcNowAsIso8601();
        string myNowStr1 = timestampToIso8601(parseUtcIso8601(myNowStr));
        TS_ASSERT_EQUALS(myNowStr1, myNowStr);
        TS_TRACE(("UTC now is " +myNowStr).c_str());

        TS_ASSERT_THROWS(parseUtcIso8601("2008-11-05T13:15:30+0100"), std::exception);
        TS_ASSERT_THROWS(parseUtcIso8601("year-11-05T13:15:30+0000"), std::exception);
        TS_ASSERT_THROWS(parseUtcIso8601(""), std::exception);
    }

    void testNowAsLocalStr()
    {
        using namespace ta::TimeUtils;

        bool withSecondsPrecision = true;

        std::string myTime = getNowAsLocalStr(withSecondsPrecision);
        TS_ASSERT_EQUALS(myTime.size(), 19U);
        TS_TRACE(("Local time now is " + myTime).c_str());

        withSecondsPrecision = false;
        myTime = getNowAsLocalStr(withSecondsPrecision);
        TS_ASSERT_EQUALS(myTime.size(), 16U);
        TS_TRACE(("Local time now (no seconds) is " + myTime).c_str());
    }

    void testTimestampToStr()
    {
        using namespace ta::TimeUtils;

        TS_ASSERT_EQUALS(timestampToUtcStr(1452600616U, true), "12-01-2016 12:10:16");
        TS_ASSERT_EQUALS(timestampToUtcStr(1452600616U, false), "12-01-2016 12:10");

        const time_t now = time(NULL);
        const std::string nowFull = getNowAsLocalStr(true);
        const std::string nowBrief = getNowAsLocalStr(false);

        TS_ASSERT(timestampToLocalStr(now, true) == nowFull || timestampToLocalStr(now+1, true) == nowFull);
        TS_ASSERT(timestampToLocalStr(now, false) == nowBrief || timestampToLocalStr(now+1, false) == nowBrief);
    }

    void testParseNowFromStr()
    {
        using namespace ta::TimeUtils;

        const time_t now = time(NULL);
        const std::string myNowStr = getNowAsLocalStr(true);
        // allow one second deviation
        TS_ASSERT(parseTimestampFromLocalStr(myNowStr) == now || parseTimestampFromLocalStr(myNowStr) == now +1);
    }

    void testParseLocalTimeFromStr()
    {
        using namespace ta::TimeUtils;

        // winter time, daylight saving does not apply
        string myTimeStr = "30-12-1976 13:15:30";
        TS_ASSERT_EQUALS(timestampToLocalStr(parseTimestampFromLocalStr(myTimeStr), true), myTimeStr);
        myTimeStr = "30-12-1976 13:15";
        TS_ASSERT_EQUALS(timestampToLocalStr(parseTimestampFromLocalStr(myTimeStr), true), myTimeStr+":00");

        // summer time, daylight saving may apply
        myTimeStr = "14-04-2003 11:45:30";
        TS_ASSERT_EQUALS(timestampToLocalStr(parseTimestampFromLocalStr(myTimeStr), true), myTimeStr);
        myTimeStr = "14-04-2003 11:45";
        TS_ASSERT_EQUALS(timestampToLocalStr(parseTimestampFromLocalStr(myTimeStr), true), myTimeStr+":00");
    }


    void testBreakTimeInterval()
    {
        using namespace ta::TimeUtils;

        unsigned int days = -1, hours = -1, minutes = -1, seconds = -1;

        unsigned int myTotalSeconds = 0;
        breakTimeInterval(myTotalSeconds, days, hours, minutes, seconds);
        TS_ASSERT_EQUALS(days, 0);
        TS_ASSERT_EQUALS(hours, 0);
        TS_ASSERT_EQUALS(minutes, 0);
        TS_ASSERT_EQUALS(seconds, 0);
        TS_ASSERT_EQUALS(makeTimeInterval(days, hours, minutes, seconds), myTotalSeconds);

        myTotalSeconds = 45;
        breakTimeInterval(45, days, hours, minutes, seconds);
        TS_ASSERT_EQUALS(days, 0);
        TS_ASSERT_EQUALS(hours, 0);
        TS_ASSERT_EQUALS(minutes, 0);
        TS_ASSERT_EQUALS(seconds, 45);
        TS_ASSERT_EQUALS(makeTimeInterval(days, hours, minutes, seconds), myTotalSeconds);

        myTotalSeconds = 60+2;
        breakTimeInterval(myTotalSeconds, days, hours, minutes, seconds);
        TS_ASSERT_EQUALS(days, 0);
        TS_ASSERT_EQUALS(hours, 0);
        TS_ASSERT_EQUALS(minutes, 1);
        TS_ASSERT_EQUALS(seconds, 2);
        TS_ASSERT_EQUALS(makeTimeInterval(days, hours, minutes, seconds), myTotalSeconds);

        myTotalSeconds = 60*60 + 2*60 + 3;
        breakTimeInterval(myTotalSeconds, days, hours, minutes, seconds);
        TS_ASSERT_EQUALS(days, 0);
        TS_ASSERT_EQUALS(hours, 1);
        TS_ASSERT_EQUALS(minutes, 2);
        TS_ASSERT_EQUALS(seconds, 3);
        TS_ASSERT_EQUALS(makeTimeInterval(days, hours, minutes, seconds), myTotalSeconds);

        myTotalSeconds = 10*24*60*60 + 2*60*60 + 3*60 + 4;
        breakTimeInterval(myTotalSeconds, days, hours, minutes, seconds);
        TS_ASSERT_EQUALS(days, 10);
        TS_ASSERT_EQUALS(hours, 2);
        TS_ASSERT_EQUALS(minutes, 3);
        TS_ASSERT_EQUALS(seconds, 4);
        TS_ASSERT_EQUALS(makeTimeInterval(days, hours, minutes, seconds), myTotalSeconds);

        myTotalSeconds = 30*24*60*60 + 59;
        breakTimeInterval(myTotalSeconds, days, hours, minutes, seconds);
        TS_ASSERT_EQUALS(days, 30);
        TS_ASSERT_EQUALS(hours, 0);
        TS_ASSERT_EQUALS(minutes, 0);
        TS_ASSERT_EQUALS(seconds, 59);
        TS_ASSERT_EQUALS(makeTimeInterval(days, hours, minutes, seconds), myTotalSeconds);
    }

    void testFormatTimeInterval()
    {
        using namespace ta::TimeUtils;
        using std::string;

        TS_ASSERT_EQUALS(formatTimeInterval(0), "0 seconds");
        TS_ASSERT_EQUALS(formatTimeInterval(0, precisionCompact), "0 seconds");
        TS_ASSERT_EQUALS(formatTimeInterval(1), "1 second");
        TS_ASSERT_EQUALS(formatTimeInterval(1, precisionCompact), "1 second");
        TS_ASSERT_EQUALS(formatTimeInterval(59), "59 seconds");
        TS_ASSERT_EQUALS(formatTimeInterval(59, precisionCompact), "59 seconds");

        TS_ASSERT_EQUALS(formatTimeInterval(60), "1 minute");
        TS_ASSERT_EQUALS(formatTimeInterval(60, precisionCompact), "1 minute");
        TS_ASSERT_EQUALS(formatTimeInterval(60+1), "1 minute and 1 second");
        TS_ASSERT_EQUALS(formatTimeInterval(60+1, precisionCompact), "1 minute and 1 second");

        TS_ASSERT_EQUALS(formatTimeInterval(60*60), "1 hour");
        TS_ASSERT_EQUALS(formatTimeInterval(60*60, precisionCompact), "1 hour");
        TS_ASSERT_EQUALS(formatTimeInterval(60*60+6), "1 hour and 6 seconds");
        TS_ASSERT_EQUALS(formatTimeInterval(60*60+6, precisionCompact), "1 hour");
        TS_ASSERT_EQUALS(formatTimeInterval(60*60+60+1), "1 hour, 1 minute and 1 second");
        TS_ASSERT_EQUALS(formatTimeInterval(60*60+60+1, precisionCompact), "1 hour and 1 minute");
        TS_ASSERT_EQUALS(formatTimeInterval(2*60*60+3*60+4), "2 hours, 3 minutes and 4 seconds");
        TS_ASSERT_EQUALS(formatTimeInterval(2*60*60+3*60+4, precisionCompact), "2 hours and 3 minutes");
        TS_ASSERT_EQUALS(formatTimeInterval(2*60*60+3*60), "2 hours and 3 minutes");
        TS_ASSERT_EQUALS(formatTimeInterval(2*60*60+3*60, precisionCompact), "2 hours and 3 minutes");
        TS_ASSERT_EQUALS(formatTimeInterval(2*60*60+4), "2 hours and 4 seconds");
        TS_ASSERT_EQUALS(formatTimeInterval(2*60*60+4, precisionCompact), "2 hours");

        TS_ASSERT_EQUALS(formatTimeInterval(24*60*60), "1 day");
        TS_ASSERT_EQUALS(formatTimeInterval(24*60*60, precisionCompact), "1 day");
        TS_ASSERT_EQUALS(formatTimeInterval(2*24*60*60 + 22*60 + 32), "2 days, 22 minutes and 32 seconds");
        TS_ASSERT_EQUALS(formatTimeInterval(2*24*60*60 + 22*60 + 32, precisionCompact), "2 days");
        TS_ASSERT_EQUALS(formatTimeInterval(2*24*60*60 + 12*60*60), "2 days and 12 hours");
        TS_ASSERT_EQUALS(formatTimeInterval(2*24*60*60 + 12*60*60, precisionCompact), "2 days and 12 hours");
        TS_ASSERT_EQUALS(formatTimeInterval(2*24*60*60 + 12*60*60 + 22*60 + 32), "2 days, 12 hours, 22 minutes and 32 seconds");
        TS_ASSERT_EQUALS(formatTimeInterval(2*24*60*60 + 12*60*60 + 22*60 + 32, precisionCompact), "2 days and 12 hours");
    }

    void testLocalTime()
    {
        using namespace ta::TimeUtils;
        using std::string;

        static const long long MsecInDay = 1000*60*60*24;
        static const long long MsecInYear = MsecInDay * 365;

        const LocalTime myLocalTimeNow, myLocalTimeYesterday(-MsecInDay), myLocalTimeTomorrow(MsecInDay);

        TS_ASSERT(myLocalTimeNow.getYear() >= 1970 && myLocalTimeNow.getYear() <= 2038);
        TS_ASSERT(myLocalTimeNow.getMonth() >= 1 && myLocalTimeNow.getMonth() <= 12);
        TS_ASSERT(myLocalTimeNow.getDay() >= 1 && myLocalTimeNow.getDay() <= 31);
        TS_ASSERT(myLocalTimeNow.getHour() <= 23);
        TS_ASSERT(myLocalTimeNow.getMinute() <= 59);
        TS_ASSERT(myLocalTimeNow.getSecond() <= 59);
        TS_ASSERT(myLocalTimeNow.getMillisec() <= 999);
        TS_ASSERT(myLocalTimeNow.getTotalMillisec() >= MsecInYear * 40 /*expect we are run after 2010*/);

        // myLocalTimeNow, myLocalTimeYesterday and myLocalTimeTomorrow might not be created exactly at the same time
        TS_ASSERT(myLocalTimeNow - myLocalTimeYesterday <= MsecInDay+1);
        TS_ASSERT(myLocalTimeNow - myLocalTimeYesterday >= MsecInDay-1);

        TS_ASSERT(myLocalTimeTomorrow - myLocalTimeNow <= MsecInDay+1);
        TS_ASSERT(myLocalTimeTomorrow - myLocalTimeNow >= MsecInDay-1);
    }

    void testLocalTimeZone()
    {
#ifdef _WIN32
        TS_SKIP("getLocalTimeZoneAsUtcOffset() is not implemented on Windows");
#else
        const int myOffset = ta::TimeUtils::getLocalTimeZoneAsUtcOffset();
        // Timezones fall between -12 to +14, see https://en.wikipedia.org/wiki/List_of_UTC_time_offset
        static const int MinutesInHour = 60;
        TS_ASSERT(myOffset <= (14*MinutesInHour));
        TS_ASSERT(myOffset >= ((-12)*MinutesInHour));
#endif
    }

    void testTimeZoneToMinutes()
    {
        // Full timezone list can be found here http://www.timeanddate.com/time/zones/
        TS_ASSERT_EQUALS(ta::TimeUtils::getTimeZoneAsUtcOffset("UTC"), 0);
        TS_ASSERT_EQUALS(ta::TimeUtils::getTimeZoneAsUtcOffset("NFT"), 690);
        TS_ASSERT_EQUALS(ta::TimeUtils::getTimeZoneAsUtcOffset("AKDT"), -480);

        TS_ASSERT_THROWS(ta::TimeUtils::getTimeZoneAsUtcOffset(""), std::exception);
        TS_ASSERT_THROWS(ta::TimeUtils::getTimeZoneAsUtcOffset("unknown-timezone"), std::exception);
        TS_ASSERT_THROWS(ta::TimeUtils::getTimeZoneAsUtcOffset(ta::TimeUtils::LocalTimeZone), std::exception);
    }

    void testFormatTimeZone()
    {
        TS_ASSERT_EQUALS(ta::TimeUtils::fmtTz(0), "UTC+0:00");
        TS_ASSERT_EQUALS(ta::TimeUtils::fmtTz(15), "UTC+0:15");
        TS_ASSERT_EQUALS(ta::TimeUtils::fmtTz(120), "UTC+2:00");
        TS_ASSERT_EQUALS(ta::TimeUtils::fmtTz(145), "UTC+2:25");
        TS_ASSERT_EQUALS(ta::TimeUtils::fmtTz(-30), "UTC-0:30");
        TS_ASSERT_EQUALS(ta::TimeUtils::fmtTz(-60), "UTC-1:00");
        TS_ASSERT_EQUALS(ta::TimeUtils::fmtTz(-615), "UTC-10:15");
    }

};
