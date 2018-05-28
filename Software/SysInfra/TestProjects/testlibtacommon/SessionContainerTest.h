#pragma once

#include "ta/sessioncontainer.h"

#include "cxxtest/TestSuite.h"

class SessionContainerTest : public CxxTest::TestSuite
{
    struct MySession
    {
        MySession() : create_time(0)
        {}
        MySession(const std::string& aData): create_time(time(NULL)), data(aData)
        {}
        inline bool isValid() const { return (create_time > 0) && (!data.empty()); }
        inline void destroy() { ++destroyCounter; }
        inline bool operator==(const MySession& rhs) const { return (create_time == rhs.create_time) && (data == rhs.data); }

        time_t create_time;
        std::string data;
        static int destroyCounter;
    };
public:
    void setUp()
    {
        MySession::destroyCounter = 0;
    }

    void test_that_sessions_can_be_added()
    {
        // given
        static const size_t SessionTtlSec = 10;
        static const unsigned int MaxSize = 2;

        {
            // when
            ta::SessionContainer<MySession> mySessions(MaxSize, SessionTtlSec);
            // then
            TS_ASSERT_EQUALS(mySessions.size(), 0U);
            TS_ASSERT_EQUALS(MySession::destroyCounter, 0U);

            // when-then
            TS_ASSERT_THROWS(mySessions.add(MySession()), ta::SessionInvalidError);
            TS_ASSERT_EQUALS(mySessions.size(), 0U);
            TS_ASSERT_EQUALS(MySession::destroyCounter, 0U);

            // given
            MySession mySession1("session 1");
            MySession mySession2("session 2");
            MySession mySession3("session 3");

            // when
            const ta::SessionContainer<MySession>::SidType mySid1 = mySessions.add(mySession1);
            // then
            TS_ASSERT_EQUALS(mySessions.size(), 1U);
            TS_ASSERT_EQUALS(MySession::destroyCounter, 0U);

            // when
            const ta::SessionContainer<MySession>::SidType mySid2 = mySessions.add(mySession2);
            // then
            TS_ASSERT_EQUALS(mySessions.size(), 2U);
            TS_ASSERT_EQUALS(MySession::destroyCounter, 0U);
            TS_ASSERT_DIFFERS(mySid1, mySid2);

            // when-then
            TS_ASSERT_THROWS(mySessions.add(mySession3), ta::SessionContainerFullError);
            TS_ASSERT_EQUALS(MySession::destroyCounter, 0U);
        }

        // test that when then container is destroyed, a session destroyer is called for each session
        TS_ASSERT_EQUALS(MySession::destroyCounter, 2U);
    }

    void test_that_sessions_can_be_fetched()
    {
        // given
        static const size_t SessionTtlSec = 10;
        static const unsigned int MaxSize = 2;

        {
            // given
            ta::SessionContainer<MySession> mySessions(MaxSize, SessionTtlSec);
            MySession mySession1("session 1");
            MySession mySession2("session 2");
            const ta::SessionContainer<MySession>::SidType mySid1 = mySessions.add(mySession1);
            const ta::SessionContainer<MySession>::SidType mySid2 = mySessions.add(mySession2);
            TS_ASSERT_EQUALS(mySessions.size(), 2U);
            TS_ASSERT_EQUALS(MySession::destroyCounter, 0U);

            // when
            MySession mySession;
            TS_ASSERT(mySessions.fetch(mySid1, mySession));

            // then
            TS_ASSERT_EQUALS(mySession, mySession1);
            TS_ASSERT_EQUALS(mySessions.size(), 1U);
            TS_ASSERT_EQUALS(MySession::destroyCounter, 0U);
            TS_ASSERT(!mySessions.fetch(mySid1, mySession));
        }
        // only sessions destroyed by a container are counted, user is responsible for correctly destroying sessions once it has been fetched from the container
        TS_ASSERT_EQUALS(MySession::destroyCounter, 1U);
    }

    void test_that_expired_sessions_are_removed()
    {
        // given
        static const unsigned int SessionTtlSec = 1;
        static const unsigned int MaxSize = 2;
        static const unsigned int MsecsInSec = 1000;

        ta::SessionContainer<MySession> mySessions(MaxSize, SessionTtlSec);
        const ta::SessionContainer<MySession>::SidType mySid1 = mySessions.add(MySession("session 1"));
        const ta::SessionContainer<MySession>::SidType mySid2 = mySessions.add(MySession("session 2"));
        TS_ASSERT_EQUALS(mySessions.size(), 2U);
        TS_ASSERT_EQUALS(MySession::destroyCounter, 0U);

        // when
        TimeUtils::sleep(SessionTtlSec*MsecsInSec + 1000);

        // then, expired sessions are not counted though they are not yet physically destroyed
        TS_ASSERT_EQUALS(mySessions.size(), 0U);
        TS_ASSERT_EQUALS(MySession::destroyCounter, 0U);

        // when, adding new session will trigger garbage collection when container size hits the ceiling
        mySessions.add(MySession("session 3"));

        // then, 2 removed, 1 added
        TS_ASSERT_EQUALS(mySessions.size(), 1U);
        TS_ASSERT_EQUALS(MySession::destroyCounter, 2U);
    }

};

int SessionContainerTest::MySession::destroyCounter = 0;

